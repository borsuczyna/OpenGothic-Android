package org.opengothic.app;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.nio.charset.Charset;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Properties;

/**
 * The mods of Spine (the mod manager of Clockwork Origins) that are downloaded: spine/&lt;id&gt;/ in the files of the application
 * keeps the files of a mod with the paths that it has in the Gothic folder (Data/modvdf/X.mod, System/X.ini, _work/...).
 * They are moved to the folder of the game only when the mod is played ({@link SpineActivation}).
 */
final class SpineStore {
    static final int GOTHIC = 0;
    static final int GOTHIC2 = 1;
    static final int BOTH = 3;

    private static final String META = ".spine";

    /** A mod that is downloaded. */
    static final class Mod {
        int id;
        String name = "";
        String version = "";
        String ini = "";          // the file in System that Gothic starts with -game:
        String language = "";
        int gameType;
        long size;
        List<String> files = new ArrayList<>();
        File dir;
    }

    final File root;

    SpineStore(File external) {
        root = new File(external, "spine");
        //noinspection ResultOfMethodCallIgnored
        root.mkdirs();
    }

    File modDir(int id) {
        return new File(root, Integer.toString(id));
    }

    // ---- the downloaded mods

    List<Mod> installed() {
        List<Mod> ret = new ArrayList<>();
        File[] dirs = root.listFiles();
        if (dirs == null)
            return ret;
        for (File d : dirs) {
            Mod m = read(d);
            if (m != null)
                ret.add(m);
        }
        Collections.sort(ret, (a, b) -> a.name.compareToIgnoreCase(b.name));
        return ret;
    }

    Mod get(int id) {
        return read(modDir(id));
    }

    static Mod read(File dir) {
        File meta = new File(dir, META);
        if (!dir.isDirectory() || !meta.isFile())
            return null;
        Properties p = new Properties();
        try (FileInputStream in = new FileInputStream(meta)) {
            p.load(new InputStreamReader(in, StandardCharsets.UTF_8));
        } catch (IOException e) {
            return null;
        }
        Mod m = new Mod();
        try {
            m.id = Integer.parseInt(p.getProperty("id", ""));
            m.gameType = Integer.parseInt(p.getProperty("game", "1"));
            m.size = Long.parseLong(p.getProperty("size", "0"));
        } catch (NumberFormatException e) {
            return null;
        }
        m.name = p.getProperty("name", "");
        m.version = p.getProperty("version", "");
        m.ini = p.getProperty("ini", "");
        m.language = p.getProperty("language", "");
        for (String f : p.getProperty("files", "").split("\n")) {
            if (!f.isEmpty())
                m.files.add(f);
        }
        m.dir = dir;
        return m.ini.isEmpty() ? null : m;
    }

    void write(Mod m) throws IOException {
        Properties p = new Properties();
        p.setProperty("id", Integer.toString(m.id));
        p.setProperty("name", m.name);
        p.setProperty("version", m.version);
        p.setProperty("ini", m.ini);
        p.setProperty("language", m.language);
        p.setProperty("game", Integer.toString(m.gameType));
        p.setProperty("size", Long.toString(m.size));
        StringBuilder files = new StringBuilder();
        for (String f : m.files)
            files.append(f).append('\n');
        p.setProperty("files", files.toString());
        File dir = modDir(m.id);
        //noinspection ResultOfMethodCallIgnored
        dir.mkdirs();
        try (OutputStream out = new FileOutputStream(new File(dir, META))) {
            p.store(new java.io.OutputStreamWriter(out, StandardCharsets.UTF_8), null);
        }
    }

    /** The remains of checks and of downloads that were left (no mod comes of them) are deleted after some days. */
    void pruneAbandoned(int activeId) {
        File[] dirs = root.listFiles();
        if (dirs == null)
            return;
        long old = System.currentTimeMillis() - 3L * 24 * 3600 * 1000;
        for (File d : dirs) {
            if (!d.isDirectory() || !d.getName().matches("[0-9]{1,9}") || new File(d, META).isFile())
                continue;
            if (Integer.parseInt(d.getName()) == activeId)
                continue;
            File marker = new File(d, ".done");
            if ((marker.isFile() ? marker.lastModified() : d.lastModified()) < old)
                delete(d);
        }
    }

    /** Deletes the mod. The files must not be in the folder of the game: {@link SpineActivation#restore} first. */
    void remove(int id) {
        delete(modDir(id));
    }

    // ---- the mods that need what the engine cannot run

    private File blockedFile() {
        return new File(root, "blocked.txt");
    }

    /** The mods that were found to need Ikarus or LeGo: the id and the library. */
    Map<Integer, String> blocked() {
        Map<Integer, String> ret = new HashMap<>();
        File f = blockedFile();
        if (!f.isFile())
            return ret;
        try (BufferedReader in = new BufferedReader(new InputStreamReader(new FileInputStream(f), StandardCharsets.UTF_8))) {
            String line;
            while ((line = in.readLine()) != null) {
                int tab = line.indexOf('\t');
                if (tab <= 0)
                    continue;
                try {
                    ret.put(Integer.parseInt(line.substring(0, tab)), line.substring(tab + 1));
                } catch (NumberFormatException ignored) {
                    // a broken line
                }
            }
        } catch (IOException ignored) {
            // nothing is known then, the mod is checked again while it is downloaded
        }
        return ret;
    }

    void block(int id, String library) {
        try (OutputStream out = new FileOutputStream(blockedFile(), true)) {
            out.write((id + "\t" + library + "\n").getBytes(StandardCharsets.UTF_8));
        } catch (IOException ignored) {
            // it is found again at the next download
        }
    }

    // ---- the mods whose scripts were looked into before the download

    private File checkedFile() {
        return new File(root, "checked.txt");
    }

    /** The mods whose scripts have none of the libraries: the id and the version that was looked into. */
    Map<Integer, String> checked() {
        Map<Integer, String> ret = new HashMap<>();
        File f = checkedFile();
        if (!f.isFile())
            return ret;
        try (BufferedReader in = new BufferedReader(new InputStreamReader(new FileInputStream(f), StandardCharsets.UTF_8))) {
            String line;
            while ((line = in.readLine()) != null) {
                String[] parts = line.split("\t");
                if (parts.length != 2)
                    continue;
                try {
                    ret.put(Integer.parseInt(parts[0]), parts[1]);
                } catch (NumberFormatException ignored) {
                    // a broken line
                }
            }
        } catch (IOException ignored) {
            // nothing is known then
        }
        return ret;
    }

    void markChecked(int id, String version) {
        try (OutputStream out = new FileOutputStream(checkedFile(), true)) {
            out.write((id + "\t" + version + "\n").getBytes(StandardCharsets.UTF_8));
        } catch (IOException ignored) {
            // it is checked again
        }
    }

    // ---- ini files of the mods

    private static final Charset ANSI = Charset.forName("windows-1252");

    /** The values of an ini file with the keys "section.key" in lower case. */
    static Map<String, String> readIni(File file) {
        Map<String, String> ret = new HashMap<>();
        try (BufferedReader in = new BufferedReader(new InputStreamReader(new FileInputStream(file), ANSI))) {
            String section = "";
            String line;
            while ((line = in.readLine()) != null) {
                line = line.trim();
                if (line.isEmpty() || line.startsWith(";") || line.startsWith("//"))
                    continue;
                if (line.startsWith("[")) {
                    int end = line.indexOf(']');
                    section = (end > 0 ? line.substring(1, end) : line.substring(1)).trim().toLowerCase(Locale.ROOT);
                    continue;
                }
                int eq = line.indexOf('=');
                if (eq <= 0)
                    continue;
                String value = line.substring(eq + 1);
                int comment = value.indexOf(';');
                if (comment >= 0)
                    value = value.substring(0, comment);
                ret.put(section + "." + line.substring(0, eq).trim().toLowerCase(Locale.ROOT), value.trim());
            }
        } catch (IOException ignored) {
            // an ini that cannot be read has nothing
        }
        return ret;
    }

    /** The files in System/ of the mod that are ini files: the paths of the mod, as in the list of files. */
    static List<String> iniFiles(List<String> files) {
        List<String> ret = new ArrayList<>();
        for (String f : files) {
            String low = f.toLowerCase(Locale.ROOT);
            if (low.startsWith("system/") && low.indexOf('/', 7) < 0 && low.endsWith(".ini"))
                ret.add(f);
        }
        Collections.sort(ret);
        return ret;
    }

    /**
     * Gothic starts the mod with its ini. A mod with more of them (a version for each language) gets the one that ends with the
     * language of the phone ("_pl"), then English, then the first.
     */
    static String pickIni(List<String> inis, String phoneLanguage) {
        if (inis.isEmpty())
            return "";
        String[] wanted = {"_" + phoneLanguage, "_en"};
        for (String suffix : wanted) {
            for (String ini : inis) {
                if (ini.toLowerCase(Locale.ROOT).endsWith(suffix + ".ini"))
                    return ini.substring(ini.lastIndexOf('/') + 1);
            }
        }
        String first = inis.get(0);
        return first.substring(first.lastIndexOf('/') + 1);
    }

    /** The names in [FILES] VDF of the ini, lower case (empty: the ini does not say). */
    static List<String> iniVdfs(File ini) {
        List<String> ret = new ArrayList<>();
        String value = readIni(ini).get("files.vdf");
        if (value == null)
            return ret;
        for (String s : value.split("[\\s,;]+")) {
            if (!s.isEmpty())
                ret.add(s.toLowerCase(Locale.ROOT));
        }
        return ret;
    }

    static void delete(File f) {
        File[] children = f.listFiles();
        if (children != null) {
            for (File c : children)
                delete(c);
        }
        //noinspection ResultOfMethodCallIgnored
        f.delete();
    }
}
