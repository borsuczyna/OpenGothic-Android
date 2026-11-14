package org.opengothic.app;

import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.provider.DocumentsContract;

import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.HashMap;
import java.util.Locale;
import java.util.Map;

/**
 * Puts a save folder of the original Gothic II (savegameN with SAVEDAT.SAV, SAVEINFO.SAV and a .SAV per world) into
 * Saves/import of the game. The engine reads it at the next New game and writes it as a save of its own.
 */
final class OriginalSaveImport {
    private static final long MAX_FILE = 256L * 1024 * 1024;
    private static final int MAX_FILES = 64;

    private OriginalSaveImport() {
    }

    /** Returns the number of files that have been copied; the folder must be a save of the original game. */
    static int copy(Context context, Uri tree, File gameRoot) throws IOException {
        Map<String, Uri> files = list(context, tree);
        if (!files.containsKey("SAVEDAT.SAV") || !files.containsKey("SAVEINFO.SAV")) {
            throw new IOException(Texts.t(
                    "This folder is not a save of the original Gothic II: choose a folder like Saves/savegame1.",
                    "To nie jest zapis z oryginalnego Gothica II: wybierz folder taki jak Saves/savegame1."));
        }

        File saves = new File(gameRoot, "Saves");
        File target = new File(saves, "import");
        File staging = new File(saves, "import-new");
        deleteTree(staging);
        if (!staging.mkdirs())
            throw new IOException(Texts.t("Could not create the folder of the saves.",
                    "Nie można utworzyć katalogu zapisów."));

        byte[] buffer = new byte[64 * 1024];
        int count = 0;
        try {
            for (Map.Entry<String, Uri> e : files.entrySet()) {
                // the name is the upper case one of the original game, never a path
                File out = new File(staging, e.getKey());
                try (InputStream in = context.getContentResolver().openInputStream(e.getValue());
                     OutputStream os = new BufferedOutputStream(new FileOutputStream(out))) {
                    if (in == null)
                        throw new IOException(Texts.t("Could not read the file: ", "Nie można odczytać pliku: ") + e.getKey());
                    long size = 0;
                    int n;
                    while ((n = in.read(buffer)) != -1) {
                        size += n;
                        if (size > MAX_FILE)
                            throw new IOException(Texts.t("The save is too big.", "Zapis jest zbyt duży."));
                        os.write(buffer, 0, n);
                    }
                }
                count++;
            }
            deleteTree(target);
            if (!staging.renameTo(target))
                throw new IOException(Texts.t("Could not write the save.", "Nie można zapisać zapisu."));
        } finally {
            deleteTree(staging);
        }
        return count;
    }

    /** The .SAV files directly in the chosen folder, by their upper case name. */
    private static Map<String, Uri> list(Context context, Uri tree) throws IOException {
        Map<String, Uri> ret = new HashMap<>();
        String docId = DocumentsContract.getTreeDocumentId(tree);
        Uri children = DocumentsContract.buildChildDocumentsUriUsingTree(tree, docId);
        String[] columns = {DocumentsContract.Document.COLUMN_DOCUMENT_ID, DocumentsContract.Document.COLUMN_DISPLAY_NAME,
                DocumentsContract.Document.COLUMN_MIME_TYPE};
        try (Cursor c = context.getContentResolver().query(children, columns, null, null, null)) {
            if (c == null)
                throw new IOException(Texts.t("Could not open the selected folder.", "Nie można otworzyć wybranego folderu."));
            while (c.moveToNext()) {
                String name = c.getString(1);
                String mime = c.getString(2);
                if (name == null || DocumentsContract.Document.MIME_TYPE_DIR.equals(mime))
                    continue;
                String upper = name.toUpperCase(Locale.ROOT);
                if (!upper.endsWith(".SAV") || upper.contains("/") || upper.contains("\\") || upper.startsWith("."))
                    continue;
                if (ret.size() >= MAX_FILES)
                    throw new IOException(Texts.t("The folder has too many files.", "Folder zawiera zbyt wiele plików."));
                ret.put(upper, DocumentsContract.buildDocumentUriUsingTree(tree, c.getString(0)));
            }
        }
        return ret;
    }

    private static void deleteTree(File f) {
        if (f == null || !f.exists())
            return;
        File[] list = f.listFiles();
        if (list != null)
            for (File c : list)
                deleteTree(c);
        f.delete();
    }
}
