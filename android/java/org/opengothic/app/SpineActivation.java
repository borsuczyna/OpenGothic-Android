package org.opengothic.app;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/**
 * Puts the files of a mod into the folder of the game for the time of the play and takes them out after it, as Spine does.
 * The files are moved (the folders are on one disk, so it takes no time and no space), a file of the game that a mod replaces
 * is kept as *.spbak. What was done is written to spine/active.txt before every step, so the game folder is put back
 * also when the application was stopped while the mod was played: the launcher calls {@link #restore} when it starts.
 */
final class SpineActivation {
    private SpineActivation() {
    }

    private static final String BACKUP = ".spbak";

    private static File journal(File spineRoot) {
        return new File(spineRoot, "active.txt");
    }

    /** True while the folder of the game holds the files of a mod. */
    static boolean active(File spineRoot) {
        return journal(spineRoot).isFile();
    }

    /**
     * Moves the files of the mod to the game folder. After a failure the game folder is as it was.
     * @return the name of the ini in System, which the engine starts with.
     */
    static String activate(File spineRoot, File gameRoot, SpineStore.Mod mod) throws IOException {
        restore(spineRoot);
        File ini = new File(mod.dir, "System/" + mod.ini);
        if (!ini.isFile()) {
            // the case of the name may differ from the one in the list
            ini = find(new File(mod.dir, "System"), mod.ini);
        }
        List<String> vdfs = ini != null ? SpineStore.iniVdfs(ini) : new ArrayList<String>();

        try (OutputStream log = new FileOutputStream(journal(spineRoot), true)) {
            for (String rel : mod.files) {
                String[] parts = rel.split("/");
                File src = new File(mod.dir, rel);
                if (!src.isFile())
                    continue;
                if (parts.length >= 3 && parts[0].equalsIgnoreCase("data") && parts[1].equalsIgnoreCase("modvdf")) {
                    // only the archives that the ini lists are loaded by Gothic, the others stay
                    String name = parts[parts.length - 1];
                    if (!vdfs.isEmpty() && !vdfs.contains(name.toLowerCase(Locale.ROOT)))
                        continue;
                    parts = new String[] {"Data", name};
                }
                place(log, src, gameRoot, parts);
            }
        } catch (IOException | RuntimeException e) {
            restore(spineRoot);
            throw e;
        }
        return mod.ini;
    }

    private static void place(OutputStream log, File src, File gameRoot, String[] parts) throws IOException {
        File dir = gameRoot;
        for (int i = 0; i < parts.length - 1; ++i) {
            File next = find(dir, parts[i]);
            if (next == null || !next.isDirectory()) {
                next = new File(dir, parts[i]);
                write(log, "D\t" + next.getPath());
                if (!next.mkdir() && !next.isDirectory())
                    throw new IOException("Cannot create " + next.getPath());
            }
            dir = next;
        }
        String name = parts[parts.length - 1];
        File existing = find(dir, name);
        File dst = existing != null ? existing : new File(dir, name);
        if (existing != null) {
            // the file of the game (or of another mod) is kept until the mod is over
            File backup = new File(dst.getPath() + BACKUP);
            if (backup.exists())
                backup.delete();
            write(log, "B\t" + dst.getPath());
            if (!dst.renameTo(backup))
                throw new IOException("Cannot move " + dst.getPath());
        }
        write(log, "M\t" + src.getPath() + "\t" + dst.getPath());
        move(src, dst);
    }

    /** Puts the game folder back as it was before {@link #activate}. Safe to call at any time. */
    static void restore(File spineRoot) {
        File file = journal(spineRoot);
        if (!file.isFile())
            return;
        List<String> lines = new ArrayList<>();
        try (BufferedReader in = new BufferedReader(new InputStreamReader(new FileInputStream(file), StandardCharsets.UTF_8))) {
            String line;
            while ((line = in.readLine()) != null)
                lines.add(line);
        } catch (IOException e) {
            return;   // tried again at the next start
        }
        for (int i = lines.size() - 1; i >= 0; --i) {
            String[] f = lines.get(i).split("\t");
            try {
                if (f[0].equals("M") && f.length == 3) {
                    File src = new File(f[1]);
                    File dst = new File(f[2]);
                    if (dst.isFile()) {
                        File parent = src.getParentFile();
                        if (parent != null)
                            //noinspection ResultOfMethodCallIgnored
                            parent.mkdirs();
                        move(dst, src);
                    }
                } else if (f[0].equals("B") && f.length == 2) {
                    File path = new File(f[1]);
                    File backup = new File(f[1] + BACKUP);
                    if (backup.isFile()) {
                        if (path.exists())
                            //noinspection ResultOfMethodCallIgnored
                            path.delete();
                        //noinspection ResultOfMethodCallIgnored
                        backup.renameTo(path);
                    }
                } else if (f[0].equals("D") && f.length == 2) {
                    //noinspection ResultOfMethodCallIgnored
                    new File(f[1]).delete();   // a folder that is not empty stays
                }
            } catch (IOException | RuntimeException ignored) {
                // the rest is put back anyway
            }
        }
        //noinspection ResultOfMethodCallIgnored
        file.delete();
    }

    private static void write(OutputStream log, String line) throws IOException {
        log.write((line + "\n").getBytes(StandardCharsets.UTF_8));
        log.flush();
    }

    /** The file or folder of the folder with the name in any case (Android file systems tell the cases apart, Gothic does not). */
    static File find(File dir, String name) {
        File exact = new File(dir, name);
        if (exact.exists())
            return exact;
        File[] children = dir.listFiles();
        if (children == null)
            return null;
        for (File c : children) {
            if (c.getName().equalsIgnoreCase(name))
                return c;
        }
        return null;
    }

    private static void move(File src, File dst) throws IOException {
        if (src.renameTo(dst))
            return;
        try (InputStream in = new FileInputStream(src); OutputStream out = new FileOutputStream(dst)) {
            byte[] buf = new byte[131072];
            int n;
            while ((n = in.read(buf)) > 0)
                out.write(buf, 0, n);
        }
        if (!src.delete())
            throw new IOException("Cannot remove " + src.getPath());
    }
}
