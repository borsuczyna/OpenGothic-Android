package org.opengothic.app;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.InetAddress;
import java.net.Socket;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Locale;
import java.util.Set;

import javax.net.ssl.HttpsURLConnection;
import javax.net.ssl.SSLSocket;
import javax.net.ssl.SSLSocketFactory;

/**
 * The server of Spine, the mod manager of Clockwork Origins (https://github.com/ClockworkOrigins/Spine). Its client talks to the
 * database server with POST requests with a JSON body; the list of the mods and of their files needs no account. Every value in
 * an answer is a string, also the numbers. The files lie on a plain web server: see {@link SpineInstaller}.
 */
final class SpineApi {
    private SpineApi() {
    }

    private static final String API = "https://clockwork-origins.com:19181/";

    /** The languages of Spine: the name it uses and the bit in "SupportedLanguages". */
    private static final String[] LANGUAGES = {"Deutsch", "English", "Polish", "Russian"};
    private static final int[] LANGUAGE_BITS = {1, 2, 4, 8};
    private static final String[] LANGUAGE_CODES = {"de", "en", "pl", "ru"};

    static final int GOTHIC = 0;
    static final int GOTHIC2 = 1;
    static final int GOTHIC_BOTH = 3;
    static final int TOTAL_CONVERSION = 0;
    static final int ENHANCEMENT = 1;

    /** A mod of the list. */
    static final class Project {
        int id;
        String name = "";
        String team = "";
        String keywords = "";
        int gameType;
        int modType;
        int languages;            // the bits of the languages that the mod has
        int[] version = new int[3];
        long size;                // the bytes to download
        long avgMinutes;          // how long the players need
        long updated;             // the day of the last update, only to sort
    }

    /** A file of a mod: the path on the server (with .z when it is compressed) and the SHA-512 of the file without it. */
    static final class Entry {
        String file;
        String hash;
    }

    static final class FileList {
        List<Entry> files = new ArrayList<>();
        String server = "";
    }

    /** Mods that Gothic can start with an ini: a total conversion or an enhancement for Gothic, Gothic II or both. */
    static boolean launchable(Project p) {
        boolean game = p.gameType == GOTHIC || p.gameType == GOTHIC2 || p.gameType == GOTHIC_BOTH;
        return game && (p.modType == TOTAL_CONVERSION || p.modType == ENHANCEMENT);
    }

    // ---- the list of the mods

    /** The answer of the server as it is: it is kept for the next start, so the list shows at once. Blocks. */
    static String requestProjects() throws IOException {
        return post("requestAllProjects", "{\"Username\":\"\",\"Password\":\"\",\"Language\":\"English\"}");
    }

    static List<Project> parseProjects(String json) throws IOException {
        List<Project> ret = new ArrayList<>();
        try {
            JSONArray array = new JSONObject(json).optJSONArray("Projects");
            if (array == null)
                throw new IOException("The list of mods is empty");
            for (int i = 0; i < array.length(); ++i) {
                JSONObject o = array.optJSONObject(i);
                if (o == null)
                    continue;
                Project p = new Project();
                p.id = (int) number(o, "ProjectID");
                p.name = o.optString("Name", "").trim();
                p.team = o.optString("TeamName", "").trim();
                p.keywords = o.optString("Keywords", "");
                p.gameType = (int) number(o, "GameType");
                p.modType = (int) number(o, "ModType");
                p.languages = (int) number(o, "SupportedLanguages");
                p.version[0] = (int) number(o, "MajorVersion");
                p.version[1] = (int) number(o, "MinorVersion");
                p.version[2] = (int) number(o, "PatchVersion");
                p.size = number(o, "DownloadSize");
                p.avgMinutes = number(o, "AvgDuration");
                p.updated = number(o, "UpdateDate");
                if (p.id > 0 && !p.name.isEmpty())
                    ret.add(p);
            }
        } catch (JSONException e) {
            throw new IOException("The list of mods cannot be read");
        }
        return ret;
    }

    static String versionOf(Project p) {
        return p.version[0] + "." + p.version[1] + (p.version[2] != 0 ? "." + p.version[2] : "");
    }

    private static long number(JSONObject o, String key) {
        try {
            return Long.parseLong(o.optString(key, "0").trim());
        } catch (NumberFormatException e) {
            return 0;
        }
    }

    // ---- the files of a mod

    /**
     * The languages in the order they are tried: the one of the phone, English, then the others. Only the languages that the mod
     * has; the ini of a mod can be in the list of one language only, so {@link SpineInstaller} takes the first with an ini.
     */
    static List<String> languageOrder(Project p, String phoneLanguage) {
        List<String> ret = new ArrayList<>();
        String[] order = {phoneLanguage, "en", "pl", "de", "ru"};
        for (String code : order) {
            for (int i = 0; i < LANGUAGES.length; ++i) {
                if (LANGUAGE_CODES[i].equals(code) && (p.languages & LANGUAGE_BITS[i]) != 0 && !ret.contains(LANGUAGES[i]))
                    ret.add(LANGUAGES[i]);
            }
        }
        if (ret.isEmpty()) {
            for (String l : LANGUAGES)
                ret.add(l);
        }
        return ret;
    }

    /** "DE EN PL" for the row of the list. */
    static String languageTags(Project p) {
        StringBuilder b = new StringBuilder();
        for (int i = 0; i < LANGUAGES.length; ++i) {
            if ((p.languages & LANGUAGE_BITS[i]) != 0)
                b.append(b.length() > 0 ? " " : "").append(LANGUAGE_CODES[i].toUpperCase(Locale.ROOT));
        }
        return b.toString();
    }

    static boolean hasLanguage(Project p, String code) {
        for (int i = 0; i < LANGUAGES.length; ++i) {
            if (LANGUAGE_CODES[i].equals(code))
                return (p.languages & LANGUAGE_BITS[i]) != 0;
        }
        return false;
    }

    /** The files of a mod in a language ("All" files come with it). Blocks. */
    static FileList files(int projectId, String language) throws IOException {
        String body = "{\"Username\":\"\",\"Password\":\"\",\"Language\":\"" + language + "\",\"ProjectID\":" + projectId + "}";
        FileList ret = new FileList();
        try {
            JSONObject o = new JSONObject(post("requestProjectFiles", body));
            JSONArray array = o.optJSONArray("Files");
            if (array != null) {
                for (int i = 0; i < array.length(); ++i) {
                    JSONObject f = array.optJSONObject(i);
                    if (f == null || f.optString("File", "").isEmpty())
                        continue;
                    Entry e = new Entry();
                    e.file = f.optString("File");
                    e.hash = f.optString("Hash", "");
                    ret.files.add(e);
                }
            }
            // the address by an IP does not have a certificate that fits: Spine itself falls back to the host with the name
            ret.server = o.optString("FallbackFileserver", "");
            if (ret.server.isEmpty())
                ret.server = o.optString("Fileserver", "");
        } catch (JSONException e) {
            throw new IOException("The files of the mod cannot be read");
        }
        if (!ret.server.isEmpty() && !ret.server.endsWith("/"))
            ret.server += "/";
        return ret;
    }

    // ---- the page of a mod

    /** The page of a mod: the description is HTML (br, b, u, ul, li, a, ...). */
    static final class Info {
        String name = "";
        String description = "";
        List<Entry> screenshots = new ArrayList<>();
        List<String> features = new ArrayList<>();
        List<Change> history = new ArrayList<>();
        boolean installAllowed = true;
    }

    static final class Change {
        String version = "";
        String text = "";
        long time;           // the day, only to sort
    }

    static final class Rating {
        int percent;        // 0..100
        int count;
        int[] stars = new int[5];
    }

    static final class Review {
        String user = "";
        String text = "";
        int rating;         // 1..5
    }

    /** The name of the language for the server: "pl" is "Polish", the languages that Spine does not have are English. */
    static String languageName(String code) {
        for (int i = 0; i < LANGUAGES.length; ++i) {
            if (LANGUAGE_CODES[i].equals(code))
                return LANGUAGES[i];
        }
        return "English";
    }

    /** The page in the language; a description that is not written in it is taken from English, then from German. Blocks. */
    static Info info(int projectId, String language) throws IOException {
        String[] order = {language, "English", "Deutsch"};
        Info ret = null;
        for (int n = 0; n < order.length; ++n) {
            String l = order[n];
            boolean asked = false;
            for (int k = 0; k < n; ++k)
                asked |= order[k].equals(l);
            if (asked)
                continue;
            Info i = infoOnce(projectId, l);
            if (ret == null)
                ret = i;
            if (!i.description.trim().isEmpty()) {
                ret.description = i.description;
                break;
            }
        }
        return ret;
    }

    private static Info infoOnce(int projectId, String language) throws IOException {
        String body = "{\"Username\":\"\",\"Password\":\"\",\"Language\":\"" + language + "\",\"ProjectID\":" + projectId + "}";
        Info ret = new Info();
        try {
            JSONObject o = new JSONObject(post("requestInfoPage", body));
            ret.name = o.optString("Name", "");
            ret.description = o.optString("Description", "");
            ret.installAllowed = !o.optString("InstallAllowed", "1").equals("0");
            JSONArray shots = o.optJSONArray("Screenshots");
            for (int i = 0; shots != null && i < shots.length(); ++i) {
                JSONObject s = shots.optJSONObject(i);
                if (s == null || s.optString("File", "").isEmpty())
                    continue;
                Entry e = new Entry();
                e.file = s.optString("File");
                e.hash = s.optString("Hash", "");
                ret.screenshots.add(e);
            }
            JSONArray features = o.optJSONArray("Features");
            for (int i = 0; features != null && i < features.length(); ++i) {
                String f = features.optString(i, "").trim();
                if (!f.isEmpty())
                    ret.features.add(f);
            }
            JSONArray history = o.optJSONArray("History");
            for (int i = 0; history != null && i < history.length(); ++i) {
                JSONObject h = history.optJSONObject(i);
                if (h == null)
                    continue;
                Change c = new Change();
                c.version = h.optString("MajorVersion", "0") + "." + h.optString("MinorVersion", "0")
                        + (h.optString("PatchVersion", "0").equals("0") ? "" : "." + h.optString("PatchVersion"));
                c.text = h.optString("Changelog", "");
                c.time = number(h, "Timestamp");
                ret.history.add(c);
            }
            java.util.Collections.sort(ret.history, (x, y) -> Long.compare(y.time, x.time));
        } catch (JSONException e) {
            throw new IOException("The page of the mod cannot be read");
        }
        return ret;
    }

    /** The average and the stars of the players. Blocks. */
    static Rating rating(int projectId) throws IOException {
        String body = "{\"ProjectID\":" + projectId + "}";
        Rating ret = new Rating();
        try {
            JSONObject w = new JSONObject(post("getWeightedRating", body));
            ret.percent = (int) number(w, "Rating");
            ret.count = (int) number(w, "RealCount");
            JSONObject s = new JSONObject(post("getRatings", body));
            for (int i = 0; i < 5; ++i)
                ret.stars[i] = (int) number(s, "Rating" + (i + 1));
        } catch (JSONException e) {
            throw new IOException("The rating cannot be read");
        }
        return ret;
    }

    /** The reviews of the players with a text. Blocks. */
    static List<Review> reviews(int projectId) throws IOException {
        List<Review> ret = new ArrayList<>();
        try {
            JSONArray array = new JSONObject(post("getReviews", "{\"ProjectID\":" + projectId + ",\"Username\":\"\",\"Password\":\"\"}"))
                    .optJSONArray("Reviews");
            for (int i = 0; array != null && i < array.length(); ++i) {
                JSONObject o = array.optJSONObject(i);
                if (o == null || o.optString("Review", "").trim().isEmpty())
                    continue;
                Review r = new Review();
                r.user = o.optString("Username", "");
                r.text = o.optString("Review", "").trim();
                r.rating = (int) number(o, "Rating");
                ret.add(r);
            }
        } catch (JSONException e) {
            throw new IOException("The reviews cannot be read");
        }
        return ret;
    }

    // ---- HTTP

    /**
     * The server of the database knows only the ciphers with the key exchange by RSA (TLS_RSA_WITH_AES_*), which newer systems
     * do not enable by default. They are added for this server; the certificate is checked as always.
     */
    private static final class RsaCiphers extends SSLSocketFactory {
        private final SSLSocketFactory base = (SSLSocketFactory) SSLSocketFactory.getDefault();

        private Socket prepare(Socket socket) {
            if (socket instanceof SSLSocket) {
                SSLSocket ssl = (SSLSocket) socket;
                Set<String> suites = new LinkedHashSet<>(Arrays.asList(ssl.getEnabledCipherSuites()));
                for (String suite : ssl.getSupportedCipherSuites()) {
                    if (suite.startsWith("TLS_RSA_WITH_AES_"))
                        suites.add(suite);
                }
                ssl.setEnabledCipherSuites(suites.toArray(new String[0]));
            }
            return socket;
        }

        @Override
        public String[] getDefaultCipherSuites() {
            return base.getDefaultCipherSuites();
        }

        @Override
        public String[] getSupportedCipherSuites() {
            return base.getSupportedCipherSuites();
        }

        @Override
        public Socket createSocket(Socket s, String host, int port, boolean autoClose) throws IOException {
            return prepare(base.createSocket(s, host, port, autoClose));
        }

        @Override
        public Socket createSocket(String host, int port) throws IOException {
            return prepare(base.createSocket(host, port));
        }

        @Override
        public Socket createSocket(String host, int port, InetAddress local, int localPort) throws IOException {
            return prepare(base.createSocket(host, port, local, localPort));
        }

        @Override
        public Socket createSocket(InetAddress host, int port) throws IOException {
            return prepare(base.createSocket(host, port));
        }

        @Override
        public Socket createSocket(InetAddress host, int port, InetAddress local, int localPort) throws IOException {
            return prepare(base.createSocket(host, port, local, localPort));
        }
    }

    private static String post(String endpoint, String body) throws IOException {
        HttpURLConnection c = (HttpURLConnection) new URL(API + endpoint).openConnection();
        if (c instanceof HttpsURLConnection)
            ((HttpsURLConnection) c).setSSLSocketFactory(new RsaCiphers());
        try {
            c.setConnectTimeout(15000);
            c.setReadTimeout(60000);
            c.setRequestMethod("POST");
            c.setRequestProperty("Content-Type", "application/json");
            c.setRequestProperty("User-Agent", "OpenGothic-Android");
            c.setDoOutput(true);
            byte[] data = body.getBytes(StandardCharsets.UTF_8);
            c.setFixedLengthStreamingMode(data.length);
            try (OutputStream out = c.getOutputStream()) {
                out.write(data);
            }
            int code = c.getResponseCode();
            if (code != 200)
                throw new IOException("Spine answered " + code);
            try (InputStream in = c.getInputStream()) {
                ByteArrayOutputStream out = new ByteArrayOutputStream();
                byte[] buf = new byte[16384];
                int n;
                while ((n = in.read(buf)) > 0)
                    out.write(buf, 0, n);
                return out.toString("UTF-8");
            }
        } finally {
            c.disconnect();
        }
    }
}
