package com.onedongua.unlockmyphone;

import android.app.IntentService;
import android.content.Intent;
import android.content.SharedPreferences;

/** Performs an unlock request without displaying UI. */
public class UnlockRequestService extends IntentService {
    public UnlockRequestService() { super("UnlockRequestService"); }

    @Override
    protected void onHandleIntent(Intent intent) {
        SharedPreferences preferences = getSharedPreferences("MainActivity", MODE_PRIVATE);
        String encryptedPin = preferences.getString(MainActivity.ENCRYPTED_PIN, null);
        if (encryptedPin == null || encryptedPin.isEmpty()) {
            startActivity(new Intent(this, MainActivity.class)
                    .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TOP));
            return;
        }
        String ip = preferences.getString(MainActivity.DEVICE_IP, null);
        try {
            if (ip == null || ip.isEmpty()) ip = UnlockClient.discoverDevice();
            if (ip != null && !ip.isEmpty()) new UnlockClient(ip).unlock(encryptedPin);
        } catch (Exception ignored) {
            // Shortcut and Tile requests are intentionally silent.
        }
    }
}
