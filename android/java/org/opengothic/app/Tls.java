package org.opengothic.app;

import android.util.Log;

import javax.net.ssl.HttpsURLConnection;
import javax.net.ssl.SSLContext;

/**
 * The default TLS of the process. The system makes it on the first use and keeps it, also when the making has failed:
 * HttpsURLConnection keeps a default factory that only throws "NoSuchAlgorithmException: Error constructing implementation
 * (algorithm: Default ...", until the application is closed. The cause is a heap that is full after a download from Steam
 * (the context loads the certificates of the system) or a security provider that was changed at the wrong time.
 * Here the context is made on its own, and only when that works it becomes the default factory, so one failure is not kept
 * and the next call tries again.
 */
final class Tls {
    private static final String TAG = "OpenGothic/Tls";
    private static boolean ready;

    private Tls() {
    }

    /** Call it before opening an https connection. It does nothing once it has worked. */
    static synchronized void ensure() {
        if (ready)
            return;
        try {
            HttpsURLConnection.setDefaultSSLSocketFactory(SSLContext.getDefault().getSocketFactory());
            ready = true;
        } catch (Exception | OutOfMemoryError e) {
            Log.w(TAG, "the default TLS is not ready: " + e);
        }
    }
}