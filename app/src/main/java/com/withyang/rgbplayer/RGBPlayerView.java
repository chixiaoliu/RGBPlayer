package com.withyang.rgbplayer;

import android.content.Context;
import android.opengl.GLSurfaceView;
import android.os.Build;
import android.util.AttributeSet;
import android.view.SurfaceHolder;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

/**
 * Created by CYLiu on 2021/1/24
 */
public class RGBPlayerView extends GLSurfaceView implements  SurfaceHolder.Callback,GLSurfaceView.Renderer{

    public RGBPlayerView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }


    @Override
    public void surfaceCreated(SurfaceHolder holder) {

        if (Build.VERSION.SDK_INT>=Build.VERSION_CODES.O){
            setRenderer(this);
        }
    }
    public void play(){
        open("/sdcard/storage/emulated/0/mvtest.mp4", getHolder().getSurface());
    }

    @Override
    public void onSurfaceCreated(GL10 gl10, EGLConfig eglConfig) {

    }

    @Override
    public void onSurfaceChanged(GL10 gl10, int i, int i1) {

    }

    @Override
    public void onDrawFrame(GL10 gl10) {

    }



    public native void open(String path, Object surface);

}
