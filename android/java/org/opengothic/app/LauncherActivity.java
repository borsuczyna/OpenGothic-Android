package org.opengothic.app;

import android.annotation.SuppressLint;
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
import android.os.Environment;
import android.os.Handler;
import android.os.Looper;
import android.provider.Settings;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewOutlineProvider;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.PopupMenu;
import android.widget.ProgressBar;
import android.widget.TextView;
import android.widget.Toast;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

public final class LauncherActivity extends Activity {
    private static final int REQUEST_SELECT_FOLDER = 1001;
    private static final int REQUEST_LOGIN = 3;
    private static final int REQUEST_PICK_ZIP = 1002;

    private static final int GOLD = 0xffe3cf9f;
    private static final int CREAM = 0xfff1e7cd;
    private static final int GREY = 0xff9fb2c0;

    private static final class GameDef {
        final String folder;
        final String title;
        final String subtitle;
        final int appId;
        final String cover;
        final String ini;

        GameDef(String folder, String title, String subtitle, int appId, String cover, String ini) {
            this.folder = folder;
            this.title = title;
            this.subtitle = subtitle;
            this.appId = appId;
            this.cover = cover;
            this.ini = ini;
        }
    }

    private static final GameDef[] GAMES = {
        new GameDef("Gothic1", "Gothic", "", 65540, "covers/gothic1.webp", "Gothic.ini"),
        new GameDef("Gothic2", "Gothic II", "Noc Kruka", 39510, "covers/gothic2.webp", "GothicII.ini"),
        new GameDef("Myrtana", "The Chronicles of Myrtana", "Archolos", 1467450, "covers/myrtana.webp", "TheChroniclesOfMyrtana.ini"),
    };

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
    private Set<Integer> owned;
    private TextView accountButton;
    private TextView updateButton;
    private boolean updateAvailable;
    private UpdateCheck.Release updateRelease;

    // Pola dla własnego panelu bocznego (styl Winlator)
    private FrameLayout rootContainer;
    private LinearLayout sideMenu;
    private View menuOverlay;
    private boolean isMenuOpen = false;

    private File external;
    private int lastPlayed = 1;
    private boolean askedAboutExit;
    private volatile int importing = -1;
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

    @Override
    public void onBackPressed() {
        if (isMenuOpen) {
            closeSideMenu();
            return;
        }
        super.onBackPressed();
    }

    private void toggleSideMenu() {
        if (isMenuOpen) {
            closeSideMenu();
        } else {
            openSideMenu();
        }
    }

    private void openSideMenu() {
        isMenuOpen = true;
        if (menuOverlay != null) menuOverlay.setVisibility(View.VISIBLE);
        if (sideMenu != null) sideMenu.setVisibility(View.VISIBLE);
    }

    private void closeSideMenu() {
        isMenuOpen = false;
        if (menuOverlay != null) menuOverlay.setVisibility(View.GONE);
        if (sideMenu != null) sideMenu.setVisibility(View.GONE);
    }

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
                    if (newer && !isFinishing() && !release.version.equals(prefs.getString("updateAsked", ""))) {
                        prefs.edit().putString("updateAsked", release.version).apply();
                        showUpdate();
                    }
                });
            } catch (IOException ignored) {
            }
        }, "update-check").start();
    }

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
        if (getExternalFilesDir(null).getUsableSpace() < Math.max(300L * 1024 * 1024, 2 * updateRelease.apkSize)) {
            new AlertDialog.Builder(this)
                    .setMessage(Texts.t("There is not enough free space on the phone.", "Brakuje wolnego miejsca w telefonie."))
                    .setPositiveButton("OK", null).show();
            return;
        }
        AppUpdate.start(this, updateRelease, () -> runOnUiThread(this::updateProgress));
        updateProgress();
    }

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

    private void checkGameExit() {
        long started = prefs.getLong("started", 0);
        if (started == 0 || askedAboutExit)
            return;
        prefs.edit().remove("started").apply();
        boolean quick = System.currentTimeMillis() - started < 25000;
        if (!quick && !Logs.crashedSince(this, started))
            return;
        askedAboutExit = true;
        String tail = Logs.tail(new File(getExternalFilesDir(null), "crash.log"), 1500);
        if (tail.isEmpty())
            tail = Logs.tail(new File(getExternalFilesDir(null), "log.txt"), 1500);
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

    private void reloadSession() {
        session = SteamBridge.available() ? store.load() : null;
        if (session == null) {
            owned = null;
            ownedAsked = false;
            boolean skipped = prefs.getBoolean(SteamBridge.SKIPPED, false);
            if (SteamBridge.available() && !skipped && !loginShown) {
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
        } else if (request == REQUEST_PICK_ZIP && result == RESULT_OK && data != null) {
            Uri uri = data.getData();
            if (uri != null && importTarget >= 0) {
                final int game = importTarget;
                File targetDir = new File(external, GAMES[game].folder);
                if (!targetDir.exists()) {
                    targetDir.mkdirs();
                }
                extractZipAsync(uri, targetDir, game);
            }
        }
    }

    private void savePathToGameTxt(String fullPath, String ini) {
        try (OutputStream out = new FileOutputStream(new File(external, "game.txt"))) {
            out.write((fullPath + "\n" + ini + "\n").getBytes(StandardCharsets.UTF_8));
        } catch (IOException ignored) {
        }
    }

    private int dp(int value) {
        return Math.round(value * getResources().getDisplayMetrics().density);
    }

    // ---- BUDOWANIE INTERFEJSU Z WŁASNĄ SZUFLADĄ (BEZ ANDROIDX) ----

    @SuppressLint("ResourceType")
    private View buildUi() {
        rootContainer = new FrameLayout(this);
        rootContainer.setBackgroundColor(0xff050b12);

        // 1. Główny widok aplikacji
        LinearLayout mainContent = new LinearLayout(this);
        mainContent.setOrientation(LinearLayout.VERTICAL);
        mainContent.setPadding(dp(28), dp(14), dp(28), dp(20));
        mainContent.setBackground(new GradientDrawable(GradientDrawable.Orientation.TL_BR,
                new int[] {0xff050b12, 0xff0e2130, 0xff060c13}));

        LinearLayout header = new LinearLayout(this);
        header.setGravity(Gravity.CENTER_VERTICAL);
        mainContent.addView(header, new LinearLayout.LayoutParams(-1, -2));

        // Przycisk hamburgera otwierający menu boczne
        TextView hamburger = chip("☰");
        hamburger.setOnClickListener(v -> toggleSideMenu());
        LinearLayout.LayoutParams hp = new LinearLayout.LayoutParams(-2, -2);
        hp.setMargins(0, 0, dp(14), 0);
        header.addView(hamburger, hp);

        LinearLayout names = new LinearLayout(this);
        names.setOrientation(LinearLayout.VERTICAL);
        header.addView(names, new LinearLayout.LayoutParams(-2, -2));
        TextView title = text("OpenGothic", 32, CREAM);
        title.setTypeface(Typeface.create("serif", Typeface.NORMAL));
        names.addView(title);
        names.addView(text(Texts.t("Your library", "Twoja biblioteka"), 14, GREY));

        LinearLayout.LayoutParams promoParams = new LinearLayout.LayoutParams(0, dp(60), 1.f);
        promoParams.setMargins(dp(16), 0, dp(6), 0);
        header.addView(buildPromo(), promoParams);

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

        LinearLayout row = new LinearLayout(this);
        row.setOrientation(LinearLayout.HORIZONTAL);
        LinearLayout.LayoutParams rp = new LinearLayout.LayoutParams(-1, 0, 1.f);
        rp.setMargins(0, dp(14), 0, 0);
        mainContent.addView(row, rp);

        for (int i = 0; i < GAMES.length; ++i) {
            Card c = buildCard(i);
            cards[i] = c;
            LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(0, -1, 1.f);
            lp.setMargins(i == 0 ? 0 : dp(8), 0, i == GAMES.length - 1 ? 0 : dp(8), 0);
            row.addView(c.root, lp);
        }

        rootContainer.addView(mainContent, new FrameLayout.LayoutParams(-1, -1));

        // 2. Nakładka przyciemniająca tło po wysunięciu menu
        menuOverlay = new View(this);
        menuOverlay.setBackgroundColor(0x88000000);
        menuOverlay.setVisibility(View.GONE);
        menuOverlay.setOnClickListener(v -> closeSideMenu());
        rootContainer.addView(menuOverlay, new FrameLayout.LayoutParams(-1, -1));

        // 3. Własne wysuwane menu boczne (styl Winlator)
        sideMenu = new LinearLayout(this);
        sideMenu.setOrientation(LinearLayout.VERTICAL);
        sideMenu.setBackgroundColor(0xff0b1823);
        sideMenu.setPadding(dp(20), dp(40), dp(20), dp(20));
        sideMenu.setVisibility(View.GONE);
        sideMenu.setClickable(true);

        TextView drawerTitle = text("Opcje źródeł", 20, CREAM);
        drawerTitle.setTypeface(Typeface.create("serif", Typeface.BOLD));
        sideMenu.addView(drawerTitle);

        View divider = new View(this);
        divider.setBackgroundColor(0xff2c4658);
        LinearLayout.LayoutParams divParams = new LinearLayout.LayoutParams(-1, dp(1));
        divParams.setMargins(0, dp(14), 0, dp(20));
        sideMenu.addView(divider, divParams);

        Button btnFolderOption = new Button(this);
        btnFolderOption.setText(Texts.t("Wybierz folder (Menedżer)", "Wybierz folder (Menedżer)"));
        btnFolderOption.setAllCaps(false);
        btnFolderOption.setTextColor(CREAM);
        btnFolderOption.setBackground(round(0x33172530, 0xff33495a, dp(1), dp(16)));
        btnFolderOption.setOnClickListener(v -> {
            closeSideMenu();
            if (importTarget >= 0) pickFolder(importTarget);
            else pickFolder(1);
        });
        LinearLayout.LayoutParams optP1 = new LinearLayout.LayoutParams(-1, dp(50));
        optP1.setMargins(0, 0, 0, dp(12));
        sideMenu.addView(btnFolderOption, optP1);

        Button btnZipOption = new Button(this);
        btnZipOption.setText(Texts.t("Wybierz plik ZIP (Wypakuj)", "Wybierz plik ZIP (Wypakuj)"));
        btnZipOption.setAllCaps(false);
        btnZipOption.setTextColor(CREAM);
        btnZipOption.setBackground(round(0x33172530, 0xff33495a, dp(1), dp(16)));
        btnZipOption.setOnClickListener(v -> {
            closeSideMenu();
            if (importTarget >= 0) pickZip(importTarget);
            else pickZip(1);
        });
        LinearLayout.LayoutParams optP2 = new LinearLayout.LayoutParams(-1, dp(50));
        sideMenu.addView(btnZipOption, optP2);

        FrameLayout.LayoutParams menuParams = new FrameLayout.LayoutParams(dp(280), -1);
        menuParams.gravity = Gravity.START;
        rootContainer.addView(sideMenu, menuParams);

        return rootContainer;
    }

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
        c.primary.setOnClickListener(v -> {
            importTarget = index;
            onPrimary(index);
        });
        info.addView(c.primary, new LinearLayout.LayoutParams(-1, dp(46)));

        c.menu = text("⋮", 22, 0xffe0e7e9);
        c.menu.setGravity(Gravity.CENTER);
        c.menu.setBackground(round(0x66050b12, 0, 0, dp(18)));
        c.menu.setOnClickListener(v -> {
            importTarget = index;
            cardMenu(index, v);
        });
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

    private File rootOf(int game) {
        String savedPath = prefs.getString("path_" + GAMES[game].folder, null);
        if (savedPath != null) {
            File custom = new File(savedPath);
            if (isInstallation(custom))
                return custom;
        }
        return new File(external, GAMES[game].folder);
    }

    private static boolean isInstallation(File dir) {
        return dir != null && dir.isDirectory()
                && new File(dir, "Data").isDirectory()
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

            if (installed) {
                c.cover.clearColorFilter();
                c.cover.setImageAlpha(255);
            } else {
                ColorMatrix gray = new ColorMatrix();
                gray.setSaturation(0.35f);
                c.cover.setColorFilter(new ColorMatrixColorFilter(gray));
                c.cover.setImageAlpha(190);
            }

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
                setChip(c, "✓  " + Texts.t("Installed (READY)", "Zainstalowane (GOTOWE)"), 0x331f7a4a, 0xff7fe0a0);
                setPrimary(c, "▶  " + Texts.t("Play", "Graj"), true, !working);
            } else {
                setChip(c, Texts.t("Not selected", "Niewybrany folder"), 0x33415563, 0xff9fb2c0);
                if (dl != null && dl.appId == g.appId && dl.error != null)
                    c.error.setText(dl.error);
                if (canDownload(i))
                    setPrimary(c, Texts.t("Download", "Pobierz"), false, !working);
                else
                    setPrimary(c, Texts.t("Wysuń menu źródeł", "Wysuń menu źródeł"), false, !working);
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
        else {
            openSideMenu();
        }
    }

    private void play(int game) {
        lastPlayed = game;
        prefs.edit().putInt("game", game).putLong("started", System.currentTimeMillis()).apply();
        
        File root = rootOf(game);
        savePathToGameTxt(root.getAbsolutePath(), GAMES[game].ini);
        
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

    @SuppressLint("NewApi")
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
            menu.getMenu().add(0, 1, 0, Texts.t("Zmień źródło gry", "Zmień źródło gry"));
        else
            menu.getMenu().add(0, 2, 0, Texts.t("Wybierz źródło gry", "Wybierz źródło gry"));
        if (Logs.hasLog(this))
            menu.getMenu().add(0, 3, 1, Texts.t("Share the log of the game", "Udostępnij log gry"));
        menu.setOnMenuItemClickListener(item -> {
            if (item.getItemId() == 1 || item.getItemId() == 2)
                openSideMenu();
            else if (item.getItemId() == 3)
                Logs.share(this);
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

    private void pickZip(int game) {
        importTarget = game;
        Intent intent = new Intent(Intent.ACTION_GET_CONTENT);
        intent.setType("application/zip");
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        startActivityForResult(Intent.createChooser(intent, Texts.t("Select ZIP archive", "Wybierz archiwum ZIP")), REQUEST_PICK_ZIP);
    }

    private void extractZipAsync(final Uri zipUri, final File targetDir, final int game) {
        importing = game;
        importText = Texts.t("Rozpakowywanie ZIP...", "Extracting ZIP...");
        refresh();
        new Thread(() -> {
            try {
                InputStream is = getContentResolver().openInputStream(zipUri);
                ZipInputStream zis = new ZipInputStream(is);
                ZipEntry ze;
                byte[] buffer = new byte[8192];
                int count;

                while ((ze = zis.getNextEntry()) != null) {
                    File file = new File(targetDir, ze.getName());
                    if (ze.isDirectory()) {
                        file.mkdirs();
                    } else {
                        File parent = file.getParentFile();
                        if (parent != null && !parent.exists()) {
                            parent.mkdirs();
                        }
                        FileOutputStream fos = new FileOutputStream(file);
                        while ((count = zis.read(buffer)) != -1) {
                            fos.write(buffer, 0, count);
                        }
                        fos.close();
                    }
                    zis.closeEntry();
                }
                zis.close();
                if (is != null) {
                    is.close();
                }

                runOnUiThread(() -> {
                    importing = -1;
                    if (isInstallation(targetDir)) {
                        String fullPath = targetDir.getAbsolutePath();
                        prefs.edit().putString("path_" + GAMES[game].folder, fullPath).apply();
                        savePathToGameTxt(fullPath, GAMES[game].ini);
                        Toast.makeText(LauncherActivity.this, Texts.t("ZIP extracted successfully!", "ZIP rozpakowany pomyślnie!"), Toast.LENGTH_LONG).show();
                    } else {
                        new AlertDialog.Builder(LauncherActivity.this)
                                .setTitle(Texts.t("Warning", "Ostrzeżenie"))
                                .setMessage(Texts.t("ZIP extracted, but no valid Gothic structure (Data / System) was found inside.", "Wypakowano ZIP, ale nie znaleziono poprawnej struktury gry (brak folderów Data / System)."))
                                .setPositiveButton("OK", null).show();
                    }
                    refresh();
                });
            } catch (Exception e) {
                runOnUiThread(() -> {
                    importing = -1;
                    refresh();
                    new AlertDialog.Builder(LauncherActivity.this)
                            .setTitle(Texts.t("Extraction error", "Błąd rozpakowywania"))
                            .setMessage(e.getMessage())
                            .setPositiveButton("OK", null).show();
                });
            }
        }, "zip-extractor").start();
    }

    private void pickFolder(final int game) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            if (!Environment.isExternalStorageManager()) {
                Intent intent = new Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION);
                startActivity(intent);
                return;
            }
        }
        importTarget = game;
        showCustomFolderPicker(game);
    }

    private void showCustomFolderPicker(final int game) {
        final File[] currentDir = { Environment.getExternalStorageDirectory() };

        final LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        layout.setPadding(30, 30, 30, 30);

        final TextView pathView = new TextView(this);
        pathView.setTextSize(13);
        pathView.setPadding(0, 0, 0, 15);
        layout.addView(pathView);

        final ListView listView = new ListView(this);
        layout.addView(listView, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, 700));

        final AlertDialog dialog = new AlertDialog.Builder(this)
                .setTitle(GAMES[game].title + " - Wybierz folder z grą")
                .setView(layout)
                .setNegativeButton(Texts.t("Cancel", "Anuluj"), null)
                .create();

        final Runnable[] loadDir = new Runnable[1];
        loadDir[0] = () -> {
            pathView.setText("📂 " + currentDir[0].getAbsolutePath());
            File[] files = currentDir[0].listFiles();
            List<File> subDirs = new ArrayList<>();

            if (currentDir[0].getParentFile() != null) {
                subDirs.add(new File(currentDir[0], ".."));
            }

            if (files != null) {
                Arrays.sort(files);
                for (File f : files) {
                    if (f.isDirectory() && !f.isHidden()) {
                        subDirs.add(f);
                    }
                }
            }

            ArrayAdapter<File> adapter = new ArrayAdapter<File>(
                    this, android.R.layout.simple_list_item_1, subDirs) {
                @Override
                public View getView(int position, View convertView, ViewGroup parent) {
                    TextView view = (TextView) super.getView(position, convertView, parent);
                    File f = getItem(position);
                    if (f != null && f.getName().equals("..")) {
                        view.setText("⬆️ [W górę]");
                    } else if (f != null) {
                        view.setText("📁 " + f.getName());
                    }
                    return view;
                }
            };
            listView.setAdapter(adapter);
        };

        listView.setOnItemClickListener((parent, view, position, id) -> {
            File selected = (File) listView.getItemAtPosition(position);
            if (selected != null) {
                if (selected.getName().equals("..")) {
                    File parentDir = currentDir[0].getParentFile();
                    if (parentDir != null) {
                        currentDir[0] = parentDir;
                        loadDir[0].run();
                    }
                } else if (selected.isDirectory()) {
                    currentDir[0] = selected;
                    if (isInstallation(currentDir[0])) {
                        String fullPath = currentDir[0].getAbsolutePath();
                        prefs.edit().putString("path_" + GAMES[game].folder, fullPath).apply();
                        savePathToGameTxt(fullPath, GAMES[game].ini);
                        dialog.dismiss();
                        refresh();
                    } else {
                        loadDir[0].run();
                    }
                }
            }
        });

        dialog.show();
        loadDir[0].run();
    }
}