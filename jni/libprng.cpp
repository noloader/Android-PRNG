// Debug settings in this block
#if 1
# undef NDEBUG
# undef DEBUG
# undef NDK_DEBUG

# define DEBUG 1
# define NDK_DEBUG 1
#endif

// Release settings in this block
#if 0
# undef NDEBUG
# undef DEBUG
# undef NDK_DEBUG

# define NDEBUG 1
#endif

//#if defined(DEBUG) || defined(NDK_DEBUG)
//# error Debug is defined
//#endif

#include <android/sensor.h>
#include <android/looper.h>
#include <android/log.h>

#include <jni.h>
#include <time.h>
#include <unistd.h>

#define LOG_TAG "PRNG"
#define LOG_DEBUG(...) ((void)__android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__))
#define LOG_INFO(...) ((void)__android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__))
#define LOG_WARN(...) ((void)__android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__))
#define LOG_ERROR(...) ((void)__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__))

#if defined(NDEBUG)
# undef LOG_VERBOSE
# define LOG_VERBOSE(...)
# undef LOG_DEBUG
# define LOG_DEBUG(...)
#endif

#define COUNTOF(x) (sizeof(x) / sizeof(x[0]))

#include <stdexcept>
using std::runtime_error;

#include <string>
using std::string;

#include <vector>
using std::vector;

#include <fstream>
using std::ifstream;

#include <cryptopp/osrng.h>
using CryptoPP::AutoSeededRandomPool;

#include <cryptopp/cryptlib.h>
using CryptoPP::Exception;

#include "libprng.h"
#include "cleanup.h"

static double TimeInMilliSeconds(double offset /*milliseconds*/);
static int SamplesPerSecondToMicroSecond(int samples);

#ifndef NDEBUG
static const char* SensorTypeToName(int sensorType);
#endif

/* https://groups.google.com/forum/#!topic/android-ndk/ukQBmKJH2eM */
static const int EXPECTED_JNI_VERSION = JNI_VERSION_1_6;

/* Used for a delay in sampling sensors. That is, we read a   */
/* sample, and then delay N microseconds before reading from  */
/* the sensor again. This gives the sensor time to latch a    */
/* a new reading.                                             */
static const int PREFERRED_INTERVAL_MICROSECONDS =
		SamplesPerSecondToMicroSecond(30 /*samples per second*/);

/* App_glue.h defines this. If its not defined, then define it. */
#ifndef LOOPER_ID_USER
# define LOOPER_ID_USER 3
#endif

static const int LOOPER_ID_PRNG = LOOPER_ID_USER + 1;

/* Assuming each float (32-bits) in the {x,y,z} tuple has         */
/* 6-bits of entropy, and each time stamp (64-bits) has 4-bits    */
/* of entropy, then each event has 22-bits. We want 32-bytes or   */
/* 256-bits for to mix into the PRNG for each call to GetBytes,   */
/* so we need 11.6 or 12 events. Note that sizeof(ASensorEvent)   */
/* is over 100 bytes, and it holds other info. Also, the          */
/* accelerometer (if present) appears to produce almost 28-bits   */
/* per member in the tuple.                                       */

/* How many sensor events we would like to sample. If we     */
/* reach TIME_LIMIT_IN_MILLISECONDS, then we stop sampling.  */
/* If we sample 0 events, then we fall back to the random    */
/* device and try to grab RANDOM_DEVICE_BYTES bytes.         */
static const int SENSOR_SAMPLE_COUNT = 12;

/* Sampling time limit, in milliseconds. 200 to 400 ms is    */
/* the sweet spot. Its the same time for the blink of an     */
/* eye. Keep in mind that Java may add from 30 to 80 ms. A   */
/* sensor rich device will be done in 125 ms because there   */
/* are so many readings.                                     */
static const double TIME_LIMIT_IN_MILLISECONDS = 0.250f * 1000;

/* How many bytes to read from /dev/urandom. We read from the */
/* random device as a fallback to ensure something is read    */
/* before providing bytes in GetBytes().                      */
static const int RANDOM_DEVICE_BYTES = 16;

/* Prototypes */
static int AddSensorData();
static int AddRandomDevice();
static int AddProcessInfo();

struct SensorContext {

	SensorContext() :
			m_looper(NULL), m_manager(NULL), m_queue(NULL), m_signaled(0), m_stop(
					0.0f) {
	}

	~SensorContext() {
		m_looper = NULL;
		m_manager = NULL;
		m_queue = NULL;
		m_signaled = 1;
		m_stop = 0.0f;
	}

	// Looper
	ALooper* m_looper;

	// Sensor Manager
	ASensorManager* m_manager;

	// And the queue
	ASensorEventQueue* m_queue;

	// If not signaled, then processing should continue.
	// If signaled, then processing should stop.
	int m_signaled;

	/* Time when the callback should stop */
	double m_stop;
};

struct Sensor {
	Sensor() :
			m_type(0), m_sensor(NULL) {
	}

	explicit Sensor(int type, string name, const ASensor* sensor) :
			m_type(type), m_name(name), m_sensor(sensor) {
	}

	int m_type;
	string m_name;
	const ASensor* m_sensor;
};

typedef vector<Sensor> SensorArray;

#ifndef NDEBUG
struct RawFloat {
	union {
		byte b[sizeof(float)];
		unsigned int n[sizeof(float) / sizeof(unsigned int)];
		float f;
	};
};
#endif

/* Return the time in milliseconds. If offset = 0.0f, then it returns  */
/*  now. Positive offsets are milliseconds into the future. Negative   */
/*  offsets are milliseconds in the past.                              */
static double TimeInMilliSeconds(double offset /*seconds*/= 0.0f) {
	struct timespec res;
	clock_gettime(CLOCK_REALTIME, &res);

	double t = 1000.0 * (double) res.tv_sec + (double) res.tv_nsec / 1e6;
	return t + offset;
}

/* Given a sample rate, returns the microseconds in an interval */
static int SamplesPerSecondToMicroSecond(int samples) {
	return (int) ((1 / (double) samples) * 1000 * 1000);
}

static AutoSeededRandomPool& GetPRNG() {
	static AutoSeededRandomPool prng;
	return prng;
}

static SensorArray& GetSensorArray() {
	static SensorArray s_list;
	static volatile bool s_init = false;

	if (!s_init) {
		LOG_DEBUG("SensorArray: initializing list");

		ASensorList sensorArray;
		ASensorManager* sensorManager = ASensorManager_getInstance();
		int n = ASensorManager_getSensorList(sensorManager, &sensorArray);

		if (n < 0) {
			LOG_ERROR("SensorArray: failed to retrieve list");
		} else if (n == 0) {
			LOG_WARN("SensorArray: no sensors available");
		} else {
			s_list.reserve(static_cast<size_t>(n));

			for (int i = 0; i < n; i++) {
				const ASensor* sensor = sensorArray[i];
				if (sensor == NULL)
					continue;

				const char* name = ASensor_getName(sensor);
				int type = ASensor_getType(sensor);

#ifndef NDEBUG
				const char* vendor = ASensor_getVendor(sensor);
				int min_delay = ASensor_getMinDelay(sensor);
				float resolution = ASensor_getResolution(sensor);

				LOG_DEBUG("SensorArray: %s (%s) %d %d %f", name, vendor, type,
						min_delay, resolution);
#endif

				s_list.push_back(Sensor(type, name, sensor));
			}

			LOG_DEBUG("SensorArray: added %d sensors", (int )s_list.size());
		}

		s_init = true;
	}

	return s_list;
}

static int SensorEvent(int fd, int events, void* data) {

	LOG_DEBUG("Entered SensorEvent");

	SensorContext* context = reinterpret_cast<SensorContext*>(data);
	if (!context) {
		LOG_ERROR("SensorEvent: context is not valid");
		return 0;
	}

	/**************** Return Values ****************/
	/* 1: continue processing; 0: stop processing. */
	/***********************************************/

	/* Return 0 while the context is signaled.  */
	if (context->m_signaled) {
		LOG_DEBUG("SensorEvent: signaled to stop");
		return 0;
	}

	/* Return 1 while the context is not signaled. */
	return 1;
}

jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
	LOG_DEBUG("Entered JNI_OnLoad");

	if (vm == NULL) {
		LOG_ERROR("JNI_OnLoad: virtual machine is not valid");
		return -1;
	}

	JNIEnv* env = NULL;
	if (vm->GetEnv(reinterpret_cast<void**>(&env), EXPECTED_JNI_VERSION)) {
		LOG_ERROR("JNI_OnLoad: GetEnv failed");
		return -1;
	}

	if (env == NULL) {
		LOG_ERROR("JNI_OnLoad: JNI environment is not valid");
		return -1;
	}

	JNINativeMethod methods[2];

	methods[0].name = "CryptoPP_Reseed";
	methods[0].signature = "([B)I";
	methods[0].fnPtr =
			reinterpret_cast<void*>(Java_com_cryptopp_prng_PRNG_CryptoPP_1Reseed);

	methods[1].name = "CryptoPP_GetBytes";
	methods[1].signature = "([B)I";
	methods[1].fnPtr =
			reinterpret_cast<void*>(Java_com_cryptopp_prng_PRNG_CryptoPP_1GetBytes);

	jclass cls = env->FindClass("com/cryptopp/prng/PRNG");
	if (cls == NULL) {
		LOG_ERROR("JNI_OnLoad: FindClass com/cryptopp/prng/PRNG failed");
	} else {
		env->RegisterNatives(cls, methods, COUNTOF(methods));
	}

	return EXPECTED_JNI_VERSION;
}

/*
 * Class:     com_cryptopp_prng_PRNG
 * Method:    CryptoPP_Reseed
 * Signature: ([B)I
 */
jint JNICALL Java_com_cryptopp_prng_PRNG_CryptoPP_1Reseed(JNIEnv* env, jclass,
		jbyteArray seed) {

	LOG_DEBUG("Entered Reseed");

	int consumed = 0;

	try {
		if (!env) {
			LOG_ERROR("Reseed: environment is NULL");
			return 0;
		}

		if (!seed) {
			// OK if the caller passed NULL for the array
			LOG_WARN("GetBytes: byte array is NULL");
			return 0;
		}

		ReadByteBuffer buffer(env, seed);

		const byte* seed_arr = buffer.GetByteArray();
		size_t seed_len = buffer.GetArrayLen();

		if ((seed_arr == NULL)) {
			LOG_ERROR("Reseed: array pointer is not valid");
		} else if ((seed_len == 0)) {
			LOG_ERROR("Reseed: array size is not valid");
		} else {
			AutoSeededRandomPool& prng = GetPRNG();
			prng.IncorporateEntropy(seed_arr, seed_len);

			LOG_INFO("Reseed: seeded with %d bytes", (int )seed_len);

			consumed += (int) seed_len;
		}
	} catch (const Exception& ex) {
		LOG_ERROR("Reseed: Crypto++ exception: \"%s\"", ex.what());
		return 0;
	}

	return consumed;
}

/*
 * Class:     com_cryptopp_prng_PRNG
 * Method:    CryptoPP_GetBytes
 * Signature: ([B)I
 */
JNIEXPORT jint JNICALL Java_com_cryptopp_prng_PRNG_CryptoPP_1GetBytes(
		JNIEnv* env, jclass, jbyteArray bytes) {

	LOG_DEBUG("Entered GetBytes");

	int retrieved = 0;

	// Always mix in data during a call to GetBytes, even if the array is null.
	// Any entropy gathered will help future calls with a non-null array.
	int rc1, rc2, rc3;

	rc1 = AddProcessInfo();
	assert(rc1 > 0);

	rc2 = AddSensorData();
	assert(rc2 > 0);

	/* Fallback to a random device on failure. This is not */
	/*   catastrophic since the Crypto++ generator is OK   */
	if (rc1 <= 0 || rc2 <= 0) {
		rc3 = AddRandomDevice();
		assert(rc3 > 0);
	}

	try {

		if (!env) {
			LOG_ERROR("GetBytes: environment is NULL");
		}

		if (!bytes) {
			// OK if the caller passed NULL for the array
			LOG_WARN("GetBytes: byte array is NULL");
		}

		if (env && bytes) {

			WriteByteBuffer buffer(env, bytes);

			byte* seed_arr = buffer.GetByteArray();
			size_t seed_len = buffer.GetArrayLen();

			if ((seed_arr == NULL)) {
				LOG_ERROR("GetBytes: array pointer is not valid");
			} else if ((seed_len == 0)) {
				LOG_ERROR("GetBytes: array size is not valid");
			} else {
				AutoSeededRandomPool& prng = GetPRNG();
				prng.GenerateBlock(seed_arr, seed_len);

				LOG_INFO("GetBytes: generated %d bytes", (int )seed_len);

				retrieved += (int) seed_len;
			}
		}
	} catch (const Exception& ex) {
		LOG_ERROR("GetBytes: Crypto++ exception: \"%s\"", ex.what());
		return 0;
	}

	return retrieved;
}

static int AddSensorData() {
	LOG_DEBUG("Entered AddSensorData");

	const SensorArray& sensorArray = GetSensorArray();
	if (sensorArray.size() == 0) {
		LOG_WARN("SensorData: no sensors available");
		return 0;
	}

	ALooper* looper = ALooper_forThread();
	if (looper == NULL)
		looper = ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);

	if (looper == NULL) {
		LOG_ERROR("SensorData: looper is not valid");
		return 0;
	}

	LOG_DEBUG("SensorData: created looper");

	ASensorManager* sensorManager = ASensorManager_getInstance();

	if (sensorManager == NULL) {
		LOG_ERROR("SensorData: sensor manager is not valid");
		return 0;
	}

	LOG_DEBUG("SensorData: created sensor manager");

	SensorContext context;

	ASensorEventQueue* queue = ASensorManager_createEventQueue(sensorManager,
			looper, LOOPER_ID_PRNG, SensorEvent,
			reinterpret_cast<void*>(&context));

	if (queue == NULL) {
		LOG_ERROR("SensorData: queue is not valid");
		return 0;
	}

	LOG_DEBUG("SensorData: created event queue");

	context.m_manager = sensorManager;
	context.m_looper = looper;
	context.m_queue = queue;
	context.m_stop = TimeInMilliSeconds(TIME_LIMIT_IN_MILLISECONDS);

	/* Accumulate the various delays. */
	int sensor_delay = 0;

	for (size_t i = 0; i < sensorArray.size(); i++) {

		const ASensor* sensor = sensorArray[i].m_sensor;
		if (sensor == NULL) {
			LOG_WARN("SensorData: sensor number %i is not valid", (int)i);
			continue;
		}

		// Take the larger of our preferred versus the sensor's rate
		//   Even if a sensor advertises 'min_delay = 0', we still
		//   try and extract data from it. Often we can get a reading.
		int rate = ASensor_getMinDelay(sensor);
		int pref = PREFERRED_INTERVAL_MICROSECONDS;
		int adj = std::max<int>(rate, pref);

		/* sensor_delay is the max delay of all sensors. */
		/*   Its used below to usleep(3) on error.       */
		sensor_delay = std::max<int>(adj, sensor_delay);

		ASensorEventQueue_enableSensor(queue, sensor);
		ASensorEventQueue_setEventRate(queue, sensor, adj);
	}

	LOG_DEBUG("SensorData: enabled sensors");

	///////////////////////////////////////////////////////////

	ASensorEvent sensor_events[SENSOR_SAMPLE_COUNT * 2];
	int totalSensors = 0, n = 0;
	const double time_start = TimeInMilliSeconds();
	double time_now = time_start;

	const int SENSOR_DELAY = sensor_delay;
	LOG_DEBUG("SensorData: sensor delay is %d microseconds", SENSOR_DELAY);

	while (context.m_signaled == 0) {

		n = ASensorEventQueue_hasEvents(queue);

#ifdef NDEBUG
		if (n <= 0) {
			LOG_DEBUG("SensorData: no events, waiting for measurement");
			usleep(SENSOR_DELAY);
			continue;
		}
#else
		if (n == 0) {
			LOG_DEBUG("SensorData: no events, waiting for measurement (1)");
			usleep(SENSOR_DELAY);
			continue;
		} else if (n < 0) {
			LOG_DEBUG("SensorData: no events, waiting for measurement (2)");
			usleep(SENSOR_DELAY);
			continue;
		}
#endif

		/* If we get this far, we should not encounter an error */
		n = ASensorEventQueue_getEvents(queue, sensor_events,
				COUNTOF(sensor_events));
		if (n == 0) {
			LOG_WARN("SensorData: no events (warn)");
			usleep(SENSOR_DELAY);
			continue;
		} else if (n < 0) {
			LOG_ERROR("SensorData: no events (error)");
			usleep(SENSOR_DELAY);
			continue;
		}

#ifndef NDEBUG
		for (int i = 0; i < n; i++) {
			const ASensorEvent ee = sensor_events[i];
			const ASensorVector vv = ee.vector;
			LOG_DEBUG("SensorData: %s, v[0]: %.9f, v[1]: %.9f, v[2]: %.9f ",
					SensorTypeToName(ee.type), vv.v[0], vv.v[1], vv.v[2]);

			RawFloat x, y, z;
			x.f = vv.x, y.f = vv.y, z.f = vv.z;
			LOG_DEBUG("                x: %08x%08x, y: %08x%08x, z: %08x%08x",
					x.n[0], x.n[1], y.n[0], y.n[1], z.n[0], z.n[1]);
		}
#endif

		try {
			AutoSeededRandomPool& prng = GetPRNG();
			prng.IncorporateEntropy((const byte*) sensor_events,
					n * sizeof(ASensorEvent));
		} catch (Exception& ex) {
			LOG_ERROR("SensorData: Crypto++ exception: \"%s\"", ex.what());
		}

		LOG_DEBUG("SensorData: added %d events, %d bytes", n,
				static_cast<int>(n * sizeof(ASensorEvent)));

		// Book keeping
		totalSensors += n;
		time_now = TimeInMilliSeconds();

		if (totalSensors >= SENSOR_SAMPLE_COUNT) {
			LOG_DEBUG("SensorData: reached event count of %d",
					SENSOR_SAMPLE_COUNT);
			context.m_signaled = 1;
		} else if (context.m_stop < time_now) {
			LOG_DEBUG("SensorData: reached time limit of %.2f ms",
					TIME_LIMIT_IN_MILLISECONDS);
			context.m_signaled = 1;
		}

		/* Give the sensors some time to latch another measurement */
		if (context.m_signaled == 0)
			usleep(SENSOR_DELAY);
	}

	///////////////////////////////////////////////////////////

	for (size_t i = 0; i < sensorArray.size(); i++) {

		const ASensor* sensor = sensorArray[i].m_sensor;
		if (sensor == NULL)
			continue;

		ASensorEventQueue_disableSensor(queue, sensor);
	}

	LOG_DEBUG("SensorData: disabled sensors");

	const double elapsed = time_now - time_start;
	LOG_INFO("SensorData: added %d total events, %d total bytes, in %.2f ms",
			totalSensors, int(totalSensors * sizeof(ASensorEvent)), elapsed);

	return totalSensors * sizeof(ASensorEvent);
}

static int AddRandomDevice() {
	LOG_DEBUG("Entered AddRandomDevice");

	ifstream ifs;
	byte buff[RANDOM_DEVICE_BYTES];

	try {
		ifs.open("/dev/urandom");
		ifs.read(reinterpret_cast<char*>(buff), sizeof(buff));
	} catch (const std::exception&) {
		LOG_ERROR("RandomDevice: failed to read random device");
		return 0;
	}

	try {
		AutoSeededRandomPool& prng = GetPRNG();
		prng.IncorporateEntropy(buff, sizeof(buff));

		LOG_INFO("RandomDevice: added %d total bytes", (int)sizeof(buff));
	} catch (const Exception& ex) {
		LOG_ERROR("RandomDevice: Crypto++ exception: \"%s\"", ex.what());
		return 0;
	}

	return sizeof(buff);
}

static int AddProcessInfo() {
	LOG_DEBUG("Entered AddProcessInfo");

	/* Bytes added across all calls */
	static unsigned long accum = 0;

	pid_t pid1, pid2;
	timespec tspec[4];

	pid1 = getpid();
	pid2 = getppid();

	/* We don't care about return values here */
	(void) clock_gettime(CLOCK_REALTIME, &tspec[0]);
	(void) clock_gettime(CLOCK_MONOTONIC, &tspec[1]);
	(void) clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &tspec[2]);
	(void) clock_gettime(CLOCK_THREAD_CPUTIME_ID, &tspec[3]);

	/* Send data into the prng in one shot. Its faster than 6 different calls. */
	byte buff[128];
	size_t idx = 0;

	memcpy(&buff[idx], &pid1, sizeof(pid1));
	idx += sizeof(pid1);

	memcpy(&buff[idx], &pid2, sizeof(pid2));
	idx += sizeof(pid2);

	memcpy(&buff[idx], tspec, sizeof(tspec));
	idx += sizeof(tspec);

	memcpy(&buff[idx], &accum, sizeof(accum));
	idx += sizeof(accum);

	accum += idx;

	try {
		AutoSeededRandomPool& prng = GetPRNG();
		prng.IncorporateEntropy(buff, sizeof(buff));

		LOG_INFO("ProcessInfo: added %d total bytes", (int)idx);
	} catch (const Exception& ex) {
		LOG_ERROR("ProcessInfo: Crypto++ exception: \"%s\"", ex.what());
		return 0;
	}

	return (int) idx;
}

#ifndef NDEBUG
static const char* SensorTypeToName(int sensorType) {
	switch (sensorType) {

	/* <ndk root>/.../sensor.h */
	case ASENSOR_TYPE_ACCELEROMETER: /* 1 */
		return "Accelerometer";
	case ASENSOR_TYPE_MAGNETIC_FIELD: /* 2 */
		return "Magnetic field";
	case ASENSOR_TYPE_GYROSCOPE: /* 4 */
		return "Gyroscope";
	case ASENSOR_TYPE_LIGHT: /* 5 */
		return "Light";
	case ASENSOR_TYPE_PROXIMITY: /* 8 */
		return "Proximity";

		/* http://developer.android.com/reference/android/hardware/Sensor.html */
	case 0:
		return "type 0";
	case 3:
		return "Orientation";
	case 6:
		return "Pressure";
	case 7:
		return "Temperature";
	case 9:
		return "Gravity";
	case 10:
		return "Linear acceleration";
	case 11:
		return "Rotation vector";
	case 12:
		return "Relative humidity";
	case 13:
		return "Ambient temperature";
	case 14:
		return "Uncalibrated magnetic field";
	case 15:
		return "Rotation vector";
	case 16:
		return "Uncalibrated gyroscope";
	case 17:
		return "Significant motion";
	case 18:
		return "type 18";
	case 19:
		return "Step counter";
	case 20:
		return "Geo-magnetic rotation vector.";
	case 21:
		return "Heart rate";
	default:
		;
	}
	return "Unknown";
}
#endif
