package org.opengothic.app;

import android.content.Context;
import android.content.SharedPreferences;

import java.io.File;

/**
 * Where the files of a game are: the folder that the launcher keeps for it in the files of the application (downloads and the
 * import of a ZIP go there), or a folder of the phone that the player pointed to and that is played from as it is, without a copy.
 */
final class GameRoots {
    private GameRoots() {
    }

    /** The folder of the player is kept under the name of the folder of the game ("path_Gothic2"). */
    private static final String PREFIX = "path_";

    static boolean isInstallation(File dir) {
        return dir != null && new File(dir, "Data").isDirectory()
                && new File(dir, "_work").isDirectory()
                && new File(dir, "System").isDirectory();
    }

    private static SharedPreferences prefs(Context c) {
        return c.getSharedPreferences(SteamBridge.PREFS, Context.MODE_PRIVATE);
    }

    /** The folder that the player has chosen while it is still an installation, otherwise null (it was moved or deleted). */
    static File linked(Context c, String folder) {
        String saved = prefs(c).getString(PREFIX + folder, null);
        if (saved == null || saved.isEmpty())
            return null;
        File dir = new File(saved);
        return isInstallation(dir) ? dir : null;
    }

    /** The folder that the game is played from: the chosen one, or the one of the launcher. */
    static File of(Context c, File external, String folder) {
        File linked = linked(c, folder);
        return linked != null ? linked : new File(external, folder);
    }

    static void link(Context c, String folder, File dir) {
        prefs(c).edit().putString(PREFIX + folder, dir.getAbsolutePath()).apply();
    }

    static void unlink(Context c, String folder) {
        prefs(c).edit().remove(PREFIX + folder).apply();
    }

    /**
     * The first line of game.txt, which the engine reads: the name of the folder in the files of the application, or the full
     * path (it starts with a slash) of a folder that the player has chosen.
     */
    static String gameTxt(Context c, String folder) {
        File linked = linked(c, folder);
        return linked != null ? linked.getAbsolutePath() : folder;
    }
}
