package com.onedongua.unlockmyphone;

import android.os.Bundle;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.Toast;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;

public class MainActivity extends AppCompatActivity {
    private static final String ENCRYPTED_PIN = "encrypted_pin";
    private UnlockClient client;
    private String ip;
    private TextView statusView;
    private Button rescanButton;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        adaptInsets();

        statusView = findViewById(R.id.tv_status);
        rescanButton = findViewById(R.id.btn_rescan);
        EditText pinInput = findViewById(R.id.et_pin);
        Button savePinButton = findViewById(R.id.btn_save_pin);
        boolean hasSavedPin = !getPreferences(MODE_PRIVATE).getString(ENCRYPTED_PIN, "").isEmpty();
        if (hasSavedPin) pinInput.setHint(R.string.pin_saved_hint);
        pinInput.setOnFocusChangeListener((v, hasFocus) -> {
            if (hasFocus && hasSavedPin) pinInput.setText("");
        });
        savePinButton.setOnClickListener(v -> {
            String pin = pinInput.getText().toString();
            if (!pin.matches("\\d{4,16}")) {
                Toast.makeText(this, R.string.pin_invalid, Toast.LENGTH_SHORT).show();
                return;
            }
            try {
                String encrypted = PinCrypto.encrypt(pin);
                getPreferences(MODE_PRIVATE).edit().putString(ENCRYPTED_PIN, encrypted).apply();
                pinInput.getText().clear();
                Toast.makeText(this, R.string.pin_saved, Toast.LENGTH_SHORT).show();
            } catch (Exception e) {
                Toast.makeText(this, R.string.pin_save_failure, Toast.LENGTH_SHORT).show();
            }
        });
        rescanButton.setOnClickListener(v -> scanForDevice());
        scanForDevice();

        Button btnUnlock = findViewById(R.id.btn_unlock);
        btnUnlock.setOnClickListener(v -> {
            if (ip == null) {
                Toast.makeText(this, R.string.cannot_find_device, Toast.LENGTH_SHORT).show();
                return;
            }
            String encryptedPin = getPreferences(MODE_PRIVATE).getString(ENCRYPTED_PIN, null);
            if (encryptedPin == null) {
                Toast.makeText(this, R.string.pin_required, Toast.LENGTH_SHORT).show();
                return;
            }
            btnUnlock.setEnabled(false);
            client = new UnlockClient(ip);
            new Thread(() -> {
                try {
                    String response = client.unlock(encryptedPin);

                    runOnUiThread(() -> {
                        if ("OK".equals(response)) {
                            Toast.makeText(this, R.string.unlock_success, Toast.LENGTH_SHORT).show();
                        } else if ("ALREADY_UNLOCKED".equals(response)) {
                            Toast.makeText(this, R.string.already_unlocked, Toast.LENGTH_SHORT).show();
                        } else {
                            // Token 错误或服务器返回其他失败状态
                            Toast.makeText(this, R.string.unlock_failure, Toast.LENGTH_SHORT).show();
                        }
                        btnUnlock.setEnabled(true);
                    });

                } catch (Exception e) {
                    e.printStackTrace();
                    runOnUiThread(() -> {
                        btnUnlock.setEnabled(true);
                        Toast.makeText(this, R.string.unlock_failure, Toast.LENGTH_SHORT).show();
                        scanForDevice();
                    });
                }
            }).start();
        });

    }

    private void scanForDevice() {
        ip = null;
        statusView.setText(R.string.scanning_device);
        rescanButton.setEnabled(false);

        new Thread(() -> {
            String discoveredIp = null;
            try {
                discoveredIp = UnlockClient.discoverDevice();
            } catch (Exception e) {
                e.printStackTrace();
            }

            String finalDiscoveredIp = discoveredIp;
            runOnUiThread(() -> {
                ip = finalDiscoveredIp;
                if (finalDiscoveredIp != null) {
                    statusView.setText(getString(R.string.device_found, finalDiscoveredIp));
                } else {
                    statusView.setText(R.string.device_not_found);
                }
                rescanButton.setEnabled(true);
            });
        }).start();
    }


    private void adaptInsets() {
        ViewCompat.setOnApplyWindowInsetsListener(findViewById(R.id.root), (v, insets) -> {
            Insets systemBarsInsets = insets.getInsets(WindowInsetsCompat.Type.systemBars());
            Insets displayCutOutInsets = insets.getInsets(WindowInsetsCompat.Type.displayCutout());
            v.setPadding(systemBarsInsets.left + displayCutOutInsets.left,
                    systemBarsInsets.top,
                    systemBarsInsets.right + displayCutOutInsets.right,
                    systemBarsInsets.bottom);
            return insets;
        });
    }

}
