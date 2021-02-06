Android-PRNG is a sample Android NDK project to demonstrate two topics. First, it shows you how to compile a real shared object using the Crypto++ library on Android. Second, it shows you how to sample sensors to accumulate seed data to use with a software based random number generator.

The project requires the Crypto++ library built for Android with architectures armeabi-v7a, arm64-v8a, x64 and x86_64. The project looks for them in the following locations based on architecture:

 * /usr/local/cryptopp/android-armeabi-v7a
 * /usr/local/cryptopp/android-arm64-v8a
 * /usr/local/cryptopp/android-x86
 * /usr/local/cryptopp/android-x86_64

You can create the prerequisites by repeatedly building and installing the Crypto++ library. The steps for the task are:

```bash
git clone https://github.com/weidai11/cryptopp.git
cd cryptopp
cp -p TestScripts/setenv-android.sh .
source ./setenv-android.sh armeabi-v7a

make -f GNUmakefile-cross distclean
make -f GNUmakefile-cross static dynamic
sudo make install PREFIX=/usr/local/cryptopp/android-armeabi-v7a
```

Lather, rinse, and repeat for each architecture.

Once you have the libraries installed, use `ndk-build` to build the library:

```bash
cd Android-PRNG
ndk-build
```

After the native libraries are built, use `ant` to build the APK and install it on a device:

```bash
ant debug install
```

Once installed, you should find it in the App Launcher.

=== References ===

The following references from the Crypto++ wiki should be helpful.

* http://www.cryptopp.com/wiki/Android_(Command_Line)
* http://www.cryptopp.com/wiki/Android.mk_(Command_Line)
* http://www.cryptopp.com/wiki/Android_Activity
* http://www.cryptopp.com/wiki/Wrapper_DLL
