package org.opengothic.app;

import android.app.Activity;
import android.app.ActivityManager;
import android.app.AlertDialog;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.res.ColorStateList;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.RippleDrawable;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.TextView;
import android.widget.Toast;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;

/**
 * The mods of Spine (the database of Gothic mods of Clockwork Origins): browse the list, download a mod into the installation
 * of Gothic or Gothic II and play it. The mods that need Ikarus or LeGo are left out, OpenGothic cannot run them.
 */
public final class SpineActivity extends Activity {
    private static final int GOLD = 0xffe3cf9f;
    private static final int CREAM = 0xfff1e7cd;
    private static final int GREY = 0xff9fb2c0;

    private static final String[] LANG_FILTER = {"", "en", "pl", "de", "ru"};
    private static final String[] GAME_FILTER_EN = {"All games", "Gothic", "Gothic II", "Downloaded"};
    private static final String[] GAME_FILTER_PL = {"Wszystkie gry", "Gothic", "Gothic II", "Pobrane"};
    private static final String[] SORT_EN = {"Name", "Newest", "Shortest"};
    private static final String[] SORT_PL = {"Nazwa", "Najnowsze", "Najkrótsze"};

    private final Handler ui = new Handler(Looper.getMainLooper());
    private File external;
    private SpineStore store;
    private SharedPreferences prefs;

    private List<SpineApi.Project> all = new ArrayList<>();
    private final List<SpineApi.Project> shown = new ArrayList<>();
    private Map<Integer, SpineStore.Mod> installed = new HashMap<>();
    private Map<Integer, String> blocked = new HashMap<>();
    private Map<Integer, String> checkedIds = new HashMap<>();

    private String query = "";
    private int gameFilter;
    private int langFilter;
    private int sort;
    private boolean showBlocked;

    private TextView status;
    private TextView gameChip;
    private TextView langChip;
    private TextView sortChip;
    private TextView blockedChip;
    private Adapter adapter;
    private boolean loading;

    @Override
    public void onCreate(Bundle state) {
        super.onCreate(state);
        external = getExternalFilesDir(null);
        if (external == null) {
            finish();
            return;
        }
        store = new SpineStore(external);
        store.pruneAbandoned(0);
        prefs = getSharedPreferences(SteamBridge.PREFS, MODE_PRIVATE);
        SystemUi.hideBars(this);
        setContentView(buildUi());
        loadCached();
        reload();
        ui.postDelayed(this::tick, 700);
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (external == null)
            return;
        resumed = true;
        SystemUi.hideBars(this);
        // the game is over: the files of the mod leave its folder
        if (!gameRunning(this))
            SpineActivation.restore(store.root);
        refreshState();
    }

    @Override
    protected void onPause() {
        resumed = false;
        super.onPause();
    }

    @Override
    protected void onDestroy() {
        ui.removeCallbacksAndMessages(null);
        super.onDestroy();
    }

    /** True while the process of the game is on the screen. */
    static boolean gameRunning(Context context) {
        ActivityManager am = (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
        if (am == null)
            return false;
        List<ActivityManager.RunningAppProcessInfo> list = am.getRunningAppProcesses();
        if (list == null)
            return false;
        for (ActivityManager.RunningAppProcessInfo p : list) {
            if (p.processName != null && p.processName.endsWith(":game")
                    && p.importance <= ActivityManager.RunningAppProcessInfo.IMPORTANCE_VISIBLE)
                return true;
        }
        return false;
    }

    // ---- the list

    private File cache() {
        return new File(store.root, "projects.json");
    }

    private void loadCached() {
        File f = cache();
        if (!f.isFile())
            return;
        try {
            byte[] data = new byte[(int) f.length()];
            try (java.io.FileInputStream in = new java.io.FileInputStream(f)) {
                int off = 0;
                while (off < data.length) {
                    int n = in.read(data, off, data.length - off);
                    if (n < 0)
                        break;
                    off += n;
                }
            }
            setProjects(SpineApi.parseProjects(new String(data, StandardCharsets.UTF_8)));
        } catch (IOException ignored) {
            // the list is loaded from the server then
        }
    }

    private void reload() {
        if (loading)
            return;
        loading = true;
        status.setText(Texts.t("Loading the list of mods...", "Wczytywanie listy modów..."));
        new Thread(() -> {
            String error = null;
            List<SpineApi.Project> list = null;
            try {
                String json = SpineApi.requestProjects();
                list = SpineApi.parseProjects(json);
                try (OutputStream out = new FileOutputStream(cache())) {
                    out.write(json.getBytes(StandardCharsets.UTF_8));
                } catch (IOException ignored) {
                    // the cache only makes the next start faster
                }
            } catch (IOException e) {
                error = e.getMessage() != null ? e.getMessage() : e.toString();
            }
            final List<SpineApi.Project> result = list;
            final String failure = error;
            ui.post(() -> {
                loading = false;
                if (isFinishing())
                    return;
                if (result != null)
                    setProjects(result);
                else
                    status.setText(Texts.t("No connection with Spine: ", "Brak połączenia ze Spine: ") + failure);
            });
        }, "spine-list").start();
    }

    private void setProjects(List<SpineApi.Project> list) {
        List<SpineApi.Project> ret = new ArrayList<>();
        for (SpineApi.Project p : list) {
            if (SpineApi.launchable(p))
                ret.add(p);
        }
        all = ret;
        refreshState();
    }

    /** What is downloaded and what is known to need Ikarus or LeGo changes after a download: the list is made again. */
    private void refreshState() {
        installed = new HashMap<>();
        for (SpineStore.Mod m : store.installed())
            installed.put(m.id, m);
        blocked = store.blocked();
        checkedIds = store.checked();
        applyFilter();
    }

    private void applyFilter() {
        shown.clear();
        List<SpineApi.Project> source = new ArrayList<>(all);
        // a mod that is downloaded stays on the list when the list of the server is not there (no connection)
        Map<Integer, Boolean> known = new HashMap<>();
        for (SpineApi.Project p : all)
            known.put(p.id, true);
        for (SpineStore.Mod m : installed.values()) {
            if (!known.containsKey(m.id)) {
                SpineApi.Project p = new SpineApi.Project();
                p.id = m.id;
                p.name = m.name;
                p.gameType = m.gameType;
                p.size = m.size;
                source.add(p);
            }
        }

        String q = query.trim().toLowerCase(Locale.ROOT);
        for (SpineApi.Project p : source) {
            boolean isBlocked = blocked.containsKey(p.id) && !SpineInstaller.ALLOW_LIBRARIES;
            if (isBlocked && !showBlocked)
                continue;
            if (gameFilter == 1 && p.gameType == SpineApi.GOTHIC2)
                continue;
            if (gameFilter == 2 && p.gameType == SpineApi.GOTHIC)
                continue;
            if (gameFilter == 3 && !installed.containsKey(p.id))
                continue;
            if (langFilter > 0 && p.languages != 0 && !SpineApi.hasLanguage(p, LANG_FILTER[langFilter]))
                continue;
            if (!q.isEmpty() && !(p.name.toLowerCase(Locale.ROOT).contains(q) || p.team.toLowerCase(Locale.ROOT).contains(q)
                    || p.keywords.toLowerCase(Locale.ROOT).contains(q)))
                continue;
            shown.add(p);
        }
        Collections.sort(shown, (a, b) -> {
            if (sort == 1)
                return Long.compare(b.updated, a.updated);
            if (sort == 2)
                return Long.compare(a.avgMinutes == 0 ? Long.MAX_VALUE : a.avgMinutes, b.avgMinutes == 0 ? Long.MAX_VALUE : b.avgMinutes);
            return a.name.compareToIgnoreCase(b.name);
        });

        gameChip.setText((Texts.polish() ? GAME_FILTER_PL : GAME_FILTER_EN)[gameFilter]);
        langChip.setText(langFilter == 0 ? Texts.t("Any language", "Dowolny język") : LANG_FILTER[langFilter].toUpperCase(Locale.ROOT));
        sortChip.setText((Texts.polish() ? SORT_PL : SORT_EN)[sort]);
        int blockedCount = 0;
        for (SpineApi.Project p : source)
            blockedCount += blocked.containsKey(p.id) ? 1 : 0;
        blockedChip.setVisibility(blockedCount > 0 && !SpineInstaller.ALLOW_LIBRARIES ? View.VISIBLE : View.GONE);
        blockedChip.setText(showBlocked ? Texts.t("Hide unsupported", "Ukryj nieobsługiwane")
                : Texts.t("Unsupported: ", "Nieobsługiwane: ") + blockedCount);
        if (!loading)
            status.setText(shown.size() + " / " + source.size() + Texts.t(" mods", " modów"));
        if (adapter != null)
            adapter.notifyDataSetChanged();
    }

    private boolean resumed;

    private void tick() {
        if (isFinishing())
            return;
        if (resumed && SpineActions.report(this))
            refreshState();   // a mod was downloaded, checked or found to need a library
        else if (SpineDownloadService.state != null && adapter != null)
            adapter.notifyDataSetChanged();
        ui.postDelayed(this::tick, 700);
    }

    // ---- the actions

    private void install(SpineApi.Project p) {
        if (SpineActions.install(this, p))
            ui.postDelayed(() -> adapter.notifyDataSetChanged(), 300);
    }

    /** The page of the mod: pictures, description, opinions, the check for Ikarus and LeGo. */
    private void details(SpineApi.Project p) {
        startActivity(SpineActions.put(new Intent(this, SpineDetailsActivity.class), p));
    }

    // ---- the screen

    private View buildUi() {
        LinearLayout page = new LinearLayout(this);
        page.setOrientation(LinearLayout.VERTICAL);
        page.setPadding(dp(24), dp(14), dp(24), dp(14));
        page.setBackground(new GradientDrawable(GradientDrawable.Orientation.TL_BR,
                new int[] {0xff050b12, 0xff0e2130, 0xff060c13}));

        LinearLayout top = new LinearLayout(this);
        top.setGravity(Gravity.CENTER_VERTICAL);
        TextView title = label("Spine", 26, CREAM);
        title.setTypeface(Typeface.create("serif", Typeface.NORMAL));
        top.addView(title);
        status = label("", 13, GREY);
        status.setPadding(dp(14), 0, 0, 0);
        top.addView(status, new LinearLayout.LayoutParams(0, -2, 1.f));
        Button back = button(Texts.t("Back", "Wróć"));
        back.setOnClickListener(v -> finish());
        top.addView(back, new LinearLayout.LayoutParams(dp(90), dp(40)));
        page.addView(top);

        LinearLayout filters = new LinearLayout(this);
        filters.setGravity(Gravity.CENTER_VERTICAL);
        filters.setPadding(0, dp(8), 0, dp(8));
        EditText search = new EditText(this);
        search.setSingleLine(true);
        search.setHint(Texts.t("Search mods", "Szukaj modów"));
        search.setHintTextColor(0xff667a86);
        search.setTextColor(CREAM);
        search.setTextSize(15);
        search.setBackground(round(0x33172530, 0xff425761, dp(1), dp(8)));
        search.setPadding(dp(12), 0, dp(12), 0);
        search.addTextChangedListener(new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence s, int a, int b, int c) {
            }

            @Override
            public void onTextChanged(CharSequence s, int a, int b, int c) {
            }

            @Override
            public void afterTextChanged(Editable s) {
                query = s.toString();
                applyFilter();
            }
        });
        filters.addView(search, new LinearLayout.LayoutParams(0, dp(40), 1.f));

        gameChip = chip();
        gameChip.setOnClickListener(v -> {
            gameFilter = (gameFilter + 1) % 4;
            applyFilter();
        });
        filters.addView(gameChip, chipParams());
        langChip = chip();
        langChip.setOnClickListener(v -> {
            langFilter = (langFilter + 1) % LANG_FILTER.length;
            applyFilter();
        });
        filters.addView(langChip, chipParams());
        sortChip = chip();
        sortChip.setOnClickListener(v -> {
            sort = (sort + 1) % 3;
            applyFilter();
        });
        filters.addView(sortChip, chipParams());
        blockedChip = chip();
        blockedChip.setOnClickListener(v -> {
            showBlocked = !showBlocked;
            applyFilter();
        });
        filters.addView(blockedChip, chipParams());
        page.addView(filters);

        ListView list = new ListView(this);
        list.setDivider(null);
        list.setSelector(android.R.color.transparent);
        adapter = new Adapter();
        list.setAdapter(adapter);
        list.setOnItemClickListener((parent, view, position, id) -> details(shown.get(position)));
        page.addView(list, new LinearLayout.LayoutParams(-1, 0, 1.f));
        return page;
    }

    private final class Adapter extends BaseAdapter {
        @Override
        public int getCount() {
            return shown.size();
        }

        @Override
        public Object getItem(int position) {
            return shown.get(position);
        }

        @Override
        public long getItemId(int position) {
            return shown.get(position).id;
        }

        @Override
        public View getView(int position, View convert, ViewGroup parent) {
            final SpineApi.Project p = shown.get(position);
            LinearLayout row = new LinearLayout(SpineActivity.this);
            row.setGravity(Gravity.CENTER_VERTICAL);
            row.setPadding(dp(14), dp(8), dp(14), dp(8));
            row.setBackground(round(0x33172530, 0xff425761, dp(1), dp(8)));

            LinearLayout text = new LinearLayout(SpineActivity.this);
            text.setOrientation(LinearLayout.VERTICAL);
            SpineStore.Mod mod = installed.get(p.id);
            SpineDownloadService.State s = SpineDownloadService.state;
            boolean active = s != null && s.id == p.id && s.running();
            String version = SpineApi.versionOf(p);
            text.addView(label(p.name + (version.equals("0.0") ? "" : "  v" + version), 17, CREAM));

            StringBuilder sub = new StringBuilder();
            sub.append(p.gameType == SpineApi.GOTHIC ? "Gothic" : p.gameType == SpineApi.GOTHIC2 ? "Gothic II" : "Gothic / II");
            if (!p.team.isEmpty())
                sub.append("  ·  ").append(p.team);
            if (p.size > 0)
                sub.append("  ·  ").append(Downloads.size(p.size));
            if (!SpineApi.languageTags(p).isEmpty())
                sub.append("  ·  ").append(SpineApi.languageTags(p));
            if (p.avgMinutes > 0)
                sub.append("  ·  ").append(p.avgMinutes / 60).append(" h");
            boolean checkedOk = SpineApi.versionOf(p).equals(checkedIds.get(p.id)) || mod != null;
            if (checkedOk)
                sub.append("  ·  ✓ ").append(Texts.t("no Ikarus/LeGo", "bez Ikarusa/LeGo"));
            if (active)
                sub = new StringBuilder(s.check ? Texts.t("Checking the scripts... ", "Sprawdzanie skryptów... ") + Downloads.size(s.bytes) : s.file);
            else if (blocked.containsKey(p.id))
                sub = new StringBuilder(Texts.t("Needs " + blocked.get(p.id) + ", OpenGothic cannot run it",
                        "Wymaga " + blocked.get(p.id) + ", OpenGothic go nie uruchomi"));
            TextView subtitle = label(sub.toString(), 12, blocked.containsKey(p.id) ? 0xffd08a6a : checkedOk && !active ? 0xff9cc79c : GREY);
            subtitle.setSingleLine(true);
            text.addView(subtitle);
            row.addView(text, new LinearLayout.LayoutParams(0, -2, 1.f));

            Button action = button("");
            action.setFocusable(false);
            boolean enabled = true;
            if (active) {
                action.setText((s.percent() >= 0 ? s.percent() + "%" : Downloads.size(s.bytes)) + "  ✕");
                action.setOnClickListener(v -> s.cancelled = true);
            } else if (mod != null) {
                action.setText(Texts.t("Play", "Graj"));
                action.setBackground(buttonShape(true));
                action.setTextColor(0xff171c20);
                action.setOnClickListener(v -> SpineActions.play(SpineActivity.this, store, mod));
            } else if (blocked.containsKey(p.id)) {
                action.setText(Texts.t("Unsupported", "Nieobsługiwany"));
                enabled = false;
            } else if (SpineActions.gameFor(external, p.gameType) < 0) {
                action.setText(Texts.t("Needs ", "Wymaga ") + SpineActions.needs(p.gameType));
                enabled = false;
            } else {
                action.setText(Texts.t("Download", "Pobierz"));
                action.setOnClickListener(v -> install(p));
            }
            action.setEnabled(enabled);
            action.setAlpha(enabled ? 1.f : 0.5f);
            LinearLayout.LayoutParams ap = new LinearLayout.LayoutParams(dp(130), dp(40));
            ap.setMargins(dp(12), 0, 0, 0);
            row.addView(action, ap);

            // the space between the rows: a ListView does not take margins
            FrameLayout wrap = new FrameLayout(SpineActivity.this);
            wrap.setPadding(0, 0, 0, dp(8));
            wrap.addView(row);
            return wrap;
        }
    }

    // ----------------------------------------------------------------------------------------------------

    private int dp(int value) {
        return Math.round(value * getResources().getDisplayMetrics().density);
    }

    private TextView label(String text, int size, int color) {
        TextView view = new TextView(this);
        view.setText(text);
        view.setTextSize(size);
        view.setTextColor(color);
        return view;
    }

    private GradientDrawable round(int fill, int stroke, int strokeWidth, int radius) {
        GradientDrawable shape = new GradientDrawable();
        shape.setColor(fill);
        shape.setCornerRadius(radius);
        shape.setStroke(strokeWidth, stroke);
        return shape;
    }

    private TextView chip() {
        TextView v = label("", 13, 0xffe0e7e9);
        v.setGravity(Gravity.CENTER);
        v.setPadding(dp(12), 0, dp(12), 0);
        v.setBackground(round(0x33172530, 0xff425761, dp(1), dp(20)));
        return v;
    }

    private LinearLayout.LayoutParams chipParams() {
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(-2, dp(36));
        lp.setMargins(dp(8), 0, 0, 0);
        return lp;
    }

    private GradientDrawable buttonShape(boolean primary) {
        return round(primary ? 0xffd8c499 : 0x33172530, primary ? 0xffe3d4b2 : 0xff425761, dp(1), dp(8));
    }

    private Button button(String text) {
        Button b = new Button(this);
        b.setText(text);
        b.setAllCaps(false);
        b.setTextSize(14);
        b.setTextColor(0xffe0e7e9);
        b.setPadding(dp(6), 0, dp(6), 0);
        b.setBackground(new RippleDrawable(ColorStateList.valueOf(0x557f9caa), buttonShape(false), null));
        return b;
    }
}
