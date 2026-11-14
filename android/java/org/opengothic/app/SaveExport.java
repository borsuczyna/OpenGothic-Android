package org.opengothic.app;

import android.content.Context;
import android.net.Uri;

import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.util.HashSet;
import java.util.Locale;
import java.util.Set;
import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;

/** Exports OpenGothic save slots through Android's Storage Access Framework. */
final class SaveExport {
    private static final int MAX_DEPTH = 4;
    private static final byte[] BUFFER = new byte[64 * 1024];

    private SaveExport() {
    }

    static int write(Context context, Uri destination, String gameId) throws IOException {
        Set<String> entries = new HashSet<>();
        int[] count = {0};

        try (OutputStream raw = context.getContentResolver().openOutputStream(destination, "w")) {
            if (raw == null)
                throw new IOException(Texts.t("Android could not open the selected destination.",
                        "Android nie mógł otworzyć wybranego miejsca zapisu."));

            try (ZipOutputStream zip = new ZipOutputStream(raw)) {
                zip.putNextEntry(new ZipEntry("opengothic/game.txt"));
                zip.write(gameId.getBytes(java.nio.charset.StandardCharsets.UTF_8));
                zip.closeEntry();
                // NativeActivity's internal working directory is the app's files directory.
                addTree(context.getFilesDir(), context.getFilesDir(), "saves/internal", 0, zip, entries, count, gameId);

                // Also check the app-specific external directory in case a build or mod stored saves there.
                File external = context.getExternalFilesDir(null);
                if (external != null && external.isDirectory())
                    addTree(external, external, "saves/external", 0, zip, entries, count, gameId);

                zip.finish();
            }
        }
        return count[0];
    }

    private static void addTree(File root, File dir, String prefix, int depth, ZipOutputStream zip,
                                Set<String> entries, int[] count, String gameId) throws IOException {
        if (depth > MAX_DEPTH || dir == null || !dir.isDirectory())
            return;

        File[] children = dir.listFiles();
        if (children == null)
            return;

        for (File file : children) {
            if (file.isDirectory()) {
                addTree(root, file, prefix, depth + 1, zip, entries, count, gameId);
                continue;
            }
            if (!file.isFile() || !isSave(file.getName()) || !belongsToGame(file.getName(), gameId))
                continue;

            String canonicalRoot = root.getCanonicalPath() + File.separator;
            String canonicalFile = file.getCanonicalPath();
            if (!canonicalFile.startsWith(canonicalRoot))
                continue;

            String relative = canonicalFile.substring(canonicalRoot.length()).replace(File.separatorChar, '/');
            String entryName = prefix + "/" + relative;
            if (!entries.add(entryName))
                continue;

            zip.putNextEntry(new ZipEntry(entryName));
            try (BufferedInputStream input = new BufferedInputStream(new FileInputStream(file))) {
                int n;
                while ((n = input.read(BUFFER)) != -1)
                    zip.write(BUFFER, 0, n);
            }
            zip.closeEntry();
            count[0]++;
        }
    }

    /** The names of the files of the saves: "<game>save_slot_N.sav" (a slot) and "<game>quicksave_N.sav" (the quick save). */
    private static final String[] MARKERS = {"save_slot_", "quicksave_"};

    /**
     * The part of the file name before the marker ("g1_", "thechroniclesofmyrtana_", "archolos_" or "" for Gothic 2),
     * null when the file is not a save of the game at all.
     */
    static String prefixOf(String name) {
        String lower = name.toLowerCase(Locale.ROOT);
        if (!lower.endsWith(".sav"))
            return null;
        for (String marker : MARKERS) {
            int slot = lower.lastIndexOf(marker);
            if (slot < 0)
                continue;
            int start = slot + marker.length();
            int end = lower.length() - 4;
            if (start >= end)
                continue;
            boolean digits = true;
            for (int i = start; i < end; i++) {
                if (lower.charAt(i) < '0' || lower.charAt(i) > '9') {
                    digits = false;
                    break;
                }
            }
            if (digits)
                return lower.substring(0, slot);
        }
        return null;
    }

    static boolean belongsToGame(String name, String gameId) {
        String prefix = prefixOf(name);
        if (prefix == null)
            return false;
        if ("gothic1".equals(gameId)) return prefix.equals("g1_");
        if ("myrtana".equals(gameId)) return prefix.equals("thechroniclesofmyrtana_");
        if ("gothic2".equals(gameId) || "gothic2_notr".equals(gameId))
            // Gothic 2 itself has no prefix, the mods of the Spine have the name of their ini ("archolos_save_slot_1.sav")
            return !prefix.equals("g1_") && !prefix.equals("thechroniclesofmyrtana_");
        return false;
    }

    static boolean isSave(String name) {
        return prefixOf(name) != null;
    }
}
