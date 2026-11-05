package org.opengothic.app;

/**
 * Looks for the libraries Ikarus and LeGo in the scripts of a mod while its files are downloaded.
 * The scripts are compiled into GOTHIC.DAT (inside a .mod or .vdf, or loose) and the DAT keeps the names of all the symbols as
 * upper case text, so the names that only the libraries declare (MEM_ReadInt, HookEngine, LeGo_Init, ...) are found in the bytes.
 * Mods that need them read and write the memory of the original program, which OpenGothic does not have.
 */
final class SpineScan {
    /** The libraries that the engine cannot run in general. */
    static final String IKARUS = "Ikarus";
    static final String LEGO = "LeGo";

    private static final String[][] MARKERS = {
        {IKARUS, "MEM_READINT"},
        {IKARUS, "MEM_WRITEINT"},
        {IKARUS, "MEM_INITALL"},
        {IKARUS, "HOOKENGINE"},
        {IKARUS, "CALL__THISCALL"},
        {IKARUS, "CALL__CDECL"},
        {LEGO, "LEGO_INIT"},
        {LEGO, "LEGO_FLAGS"},
        {LEGO, "FF_APPLYONCEEXTGT"},
        {LEGO, "FF_APPLYEXTGT"},
    };

    private static final int LONGEST = 17;

    private final byte[][]  words = new byte[MARKERS.length][];
    private final boolean[] first = new boolean[256];
    private final byte[]    tail  = new byte[LONGEST];
    private int             tailLength;
    private String          found;

    SpineScan() {
        for (int i = 0; i < MARKERS.length; ++i) {
            words[i] = MARKERS[i][1].getBytes(java.nio.charset.StandardCharsets.US_ASCII);
            first[words[i][0] & 0xff] = true;
        }
    }

    /** Only the files with compiled scripts are worth the time. */
    static boolean worthScanning(String name) {
        String low = name.toLowerCase(java.util.Locale.ROOT);
        return low.endsWith(".mod") || low.endsWith(".vdf") || low.endsWith(".dat");
    }

    /** The library whose marker was found ({@link #IKARUS} or {@link #LEGO}), or null. */
    String found() {
        return found;
    }

    /** The next piece of the file; the pieces are one stream, a word may be cut between two of them. */
    void feed(byte[] data, int length) {
        if (found != null || length <= 0)
            return;
        // the end of the previous piece and the beginning of this one: the words that are cut between them
        if (tailLength > 0) {
            int head = Math.min(length, LONGEST - 1);
            byte[] joint = new byte[tailLength + head];
            System.arraycopy(tail, 0, joint, 0, tailLength);
            System.arraycopy(data, 0, joint, tailLength, head);
            search(joint, joint.length, tailLength);
            if (found != null)
                return;
        }
        search(data, length, 0);
        int keep = Math.min(length, LONGEST - 1);
        if (length >= LONGEST - 1) {
            System.arraycopy(data, length - keep, tail, 0, keep);
            tailLength = keep;
        } else {
            // a short piece: it is added to what was kept
            int total = Math.min(tailLength + length, LONGEST - 1);
            byte[] merged = new byte[total];
            int fromTail = total - length;
            if (fromTail > 0)
                System.arraycopy(tail, tailLength - fromTail, merged, 0, fromTail);
            System.arraycopy(data, 0, merged, Math.max(0, fromTail), Math.min(length, total));
            System.arraycopy(merged, 0, tail, 0, total);
            tailLength = total;
        }
    }

    /** A word that starts before {@code from} was already seen (or ends inside the first part, which is not enough to find it). */
    private void search(byte[] data, int length, int from) {
        for (int i = 0; i < length; ++i) {
            if (!first[data[i] & 0xff])
                continue;
            for (int w = 0; w < words.length; ++w) {
                byte[] word = words[w];
                if (i + word.length > length || data[i] != word[0])
                    continue;
                // a word that lies fully in the old part was found before
                if (from > 0 && i + word.length <= from)
                    continue;
                int k = 1;
                while (k < word.length && data[i + k] == word[k])
                    ++k;
                if (k == word.length) {
                    found = MARKERS[w][0];
                    return;
                }
            }
        }
    }
}
