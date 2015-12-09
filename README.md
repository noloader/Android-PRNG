Android-PRNG is a sample Android NDK project to demonstrate two topics. First, it shows you how to compile a real shared object using the Crypto++ library on Android. Second, it shows you how to sample sensors to accumulate seed data to use with a software based random number generator.

*****

The project requires the Crypto++ library built for Android with architectures armeabi, x86, mips, armeabi-v7a and x86_64. The project looks for them in the following locations based on architecture:

 * /usr/local/cryptopp/android-armeabi
 * /usr/local/cryptopp/android-armeabi-v7a
 * /usr/local/cryptopp/android-mips
 * /usr/local/cryptopp/android-x86
 * /usr/local/cryptopp/android-x86_64
 
You can do it by repeatedly building and installing the Crypto++ library. For that task, see "Android (Command Line)" on the Crypto++ wiki at http://www.cryptopp.com/wiki/Android_(Command_Line) .

The executive summary from "Android (Command Line)" is:

    git clone https://github.com/weidai11/cryptopp.git
    cd cryptopp
    wget https://www.cryptopp.com/w/images/0/0a/Setenv-android.sh.zip
    unzip -aoq Setenv-android.sh.zip
    . ./setenv-android.sh armeabi stlport
  
    make -f GNUmakefile-cross distclean
    make -f GNUmakefile-cross static dynamic
    sudo make install PREFIX=/usr/local/cryptopp/android-armeabi
  
Lather, rinse, repeat for each architecture.

*****

Once you have the libraries installed, use `ndk-build` to build the library:

    cd Android-PRNG
    ndk-build
    
After the native libraries are built, use `ant` to build the APK and install it on a device:

    ant debug install
    
Once installed, you should find it in the App Launcher.
