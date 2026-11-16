package org.opengothic.app;

import android.app.NativeActivity;
import android.content.res.Configuration;
import android.os.Build;
import android.os.Bundle;
import android.view.MotionEvent;
import android.view.SurfaceHolder;
import android.view.WindowManager;

/**
 * The game itself. Controls are drawn and handled by the native engine, there is no Java view under the surface.
 */
public final class GameActivity extends NativeActivity {
    static {
        // NativeActivity does not call JNI_OnLoad, the engine needs it to reach Haptics
        System.loadLibrary("Gothic2Notr");
    }

    @Override
    public void onCreate(Bundle state) {
        LifecycleLog.begin(this);
        super.onCreate(state);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            WindowManager.LayoutParams lp = getWindow().getAttributes();
            lp.layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
            getWindow().setAttributes(lp);
        }
        Haptics.init(this);
        SystemUi.hideBars(this);
    }

    @Override
    public void onWindowFocusChanged(boolean focused) {
        super.onWindowFocusChanged(focused);
        LifecycleLog.note("window focus " + focused);
        if (focused)
            SystemUi.hideBars(this);
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent event) {
        // The native input queue receives touches first. What it did not handle must not turn into Java clicks.
        return true;
    }

    // ---- what the system does to the activity: files/lifecycle.txt

    @Override
    protected void onStart() {
        super.onStart();
        LifecycleLog.note("onStart");
    }

    @Override
    protected void onResume() {
        super.onResume();
        LifecycleLog.note("onResume");
    }

    @Override
    protected void onPause() {
        LifecycleLog.note("onPause, finishing " + isFinishing());
        super.onPause();
    }

    @Override
    protected void onStop() {
        LifecycleLog.note("onStop");
        super.onStop();
    }

    @Override
    protected void onDestroy() {
        LifecycleLog.note("onDestroy, finishing " + isFinishing() + ", changing configurations " + isChangingConfigurations());
        super.onDestroy();
    }

    @Override
    protected void onUserLeaveHint() {
        LifecycleLog.note("onUserLeaveHint (home, recents or a gesture)");
        super.onUserLeaveHint();
    }

    @Override
    public void onTrimMemory(int level) {
        LifecycleLog.note("onTrimMemory " + level);
        super.onTrimMemory(level);
    }

    @Override
    public void onLowMemory() {
        LifecycleLog.note("onLowMemory");
        super.onLowMemory();
    }

    @Override
    public void onMultiWindowModeChanged(boolean multiWindow) {
        LifecycleLog.note("multi window " + multiWindow);
        super.onMultiWindowModeChanged(multiWindow);
    }

    @Override
    public void onConfigurationChanged(Configuration c) {
        super.onConfigurationChanged(c);
        LifecycleLog.note("onConfigurationChanged: orientation " + c.orientation + ", " + c.screenWidthDp + "x" + c.screenHeightDp + " dp, density "
                + c.densityDpi + ", uiMode " + c.uiMode + ", fontScale " + c.fontScale + ", locale " + c.getLocales().toLanguageTags());
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        LifecycleLog.note("surfaceCreated");
        super.surfaceCreated(holder);
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        LifecycleLog.note("surfaceChanged " + width + "x" + height + ", format " + format);
        super.surfaceChanged(holder, format, width, height);
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        LifecycleLog.note("surfaceDestroyed (the engine takes it for the end of the game)");
        super.surfaceDestroyed(holder);
    }
}
