package org.opengothic.app;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.List;

/**
 * Are the archives of the game whole. A Gothic archive (VDF) has "PSVDSC_V2.00" after a comment of 256 bytes; an interrupted download or a damaged
 * ZIP leaves files of the right size with no content. The engine then finds no world and has nothing to play.
 */
final class GameFiles {
    private GameFiles() {
    }

    /** The size of the comment at the start of a VDF, the signature comes after it. */
    private static final int COMMENT = 256;

    private static final byte[] SIGNATURE = "PSVDSC_V2.00".getBytes(java.nio.charset.StandardCharsets.US_ASCII);

    /** The names of the archives in Data that are not a VDF (no signature): empty when the installation is good. */
    static List<String> brokenArchives(File root) {
        List<String> broken = new ArrayList<>();
        File[] files = new File(root, "Data").listFiles();
        if (files == null)
            return broken;
        for (File f : files) {
            String name = f.getName().toLowerCase(java.util.Locale.ROOT);
            if (!f.isFile() || !name.endsWith(".vdf"))
                continue;
            if (!hasSignature(f))
                broken.add(f.getName());
        }
        return broken;
    }

    /**
     * A line for the player about an archive that is not a VDF: the name, the size and the first bytes. An empty file or
     * zeros are a damaged copy, another start is a file that the check does not know.
     */
    static String describe(File root, String name) {
        File f = new File(new File(root, "Data"), name);
        StringBuilder text = new StringBuilder(name).append(" (").append(f.length()).append(" B");
        byte[] head = new byte[8];
        try (InputStream in = new FileInputStream(f)) {
            int n = in.read(head);
            if (n > 0) {
                text.append(", ");
                for (int i = 0; i < n; ++i)
                    text.append(String.format(java.util.Locale.ROOT, "%02x", head[i] & 0xff)).append(i + 1 < n ? " " : "");
            }
        } catch (IOException e) {
            text.append(", ").append(e.getClass().getSimpleName());
        }
        return text.append(")").toString();
    }

    /**
     * A VDF is 256 bytes of a text comment (Gothic archives begin with "Gothic ...") and then the signature, so the
     * signature is at the offset 256. An archive that begins with the signature is good too.
     */
    private static boolean hasSignature(File f) {
        byte[] head = new byte[COMMENT + SIGNATURE.length];
        int done = 0;
        try (InputStream in = new FileInputStream(f)) {
            while (done < head.length) {
                int n = in.read(head, done, head.length - done);
                if (n < 0)
                    break;
                done += n;
            }
        } catch (IOException e) {
            return false;
        }
        return matches(head, done, COMMENT) || matches(head, done, 0);
    }

    private static boolean matches(byte[] data, int length, int offset) {
        if (length < offset + SIGNATURE.length)
            return false;
        for (int i = 0; i < SIGNATURE.length; ++i) {
            if (data[offset + i] != SIGNATURE[i])
                return false;
        }
        return true;
    }
}
