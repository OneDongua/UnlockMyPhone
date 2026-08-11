package com.onedongua.unlockmyphone;

import android.os.Bundle;
import android.widget.Button;
import android.widget.Toast;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;

public class MainActivity extends AppCompatActivity {
    private UnlockClient client;
    private String ip;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        adaptInsets();

        new Thread(() -> {
            try {
                String ip = UnlockClient.discoverDevice();

                runOnUiThread(() -> {
                    if (ip != null) {
                        this.ip = ip;
                    }
                });

            } catch (Exception e) {
                e.printStackTrace();
            }
        }).start();

        Button btnUnlock = findViewById(R.id.btn_unlock);
        btnUnlock.setOnClickListener(v -> {
            if (ip == null) {
                Toast.makeText(this, R.string.cannot_find_device, Toast.LENGTH_SHORT).show();
                return;
            }
            btnUnlock.setEnabled(false);

            client = new UnlockClient(
                    ip,
                    "tP0lL6mR8pG0uQ4pZ6sK5kS4rE9eS8cC"
            );
            new Thread(() -> {
                try {
                    boolean success = client.unlock();

                    runOnUiThread(() -> {
                        if (success) {
                            Toast.makeText(this, R.string.unlock_success, Toast.LENGTH_SHORT).show();
                        } else {
                            // Token 错误或服务器返回非 OK
                            Toast.makeText(this, R.string.unlock_failure, Toast.LENGTH_SHORT).show();
                        }
                        btnUnlock.setEnabled(true);
                    });

                } catch (Exception e) {
                    e.printStackTrace();
                }
            }).start();
        });

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
