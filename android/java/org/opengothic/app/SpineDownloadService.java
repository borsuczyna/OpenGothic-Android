package org.opengothic.app;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
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
import java.io.IOException;
import java.util.Locale;

/**
 * The download of a mod of Spine, or the check of its scripts: a foreground service, so it goes on when the screen is gone.
 * One mod at a time; {@link #state} is what the screens show.
 */
public final class SpineDownloadService extends Service {
    static final String ACTION_STOP = "org.opengothic.app.SPINE_STOP";
    static final String EXTRA_CHECK = "check";

    private static final String CHANNEL = "spine-download";
    private static final int NOTIFICATION = 9;

    /** What the service is doing: it writes it, the screens read it. */
    static final class State {
        final int id;
        final String title;
        final boolean check;         // only the scripts are looked into
        volatile long bytes;
        volatile long total;
        volatile String file = "";
        volatile boolean done;
        volatile boolean cancelled;
        volatile String error;       // what the player reads
        volatile String library;     // the mod needs Ikarus or LeGo
        volatile boolean reported;   // the player was told how it ended

        State(int id, String title, boolean check) {
            this.id = id;
            this.title = title;
            this.check = check;
        }

        boolean running() {
            return !done && error == null && library == null && !cancelled;
        }

        /** -1 when the size is not known (a check reads as much as it needs). */
        int percent() {
            return total > 0 ? (int) Math.min(99, bytes * 100 / total) : -1;
        }
    }

    static volatile State state;

    static State running() {
        State s = state;
        return s != null && s.running() ? s : null;
    }

    static Intent intent(Context context, SpineApi.Project p, boolean check) {
        return SpineActions.put(new Intent(context, SpineDownloadService.class), p).putExtra(EXTRA_CHECK, check);
    }

    private PowerManager.WakeLock wake;

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        if (intent != null && ACTION_STOP.equals(intent.getAction())) {
            State s = state;
            if (s != null)
                s.cancelled = true;
            return START_NOT_STICKY;
        }
        if (intent == null || running() != null) {
            return START_NOT_STICKY;   // one mod at a time, or the system has restarted the service with nothing to do
        }

        final SpineApi.Project p = SpineActions.project(intent);
        final boolean check = intent.getBooleanExtra(EXTRA_CHECK, false);
        final File external = getExternalFilesDir(null);
        if (p.id == 0 || external == null) {
            stopSelf();
            return START_NOT_STICKY;
        }

        final State s = new State(p.id, p.name, check);
        s.total = check ? 0 : p.size;
        state = s;
        startForeground(p.name, "...", 0);

        PowerManager pm = (PowerManager) getSystemService(Context.POWER_SERVICE);
        if (pm != null) {
            wake = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "opengothic:spine");
            wake.acquire(3L * 60 * 60 * 1000);
        }
        new Thread(() -> run(s, p, external), "spine-download").start();
        return START_NOT_STICKY;
    }

    private void run(final State s, SpineApi.Project p, File external) {
        final long[] lastNotify = {0};
        SpineInstaller.Progress progress = new SpineInstaller.Progress() {
            @Override
            public void update(long bytes, long total, String file) {
                s.bytes = bytes;
                s.total = s.check ? 0 : Math.max(total, bytes);
                s.file = file;
                long now = System.currentTimeMillis();
                if (now - lastNotify[0] > 1000) {
                    lastNotify[0] = now;
                    notifyProgress(s);
                }
            }

            @Override
            public boolean cancelled() {
                return s.cancelled;
            }
        };
        try {
            SpineStore store = new SpineStore(external);
            String language = Locale.getDefault().getLanguage();
            if (s.check)
                SpineInstaller.check(store, p, language, progress);
            else
                SpineInstaller.install(store, p, language, progress);
            s.done = true;
        } catch (SpineInstaller.Blocked b) {
            s.library = b.library;
        } catch (SpineInstaller.Cancelled c) {
            s.cancelled = true;
        } catch (IOException e) {
            Log.e("OpenGothic/Spine", (s.check ? "check of " : "download of ") + p.name + " failed", e);
            s.error = e.getMessage() != null ? e.getMessage() : e.toString();
        } catch (OutOfMemoryError | RuntimeException e) {
            Log.e("OpenGothic/Spine", (s.check ? "check of " : "download of ") + p.name + " failed", e);
            s.error = "Failed: " + e;
        } finally {
            if (wake != null && wake.isHeld())
                wake.release();
            finish(s);
        }
    }

    private void notifyProgress(State s) {
        NotificationManager nm = (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);
        if (nm != null) {
            int percent = s.percent();
            nm.notify(NOTIFICATION, notification(s.title, (percent >= 0 ? percent + "%  ·  " : "") + Downloads.size(s.bytes), percent));
        }
    }

    private void finish(State s) {
        stopForeground(false);
        NotificationManager nm = (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);
        if (nm != null) {
            String text;
            if (s.library != null)
                text = Texts.t("It needs " + s.library + ", which OpenGothic cannot run. Removed.",
                        "Wymaga " + s.library + ", którego OpenGothic nie uruchomi. Usunięto.");
            else if (s.error != null)
                text = s.error;
            else if (s.cancelled)
                text = Texts.t("Stopped", "Zatrzymano");
            else if (s.check)
                text = Texts.t("No Ikarus or LeGo found. It can be downloaded.", "Brak Ikarusa i LeGo. Można pobierać.");
            else
                text = Texts.t("Ready to play", "Gotowe do gry");
            Notification.Builder b = builder().setContentTitle(s.title)
                    .setContentText(text)
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

    @SuppressWarnings("deprecation")
    private Notification.Builder builder() {
        // notification channels exist since Android 8, the minimum of the app is lower
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O)
            return new Notification.Builder(this);
        NotificationManager nm = (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);
        if (nm != null && nm.getNotificationChannel(CHANNEL) == null)
            nm.createNotificationChannel(new NotificationChannel(CHANNEL, "Mods from Spine", NotificationManager.IMPORTANCE_LOW));
        return new Notification.Builder(this, CHANNEL);
    }

    private Notification notification(String title, String text, int percent) {
        Intent stop = new Intent(this, SpineDownloadService.class).setAction(ACTION_STOP);
        PendingIntent stopIntent = PendingIntent.getService(this, 0, stop, PendingIntent.FLAG_IMMUTABLE | PendingIntent.FLAG_UPDATE_CURRENT);
        Notification.Builder b = builder()
                .setContentTitle(title)
                .setContentText(text)
                .setSmallIcon(android.R.drawable.stat_sys_download)
                .setOngoing(true)
                .setOnlyAlertOnce(true)
                .addAction(new Notification.Action.Builder(Icon.createWithResource(this, android.R.drawable.ic_menu_close_clear_cancel),
                        Texts.t("Stop", "Zatrzymaj"), stopIntent).build());
        if (percent > 0)
            b.setProgress(100, Math.min(100, percent), false);
        else
            b.setProgress(0, 0, true);
        return b.build();
    }

    @Override
    public void onDestroy() {
        State s = state;
        if (s != null && s.running())
            s.cancelled = true;
        super.onDestroy();
    }
}
