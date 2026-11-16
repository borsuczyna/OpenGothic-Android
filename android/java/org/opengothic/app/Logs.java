package org.opengothic.app;

import android.annotation.TargetApi;
import android.app.Activity;
import android.app.ActivityManager;
import android.app.ApplicationExitInfo;
import android.content.Context;
import android.content.Intent;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.nio.charset.StandardCharsets;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.List;
import java.util.Locale;

/**
 * The logs of the game: the engine writes log.txt and crash.log to the files of the application, where only the application
 * can read them. The launcher shows the end of them and hands them to another application (a message, a mail).
 */
final class Logs {
    private Logs() {
    }

    /** The last part of a file, from the start of a line. Empty when there is no file. */
    static String tail(File file, int maxBytes) {
        if (!file.isFile())
            return "";
        try (RandomAccessFile in = new RandomAccessFile(file, "r")) {
            long size = in.length();
            long from = Math.max(0, size - maxBytes);
            in.seek(from);
            byte[] data = new byte[(int) (size - from)];
            in.readFully(data);
            String text = new String(data, StandardCharsets.UTF_8);
            if (from > 0) {
                int line = text.indexOf('\n');
                if (line >= 0)
                    text = text.substring(line + 1);
            }
            return text;
        } catch (IOException e) {
            return "";
        }
    }

    /** The log of the last run of the game and the crash, if there was one. */
    static String report(Context c, int maxBytes) {
        File dir = c.getFilesDir();
        StringBuilder b = new StringBuilder();
        b.append("OpenGothic ").append(version(c)).append(", Android ").append(android.os.Build.VERSION.RELEASE)
                .append(" (API ").append(android.os.Build.VERSION.SDK_INT).append("), ")
                .append(android.os.Build.MANUFACTURER).append(' ').append(android.os.Build.MODEL).append('\n');
        File external = c.getExternalFilesDir(null);
        if (external != null) {
            String chosen = tail(new File(external, "game.txt"), 200).trim().replace('\n', ' ');
            if (!chosen.isEmpty())
                b.append("game: ").append(chosen).append('\n');
        }
        // the safe GPU mode of the game: the shader that the driver was compiling when it killed the game
        for (String name : new String[] {"safegpu.lock", "safegpu.mode"}) {
            String text = tail(new File(dir, name), 1000).trim();
            if (!text.isEmpty())
                b.append("\n---- ").append(name).append('\n').append(text).append('\n');
        }
        // the last lines first: that is where the pipeline of the crash is named
        String log = tail(new File(dir, "log.txt"), maxBytes);
        String[] lines = log.split("\n");
        StringBuilder last = new StringBuilder();
        for (int i = Math.max(0, lines.length - 12); i < lines.length; ++i)
            last.append(lines[i]).append('\n');
        b.append("\n---- the last lines of log.txt\n").append(last);
        String crash = tail(new File(dir, "crash.log"), maxBytes / 3);
        if (!crash.isEmpty())
            b.append("\n---- crash.log\n").append(crash).append('\n');
        // why the game ended when it did not crash: what the system did to its activity, and what it says about the end of the process
        String life = tail(new File(dir, "lifecycle.txt"), 4000);
        if (!life.isEmpty())
            b.append("\n---- lifecycle.txt (the activity of the game, seconds since its start)\n").append(life).append('\n');
        String exits = exitReasons(c);
        if (!exits.isEmpty())
            b.append("\n---- the system about the end of the process (newest first)\n").append(exits);
        b.append("\n---- log.txt\n").append(log);
        return b.toString();
    }

    @TargetApi(30)
    private static String exitReasons(Context c) {
        if (android.os.Build.VERSION.SDK_INT < 30)
            return "";
        try {
            ActivityManager am = (ActivityManager) c.getSystemService(Context.ACTIVITY_SERVICE);
            if (am == null)
                return "";
            List<ApplicationExitInfo> list = am.getHistoricalProcessExitReasons(c.getPackageName(), 0, 5);
            SimpleDateFormat format = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.ROOT);
            StringBuilder b = new StringBuilder();
            for (ApplicationExitInfo i : list) {
                b.append(format.format(new Date(i.getTimestamp()))).append(' ').append(i.getProcessName()).append(": ")
                        .append(reasonName(i.getReason())).append(" (").append(i.getReason()).append("), status ").append(i.getStatus())
                        .append(", importance ").append(i.getImportance()).append(", rss ").append(i.getRss() / 1024).append(" MB");
                String description = i.getDescription();
                if (description != null && !description.isEmpty())
                    b.append(", ").append(description);
                b.append('\n');
            }
            return b.toString();
        } catch (RuntimeException e) {
            return "";
        }
    }

    private static String reasonName(int reason) {
        switch (reason) {
            case 1:  return "EXIT_SELF";
            case 2:  return "SIGNALED";
            case 3:  return "LOW_MEMORY";
            case 4:  return "CRASH";
            case 5:  return "CRASH_NATIVE";
            case 6:  return "ANR";
            case 7:  return "INITIALIZATION_FAILURE";
            case 8:  return "PERMISSION_CHANGE";
            case 9:  return "EXCESSIVE_RESOURCE_USAGE";
            case 10: return "USER_REQUESTED";
            case 11: return "USER_STOPPED";
            case 12: return "DEPENDENCY_DIED";
            case 13: return "OTHER";
            case 14: return "FREEZER";
            case 15: return "PACKAGE_STATE_CHANGE";
            case 16: return "PACKAGE_UPDATED";
            default: return "UNKNOWN";
        }
    }

    static String version(Context c) {
        try {
            String v = c.getPackageManager().getPackageInfo(c.getPackageName(), 0).versionName;
            return v != null ? v : "";
        } catch (android.content.pm.PackageManager.NameNotFoundException e) {
            return "";
        }
    }

    /** Hands the report to another application and keeps a copy where a file manager or adb can reach it. */
    static void share(Activity a) {
        String text = report(a, 60000);
        File external = a.getExternalFilesDir(null);
        if (external != null) {
            try (FileOutputStream out = new FileOutputStream(new File(external, "last-log.txt"))) {
                out.write(report(a, 400000).getBytes(StandardCharsets.UTF_8));
            } catch (IOException ignored) {
                // the copy is only a convenience
            }
        }
        Intent send = new Intent(Intent.ACTION_SEND);
        send.setType("text/plain");
        send.putExtra(Intent.EXTRA_SUBJECT, "OpenGothic log");
        send.putExtra(Intent.EXTRA_TEXT, text);
        a.startActivity(Intent.createChooser(send, Texts.t("Share the log", "Udostępnij log")));
    }

    /** Is there a log of a crash that is newer than the given time. */
    static boolean crashedSince(Context c, long time) {
        File crash = new File(c.getFilesDir(), "crash.log");
        return crash.isFile() && crash.lastModified() >= time;
    }

    static boolean hasLog(Context c) {
        return new File(c.getFilesDir(), "log.txt").isFile();
    }
}
