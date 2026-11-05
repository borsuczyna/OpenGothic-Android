package org.opengothic.app;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/**
 * Reads the beginning of a .mod or .vdf file (a VDF archive of Gothic) while it streams in, to know where its compiled scripts
 * (*.DAT) are: the header and the catalog are at the start of the file, so only the bytes up to the end of the last DAT have to be
 * read to look for Ikarus and LeGo ({@link SpineScan}), not the whole archive with its textures and sounds.
 *
 * Header: 256 bytes comment, 16 bytes signature ("PSVDSC_V2.00"), then u32: entries, files, timestamp, data size, offset of the
 * catalog, version (296 bytes). An entry of the catalog is 80 bytes: name[64], u32 offset, u32 size, u32 type, u32 attributes.
 */
final class SpineVdf {
    private static final int HEADER = 296;
    private static final int ENTRY = 80;
    private static final int MAX_ENTRIES = 200000;
    private static final long TYPE_DIRECTORY = 0x40000000L;

    private enum Phase { HEADER, CATALOG, DATA, DONE, NOT_VDF }

    private Phase phase = Phase.HEADER;
    private final byte[] header = new byte[HEADER];
    private int headerFill;
    private byte[] catalog;
    private int catalogFill;
    private long catalogStart;
    private long position;
    private long end;
    private final List<long[]> ranges = new ArrayList<>();   // offset, end
    private int range;
    private SpineScan scan;
    private String found;

    /** Not an archive: the file is not looked into by this class. */
    boolean notVdf() {
        return phase == Phase.NOT_VDF;
    }

    /** Everything that can hold scripts was read (or there are none). */
    boolean finished() {
        return phase == Phase.DONE || phase == Phase.NOT_VDF || found != null;
    }

    /** The library that the scripts need ({@link SpineScan#IKARUS}, {@link SpineScan#LEGO}), or null. */
    String found() {
        return found;
    }

    /** How many bytes of the archive were needed: the end of the last script file. */
    long needed() {
        return end;
    }

    /** The next bytes of the (inflated) file. */
    void feed(byte[] data, int length) {
        int off = 0;
        while (off < length && !finished()) {
            switch (phase) {
                case HEADER: {
                    int n = Math.min(length - off, HEADER - headerFill);
                    System.arraycopy(data, off, header, headerFill, n);
                    headerFill += n;
                    off += n;
                    position += n;
                    if (headerFill == HEADER)
                        readHeader();
                    break;
                }
                case CATALOG: {
                    // bytes before the catalog are skipped
                    if (position < catalogStart) {
                        int skip = (int) Math.min(length - off, catalogStart - position);
                        off += skip;
                        position += skip;
                        break;
                    }
                    int n = Math.min(length - off, catalog.length - catalogFill);
                    System.arraycopy(data, off, catalog, catalogFill, n);
                    catalogFill += n;
                    off += n;
                    position += n;
                    if (catalogFill == catalog.length)
                        readCatalog();
                    break;
                }
                case DATA: {
                    int n = length - off;
                    scanData(data, off, n);
                    off += n;
                    position += n;
                    if (found == null && position >= end)
                        phase = Phase.DONE;
                    break;
                }
                default:
                    return;
            }
        }
    }

    private static long u32(byte[] b, int at) {
        return (b[at] & 0xffL) | (b[at + 1] & 0xffL) << 8 | (b[at + 2] & 0xffL) << 16 | (b[at + 3] & 0xffL) << 24;
    }

    private void readHeader() {
        String signature = new String(header, 256, 12, java.nio.charset.StandardCharsets.ISO_8859_1);
        long entries = u32(header, 272);
        long root = u32(header, 288);
        if (!signature.equals("PSVDSC_V2.00") || entries > MAX_ENTRIES || root < HEADER || root > (1L << 31)) {
            phase = Phase.NOT_VDF;
            return;
        }
        if (entries == 0) {
            phase = Phase.DONE;
            return;
        }
        catalogStart = root;
        catalog = new byte[(int) (entries * ENTRY)];
        phase = Phase.CATALOG;
    }

    private void readCatalog() {
        // The libraries are in the scripts of the content (GOTHIC.DAT), they are run by the main parser. The scripts of the menu,
        // the sounds and the effects (MENU.DAT, SFX.DAT, ...) do not have them, and MENU.DAT is often at the end of the archive.
        List<long[]> content = new ArrayList<>();
        List<long[]> others = new ArrayList<>();
        for (int i = 0; i + ENTRY <= catalog.length; i += ENTRY) {
            long type = u32(catalog, i + 72);
            if ((type & TYPE_DIRECTORY) != 0)
                continue;
            int len = 0;
            while (len < 64 && catalog[i + len] != 0)
                ++len;
            String name = new String(catalog, i, len, java.nio.charset.StandardCharsets.ISO_8859_1).trim().toUpperCase(Locale.ROOT);
            if (!name.endsWith(".DAT"))
                continue;
            long offset = u32(catalog, i + 64);
            long size = u32(catalog, i + 68);
            if (size <= 0)
                continue;
            if (name.startsWith("GOTHIC"))
                content.add(new long[] {offset, offset + size});
            else if (!name.equals("MENU.DAT"))
                others.add(new long[] {offset, offset + size});
        }
        catalog = null;
        // an unusual archive (no GOTHIC*.DAT) has all its scripts looked into
        ranges.addAll(content.isEmpty() ? others : content);
        if (ranges.isEmpty()) {
            phase = Phase.DONE;   // an archive without scripts: textures, sounds, ...
            return;
        }
        java.util.Collections.sort(ranges, (a, b) -> Long.compare(a[0], b[0]));
        for (long[] r : ranges)
            end = Math.max(end, r[1]);
        phase = Phase.DATA;
        if (position >= end)
            phase = Phase.DONE;
    }

    /** The part of a piece that lies inside a DAT file goes to the search; every file has its own search. */
    private void scanData(byte[] data, int off, int length) {
        long from = position;
        long to = position + length;
        while (range < ranges.size() && found == null) {
            long[] r = ranges.get(range);
            if (r[0] >= to)
                break;
            if (r[1] <= from) {
                ++range;
                scan = null;
                continue;
            }
            long a = Math.max(from, r[0]);
            long b = Math.min(to, r[1]);
            if (scan == null)
                scan = new SpineScan();
            scan.feed(slice(data, off + (int) (a - from), (int) (b - a)), (int) (b - a));
            found = scan.found();
            if (r[1] > to)
                break;
            ++range;
            scan = null;
        }
    }

    private static byte[] slice(byte[] data, int off, int length) {
        if (off == 0 && length == data.length)
            return data;
        byte[] ret = new byte[length];
        System.arraycopy(data, off, ret, 0, length);
        return ret;
    }
}
