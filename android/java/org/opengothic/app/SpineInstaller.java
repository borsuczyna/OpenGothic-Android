package org.opengothic.app;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.FilterInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.SequenceInputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Locale;
import java.util.zip.Inflater;
import java.util.zip.InflaterInputStream;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

/**
 * Downloads a mod of Spine. A file lies at {server}{ProjectID}/{path}; a path that ends with ".z" is a zlib stream (not gzip) of
 * the file, and the hash of the list is the SHA-512 of the file without it. The files are checked while they are written, and the
 * scripts are searched for Ikarus and LeGo ({@link SpineScan}): a mod that needs them is deleted at once, before the rest is
 * downloaded.
 *
 * {@link #check} does the same search without the download of the whole mod: the compiled scripts are inside the archives
 * (.mod, .vdf), whose catalog is at the start, so only the beginning of them up to the end of the last *.DAT is read
 * ({@link SpineVdf}). The bytes that were read are kept (*.zpart), and the download goes on from them with a Range request.
 */
final class SpineInstaller {
    private SpineInstaller() {
    }

    /** What the download tells and asks. */
    interface Progress {
        /** The bytes read from the network, the size of the whole mod (from the list, only a guide; 0 in a check) and the file. */
        void update(long bytes, long total, String file);

        boolean cancelled();
    }

    static final class Cancelled extends IOException {
        Cancelled() {
            super("Cancelled");
        }
    }

    /** The mod needs a library that the engine cannot run. */
    static final class Blocked extends IOException {
        final String library;

        Blocked(String library) {
            super("The mod needs " + library);
            this.library = library;
        }
    }

    /**
     * Set to true to let in the mods that use Ikarus and LeGo: OpenGothic has a partial emulation of them (docs/IKARUS.md) that was
     * made for one mod, the others are likely to break.
     */
    static final boolean ALLOW_LIBRARIES = false;

    private static final long RESERVE = 200L * 1024 * 1024;

    /** The language that is taken, the files in it and the folder of the download with what is done. */
    private static final class Plan {
        SpineApi.FileList list;
        String language = "";
        File dir;
        File done;
        List<String> finished = new ArrayList<>();
    }

    private static Plan plan(SpineStore store, SpineApi.Project p, String phoneLanguage) throws IOException {
        Plan plan = new Plan();
        // the language is the first that has an ini: Gothic starts a mod with it
        for (String candidate : SpineApi.languageOrder(p, phoneLanguage)) {
            SpineApi.FileList l = SpineApi.files(p.id, candidate);
            if (!inis(l).isEmpty()) {
                plan.list = l;
                plan.language = candidate;
                break;
            }
        }
        if (plan.list == null)
            throw new IOException("The mod has no ini file to start it with");
        if (plan.list.server.isEmpty())
            throw new IOException("The server did not say where the files are");

        plan.dir = store.modDir(p.id);
        plan.done = new File(plan.dir, ".done");
        List<String> finished = readLines(plan.done, "language\t" + plan.language);
        if (finished == null || !plan.done.isFile()) {
            // another language, or a mod that is downloaded again (a new version): nothing of the old files is kept
            SpineStore.delete(plan.dir);
            finished = new ArrayList<>();
        }
        if (!plan.dir.isDirectory() && !plan.dir.mkdirs())
            throw new IOException("Cannot create " + plan.dir.getPath());
        if (!plan.done.isFile())
            appendLine(plan.done, "language\t" + plan.language);
        plan.finished = finished;
        return plan;
    }

    /** The archives with the scripts come first: a mod that cannot run is found before the rest of it is downloaded. */
    private static List<SpineApi.Entry> ordered(List<SpineApi.Entry> files) {
        List<SpineApi.Entry> ret = new ArrayList<>(files);
        Collections.sort(ret, (a, b) -> Integer.compare(rank(a.file), rank(b.file)));
        return ret;
    }

    private static int rank(String file) {
        String low = file.toLowerCase(Locale.ROOT);
        if (low.endsWith(".ini.z") || low.endsWith(".ini"))
            return 0;
        if (isArchive(low) || low.endsWith(".dat.z") || low.endsWith(".dat"))
            return 1;
        return 2;
    }

    private static boolean isArchive(String file) {
        String low = file.toLowerCase(Locale.ROOT);
        if (low.endsWith(".z"))
            low = low.substring(0, low.length() - 2);
        return low.endsWith(".mod") || low.endsWith(".vdf");
    }

    // ---- the check

    /**
     * Looks for Ikarus and LeGo in the scripts of the mod, reading only the beginning of its archives.
     * @throws Blocked when the mod needs one of them; its files are deleted then
     */
    static void check(SpineStore store, SpineApi.Project p, String phoneLanguage, Progress progress) throws IOException {
        Plan plan = plan(store, p, phoneLanguage);
        File probed = new File(plan.dir, ".probed");
        List<String> already = readLines(probed, null);
        long[] bytes = new long[1];
        try {
            for (SpineApi.Entry e : ordered(plan.list.files)) {
                if (progress.cancelled())
                    throw new Cancelled();
                String local = localName(e.file);
                if (local == null || !isArchive(local) || plan.finished.contains(e.file) || (already != null && already.contains(e.file)))
                    continue;
                probe(plan.list.server + p.id + "/" + encode(e.file), new File(plan.dir, local), e, progress, bytes);
                appendLine(probed, e.file);
            }
        } catch (Blocked b) {
            SpineStore.delete(plan.dir);
            store.block(p.id, b.library);
            throw b;
        }
        store.markChecked(p.id, SpineApi.versionOf(p));
    }

    private static void probe(String url, File target, SpineApi.Entry entry, Progress progress, long[] bytes) throws IOException {
        File parent = target.getParentFile();
        if (parent != null && !parent.isDirectory() && !parent.mkdirs())
            throw new IOException("Cannot create " + parent.getPath());
        IOException last = null;
        for (int attempt = 0; attempt < 3; ++attempt) {
            long before = bytes[0];
            try {
                probeOnce(url, target, entry, progress, bytes);
                return;
            } catch (Cancelled | Blocked e) {
                throw e;
            } catch (IOException e) {
                last = e;
                bytes[0] = before;
                sleep(1500L * (attempt + 1));
            }
        }
        throw last;
    }

    private static void probeOnce(String url, File target, SpineApi.Entry entry, final Progress progress, final long[] bytes)
            throws IOException {
        final File zpart = new File(target.getPath() + ".zpart");
        HttpURLConnection c = open(url);
        OutputStream keep = null;
        boolean ok = false;
        try {
            if (c.getResponseCode() != 200)
                throw new IOException("The server answered " + c.getResponseCode() + " for " + entry.file);
            final OutputStream tee = keep = new FileOutputStream(zpart);
            final String name = target.getName();
            InputStream net = new FilterInputStream(c.getInputStream()) {
                @Override
                public int read(byte[] b, int off, int len) throws IOException {
                    if (progress.cancelled())
                        throw new Cancelled();
                    int n = super.read(b, off, len);
                    if (n > 0) {
                        tee.write(b, off, n);   // the bytes as they come: the download goes on from them
                        bytes[0] += n;
                        progress.update(bytes[0], 0, name);
                    }
                    return n;
                }

                @Override
                public int read() throws IOException {
                    byte[] one = new byte[1];
                    return read(one, 0, 1) < 0 ? -1 : one[0] & 0xff;
                }
            };
            InputStream in = entry.file.endsWith(".z") ? new InflaterInputStream(net, new Inflater(), 32768) : net;

            SpineVdf vdf = new SpineVdf();
            byte[] buf = new byte[65536];
            int n;
            while (!vdf.finished() && (n = in.read(buf)) >= 0)
                vdf.feed(buf, n);
            if (vdf.found() != null)
                throw new Blocked(vdf.found());
            // an archive with nothing in it that could be looked into is read again from its start by the download
            ok = !vdf.notVdf();
        } finally {
            if (keep != null) {
                try {
                    keep.close();
                } catch (IOException ignored) {
                    // the file is deleted below when it is of no use
                }
            }
            c.disconnect();
            if (!ok && zpart.exists())
                //noinspection ResultOfMethodCallIgnored
                zpart.delete();
        }
    }

    // ---- the download

    static SpineStore.Mod install(SpineStore store, SpineApi.Project p, String phoneLanguage, Progress progress) throws IOException {
        if (store.root.getUsableSpace() < p.size * 2 + RESERVE)
            throw new IOException("Not enough free space");

        Plan plan = plan(store, p, phoneLanguage);
        File dir = plan.dir;
        List<String> probed = readLines(new File(dir, ".probed"), null);

        long[] bytes = new long[1];
        try {
            for (SpineApi.Entry e : ordered(plan.list.files)) {
                if (progress.cancelled())
                    throw new Cancelled();
                String local = localName(e.file);
                if (local == null || plan.finished.contains(e.file))
                    continue;
                File target = new File(dir, local);
                // the beginning of an archive that was read by the check is not downloaded again
                boolean resume = probed != null && probed.contains(e.file);
                fetch(plan.list.server + p.id + "/" + encode(e.file), target, e, progress, bytes, p.size, resume);
                if (local.toLowerCase(Locale.ROOT).endsWith(".zip")) {
                    unzip(target, dir);
                    //noinspection ResultOfMethodCallIgnored
                    target.delete();
                }
                appendLine(plan.done, e.file);
            }
        } catch (Blocked b) {
            SpineStore.delete(dir);
            store.block(p.id, b.library);
            throw b;
        }

        SpineStore.Mod mod = new SpineStore.Mod();
        mod.id = p.id;
        mod.name = p.name;
        mod.version = SpineApi.versionOf(p);
        mod.language = plan.language;
        mod.gameType = p.gameType;
        mod.size = p.size;
        collect(dir, "", mod.files);
        mod.ini = SpineStore.pickIni(SpineStore.iniFiles(mod.files), phoneLanguage);
        if (mod.ini.isEmpty())
            throw new IOException("The mod has no ini file to start it with");
        store.write(mod);
        //noinspection ResultOfMethodCallIgnored
        plan.done.delete();
        //noinspection ResultOfMethodCallIgnored
        new File(dir, ".probed").delete();
        mod.dir = dir;
        return mod;
    }

    private static List<String> inis(SpineApi.FileList list) {
        List<String> names = new ArrayList<>();
        for (SpineApi.Entry e : list.files) {
            String local = localName(e.file);
            if (local != null)
                names.add(local);
        }
        return SpineStore.iniFiles(names);
    }

    /**
     * The path in the mod, or null for a file that is not taken: it has to stay in the folder of the mod, and the programs of
     * Windows (the runtimes and plugins that Spine installs) are of no use.
     */
    static String localName(String file) {
        String path = file.replace('\\', '/');
        if (path.startsWith("/") || path.contains("//"))
            return null;
        for (String part : path.split("/")) {
            if (part.equals("..") || part.equals(".") || part.isEmpty() || part.startsWith("."))
                return null;
        }
        if (path.endsWith(".z"))
            path = path.substring(0, path.length() - 2);
        String low = path.toLowerCase(Locale.ROOT);
        if (low.endsWith(".exe") || low.endsWith(".dll") || low.endsWith(".manifest"))
            return null;
        return path;
    }

    static String encode(String path) {
        StringBuilder b = new StringBuilder();
        for (byte c : path.getBytes(StandardCharsets.UTF_8)) {
            int v = c & 0xff;
            if ((v >= 'a' && v <= 'z') || (v >= 'A' && v <= 'Z') || (v >= '0' && v <= '9') || "-_.~/".indexOf(v) >= 0)
                b.append((char) v);
            else
                b.append('%').append(String.format(Locale.ROOT, "%02X", v));
        }
        return b.toString();
    }

    private static HttpURLConnection open(String url) throws IOException {
        HttpURLConnection c = (HttpURLConnection) new URL(url).openConnection();
        c.setConnectTimeout(20000);
        c.setReadTimeout(60000);
        c.setRequestProperty("User-Agent", "OpenGothic-Android");
        return c;
    }

    private static void sleep(long ms) throws Cancelled {
        try {
            Thread.sleep(ms);
        } catch (InterruptedException ie) {
            throw new Cancelled();
        }
    }

    // ---- one file

    private static void fetch(String url, File target, SpineApi.Entry entry, Progress progress, long[] bytes, long total, boolean resume)
            throws IOException {
        File parent = target.getParentFile();
        if (parent != null && !parent.isDirectory() && !parent.mkdirs())
            throw new IOException("Cannot create " + parent.getPath());

        IOException last = null;
        for (int attempt = 0; attempt < 3; ++attempt) {
            long before = bytes[0];
            try {
                fetchOnce(url, target, entry, progress, bytes, total, resume);
                return;
            } catch (Cancelled | Blocked e) {
                throw e;
            } catch (IOException e) {
                last = e;
                bytes[0] = before;
                sleep(1500L * (attempt + 1));
            }
        }
        throw last;
    }

    private static void fetchOnce(String url, File target, SpineApi.Entry entry, final Progress progress, final long[] bytes,
                                  final long total, boolean resume) throws IOException {
        File part = new File(target.getPath() + ".part");
        File zpart = new File(target.getPath() + ".zpart");
        long have = resume && zpart.isFile() ? zpart.length() : 0;
        HttpURLConnection c = open(url);
        try {
            if (have > 0)
                c.setRequestProperty("Range", "bytes=" + have + "-");
            int code = c.getResponseCode();

            final String name = target.getName();
            InputStream body;
            long prefix = 0;
            if (have > 0 && code == 416) {
                body = new FileInputStream(zpart);   // the whole file was read already
                prefix = have;
            } else if (code == 206 && have > 0) {
                body = new SequenceInputStream(new FileInputStream(zpart), counted(c.getInputStream(), progress, bytes, total, name));
                prefix = have;
            } else if (code == 200) {
                // the server sends all of it (it does not know Range, or there is no start to go on from)
                body = counted(c.getInputStream(), progress, bytes, total, name);
            } else {
                throw new IOException("The server answered " + code + " for " + entry.file);
            }
            bytes[0] += prefix;
            progress.update(bytes[0], total, name);
            InputStream in = entry.file.endsWith(".z") ? new InflaterInputStream(body, new Inflater(), 32768) : body;

            MessageDigest sha;
            try {
                sha = MessageDigest.getInstance("SHA-512");
            } catch (NoSuchAlgorithmException e) {
                throw new IOException("No SHA-512");
            }
            // an archive is looked into by its catalog (only the scripts), a loose script file all through
            boolean archive = isArchive(target.getName());
            SpineVdf vdf = !ALLOW_LIBRARIES && archive ? new SpineVdf() : null;
            SpineScan scan = !ALLOW_LIBRARIES && !archive && SpineScan.worthScanning(target.getName()) ? new SpineScan() : null;

            try (InputStream source = in; OutputStream out = new FileOutputStream(part)) {
                byte[] buf = new byte[131072];
                int n;
                while ((n = source.read(buf)) > 0) {
                    sha.update(buf, 0, n);
                    out.write(buf, 0, n);
                    if (vdf != null && !vdf.finished()) {
                        vdf.feed(buf, n);
                        if (vdf.found() != null)
                            throw new Blocked(vdf.found());
                        if (vdf.notVdf() && scan == null)
                            scan = new SpineScan();
                    }
                    if (scan != null) {
                        scan.feed(buf, n);
                        if (scan.found() != null)
                            throw new Blocked(scan.found());
                    }
                }
            }
            if (!entry.hash.isEmpty() && !hex(sha.digest()).equalsIgnoreCase(entry.hash))
                throw new IOException("The file " + entry.file + " is damaged");
            if (target.exists() && !target.delete())
                throw new IOException("Cannot replace " + target.getPath());
            if (!part.renameTo(target))
                throw new IOException("Cannot save " + target.getPath());
            //noinspection ResultOfMethodCallIgnored
            zpart.delete();
        } finally {
            c.disconnect();
            if (part.exists())
                //noinspection ResultOfMethodCallIgnored
                part.delete();
        }
    }

    /** The bytes that come from the network are counted, and the cancel is looked at. */
    private static InputStream counted(InputStream net, final Progress progress, final long[] bytes, final long total, final String name) {
        return new FilterInputStream(net) {
            @Override
            public int read(byte[] b, int off, int len) throws IOException {
                if (progress.cancelled())
                    throw new Cancelled();
                int n = super.read(b, off, len);
                if (n > 0) {
                    bytes[0] += n;
                    progress.update(bytes[0], total, name);
                }
                return n;
            }

            @Override
            public int read() throws IOException {
                byte[] one = new byte[1];
                return read(one, 0, 1) < 0 ? -1 : one[0] & 0xff;
            }
        };
    }

    private static String hex(byte[] data) {
        StringBuilder b = new StringBuilder(data.length * 2);
        for (byte x : data)
            b.append(String.format(Locale.ROOT, "%02x", x & 0xff));
        return b.toString();
    }

    /** A zip file of a mod is unpacked into the folder of the mod (Spine does the same). */
    private static void unzip(File zip, File dir) throws IOException {
        String prefix = dir.getCanonicalPath() + File.separator;
        try (ZipInputStream in = new ZipInputStream(new FileInputStream(zip))) {
            ZipEntry e;
            byte[] buf = new byte[131072];
            while ((e = in.getNextEntry()) != null) {
                String local = e.isDirectory() ? "" : localName(e.getName());
                if (local == null || local.isEmpty())
                    continue;
                File out = new File(dir, local);
                if (!out.getCanonicalPath().startsWith(prefix))
                    throw new IOException("Unsafe path in the mod");
                File parent = out.getParentFile();
                if (parent != null && !parent.isDirectory() && !parent.mkdirs())
                    throw new IOException("Cannot create " + parent.getPath());
                SpineScan scan = !ALLOW_LIBRARIES && SpineScan.worthScanning(local) ? new SpineScan() : null;
                try (OutputStream o = new FileOutputStream(out)) {
                    int n;
                    while ((n = in.read(buf)) > 0) {
                        o.write(buf, 0, n);
                        if (scan != null) {
                            scan.feed(buf, n);
                            if (scan.found() != null)
                                throw new Blocked(scan.found());
                        }
                    }
                }
            }
        }
    }

    // ---- what is done

    /**
     * The lines of the file after the first one when the first is {@code header} (any when it is null); null when the file is of
     * another kind (a download in another language starts again). Missing file: an empty list.
     */
    private static List<String> readLines(File file, String header) {
        List<String> ret = new ArrayList<>();
        if (!file.isFile())
            return ret;
        try (BufferedReader in = new BufferedReader(new InputStreamReader(new FileInputStream(file), StandardCharsets.UTF_8))) {
            String line;
            if (header != null) {
                line = in.readLine();
                if (line == null || !line.equals(header))
                    return null;
            }
            while ((line = in.readLine()) != null) {
                if (!line.isEmpty())
                    ret.add(line);
            }
        } catch (IOException e) {
            return null;
        }
        return ret;
    }

    private static void appendLine(File file, String line) throws IOException {
        try (OutputStream out = new FileOutputStream(file, true)) {
            out.write((line + "\n").getBytes(StandardCharsets.UTF_8));
        }
    }

    private static void collect(File dir, String prefix, List<String> out) {
        File[] children = dir.listFiles();
        if (children == null)
            return;
        for (File f : children) {
            if (f.getName().startsWith(".") || f.getName().endsWith(".part") || f.getName().endsWith(".zpart"))
                continue;
            if (f.isDirectory())
                collect(f, prefix + f.getName() + "/", out);
            else
                out.add(prefix + f.getName());
        }
    }
}
