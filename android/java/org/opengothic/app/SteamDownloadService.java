package org.opengothic.app;

import android.annotation.TargetApi;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ServiceInfo;
import android.graphics.drawable.Icon;
import android.os.Build;
import android.os.IBinder;
import android.os.PowerManager;
import android.util.Log;

import java.io.File;

/**
 * Downloads a game from Steam when the screen of the application is gone: a service that the system leaves alone,
 * with a notification. It signs in with the session that was kept (the screen of Steam has made it) and puts the files
 * to the folder of the game. The screen reads the state from here.
 */
// Only a screen that needs Android 8 starts it (the library does), so the newer calls of the system are safe here
@TargetApi(26)
public final class SteamDownloadService extends Service {
    static final String ACTION_STOP = "org.opengothic.app.STEAM_STOP";
    static final String EXTRA_APP = "app";
    static final String EXTRA_FOLDER = "folder";
    static final String EXTRA_TITLE = "title";
    static final String EXTRA_LANGUAGE = "language";

    private static final String CHANNEL = "steam-download";
    private static final int NOTIFICATION = 7;

    private volatile SteamSession session;
    private PowerManager.WakeLock wake;
    private static boolean guarded;

    /**
     * The library works in threads of its own: an error in one of them (the memory most of all) would end the application.
     * While a download goes on it ends the download instead, tells why, and keeps the reason in a file.
     */
    private void guard() {
        if (guarded)
            return;
        guarded = true;
        final Thread.UncaughtExceptionHandler previous = Thread.getDefaultUncaughtExceptionHandler();
        Thread.setDefaultUncaughtExceptionHandler((thread, error) -> {
            Downloads.State s = Downloads.running();
            if (s == null) {
                if (previous != null)
                    previous.uncaughtException(thread, error);
                return;
            }
            Log.e("OpenGothic/Steam", "uncaught in " + thread.getName(), error);
            record(error);
            s.error = error instanceof OutOfMemoryError ? "Not enough memory for the download, try again."
                    : "The download failed: " + error;
            SteamSession steam = session;
            if (steam != null)
                steam.cancel();
        });
    }

    /** What went wrong, for the person who looks for it: files/steam-error.txt of the application. */
    private void record(Throwable error) {
        File dir = getExternalFilesDir(null);
        if (dir == null)
            return;
        try (java.io.PrintWriter out = new java.io.PrintWriter(new java.io.FileWriter(new File(dir, "steam-error.txt"), true))) {
            out.println("---- " + new java.util.Date());
            error.printStackTrace(out);
        } catch (java.io.IOException ignored) {
        }
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        if (intent != null && ACTION_STOP.equals(intent.getAction())) {
            SteamSession s = session;
            if (s != null)
                s.cancel();
            return START_NOT_STICKY;
        }
        if (intent == null || Downloads.running() != null) {
            // a download is going on already (or the system has restarted the service with nothing to do)
            return START_NOT_STICKY;
        }

        final int appId = intent.getIntExtra(EXTRA_APP, 0);
        final String folder = intent.getStringExtra(EXTRA_FOLDER);
        final String title = intent.getStringExtra(EXTRA_TITLE) != null ? intent.getStringExtra(EXTRA_TITLE) : "game";
        final String language = intent.getStringExtra(EXTRA_LANGUAGE) != null ? intent.getStringExtra(EXTRA_LANGUAGE) : "english";
        if (appId == 0 || folder == null) {
            stopSelf();
            return START_NOT_STICKY;
        }

        guard();
        final Downloads.State s = new Downloads.State(appId, title);
        s.text = "...";
        Downloads.state = s;
        startForeground(title, "Starting...", 0);

        PowerManager pm = (PowerManager) getSystemService(Context.POWER_SERVICE);
        if (pm != null) {
            wake = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "opengothic:steam");
            wake.acquire(6L * 60 * 60 * 1000);
        }
        new Thread(() -> run(s, folder, language), "steam-download").start();
        return START_NOT_STICKY;
    }

    private void run(final Downloads.State s, String folder, String language) {
        SteamSession steam = null;
        try {
            File external = getExternalFilesDir(null);
            SteamStore.Session saved = new SteamStore(this).load();
            if (external == null || saved == null)
                throw new SteamSession.Failure("Sign in to Steam first.", true);

            steam = new SteamSession();
            session = steam;
            s.text = "Connecting to Steam...";
            update(s);
            steam.connect();
            steam.resume(saved.account, saved.refreshToken);

            File part = new File(external, folder + ".download");
            steam.download(s.appId, part, part.exists(), language, new SteamSession.Progress() {
                @Override
                public void status(String text) {
                    s.text = text;
                    update(s);
                }

                @Override
                public void bytes(long done, long total) {
                    s.text = "";
                    s.progress(done, total);
                    update(s);
                }
            });

            File game = installationIn(part);
            if (game == null)
                throw new SteamSession.Failure("The files of the game are not what was expected (no Data, _work and System).", false);
            remove(new File(game, ".DepotDownloader"));
            File target = new File(external, folder);
            if (target.exists())
                throw new SteamSession.Failure("The game is already installed.", false);
            if (!game.renameTo(target))
                throw new SteamSession.Failure("The files could not be moved to the place of the game.", false);
            remove(part);

            s.percent = 1.f;
            s.text = "";
            s.done = true;
        } catch (SteamSession.Failure e) {
            s.error = e.getMessage();
        } catch (Throwable e) {
            Log.e("OpenGothic/Steam", "download", e);
            record(e);
            s.error = e instanceof OutOfMemoryError ? "Not enough memory for the download, try again." : "The download failed: " + e;
        } finally {
            session = null;
            if (steam != null)
                steam.close();
            if (wake != null && wake.isHeld())
                wake.release();
            finish(s);
        }
    }

    private void update(Downloads.State s) {
        NotificationManager nm = (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);
        if (nm != null)
            nm.notify(NOTIFICATION, notification(s.title, s.text.isEmpty() ? Math.round(s.percent * 100) + "%  \u00b7  " + Downloads.describe(s) : s.text,
                    Math.round(s.percent * 100)));
    }

    private void finish(Downloads.State s) {
        stopForeground(false);
        NotificationManager nm = (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);
        if (nm != null) {
            Notification.Builder b = builder().setContentTitle(s.title)
                    .setContentText(s.error != null ? s.error : Texts.t("Ready to play", "Gotowe do gry"))
                    .setSmallIcon(android.R.drawable.stat_sys_download_done)
                    .setAutoCancel(true)
                    .setOngoing(false);
            nm.notify(NOTIFICATION, b.build());
        }
        stopSelf();
    }

    private void startForeground(String title, String text, int percent) {
        Notification n = notification(title, text, percent);
        if (Build.VERSION.SDK_INT >= 29)
            startForeground(NOTIFICATION, n, ServiceInfo.FOREGROUND_SERVICE_TYPE_DATA_SYNC);
        else
            startForeground(NOTIFICATION, n);
    }

    private Notification.Builder builder() {
        NotificationManager nm = (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);
        if (nm != null && nm.getNotificationChannel(CHANNEL) == null)
            nm.createNotificationChannel(new NotificationChannel(CHANNEL, "Downloads from Steam", NotificationManager.IMPORTANCE_LOW));
        return new Notification.Builder(this, CHANNEL);
    }

    private Notification notification(String title, String text, int percent) {
        Intent stop = new Intent(this, SteamDownloadService.class).setAction(ACTION_STOP);
        android.app.PendingIntent stopIntent = android.app.PendingIntent.getService(this, 0, stop,
                android.app.PendingIntent.FLAG_IMMUTABLE | android.app.PendingIntent.FLAG_UPDATE_CURRENT);
        Notification.Builder b = builder()
                .setContentTitle(title)
                .setContentText(text)
                .setSmallIcon(android.R.drawable.stat_sys_download)
                .setOngoing(true)
                .setOnlyAlertOnce(true)
                .addAction(new Notification.Action.Builder(Icon.createWithResource(this, android.R.drawable.ic_menu_close_clear_cancel), "Stop", stopIntent).build());
        if (percent > 0)
            b.setProgress(100, Math.min(100, percent), false);
        else
            b.setProgress(0, 0, true);
        return b.build();
    }

    @Override
    public void onDestroy() {
        SteamSession s = session;
        if (s != null)
            s.cancel();
        super.onDestroy();
    }

    // ---- the files

    /** The game is in the folder, or in the only folder in it. */
    static File installationIn(File dir) {
        if (isInstallation(dir))
            return dir;
        File[] children = dir.listFiles();
        if (children == null)
            return null;
        File found = null;
        for (File c : children) {
            if (c.isDirectory() && isInstallation(c)) {
                if (found != null)
                    return null;
                found = c;
            }
        }
        return found;
    }

    static boolean isInstallation(File dir) {
        return new File(dir, "Data").isDirectory()
                && new File(dir, "_work").isDirectory()
                && new File(dir, "System").isDirectory();
    }

    private static void remove(File f) {
        File[] children = f.listFiles();
        if (children != null) {
            for (File c : children)
                remove(c);
        }
        if (f.exists())
            f.delete();
    }
}
