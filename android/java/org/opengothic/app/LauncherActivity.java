package org.opengothic.app;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.res.ColorStateList;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.ColorMatrix;
import android.graphics.ColorMatrixColorFilter;
import android.graphics.Outline;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.RippleDrawable;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.Gravity;
import android.view.View;
import android.view.ViewOutlineProvider;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.PopupMenu;
import android.widget.ProgressBar;
import android.widget.TextView;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

/**
 * The library: the games that can be played here, each with its own place on the phone. A game is downloaded from Steam
 * (the account of the player) or imported from a ZIP of an installation, and then it is played with one touch.
 */
public final class LauncherActivity extends Activity {
    private static final int REQUEST_IMPORT = 1;
    private static final int REQUEST_LOGIN = 3;
    private static final int MAX_ENTRIES = 200000;
    private static final long MAX_SIZE = 20L * 1024 * 1024 * 1024;

    private static final int GOLD = 0xffe3cf9f;
    private static final int CREAM = 0xfff1e7cd;
    private static final int GREY = 0xff9fb2c0;

    /** The games that can be installed next to each other. The folder of Gothic II is the one that older versions had. */
    private static final class GameDef {
        final String folder;
        final String title;
        final String subtitle;
        final int appId;
        final String cover;
        final String ini;   // the ini of the mod in System, "-" for a game with no mod

        GameDef(String folder, String title, String subtitle, int appId, String cover, String ini) {
            this.folder = folder;
            this.title = title;
            this.subtitle = subtitle;
            this.appId = appId;
            this.cover = cover;
            this.ini = ini;
        }
    }

    // app ids of Steam: Gothic 1 Classic, Gothic II: Gold Edition (with Night of the Raven), The Chronicles of Myrtana: Archolos
    private static final GameDef[] GAMES = {
        new GameDef("Gothic1", "Gothic", "", 65540, "covers/gothic1.webp", "-"),
        new GameDef("Gothic2", "Gothic II", "Noc Kruka", 39510, "covers/gothic2.webp", "-"),
        new GameDef("Myrtana", "The Chronicles of Myrtana", "Archolos", 1467450, "covers/myrtana.webp", "TheChroniclesOfMyrtana.ini"),
    };

    /** The names that Steam gives the languages of a game and what the player reads. */
    private static final String[][] LANGUAGES = {
        {"polish", "Polski"}, {"english", "English"}, {"german", "Deutsch"}, {"russian", "Русский"},
        {"french", "Français"}, {"spanish", "Español"}, {"italian", "Italiano"}, {"czech", "Čeština"},
    };

    private static final class Card {
        FrameLayout root;
        ImageView cover;
        TextView title;
        TextView subtitle;
        TextView chip;
        TextView error;
        TextView detail;
        ProgressBar bar;
        Button primary;
        TextView menu;
    }

    private final Card[] cards = new Card[GAMES.length];
    private final Handler ui = new Handler(Looper.getMainLooper());
    private SharedPreferences prefs;
    private SteamStore store;
    private SteamStore.Session session;
    private Set<Integer> owned;           // null: not known
    private TextView accountButton;
    private TextView updateButton;
    private boolean updateAvailable;
    private UpdateCheck.Release updateRelease;

    private File external;
    private int lastPlayed = 1;
    private boolean askedAboutExit;
    private volatile int importing = -1;  // the game that is imported or removed now
    private volatile String importText = "";
    private int importTarget = -1;
    private boolean ownedAsked;
    private boolean loginShown;

    @Override
    public void onCreate(Bundle state) {
        super.onCreate(state);
        external = getExternalFilesDir(null);
        if (external == null) {
            finish();
            return;
        }
        prefs = getSharedPreferences(SteamBridge.PREFS, MODE_PRIVATE);
        store = new SteamStore(this);
        lastPlayed = Math.max(0, Math.min(GAMES.length - 1, prefs.getInt("game", 1)));
        SystemUi.hideBars(this);
        setContentView(buildUi());
        reloadSession();
        refresh();
        checkUpdate();
        ui.postDelayed(this::tick, 600);
    }

    /** Asks GitHub once at the start, a failure (no connection) only means that nothing is shown. */
    private void checkUpdate() {
        final String mine = version();
        new Thread(() -> {
            try {
                final UpdateCheck.Release release = UpdateCheck.release();
                final boolean newer = UpdateCheck.newer(release.version, mine);
                runOnUiThread(() -> {
                    updateAvailable = newer;
                    updateRelease   = release;
                    if (updateButton != null)
                        updateButton.setVisibility(newer ? View.VISIBLE : View.GONE);
                    // offered by itself once for every version: the chip stays for the player who said "later"
                    if (newer && !isFinishing() && !release.version.equals(prefs.getString("updateAsked", ""))) {
                        prefs.edit().putString("updateAsked", release.version).apply();
                        showUpdate();
                    }
                });
            } catch (IOException ignored) {
                // no connection or no answer: no label
            }
        }, "update-check").start();
    }

    /** The newer version: the player is asked, then the APK of the release is downloaded and the system asks to install it. */
    private void showUpdate() {
        final UpdateCheck.Release release = updateRelease;
        if (release == null || AppUpdate.running())
            return;
        final boolean hasApk = !release.apkUrl.isEmpty();
        String size = release.apkSize > 0 ? " (" + (release.apkSize + 1048575) / 1048576 + " MB)" : "";
        AlertDialog.Builder b = new AlertDialog.Builder(this)
                .setTitle(Texts.t("Update available", "Dostępna aktualizacja") + " " + release.version)
                .setMessage(Texts.t("Your version: ", "Twoja wersja: ") + version() + "\n\n"
                        + (hasApk
                        ? Texts.t("Download the new version" + size + " and install it now? Android asks to confirm the installation. Your games and saves stay.",
                                "Pobrać nową wersję" + size + " i zainstalować teraz? Android poprosi o potwierdzenie instalacji. Gry i zapisy zostają.")
                        : Texts.t("The release has no APK to install by itself. Open its page?", "Wydanie nie ma pliku APK do automatycznej instalacji. Otworzyć jego stronę?")))
                .setNegativeButton(Texts.t("Later", "Później"), null);
        if (hasApk) {
            b.setPositiveButton(Texts.t("Install", "Zainstaluj"), (d, w) -> startUpdate());
            b.setNeutralButton(Texts.t("Release page", "Strona wydania"), (d, w) -> openReleasePage());
        } else {
            b.setPositiveButton(Texts.t("Release page", "Strona wydania"), (d, w) -> openReleasePage());
        }
        b.show();
    }

    private void openReleasePage() {
        startActivity(new Intent(Intent.ACTION_VIEW, Uri.parse(UpdateCheck.PAGE)));
    }

    private void startUpdate() {
        if (updateRelease == null)
            return;
        if (!AppUpdate.canInstall(this)) {
            // Android lets an application install packages only after the player has allowed it, once, in the settings of the application
            new AlertDialog.Builder(this)
                    .setMessage(Texts.t("Android needs your permission for the launcher to install updates. Allow it on the next screen, come back and choose the update again.",
                            "Android wymaga Twojej zgody, żeby launcher mógł instalować aktualizacje. Zezwól na następnym ekranie, wróć i wybierz aktualizację jeszcze raz."))
                    .setPositiveButton(Texts.t("Open settings", "Otwórz ustawienia"), (d, w) -> {
                        try {
                            startActivity(AppUpdate.permissionIntent(this));
                        } catch (RuntimeException e) {
                            openReleasePage();
                        }
                    })
                    .setNegativeButton(Texts.t("Cancel", "Anuluj"), null)
                    .show();
            return;
        }
        // the APK is written to the session of the installer, and the installer unpacks it: twice its size at least
        if (getFilesDir().getUsableSpace() < Math.max(300L * 1024 * 1024, 2 * updateRelease.apkSize)) {
            new AlertDialog.Builder(this)
                    .setMessage(Texts.t("There is not enough free space on the phone.", "Brakuje wolnego miejsca w telefonie."))
                    .setPositiveButton("OK", null).show();
            return;
        }
        AppUpdate.start(this, updateRelease, () -> runOnUiThread(this::updateProgress));
        updateProgress();
    }

    /** The chip shows the download; a failure is told once. */
    private void updateProgress() {
        if (updateButton == null || isFinishing())
            return;
        int p = AppUpdate.percent;
        if (p >= 0) {
            updateButton.setText(p >= 101 ? Texts.t("Installing...", "Instalacja...")
                    : Texts.t("Downloading ", "Pobieranie ") + p + "%");
            updateButton.setVisibility(View.VISIBLE);
            return;
        }
        updateButton.setText(Texts.t("Update available", "Dostępna aktualizacja"));
        updateButton.setVisibility(updateAvailable ? View.VISIBLE : View.GONE);
        if (!AppUpdate.error.isEmpty()) {
            final String why = AppUpdate.error;
            AppUpdate.error = "";
            new AlertDialog.Builder(this)
                    .setTitle(Texts.t("The update failed", "Aktualizacja się nie udała"))
                    .setMessage(why)
                    .setPositiveButton(Texts.t("Release page", "Strona wydania"), (d, w) -> openReleasePage())
                    .setNegativeButton(Texts.t("Close", "Zamknij"), null)
                    .show();
        }
    }

    private String version() {
        try {
            String name = getPackageManager().getPackageInfo(getPackageName(), 0).versionName;
            return name != null ? name : "";
        } catch (android.content.pm.PackageManager.NameNotFoundException e) {
            return "";
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (external == null)
            return;
        reloadSession();
        refresh();
        checkGameExit();
    }

    /**
     * The game that ends a few seconds after it has started did not want to: the end of its log is shown, so that the reason
     * is known (and can be sent to somebody) without a computer.
     */
    private void checkGameExit() {
        long started = prefs.getLong("started", 0);
        if (started == 0 || askedAboutExit)
            return;
        prefs.edit().remove("started").apply();
        boolean quick = System.currentTimeMillis() - started < 25000;
        if (!quick && !Logs.crashedSince(this, started))
            return;
        askedAboutExit = true;
        String tail = Logs.tail(new java.io.File(getFilesDir(), "crash.log"), 1500);
        if (tail.isEmpty())
            tail = Logs.tail(new java.io.File(getFilesDir(), "log.txt"), 1500);
        new AlertDialog.Builder(this)
                .setTitle(Texts.t("The game has closed", "Gra się zamknęła"))
                .setMessage(Texts.t("The game ended a moment after the start. If it was not meant to, send the log.\n\n",
                        "Gra zakończyła się chwilę po starcie. Jeśli to nie było zamierzone, wyślij log.\n\n") + tail)
                .setPositiveButton(Texts.t("Share the log", "Udostępnij log"), (d, w) -> {
                    askedAboutExit = false;
                    Logs.share(this);
                })
                .setNegativeButton(Texts.t("Close", "Zamknij"), (d, w) -> askedAboutExit = false)
                .setOnCancelListener(d -> askedAboutExit = false)
                .show();
    }

    @Override
    protected void onDestroy() {
        ui.removeCallbacksAndMessages(null);
        super.onDestroy();
    }

    // ---- the account

    private void reloadSession() {
        session = SteamBridge.available() ? store.load() : null;
        if (session == null) {
            owned = null;
            ownedAsked = false;
            boolean skipped = prefs.getBoolean(SteamBridge.SKIPPED, false);
            if (SteamBridge.available() && !skipped && !loginShown) {
                // nobody is signed in: the login is the first thing that is seen
                loginShown = true;
                startActivityForResult(SteamBridge.loginIntent(this), REQUEST_LOGIN);
            }
            return;
        }
        if (owned == null)
            owned = store.loadOwned();
        if (!ownedAsked) {
            ownedAsked = true;
            askOwned(session);
        }
    }

    /** The games of the account change when the player buys one: asked again at every start. */
    private void askOwned(final SteamStore.Session s) {
        new Thread(() -> {
            Set<Integer> have = null;
            boolean again = false;
            try {
                Set<Integer> ids = new HashSet<>();
                for (GameDef g : GAMES)
                    ids.add(g.appId);
                have = SteamBridge.owned(s, ids);
            } catch (SteamBridge.Denied e) {
                again = e.signIn;
            }
            final Set<Integer> result = have;
            final boolean gone = again;
            runOnUiThread(() -> {
                if (isFinishing())
                    return;
                if (gone) {
                    store.clear();
                    session = null;
                    owned = null;
                    ownedAsked = false;
                    loginShown = false;
                    reloadSession();
                } else if (result != null) {
                    owned = result;
                    store.saveOwned(result);
                }
                refresh();
            });
        }, "steam-owned").start();
    }

    @Override
    protected void onActivityResult(int request, int result, Intent data) {
        super.onActivityResult(request, result, data);
        if (request == REQUEST_LOGIN) {
            loginShown = false;
            if (result != RESULT_OK && !prefs.getBoolean(SteamBridge.SKIPPED, false)) {
                finish();
                return;
            }
            ownedAsked = false;
            reloadSession();
            refresh();
            return;
        }
        if (request != REQUEST_IMPORT || result != RESULT_OK || data == null || data.getData() == null || importTarget < 0)
            return;

        final Uri uri = data.getData();
        final int game = importTarget;
        importing = game;
        importText = Texts.t("Importing...", "Importowanie...");
        refresh();
        new Thread(() -> importArchive(uri, game), "game-import").start();
    }

    // ---- the screen

    private int dp(int value) {
        return Math.round(value * getResources().getDisplayMetrics().density);
    }

    private View buildUi() {
        LinearLayout page = new LinearLayout(this);
        page.setOrientation(LinearLayout.VERTICAL);
        page.setPadding(dp(28), dp(14), dp(28), dp(20));
        page.setBackground(new GradientDrawable(GradientDrawable.Orientation.TL_BR,
                new int[] {0xff050b12, 0xff0e2130, 0xff060c13}));

        // the header: the name, and the account and the mods on the right
        LinearLayout header = new LinearLayout(this);
        header.setGravity(Gravity.CENTER_VERTICAL);
        page.addView(header, new LinearLayout.LayoutParams(-1, -2));

        LinearLayout names = new LinearLayout(this);
        names.setOrientation(LinearLayout.VERTICAL);
        header.addView(names, new LinearLayout.LayoutParams(-2, -2));
        TextView title = text("OpenGothic", 32, CREAM);
        title.setTypeface(Typeface.create("serif", Typeface.NORMAL));
        names.addView(title);
        names.addView(text(Texts.t("Your library", "Twoja biblioteka"), 14, GREY));

        // the game of the author: a small banner between the name and the buttons
        LinearLayout.LayoutParams promoParams = new LinearLayout.LayoutParams(0, dp(60), 1.f);
        promoParams.setMargins(dp(16), 0, dp(6), 0);
        header.addView(buildPromo(), promoParams);

        // a newer version of the application is published: the page of it opens
        updateButton = chip(Texts.t("Update available", "Dostępna aktualizacja"));
        updateButton.setTextColor(GOLD);
        updateButton.setBackground(round(0x33c9a24a, 0xff8a7640, dp(1), dp(20)));
        updateButton.setVisibility(View.GONE);
        updateButton.setOnClickListener(v -> showUpdate());
        header.addView(updateButton);

        TextView mods = chip(Texts.t("Mods", "Mody"));
        mods.setOnClickListener(v -> startActivity(new Intent(this, ModsActivity.class)));
        LinearLayout.LayoutParams mp0 = new LinearLayout.LayoutParams(-2, -2);
        mp0.setMargins(dp(10), 0, 0, 0);
        header.addView(mods, mp0);
        accountButton = chip("");
        accountButton.setOnClickListener(v -> accountMenu());
        LinearLayout.LayoutParams ap = new LinearLayout.LayoutParams(-2, -2);
        ap.setMargins(dp(10), 0, 0, 0);
        header.addView(accountButton, ap);

        // the games
        LinearLayout row = new LinearLayout(this);
        row.setOrientation(LinearLayout.HORIZONTAL);
        LinearLayout.LayoutParams rp = new LinearLayout.LayoutParams(-1, 0, 1.f);
        rp.setMargins(0, dp(14), 0, 0);
        page.addView(row, rp);
        for (int i = 0; i < GAMES.length; ++i) {
            Card c = buildCard(i);
            cards[i] = c;
            LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(0, -1, 1.f);
            lp.setMargins(i == 0 ? 0 : dp(8), 0, i == GAMES.length - 1 ? 0 : dp(8), 0);
            row.addView(c.root, lp);
        }
        return page;
    }

    /** Summer Hideout: the picture, the logo, two lines about the game and the way to its Discord. */
    private View buildPromo() {
        final int radius = dp(16);
        FrameLayout box = new FrameLayout(this);
        box.setClipToOutline(true);
        box.setOutlineProvider(new ViewOutlineProvider() {
            @Override
            public void getOutline(View view, Outline outline) {
                outline.setRoundRect(0, 0, view.getWidth(), view.getHeight(), radius);
            }
        });
        box.setBackgroundColor(0xff0b1823);
        box.setOnClickListener(v -> openDiscord());

        ImageView bg = new ImageView(this);
        bg.setScaleType(ImageView.ScaleType.CENTER_CROP);
        Bitmap picture = loadCover("promo/summerhideout-bg.webp");
        if (picture != null)
            bg.setImageBitmap(picture);
        box.addView(bg, new FrameLayout.LayoutParams(-1, -1));

        // the picture is darker where the text is
        View shade = new View(this);
        shade.setBackground(new GradientDrawable(GradientDrawable.Orientation.LEFT_RIGHT,
                new int[] {0x33050b12, 0xb3050b12, 0xe6050b12}));
        box.addView(shade, new FrameLayout.LayoutParams(-1, -1));

        LinearLayout row = new LinearLayout(this);
        row.setGravity(Gravity.CENTER_VERTICAL);
        row.setPadding(dp(10), dp(4), dp(8), dp(4));
        box.addView(row, new FrameLayout.LayoutParams(-1, -1));

        ImageView logo = new ImageView(this);
        logo.setScaleType(ImageView.ScaleType.FIT_CENTER);
        Bitmap mark = loadCover("promo/summerhideout-logo.webp");
        if (mark != null)
            logo.setImageBitmap(mark);
        row.addView(logo, new LinearLayout.LayoutParams(dp(84), -1));

        LinearLayout texts = new LinearLayout(this);
        texts.setOrientation(LinearLayout.VERTICAL);
        texts.setGravity(Gravity.CENTER_VERTICAL);
        LinearLayout.LayoutParams tp = new LinearLayout.LayoutParams(0, -2, 1.f);
        tp.setMargins(dp(10), 0, dp(8), 0);
        row.addView(texts, tp);

        LinearLayout titleRow = new LinearLayout(this);
        titleRow.setGravity(Gravity.BOTTOM);
        TextView name = text("Summer Hideout", 14, CREAM);
        name.setTypeface(Typeface.create("serif", Typeface.NORMAL));
        titleRow.addView(name);
        TextView author = text(Texts.t("From the developer", "Od twórcy"), 10, GREY);
        author.setPadding(dp(8), 0, 0, dp(2));
        titleRow.addView(author);
        texts.addView(titleRow);

        TextView about = text(Texts.t(
                "Live the summer of 2005 in a Polish village, rebuild a car and uncover the dark secrets of Racibórz-Zdrój.",
                "Przeżyj lato 2005 na polskiej wsi, odbuduj samochód i odkryj mroczne tajemnice Racibórz-Zdroju."), 10, 0xffd3dde3);
        about.setMaxLines(3);
        about.setEllipsize(android.text.TextUtils.TruncateAt.END);
        texts.addView(about);

        TextView discord = text("Discord  \u203a", 13, GOLD);
        discord.setGravity(Gravity.CENTER);
        discord.setPadding(dp(14), dp(6), dp(14), dp(6));
        discord.setBackground(round(0x33c9a24a, GOLD, dp(1), dp(18)));
        discord.setOnClickListener(v -> openDiscord());
        row.addView(discord);

        GradientDrawable border = new GradientDrawable();
        border.setColor(0);
        border.setCornerRadius(radius);
        border.setStroke(dp(1), 0xff2c4658);
        box.setForeground(border);
        return box;
    }

    private void openDiscord() {
        try {
            startActivity(new Intent(Intent.ACTION_VIEW, Uri.parse("https://discord.gg/summer-hideout")));
        } catch (android.content.ActivityNotFoundException ignored) {
            // no browser on the phone
        }
    }

    private Card buildCard(final int index) {
        final Card c = new Card();
        final int radius = dp(22);

        c.root = new FrameLayout(this);
        c.root.setClipToOutline(true);
        c.root.setOutlineProvider(new ViewOutlineProvider() {
            @Override
            public void getOutline(View view, Outline outline) {
                outline.setRoundRect(0, 0, view.getWidth(), view.getHeight(), radius);
            }
        });
        c.root.setBackgroundColor(0xff0b1823);

        c.cover = new ImageView(this);
        c.cover.setScaleType(ImageView.ScaleType.CENTER_CROP);
        Bitmap cover = loadCover(GAMES[index].cover);
        if (cover != null)
            c.cover.setImageBitmap(cover);
        c.root.addView(c.cover, new FrameLayout.LayoutParams(-1, -1));

        // the lower part is darker, the texts are on it
        View shade = new View(this);
        shade.setBackground(new GradientDrawable(GradientDrawable.Orientation.TOP_BOTTOM,
                new int[] {0x00050b12, 0x00050b12, 0xf0050b12}));
        c.root.addView(shade, new FrameLayout.LayoutParams(-1, -1));

        LinearLayout info = new LinearLayout(this);
        info.setOrientation(LinearLayout.VERTICAL);
        info.setPadding(dp(14), dp(10), dp(14), dp(14));
        c.root.addView(info, new FrameLayout.LayoutParams(-1, -2, Gravity.BOTTOM));

        c.title = text(GAMES[index].title, 24, CREAM);
        c.title.setTypeface(Typeface.create("serif", Typeface.NORMAL));
        c.title.setMaxLines(2);
        info.addView(c.title);
        c.subtitle = text(GAMES[index].subtitle, 14, GREY);
        c.subtitle.setVisibility(GAMES[index].subtitle.isEmpty() ? View.GONE : View.VISIBLE);
        info.addView(c.subtitle);

        c.chip = text("", 12, GREY);
        c.chip.setPadding(dp(10), dp(3), dp(10), dp(3));
        LinearLayout.LayoutParams chipParams = new LinearLayout.LayoutParams(-2, -2);
        chipParams.setMargins(0, dp(6), 0, dp(6));
        info.addView(c.chip, chipParams);

        c.detail = text("", 12, GREY);
        c.detail.setVisibility(View.GONE);
        info.addView(c.detail);

        c.error = text("", 12, 0xffe29a8c);
        c.error.setMaxLines(3);
        info.addView(c.error);

        c.bar = new ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal);
        c.bar.setMax(1000);
        c.bar.setProgressTintList(ColorStateList.valueOf(GOLD));
        c.bar.setVisibility(View.GONE);
        LinearLayout.LayoutParams barParams = new LinearLayout.LayoutParams(-1, dp(6));
        barParams.setMargins(0, 0, 0, dp(8));
        info.addView(c.bar, barParams);

        c.primary = new Button(this);
        c.primary.setAllCaps(false);
        c.primary.setTextSize(16);
        c.primary.setOnClickListener(v -> onPrimary(index));
        info.addView(c.primary, new LinearLayout.LayoutParams(-1, dp(46)));

        // the menu of the game: the files of it
        c.menu = text("⋮", 22, 0xffe0e7e9);
        c.menu.setGravity(Gravity.CENTER);
        c.menu.setBackground(round(0x66050b12, 0, 0, dp(18)));
        c.menu.setOnClickListener(v -> cardMenu(index, v));
        FrameLayout.LayoutParams mp = new FrameLayout.LayoutParams(dp(36), dp(36), Gravity.TOP | Gravity.END);
        mp.setMargins(0, dp(10), dp(10), 0);
        c.root.addView(c.menu, mp);
        return c;
    }

    private Bitmap loadCover(String asset) {
        try (InputStream in = getAssets().open(asset)) {
            return BitmapFactory.decodeStream(in);
        } catch (IOException e) {
            return null;
        }
    }

    private TextView text(String s, int size, int color) {
        TextView v = new TextView(this);
        v.setText(s);
        v.setTextSize(size);
        v.setTextColor(color);
        return v;
    }

    private TextView chip(String s) {
        TextView v = text(s, 14, 0xffe0e7e9);
        v.setGravity(Gravity.CENTER);
        v.setPadding(dp(16), dp(8), dp(16), dp(8));
        v.setBackground(round(0x33172530, 0xff33495a, dp(1), dp(20)));
        return v;
    }

    private GradientDrawable round(int fill, int stroke, int strokeWidth, int radius) {
        GradientDrawable d = new GradientDrawable();
        d.setColor(fill);
        d.setCornerRadius(radius);
        if (strokeWidth > 0)
            d.setStroke(strokeWidth, stroke);
        return d;
    }

    // ---- the state of the games

    private File rootOf(int game) {
        return new File(external, GAMES[game].folder);
    }

    private static boolean isInstallation(File dir) {
        return new File(dir, "Data").isDirectory()
                && new File(dir, "_work").isDirectory()
                && new File(dir, "System").isDirectory();
    }

    private boolean canDownload(int game) {
        return session != null && owned != null && owned.contains(GAMES[game].appId) && SteamBridge.available();
    }

    private void tick() {
        if (isFinishing())
            return;
        if (Downloads.state != null || importing >= 0)
            refresh();
        ui.postDelayed(this::tick, 600);
    }

    private void refresh() {
        if (external == null || cards[0] == null)
            return;
        Downloads.State dl = Downloads.state;
        Downloads.State busy = Downloads.running();
        boolean working = busy != null || importing >= 0;

        accountButton.setText(session != null ? (session.account.isEmpty() ? "Steam" : session.account)
                : Texts.t("Sign in", "Zaloguj"));
        accountButton.setVisibility(session != null || SteamBridge.available() ? View.VISIBLE : View.GONE);

        for (int i = 0; i < GAMES.length; ++i) {
            Card c = cards[i];
            GameDef g = GAMES[i];
            boolean installed = isInstallation(rootOf(i));
            boolean downloading = busy != null && busy.appId == g.appId;
            boolean importingThis = importing == i;

            c.error.setText("");
            c.detail.setVisibility(View.GONE);
            c.bar.setVisibility(View.GONE);
            c.menu.setVisibility((installed || canDownload(i)) && !working ? View.VISIBLE : View.GONE);

            // not installed: the picture is dimmed
            if (installed) {
                c.cover.clearColorFilter();
                c.cover.setImageAlpha(255);
            } else {
                ColorMatrix gray = new ColorMatrix();
                gray.setSaturation(0.35f);
                c.cover.setColorFilter(new ColorMatrixColorFilter(gray));
                c.cover.setImageAlpha(190);
            }

            // the border of the game that was played last
            boolean marked = i == lastPlayed && installed;
            GradientDrawable border = new GradientDrawable();
            border.setColor(0);
            border.setCornerRadius(dp(22));
            border.setStroke(dp(marked ? 2 : 1), marked ? GOLD : 0xff23394a);
            c.root.setForeground(border);

            if (downloading) {
                int pct = Math.round(busy.percent * 100);
                setChip(c, Texts.t("Downloading", "Pobieranie") + " " + pct + "%", 0x33c9a24a, 0xffe7c873);
                c.bar.setVisibility(View.VISIBLE);
                c.bar.setProgress(Math.round(busy.percent * 1000));
                c.detail.setText(Downloads.describe(busy));
                c.detail.setVisibility(View.VISIBLE);
                setPrimary(c, Texts.t("Stop", "Zatrzymaj"), false, true);
            } else if (importingThis) {
                setChip(c, importText, 0x33c9a24a, 0xffe7c873);
                setPrimary(c, "...", false, false);
            } else if (installed) {
                setChip(c, "✓  " + Texts.t("Installed", "Zainstalowane"), 0x331f7a4a, 0xff7fe0a0);
                setPrimary(c, "▶  " + Texts.t("Play", "Graj"), true, !working);
            } else {
                setChip(c, Texts.t("Not installed", "Niezainstalowane"), 0x33415563, 0xff9fb2c0);
                if (dl != null && dl.appId == g.appId && dl.error != null)
                    c.error.setText(dl.error);
                if (canDownload(i))
                    setPrimary(c, Texts.t("Download", "Pobierz"), false, !working);
                else
                    setPrimary(c, Texts.t("Import", "Importuj"), false, !working);
            }
        }
    }

    private void setChip(Card c, String text, int fill, int color) {
        c.chip.setText(text);
        c.chip.setTextColor(color);
        c.chip.setBackground(round(fill, 0, 0, dp(14)));
    }

    private void setPrimary(Card c, String text, boolean gold, boolean enabled) {
        c.primary.setText(text);
        c.primary.setEnabled(enabled);
        c.primary.setTextColor(new ColorStateList(
                new int[][] {new int[] {-android.R.attr.state_enabled}, new int[] {}},
                new int[] {0xff62727b, gold ? 0xff171c20 : 0xffe9eef0}));
        GradientDrawable shape = round(gold ? GOLD : 0x66172530, gold ? 0xffefe0b8 : 0xff425761, dp(1), dp(12));
        c.primary.setBackground(new RippleDrawable(ColorStateList.valueOf(0x557f9caa), shape, null));
    }

    // ---- what the player does

    private void onPrimary(int game) {
        Downloads.State busy = Downloads.running();
        if (busy != null && busy.appId == GAMES[game].appId) {
            startService(SteamBridge.stopIntent(this));
            return;
        }
        if (isInstallation(rootOf(game))) {
            play(game);
            return;
        }
        if (canDownload(game))
            chooseLanguage(game);
        else
            pickArchive(game);
    }

    private void play(int game) {
        lastPlayed = game;
        prefs.edit().putInt("game", game).putLong("started", System.currentTimeMillis()).apply();
        // the engine reads which of the installations it is and which mod (the ini of it): it has no command line
        try (OutputStream out = new FileOutputStream(new File(external, "game.txt"))) {
            out.write((GAMES[game].folder + "\n" + GAMES[game].ini + "\n").getBytes(StandardCharsets.UTF_8));
        } catch (IOException ignored) {
            // the engine takes Gothic II then
        }
        startActivity(new Intent(this, GameActivity.class));
    }

    private void chooseLanguage(final int game) {
        int selected = 1;
        String phone = Texts.steamLanguage();
        final String[] names = new String[LANGUAGES.length];
        for (int i = 0; i < LANGUAGES.length; ++i) {
            names[i] = LANGUAGES[i][1];
            if (LANGUAGES[i][0].equals(phone))
                selected = i;
        }
        final int[] chosen = {selected};
        new AlertDialog.Builder(this)
                .setTitle(Texts.t("Language of the game", "Język gry"))
                .setSingleChoiceItems(names, selected, (d, which) -> chosen[0] = which)
                .setPositiveButton(Texts.t("Download", "Pobierz"), (d, w) -> startDownload(game, LANGUAGES[chosen[0]][0]))
                .setNegativeButton(Texts.t("Cancel", "Anuluj"), null)
                .show();
    }

    private void startDownload(int game, String language) {
        if (external.getUsableSpace() < 1500L * 1024 * 1024) {
            new AlertDialog.Builder(this)
                    .setMessage(Texts.t("There is not enough free space on the phone.", "Brakuje wolnego miejsca w telefonie."))
                    .setPositiveButton("OK", null).show();
            return;
        }
        if (Build.VERSION.SDK_INT >= 33)
            requestPermissions(new String[] {"android.permission.POST_NOTIFICATIONS"}, 1);
        Downloads.state = null;
        startForegroundService(SteamBridge.downloadIntent(this, GAMES[game].appId, GAMES[game].folder, GAMES[game].title, language));
        refresh();
    }

    private void cardMenu(final int game, View anchor) {
        PopupMenu menu = new PopupMenu(this, anchor);
        if (isInstallation(rootOf(game)))
            menu.getMenu().add(0, 1, 0, Texts.t("Remove files", "Usuń pliki"));
        else
            menu.getMenu().add(0, 2, 0, Texts.t("Import from ZIP", "Importuj z ZIP"));
        if (Logs.hasLog(this))
            menu.getMenu().add(0, 3, 1, Texts.t("Share the log of the game", "Udostępnij log gry"));
        menu.setOnMenuItemClickListener(item -> {
            if (item.getItemId() == 1)
                askRemove(game);
            else if (item.getItemId() == 3)
                Logs.share(this);
            else
                pickArchive(game);
            return true;
        });
        menu.show();
    }

    private void accountMenu() {
        if (session == null) {
            if (SteamBridge.available()) {
                loginShown = true;
                startActivityForResult(SteamBridge.loginIntent(this), REQUEST_LOGIN);
            }
            return;
        }
        new AlertDialog.Builder(this)
                .setTitle(session.account)
                .setPositiveButton(Texts.t("Sign out", "Wyloguj"), (d, w) -> {
                    store.clear();
                    prefs.edit().putBoolean(SteamBridge.SKIPPED, false).apply();
                    session = null;
                    owned = null;
                    ownedAsked = false;
                    loginShown = false;
                    reloadSession();
                    refresh();
                })
                .setNegativeButton(Texts.t("Cancel", "Anuluj"), null)
                .show();
    }

    private void askRemove(final int game) {
        if (importing >= 0 || Downloads.running() != null)
            return;
        new AlertDialog.Builder(this)
                .setTitle(GAMES[game].title)
                .setMessage(Texts.t("The files of the game will be deleted from this phone. The saves are kept.",
                        "Pliki gry zostaną usunięte z telefonu. Zapisy zostaną."))
                .setPositiveButton(Texts.t("Remove", "Usuń"), (d, w) -> removeFiles(game, null))
                .setNegativeButton(Texts.t("Cancel", "Anuluj"), null)
                .show();
    }

    private void removeFiles(final int game, final Runnable then) {
        if (importing >= 0 || Downloads.running() != null)
            return;
        importing = game;
        importText = Texts.t("Removing...", "Usuwanie...");
        refresh();
        new Thread(() -> {
            remove(rootOf(game));
            runOnUiThread(() -> {
                importing = -1;
                refresh();
                if (then != null)
                    then.run();
            });
        }, "game-remove").start();
    }

    // ---- the import of a ZIP

    private void pickArchive(final int game) {
        new AlertDialog.Builder(this)
                .setTitle(GAMES[game].title)
                .setMessage(Texts.t("Choose a ZIP of your own installation of the game. It has to contain the Data, _work and System folders.",
                        "Wskaż ZIP z własną instalacją gry. Musi zawierać foldery Data, _work i System."))
                .setPositiveButton(Texts.t("Choose", "Wybierz"), (d, w) -> {
                    importTarget = game;
                    Intent pick = new Intent(Intent.ACTION_OPEN_DOCUMENT);
                    pick.setType("*/*");
                    pick.addCategory(Intent.CATEGORY_OPENABLE);
                    startActivityForResult(pick, REQUEST_IMPORT);
                })
                .setNegativeButton(Texts.t("Cancel", "Anuluj"), null)
                .show();
    }

    /** Unpacks next to the final folder, which is created only after the whole archive is valid. */
    private void importArchive(Uri uri, int game) {
        File root = rootOf(game);
        File staging = new File(root.getParentFile(), "import-staging");
        String failure = null;
        try {
            remove(staging);
            if (!staging.mkdirs())
                throw new IOException("Cannot create import directory");
            unzip(uri, staging);

            File source = findInstallation(staging);
            if (source == null)
                throw new IOException(Texts.t("Missing Data, _work or System. ZIP the complete game installation.",
                        "Brakuje Data, _work lub System. Spakuj całą instalację gry."));
            if (root.exists())
                throw new IOException("The folder of the game already exists; refusing to overwrite it");
            if (!source.renameTo(root))
                throw new IOException("Cannot finalize import");
            remove(staging);
        } catch (Exception error) {
            remove(staging);
            failure = error.getMessage();
        }
        final String shown = failure;
        runOnUiThread(() -> {
            importing = -1;
            refresh();
            if (shown != null)
                new AlertDialog.Builder(this).setMessage(shown).setPositiveButton("OK", null).show();
        });
    }

    private void unzip(Uri uri, File staging) throws IOException {
        String prefix = staging.getCanonicalPath() + File.separator;
        try (InputStream stream = getContentResolver().openInputStream(uri)) {
            if (stream == null)
                throw new IOException("Cannot open ZIP");
            try (ZipInputStream zip = new ZipInputStream(new BufferedInputStream(stream))) {
                byte[] buffer = new byte[131072];
                long total = 0;
                int count = 0;
                ZipEntry entry;
                while ((entry = zip.getNextEntry()) != null) {
                    if (++count > MAX_ENTRIES)
                        throw new IOException("Too many ZIP entries");
                    File out = new File(staging, entry.getName().replace('\\', '/'));
                    if (!out.getCanonicalPath().startsWith(prefix))
                        throw new IOException("Unsafe ZIP path");

                    if (entry.isDirectory()) {
                        makeDirs(out);
                        continue;
                    }
                    makeDirs(out.getParentFile());
                    try (OutputStream output = new BufferedOutputStream(new FileOutputStream(out))) {
                        int n;
                        while ((n = zip.read(buffer)) != -1) {
                            total += n;
                            if (total > MAX_SIZE)
                                throw new IOException("ZIP exceeds 20 GiB import limit");
                            output.write(buffer, 0, n);
                        }
                    }
                    if (count % 25 == 0)
                        importText = Texts.t("Importing", "Importowanie") + " " + total / (1024 * 1024) + " MB";
                }
            }
        }
    }

    /** The installation is either at the root of the archive or in a single enclosing folder. */
    private static File findInstallation(File staging) throws IOException {
        if (isInstallation(staging))
            return staging;
        File found = null;
        File[] children = staging.listFiles();
        if (children == null)
            return null;
        for (File child : children) {
            if (!isInstallation(child))
                continue;
            if (found != null)
                throw new IOException("ZIP contains multiple installations");
            found = child;
        }
        return found;
    }

    private static void makeDirs(File dir) throws IOException {
        if (!dir.isDirectory() && !dir.mkdirs())
            throw new IOException("Cannot create folder");
    }

    private static void remove(File f) {
        File[] children = f.listFiles();
        if (children != null) {
            for (File c : children)
                remove(c);
        }
        if (f.exists())
            f.delete();
    }
}
