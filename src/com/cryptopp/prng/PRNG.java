package com.cryptopp.prng;

public class PRNG {

    static {
        System.loadLibrary("c++_shared");
        System.loadLibrary("cryptopp");
        System.loadLibrary("prng");
    }

    private static native int CryptoPP_Reseed(byte[] bytes);

    private static native int CryptoPP_GetBytes(byte[] bytes);

    private static Object lock = new Object();

    // Class method. Returns the number of bytes consumed from the seed.
    public static int Reseed(byte[] seed) {

        synchronized (lock) {
            return CryptoPP_Reseed(seed);
        }
    }

    // Class method. Returns the number of bytes generated.
    public static int GetBytes(byte[] bytes) {
        synchronized (lock) {
            return CryptoPP_GetBytes(bytes);
        }
    }

    // Instance method. Returns the number of bytes consumed from the seed.
    public int reseed(byte[] seed) {
        synchronized (lock) {
            return CryptoPP_Reseed(seed);
        }
    }

    // Instance method. Returns the number of bytes generated.
    public int getBytes(byte[] bytes) {
        synchronized (lock) {
            return CryptoPP_GetBytes(bytes);
        }
    }
}
