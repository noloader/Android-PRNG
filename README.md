Android-PRNG is a sample Android NDK (and Eclipse) project to demonstrate two topics. First, it shows you how to compile a real shared object using the Crypto++ library built for Android. Second, it shows you how to sample sensors to accumulate a seed to use with a software based random number generator.

The project requires the Crypto++ library built for Android with architectures armeabi, x86, mips, armeabi-v7a and x86_64. The project looks for them in the following locations based on architecture:

 * /usr/local/cryptopp/android-armeabi
 * /usr/local/cryptopp/android-armeabi-v7a
 * /usr/local/cryptopp/android-mips
 * /usr/local/cryptopp/android-x86
 * /usr/local/cryptopp/android-x86_64
 
You can do it by repeatedly building and installing the Crypto++ library. For that task, see "Android (Command Line)" on the Crypto++ wiki (http://www.cryptopp.com/wiki/Android_%28Command_Line%29).

The maximum brevity example of building from "Android (Command Line)":

  git clone https://github.com/weidai11/cryptopp.git
  cd cryptopp
  wget https://www.cryptopp.com/w/images/0/0a/Setenv-android.sh.zip
  unzip -aoq Setenv-android.sh.zip
  . ./setenv-android.sh armeabi stlport
  
  make distclean && make static dynamic
  sudo make install PREFIX=/usr/local/cryptopp/android-armeabi
  
Lather, rinse, repeat.
