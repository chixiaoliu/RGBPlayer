package com.withyang.rgbplayer;

import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import androidx.core.content.PermissionChecker;

import android.Manifest;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.TextView;

public class MainActivity extends AppCompatActivity {

    // Used to load the 'native-lib' library on application startup.
    static {
        System.loadLibrary("native-lib");
    }

    private RGBPlayerView rgb_play;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN, WindowManager.LayoutParams.FLAG_FULLSCREEN);
        setContentView(R.layout.activity_main);

        // Example of a call to a native method
        Button play = findViewById(R.id.sample_text);
        rgb_play = findViewById(R.id.rgb_play);
        play.setText(stringFromJNI());
        play.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                        rgb_play.play();

            }
        });

        int result = PermissionChecker.checkSelfPermission(this, Manifest.permission.WRITE_EXTERNAL_STORAGE);
        if (result == -1) {
            ActivityCompat.requestPermissions(this, new String[]{Manifest.permission.WRITE_EXTERNAL_STORAGE}, 1000);
        }
        rgb_play.post(new Runnable() {
            @Override
            public void run() {
                int width = rgb_play.getWidth();
                int height = rgb_play.getHeight();
                Log.e("Main",width+"height"+height);
            }
        });
    }

    /**
     * A native method that is implemented by the 'native-lib' native library,
     * which is packaged with this application.
     */
    public native String stringFromJNI();
}
