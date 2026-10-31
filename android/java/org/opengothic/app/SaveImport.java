package org.opengothic.app;

import android.content.Context;
import android.net.Uri;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Locale;
import java.util.Map;
import java.util.Set;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

/** Restores save files from an archive created by SaveExport. */
final class SaveImport {
    static final int OVERWRITE = 0;
    static final int ADD_SEPARATELY = 1;
    private static final int MAX_ENTRIES = 100000;
    private static final long MAX_TOTAL = 2L * 1024 * 1024 * 1024;
    private static final long MAX_FILE = 256L * 1024 * 1024;

    private SaveImport() { }

    static int read(Context context, Uri source, int mode, String gameId) throws IOException {
        File internal = context.getFilesDir().getCanonicalFile();
        File external = context.getExternalFilesDir(null);
        if (external != null) external = external.getCanonicalFile();
        Map<String, Set<Integer>> usedSlots = new HashMap<>();
        Set<String> destinations = new HashSet<>();
        int count = 0;
        int entries = 0;
        long total = 0;

        try (InputStream raw = context.getContentResolver().openInputStream(source)) {
            if (raw == null) throw new IOException("Nie można otworzyć wybranego archiwum.");
            try (ZipInputStream zip = new ZipInputStream(new BufferedInputStream(raw))) {
                ZipEntry entry;
                byte[] buffer = new byte[64 * 1024];
                while ((entry = zip.getNextEntry()) != null) {
                    if (++entries > MAX_ENTRIES) throw new IOException("Archiwum zawiera zbyt wiele plików.");
                    if (entry.isDirectory()) { zip.closeEntry(); continue; }
                    String name = entry.getName().replace('\\', '/');
                    File root;
                    String relative;
                    if (name.startsWith("saves/internal/")) {
                        root = internal;
                        relative = name.substring("saves/internal/".length());
                    } else if (name.startsWith("saves/external/")) {
                        if (external == null) { zip.closeEntry(); continue; }
                        root = external;
                        relative = name.substring("saves/external/".length());
                    } else { zip.closeEntry(); continue; }

                    if (!SaveExport.isSave(new File(relative).getName()) || !SaveExport.belongsToGame(new File(relative).getName(), gameId) || !safeRelative(relative)) {
                        zip.closeEntry(); continue;
                    }
                    File original = new File(root, relative).getCanonicalFile();
                    if (!original.getPath().startsWith(root.getPath() + File.separator)) {
                        zip.closeEntry(); continue;
                    }
                    File destination = original;
                    String parentKey = original.getParent();
                    if (mode == ADD_SEPARATELY) {
                        Set<Integer> slots = usedSlots.get(parentKey);
                        if (slots == null) {
                            slots = scanSlots(original.getParentFile(), gameId);
                            usedSlots.put(parentKey, slots);
                        }
                        int next = 0;
                        while (slots.contains(next)) next++;
                        slots.add(next);
                        String originalName = original.getName();
                        int marker = originalName.toLowerCase(Locale.ROOT).lastIndexOf("save_slot_");
                        String prefix = marker >= 0 ? originalName.substring(0, marker) : "";
                        destination = new File(original.getParentFile(), prefix + "save_slot_" + next + ".sav");
                    }
                    String destKey = destination.getCanonicalPath();
                    if (!destinations.add(destKey)) throw new IOException("Archiwum zawiera powtórzone zapisy.");
                    File parent = destination.getParentFile();
                    if (!parent.exists() && !parent.mkdirs()) throw new IOException("Nie można utworzyć katalogu zapisów.");

                    File temp = File.createTempFile("save-import-", ".tmp", parent);
                    long fileBytes = 0;
                    boolean complete = false;
                    try {
                        try (BufferedOutputStream out = new BufferedOutputStream(new FileOutputStream(temp))) {
                            int n;
                            while ((n = zip.read(buffer)) != -1) {
                                fileBytes += n;
                                total += n;
                                if (fileBytes > MAX_FILE || total > MAX_TOTAL)
                                    throw new IOException("Archiwum zapisów jest zbyt duże.");
                                out.write(buffer, 0, n);
                            }
                        }
                        if (fileBytes == 0) throw new IOException("Archiwum zawiera pusty plik zapisu.");
                        File backup = null;
                        if (destination.exists()) {
                            backup = File.createTempFile("save-backup-", ".tmp", parent);
                            if (!backup.delete() || !destination.renameTo(backup)) {
                                backup.delete();
                                throw new IOException("Nie można zabezpieczyć istniejącego zapisu: " + destination.getName());
                            }
                        }
                        if (!temp.renameTo(destination)) {
                            if (backup != null) backup.renameTo(destination);
                            throw new IOException("Nie można zapisać pliku: " + destination.getName());
                        }
                        if (backup != null) backup.delete();
                        complete = true;
                        count++;
                    } finally {
                        if (!complete) temp.delete();
                    }
                    zip.closeEntry();
                }
            }
        }
        return count;
    }

    private static Set<Integer> scanSlots(File dir, String gameId) {
        Set<Integer> slots = new HashSet<>();
        File[] files = dir != null ? dir.listFiles() : null;
        if (files != null) for (File file : files) {
            String name = file.getName().toLowerCase(Locale.ROOT);
            if (SaveExport.belongsToGame(name, gameId)) {
                try {
                    int marker = name.lastIndexOf("save_slot_");
                    slots.add(Integer.parseInt(name.substring(marker + 10, name.length() - 4)));
                } catch (RuntimeException ignored) { }
            }
        }
        return slots;
    }

    private static boolean safeRelative(String path) {
        if (path.isEmpty() || path.startsWith("/") || path.indexOf(':') >= 0) return false;
        for (String part : path.split("/")) if (part.isEmpty() || "..".equals(part) || ".".equals(part)) return false;
        return true;
    }
}
