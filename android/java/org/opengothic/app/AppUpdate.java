package org.opengothic.app;

import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageInstaller;
import android.net.Uri;
import android.os.Build;
import android.provider.Settings;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;

/**
 * Updates the application from its release on GitHub: the APK is downloaded straight into a session of the system installer (no file
 * of its own, no FileProvider) and committed. Android always asks the player to confirm an installation of an application and
 * the new APK has to be signed with the same key as the installed one: the system refuses it otherwise and says why.
 */
final class AppUpdate {
    private AppUpdate() {
    }

    static final String ACTION = "org.opengothic.app.UPDATE_STATUS";

    /** 0..100 while the APK is downloaded, 101 when it is handed over to the installer, -1 when nothing runs. */
    static volatile int percent = -1;
    /** The reason of the last failure, empty when there was none. */
    static volatile String error = "";

    static boolean running() {
        return percent >= 0;
    }

    /** May the application install packages: the player has to allow it for it once ("install unknown apps"). */
    static boolean canInstall(Context c) {
        return Build.VERSION.SDK_INT < 26 || c.getPackageManager().canRequestPackageInstalls();
    }

    /** The page of the settings where the player allows it. */
    static Intent permissionIntent(Context c) {
        if (Build.VERSION.SDK_INT >= 26)
            return new Intent(Settings.ACTION_MANAGE_UNKNOWN_APP_SOURCES, Uri.parse("package:" + c.getPackageName()));
        return new Intent(Settings.ACTION_SECURITY_SETTINGS);
    }

    /** Starts the download in its own thread; {@code changed} is called (on that thread) at every step. */
    static void start(Context context, UpdateCheck.Release release, Runnable changed) {
        if (running() || release.apkUrl.isEmpty())
            return;
        final Context app = context.getApplicationContext();
        percent = 0;
        error = "";
        new Thread(() -> {
            try {
                install(app, release, changed);
            } catch (Exception | OutOfMemoryError e) {
                error = e.getMessage() != null ? e.getMessage() : e.getClass().getSimpleName();
            } finally {
                percent = -1;
                changed.run();
            }
        }, "app-update").start();
    }

    private static void install(Context app, UpdateCheck.Release release, Runnable changed) throws IOException {
        PackageInstaller installer = app.getPackageManager().getPackageInstaller();
        // the sessions that an earlier try has left: they hold the space
        for (PackageInstaller.SessionInfo old : installer.getMySessions()) {
            try {
                installer.abandonSession(old.getSessionId());
            } catch (SecurityException ignored) {
                // not ours any more
            }
        }

        PackageInstaller.SessionParams params = new PackageInstaller.SessionParams(PackageInstaller.SessionParams.MODE_FULL_INSTALL);
        params.setAppPackageName(app.getPackageName());
        if (release.apkSize > 0)
            params.setSize(release.apkSize);
        if (Build.VERSION.SDK_INT >= 31)
            params.setRequireUserAction(PackageInstaller.SessionParams.USER_ACTION_NOT_REQUIRED);

        final int id = installer.createSession(params);
        PackageInstaller.Session session = installer.openSession(id);
        boolean committed = false;
        Tls.ensure();
        HttpURLConnection c = (HttpURLConnection) new URL(release.apkUrl).openConnection();
        try {
            c.setConnectTimeout(15000);
            c.setReadTimeout(30000);
            c.setInstanceFollowRedirects(true);
            c.setRequestProperty("User-Agent", "OpenGothic-Android");
            if (c.getResponseCode() != 200)
                throw new IOException("GitHub answered " + c.getResponseCode());
            long total = c.getContentLengthLong() > 0 ? c.getContentLengthLong() : release.apkSize;

            long done = 0;
            try (InputStream in = c.getInputStream(); OutputStream out = session.openWrite("opengothic.apk", 0, total > 0 ? total : -1)) {
                byte[] buf = new byte[64 * 1024];
                int n;
                int last = -1;
                while ((n = in.read(buf)) > 0) {
                    out.write(buf, 0, n);
                    done += n;
                    int p = total > 0 ? (int) Math.min(100, done * 100 / total) : 0;
                    if (p != last) {
                        last = p;
                        percent = p;
                        changed.run();
                    }
                }
                if (total > 0 && done != total)
                    throw new IOException("The download is incomplete");
                session.fsync(out);
            }

            percent = 101;
            changed.run();

            Intent status = new Intent(app, UpdateReceiver.class).setAction(ACTION);
            // the system adds the status to it: it has to be mutable (explicit, so it is safe)
            int flags = PendingIntent.FLAG_UPDATE_CURRENT | (Build.VERSION.SDK_INT >= 31 ? PendingIntent.FLAG_MUTABLE : 0);
            PendingIntent pending = PendingIntent.getBroadcast(app, id, status, flags);
            session.commit(pending.getIntentSender());
            committed = true;
        } finally {
            c.disconnect();
            if (!committed) {
                session.abandon();
            }
            session.close();
        }
    }
}
