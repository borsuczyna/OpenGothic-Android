package org.opengothic.app;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Build;
import android.widget.Toast;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;

/** What the screens of Spine do with a mod: download, check, play, remove. */
final class SpineActions {
    private SpineActions() {
    }

    static final String[] GAME_FOLDERS = {"Gothic1", "Gothic2"};
    static final String[] GAME_TITLES = {"Gothic", "Gothic II"};

    private static final String EXTRA_ID = "id";
    private static final String EXTRA_NAME = "name";
    private static final String EXTRA_TEAM = "team";
    private static final String EXTRA_KEYWORDS = "keywords";
    private static final String EXTRA_GAME = "game";
    private static final String EXTRA_TYPE = "type";
    private static final String EXTRA_LANGUAGES = "languages";
    private static final String EXTRA_SIZE = "size";
    private static final String EXTRA_VERSION = "version";
    private static final String EXTRA_MINUTES = "minutes";
    private static final String EXTRA_UPDATED = "updated";

    /** A mod travels between the screens and to the service in the extras of an intent. */
    static Intent put(Intent intent, SpineApi.Project p) {
        return intent.putExtra(EXTRA_ID, p.id).putExtra(EXTRA_NAME, p.name).putExtra(EXTRA_TEAM, p.team)
                .putExtra(EXTRA_KEYWORDS, p.keywords).putExtra(EXTRA_GAME, p.gameType).putExtra(EXTRA_TYPE, p.modType)
                .putExtra(EXTRA_LANGUAGES, p.languages).putExtra(EXTRA_SIZE, p.size).putExtra(EXTRA_VERSION, p.version)
                .putExtra(EXTRA_MINUTES, p.avgMinutes).putExtra(EXTRA_UPDATED, p.updated);
    }

    static SpineApi.Project project(Intent intent) {
        SpineApi.Project p = new SpineApi.Project();
        p.id = intent.getIntExtra(EXTRA_ID, 0);
        p.name = intent.getStringExtra(EXTRA_NAME) != null ? intent.getStringExtra(EXTRA_NAME) : "mod";
        p.team = intent.getStringExtra(EXTRA_TEAM) != null ? intent.getStringExtra(EXTRA_TEAM) : "";
        p.keywords = intent.getStringExtra(EXTRA_KEYWORDS) != null ? intent.getStringExtra(EXTRA_KEYWORDS) : "";
        p.gameType = intent.getIntExtra(EXTRA_GAME, SpineApi.GOTHIC2);
        p.modType = intent.getIntExtra(EXTRA_TYPE, SpineApi.TOTAL_CONVERSION);
        p.languages = intent.getIntExtra(EXTRA_LANGUAGES, 0);
        p.size = intent.getLongExtra(EXTRA_SIZE, 0);
        int[] version = intent.getIntArrayExtra(EXTRA_VERSION);
        if (version != null && version.length == 3)
            p.version = version;
        p.avgMinutes = intent.getLongExtra(EXTRA_MINUTES, 0);
        p.updated = intent.getLongExtra(EXTRA_UPDATED, 0);
        return p;
    }

    // ---- the games

    /** The folder of a game: the one the player has chosen in the launcher, or the one in the files of the application. */
    static File rootOf(Context c, File external, int game) {
        return GameRoots.of(c, external, GAME_FOLDERS[game]);
    }

    /** The index of the game that runs a mod (0 Gothic, 1 Gothic II) or -1 when it is not installed. */
    static int gameFor(Context c, File external, int gameType) {
        boolean g1 = GameRoots.isInstallation(rootOf(c, external, 0));
        boolean g2 = GameRoots.isInstallation(rootOf(c, external, 1));
        if (gameType == SpineApi.GOTHIC)
            return g1 ? 0 : -1;
        if (gameType == SpineApi.GOTHIC2)
            return g2 ? 1 : -1;
        return g2 ? 1 : g1 ? 0 : -1;
    }

    static String needs(int gameType) {
        return gameType == SpineApi.GOTHIC ? GAME_TITLES[0] : GAME_TITLES[1];
    }

    // ---- the actions

    /** The download or the check goes in the service; true when it was started. */
    private static boolean start(Activity a, SpineApi.Project p, boolean check) {
        File external = a.getExternalFilesDir(null);
        if (external == null)
            return false;
        if (SpineDownloadService.running() != null) {
            Toast.makeText(a, Texts.t("Another mod is being downloaded.", "Trwa pobieranie innego moda."), Toast.LENGTH_SHORT).show();
            return false;
        }
        if (gameFor(a, external, p.gameType) < 0) {
            Toast.makeText(a, Texts.t("Install " + needs(p.gameType) + " first.", "Najpierw zainstaluj " + needs(p.gameType) + "."),
                    Toast.LENGTH_LONG).show();
            return false;
        }
        if (!check && external.getUsableSpace() < p.size * 2 + 200L * 1024 * 1024) {
            Toast.makeText(a, Texts.t("There is not enough free space on the phone.", "Brakuje wolnego miejsca w telefonie."),
                    Toast.LENGTH_LONG).show();
            return false;
        }
        if (Build.VERSION.SDK_INT >= 33)
            a.requestPermissions(new String[] {"android.permission.POST_NOTIFICATIONS"}, 1);
        SpineDownloadService.state = null;
        Intent intent = SpineDownloadService.intent(a, p, check);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O)
            a.startForegroundService(intent);
        else
            a.startService(intent);
        return true;
    }

    /** Tells once how the last download or check ended; the screen that is shown does it. Returns true when it did. */
    static boolean report(Activity a) {
        SpineDownloadService.State s = SpineDownloadService.state;
        if (s == null || s.running() || s.reported)
            return false;
        s.reported = true;
        String text = null;
        if (s.library != null)
            text = Texts.t(s.title + " needs " + s.library + " and cannot run in OpenGothic. Removed.",
                    s.title + " wymaga " + s.library + " i nie działa w OpenGothic. Usunięto.");
        else if (s.error != null)
            text = s.title + ": " + s.error;
        else if (s.done && s.check)
            text = Texts.t(s.title + ": no Ikarus or LeGo in the scripts. It can be downloaded.",
                    s.title + ": brak Ikarusa i LeGo w skryptach. Można pobierać.");
        else if (s.done)
            text = Texts.t(s.title + " is ready to play.", s.title + " jest gotowy do gry.");
        if (text != null)
            Toast.makeText(a, text, Toast.LENGTH_LONG).show();
        return true;
    }

    static boolean install(Activity a, SpineApi.Project p) {
        return start(a, p, false);
    }

    /** Looks into the scripts for Ikarus and LeGo, reading only the beginning of the archives. */
    static boolean check(Activity a, SpineApi.Project p) {
        return start(a, p, true);
    }

    static void play(Activity a, SpineStore store, SpineStore.Mod mod) {
        File external = a.getExternalFilesDir(null);
        if (external == null)
            return;
        int game = gameFor(a, external, mod.gameType);
        if (game < 0) {
            Toast.makeText(a, Texts.t("Install " + needs(mod.gameType) + " first.", "Najpierw zainstaluj " + needs(mod.gameType) + "."),
                    Toast.LENGTH_LONG).show();
            return;
        }
        if (mod.ini.indexOf('/') >= 0 || mod.ini.indexOf('\n') >= 0) {
            Toast.makeText(a, "Bad name of the ini", Toast.LENGTH_LONG).show();
            return;
        }
        try {
            // the files of the mod go to the folder of the game until the game is over
            SpineActivation.activate(store.root, rootOf(a, external, game), mod);
            try (OutputStream out = new FileOutputStream(new File(external, "game.txt"))) {
                out.write((GameRoots.gameTxt(a, GAME_FOLDERS[game]) + "\n" + mod.ini + "\n").getBytes(StandardCharsets.UTF_8));
            }
        } catch (IOException e) {
            SpineActivation.restore(store.root);
            new AlertDialog.Builder(a).setMessage(e.getMessage()).setPositiveButton("OK", null).show();
            return;
        }
        SharedPreferences prefs = a.getSharedPreferences(SteamBridge.PREFS, Activity.MODE_PRIVATE);
        prefs.edit().putInt("game", game).putLong("started", System.currentTimeMillis()).apply();
        a.startActivity(new Intent(a, GameActivity.class));
    }

    static void remove(Activity a, final SpineStore store, final SpineStore.Mod mod, final Runnable then) {
        new AlertDialog.Builder(a)
                .setTitle(Texts.t("Remove ", "Usunąć ") + mod.name + "?")
                .setMessage(Texts.t("The files of the mod are deleted. The saves stay.", "Pliki moda zostaną usunięte. Zapisy zostają."))
                .setPositiveButton(Texts.t("Remove", "Usuń"), (d, w) -> {
                    SpineActivation.restore(store.root);
                    store.remove(mod.id);
                    if (then != null)
                        then.run();
                })
                .setNegativeButton(Texts.t("Cancel", "Anuluj"), null)
                .show();
    }
}
