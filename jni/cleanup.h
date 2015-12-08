#include <jni.h>

/* Header for class com_deltoid_prng_cleanup */

#ifndef _Included_com_deltoid_prng_cleanup
#define _Included_com_deltoid_prng_cleanup

class ReadByteBuffer
{
public:
	explicit ReadByteBuffer(JNIEnv*& env, jbyteArray& barr)
	: m_env(env), m_arr(barr), m_ptr(NULL), m_len(0)
	{
		if(m_env && m_arr)
		{
			m_ptr = m_env->GetByteArrayElements(m_arr, NULL);
			m_len = m_env->GetArrayLength(m_arr);
		}
	}

	~ReadByteBuffer()
	{
		if(m_env && m_arr)
		{
			m_env->ReleaseByteArrayElements(m_arr, m_ptr, JNI_ABORT);
		}
	}

	const byte* GetByteArray() const {
		return (const byte*) m_ptr;
	}

	size_t GetArrayLen() const {
		if(m_len < 0)
			return 0;
		return (size_t) m_len;
	}

private:
	JNIEnv*& m_env;
	jbyteArray& m_arr;

	jbyte* m_ptr;
	jint m_len;
};

class WriteByteBuffer
{
public:
	explicit WriteByteBuffer(JNIEnv*& env, jbyteArray& barr)
	: m_env(env), m_arr(barr), m_ptr(NULL), m_len(0)
	{
		if(m_env && m_arr)
		{
			m_ptr = m_env->GetByteArrayElements(m_arr, NULL);
			m_len = m_env->GetArrayLength(m_arr);
		}
	}

	~WriteByteBuffer()
	{
		if(m_env && m_arr)
		{
			m_env->ReleaseByteArrayElements(m_arr, m_ptr, 0);
		}
	}

	byte* GetByteArray() const {
		return (byte*) m_ptr;
	}

	size_t GetArrayLen() const {
		if(m_len < 0)
			return 0;
		return (size_t) m_len;
	}

private:
	JNIEnv*& m_env;
	jbyteArray& m_arr;

	jbyte* m_ptr;
	jint m_len;
};

#endif
