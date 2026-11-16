package org.opengothic.app;

import android.content.Context;
import android.os.Build;
import android.os.SystemClock;

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.util.Locale;

/**
 * What the system did to the activity of the game, with the time since its start: files/lifecycle.txt, the log of the last run.
 * The game ends by itself when its window is taken away (the native glue takes the loss of the window for the end), and the
 * reason is not in log.txt. The launcher adds this file to the report.
 */
final class LifecycleLog {
    private static long started;
    private static File file;

    private LifecycleLog() {
    }

    static synchronized void begin(Context c) {
        started = SystemClock.elapsedRealtime();
        file    = new File(c.getFilesDir(), "lifecycle.txt");
        file.delete();
        note("begin, " + Build.MANUFACTURER + " " + Build.MODEL + ", Android " + Build.VERSION.RELEASE + " (API " + Build.VERSION.SDK_INT + ")");
    }

    static synchronized void note(String what) {
        if (file == null)
            return;
        long ms = SystemClock.elapsedRealtime() - started;
        try (FileWriter w = new FileWriter(file, true)) {
            w.write(String.format(Locale.ROOT, "+%d.%03d s  %s\n", ms / 1000, ms % 1000, what));
        } catch (IOException ignored) {
            // only a help for the report
        }
    }
}