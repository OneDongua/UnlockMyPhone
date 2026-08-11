package com.onedongua.unlockmyphone;

import android.util.Base64;

import java.nio.charset.StandardCharsets;
import java.security.SecureRandom;

import javax.crypto.Cipher;
import javax.crypto.spec.IvParameterSpec;
import javax.crypto.spec.SecretKeySpec;

final class PinCrypto {
    // Must match unlockd/main.c. This is the protocol key, not the user's PIN.
    private static final String SHARED_KEY = "tP0lL6mR8pG0uQ4pZ6sK5kS4rE9eS8cC";

    private PinCrypto() { }

    static String encrypt(String pin) throws Exception {
        byte[] iv = new byte[16];
        new SecureRandom().nextBytes(iv);
        Cipher cipher = Cipher.getInstance("AES/CTR/NoPadding");
        cipher.init(Cipher.ENCRYPT_MODE,
                new SecretKeySpec(SHARED_KEY.getBytes(StandardCharsets.US_ASCII), "AES"),
                new IvParameterSpec(iv));
        byte[] encrypted = cipher.doFinal(pin.getBytes(StandardCharsets.UTF_8));
        byte[] payload = new byte[iv.length + encrypted.length];
        System.arraycopy(iv, 0, payload, 0, iv.length);
        System.arraycopy(encrypted, 0, payload, iv.length, encrypted.length);
        return Base64.encodeToString(payload, Base64.NO_WRAP);
    }
}
