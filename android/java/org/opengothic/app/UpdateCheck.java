package org.opengothic.app;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.ArrayList;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Is there a newer version of the application: the release of the repository has the number of the version in its name
 * (the tag of it does not change). The release also carries the APK: {@link AppUpdate} downloads and installs it.
 */
final class UpdateCheck {
    private UpdateCheck() {
    }

    static final String PAGE = "https://github.com/borsuczyna/OpenGothic-Android/releases/tag/release";
    private static final String API = "https://api.github.com/repos/borsuczyna/OpenGothic-Android/releases/tags/release";

    /** The published release: its version and the APK that it carries (empty url when it has none). */
    static final class Release {
        final String version;
        final String apkUrl;
        final long   apkSize;

        Release(String version, String apkUrl, long apkSize) {
            this.version = version;
            this.apkUrl  = apkUrl;
            this.apkSize = apkSize;
        }
    }

    /** The version of the published release, for example "0.7.2". Blocks: not for the main thread. */
    static String latest() throws IOException {
        return release().version;
    }

    /** The published release with the link of its APK. Blocks: not for the main thread. */
    static Release release() throws IOException {
        Tls.ensure();
        HttpURLConnection c = (HttpURLConnection) new URL(API).openConnection();
        try {
            c.setConnectTimeout(10000);
            c.setReadTimeout(15000);
            c.setRequestProperty("Accept", "application/vnd.github+json");
            c.setRequestProperty("User-Agent", "OpenGothic-Android");
            if (c.getResponseCode() != 200)
                throw new IOException("GitHub answered " + c.getResponseCode());
            String text;
            try (InputStream in = c.getInputStream()) {
                ByteArrayOutputStream out = new ByteArrayOutputStream();
                byte[] buf = new byte[8192];
                int n;
                while ((n = in.read(buf)) > 0) {
                    out.write(buf, 0, n);
                    if (out.size() > 2 * 1024 * 1024)
                        throw new IOException("The answer is too big");
                }
                text = out.toString("UTF-8");
            }
            JSONObject release = new JSONObject(text);
            String version = release.optString("name");
            if (parse(version).isEmpty())
                version = release.optString("tag_name");
            if (parse(version).isEmpty())
                throw new IOException("The release has no version");

            String apkUrl = "";
            long   apkSize = 0;
            org.json.JSONArray assets = release.optJSONArray("assets");
            for (int i = 0; assets != null && i < assets.length(); ++i) {
                JSONObject a = assets.optJSONObject(i);
                if (a == null)
                    continue;
                String name = a.optString("name");
                String url  = a.optString("browser_download_url");
                // only what GitHub itself serves for this repository
                if (!name.toLowerCase(java.util.Locale.ROOT).endsWith(".apk") || !url.startsWith("https://github.com/borsuczyna/OpenGothic-Android/"))
                    continue;
                apkUrl  = url;
                apkSize = a.optLong("size", 0);
                break;
            }
            return new Release(version, apkUrl, apkSize);
        } catch (JSONException e) {
            throw new IOException("The answer of GitHub is not JSON");
        } finally {
            c.disconnect();
        }
    }

    /** The numbers of "v0.7.3" or "0.7.3-beta": 0, 7, 3. Empty when there are none. */
    static List<Integer> parse(String version) {
        List<Integer> ret = new ArrayList<>();
        if (version == null)
            return ret;
        Matcher m = Pattern.compile("(\\d+(?:\\.\\d+)*)").matcher(version);
        if (!m.find())
            return ret;
        for (String part : m.group(1).split("\\.")) {
            try {
                ret.add(Integer.parseInt(part));
            } catch (NumberFormatException e) {
                ret.add(0);
            }
        }
        return ret;
    }

    /** Is the version of the release higher than the one of this application. */
    static boolean newer(String remote, String local) {
        List<Integer> a = parse(remote);
        List<Integer> b = parse(local);
        if (a.isEmpty() || b.isEmpty())
            return false;
        for (int i = 0; i < Math.max(a.size(), b.size()); ++i) {
            int x = i < a.size() ? a.get(i) : 0;
            int y = i < b.size() ? b.get(i) : 0;
            if (x != y)
                return x > y;
        }
        return false;
    }
}
