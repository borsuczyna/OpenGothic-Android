package org.opengothic.app;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.content.res.ColorStateList;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.RippleDrawable;
import android.net.Uri;
import android.os.Bundle;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.TextView;
import android.widget.Toast;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Export and import of the saves of the mods of Spine: the list of the downloaded mods, and for each of them the saves in a ZIP.
 * The saves of a mod are the files that the engine names after the ini file of the mod ("archolos_save_slot_1.sav"), so a mod has
 * its own saves and the saves of the other games stay untouched. The archive is the one of {@link SaveExport} and {@link SaveImport}.
 */
public final class SpineSavesActivity extends Activity {
    private static final int CREAM = 0xfff1e7cd;
    private static final int GREY = 0xff9fb2c0;

    private static final int REQUEST_EXPORT = 1;
    private static final int REQUEST_IMPORT = 2;

    private SpineStore store;
    private final List<SpineStore.Mod> mods = new ArrayList<>();
    private final Map<Integer, Integer> counts = new HashMap<>();
    private final Map<Integer, String> gameIds = new HashMap<>();

    /** The mod that the file chooser is open for, and the way the saves are put in on import. */
    private SpineStore.Mod pending;
    private int pendingMode = SaveImport.OVERWRITE;

    private TextView status;
    private Adapter adapter;
    private boolean busy;

    @Override
    public void onCreate(Bundle state) {
        super.onCreate(state);
        File external = getExternalFilesDir(null);
        if (external == null) {
            finish();
            return;
        }
        store = new SpineStore(external);
        SystemUi.hideBars(this);
        setContentView(buildUi());
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (store == null)
            return;
        SystemUi.hideBars(this);
        reload();
    }

    /** The downloaded mods at once, the numbers of their saves after the folders are read on a thread. */
    private void reload() {
        mods.clear();
        gameIds.clear();
        for (SpineStore.Mod m : store.installed()) {
            mods.add(m);
            gameIds.put(m.id, SaveExport.modGameId(m));
        }
        status.setText(mods.isEmpty()
                ? Texts.t("No mods are downloaded yet.", "Nie pobrano jeszcze \u017cadnych mod\u00f3w.")
                : Texts.t("Downloaded mods: ", "Pobrane mody: ") + mods.size());
        adapter.notifyDataSetChanged();

        final List<SpineStore.Mod> snapshot = new ArrayList<>(mods);
        final Map<Integer, String> ids = new HashMap<>(gameIds);
        new Thread(() -> {
            List<String> names = SaveExport.listSaveNames(this);
            final Map<Integer, Integer> found = new HashMap<>();
            for (SpineStore.Mod m : snapshot) {
                String id = ids.get(m.id);
                int n = 0;
                if (id != null && !id.isEmpty()) {
                    for (String name : names) {
                        if (SaveExport.belongsToGame(name, id))
                            n++;
                    }
                }
                found.put(m.id, n);
            }
            runOnUiThread(() -> {
                if (isFinishing())
                    return;
                counts.clear();
                counts.putAll(found);
                adapter.notifyDataSetChanged();
            });
        }, "spine-saves-scan").start();
    }

    // ---- the actions

    private void export(SpineStore.Mod mod) {
        Integer n = counts.get(mod.id);
        if (n == null || n == 0) {
            Toast.makeText(this, Texts.t("This mod has no saves yet.", "Ten mod nie ma jeszcze zapis\u00f3w."),
                    Toast.LENGTH_LONG).show();
            return;
        }
        pending = mod;
        Intent create = new Intent(Intent.ACTION_CREATE_DOCUMENT);
        create.addCategory(Intent.CATEGORY_OPENABLE);
        create.setType("application/zip");
        create.putExtra(Intent.EXTRA_TITLE, "OpenGothic-" + fileSafe(mod.name) + "-Saves.zip");
        startActivityForResult(create, REQUEST_EXPORT);
    }

    private void importInto(final SpineStore.Mod mod) {
        // the engine writes the saves while it runs: a file that is replaced under it would be overwritten again
        if (SpineActivity.gameRunning(this)) {
            new AlertDialog.Builder(this)
                    .setMessage(Texts.t("Close the game first: the saves cannot be replaced while it is running.",
                            "Najpierw zamknij gr\u0119: zapis\u00f3w nie mo\u017cna podmieni\u0107, gdy gra dzia\u0142a."))
                    .setPositiveButton("OK", null)
                    .show();
            return;
        }
        final CharSequence[] modes = {
                Texts.t("Overwrite saves with matching names", "Nadpisz zapisy o tych samych nazwach"),
                Texts.t("Add all saves as separate slots", "Dodaj wszystkie zapisy jako osobne sloty")
        };
        new AlertDialog.Builder(this)
                .setTitle(Texts.t("Import saves: ", "Import zapis\u00f3w: ") + mod.name)
                .setItems(modes, (dialog, which) -> {
                    pending = mod;
                    pendingMode = which == 0 ? SaveImport.OVERWRITE : SaveImport.ADD_SEPARATELY;
                    Intent open = new Intent(Intent.ACTION_OPEN_DOCUMENT);
                    open.addCategory(Intent.CATEGORY_OPENABLE);
                    open.setType("application/zip");
                    startActivityForResult(open, REQUEST_IMPORT);
                })
                .setNegativeButton(Texts.t("Cancel", "Anuluj"), null)
                .show();
    }

    @Override
    protected void onActivityResult(int request, int result, Intent data) {
        super.onActivityResult(request, result, data);
        if ((request != REQUEST_EXPORT && request != REQUEST_IMPORT) || result != RESULT_OK || data == null
                || data.getData() == null || pending == null)
            return;
        transfer(request == REQUEST_EXPORT, data.getData(), pending, pendingMode);
    }

    /** Writes the saves of the mod to a ZIP or reads them from one on a thread, the result is shown in a dialog. */
    private void transfer(final boolean exporting, final Uri uri, final SpineStore.Mod mod, final int mode) {
        if (busy)
            return;
        final String gameId = gameIds.get(mod.id);
        if (gameId == null || gameId.isEmpty())
            return;
        busy = true;
        new Thread(() -> {
            String failure = null;
            int count = 0;
            try {
                count = exporting ? SaveExport.write(this, uri, gameId) : SaveImport.read(this, uri, mode, gameId);
                if (count == 0)
                    failure = exporting
                            ? Texts.t("No save files were found.", "Nie znaleziono plik\u00f3w zapis\u00f3w.")
                            : Texts.t("No saves of this mod were found in the archive.",
                                    "W archiwum nie znaleziono zapis\u00f3w tego moda.");
            } catch (IOException | RuntimeException e) {
                failure = e.getMessage() != null ? e.getMessage() : e.getClass().getSimpleName();
            }
            final int done = count;
            final String error = failure;
            runOnUiThread(() -> {
                busy = false;
                if (isFinishing())
                    return;
                new AlertDialog.Builder(this)
                        .setTitle((exporting ? Texts.t("Save export: ", "Eksport zapis\u00f3w: ")
                                : Texts.t("Save import: ", "Import zapis\u00f3w: ")) + mod.name)
                        .setMessage(error != null ? error : exporting
                                ? Texts.t("Exported " + done + " save files.",
                                        "Wyeksportowano " + done + " plik\u00f3w zapis\u00f3w.")
                                : Texts.t("Imported " + done + " save files.",
                                        "Zaimportowano " + done + " plik\u00f3w zapis\u00f3w."))
                        .setPositiveButton("OK", null)
                        .show();
                if (!exporting)
                    reload();
            });
        }, exporting ? "spine-save-export" : "spine-save-import").start();
    }

    /** A name that is fit for a file on every storage. */
    private static String fileSafe(String name) {
        StringBuilder out = new StringBuilder();
        for (int i = 0; i < name.length(); i++) {
            char c = name.charAt(i);
            out.append((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '.'
                    ? c : '_');
        }
        return out.length() == 0 ? "mod" : out.toString();
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
        TextView title = label(Texts.t("Spine saves", "Zapisy Spine"), 26, CREAM);
        title.setTypeface(Typeface.create("serif", Typeface.NORMAL));
        top.addView(title);
        status = label("", 13, GREY);
        status.setPadding(dp(14), 0, 0, 0);
        top.addView(status, new LinearLayout.LayoutParams(0, -2, 1.f));
        Button back = button(Texts.t("Back", "Wr\u00f3\u0107"));
        back.setOnClickListener(v -> finish());
        top.addView(back, new LinearLayout.LayoutParams(dp(90), dp(40)));
        page.addView(top);

        TextView hint = label(Texts.t(
                "Export the saves of a downloaded mod to a ZIP, or import them back, for example on another phone. "
                        + "The saves of the other games are not touched.",
                "Wyeksportuj zapisy pobranego moda do pliku ZIP albo zaimportuj je z powrotem, np. na innym telefonie. "
                        + "Zapisy innych gier nie s\u0105 zmieniane."), 12, GREY);
        hint.setPadding(0, dp(8), 0, dp(8));
        page.addView(hint);

        ListView list = new ListView(this);
        list.setDivider(null);
        list.setSelector(android.R.color.transparent);
        adapter = new Adapter();
        list.setAdapter(adapter);
        page.addView(list, new LinearLayout.LayoutParams(-1, 0, 1.f));
        return page;
    }

    private final class Adapter extends BaseAdapter {
        @Override
        public int getCount() {
            return mods.size();
        }

        @Override
        public Object getItem(int position) {
            return mods.get(position);
        }

        @Override
        public long getItemId(int position) {
            return mods.get(position).id;
        }

        @Override
        public View getView(int position, View convert, ViewGroup parent) {
            final SpineStore.Mod mod = mods.get(position);
            final String gameId = gameIds.get(mod.id);
            final boolean supported = gameId != null && !gameId.isEmpty();
            final Integer saves = counts.get(mod.id);

            LinearLayout row = new LinearLayout(SpineSavesActivity.this);
            row.setGravity(Gravity.CENTER_VERTICAL);
            row.setPadding(dp(14), dp(8), dp(14), dp(8));
            row.setBackground(round(0x33172530, 0xff425761, dp(1), dp(8)));

            LinearLayout text = new LinearLayout(SpineSavesActivity.this);
            text.setOrientation(LinearLayout.VERTICAL);
            text.addView(label(mod.name + (mod.version.isEmpty() ? "" : "  v" + mod.version), 17, CREAM));
            String sub;
            if (!supported)
                sub = Texts.t("The saves of this mod cannot be told from the saves of Gothic II",
                        "Zapis\u00f3w tego moda nie da si\u0119 odr\u00f3\u017cni\u0107 od zapis\u00f3w Gothica II");
            else if (saves == null)
                sub = Texts.t("Counting the saves...", "Liczenie zapis\u00f3w...");
            else
                sub = Texts.t("Saves: ", "Zapisy: ") + saves;
            TextView subtitle = label(sub, 12, !supported ? 0xffd08a6a : saves != null && saves > 0 ? 0xff9cc79c : GREY);
            subtitle.setSingleLine(true);
            text.addView(subtitle);
            row.addView(text, new LinearLayout.LayoutParams(0, -2, 1.f));

            Button exportButton = button(Texts.t("Export", "Eksport"));
            exportButton.setFocusable(false);
            exportButton.setEnabled(supported && saves != null && saves > 0);
            exportButton.setAlpha(exportButton.isEnabled() ? 1.f : 0.5f);
            exportButton.setOnClickListener(v -> export(mod));
            LinearLayout.LayoutParams ep = new LinearLayout.LayoutParams(dp(110), dp(40));
            ep.setMargins(dp(12), 0, 0, 0);
            row.addView(exportButton, ep);

            Button importButton = button(Texts.t("Import", "Import"));
            importButton.setFocusable(false);
            importButton.setEnabled(supported);
            importButton.setAlpha(supported ? 1.f : 0.5f);
            importButton.setOnClickListener(v -> importInto(mod));
            LinearLayout.LayoutParams ip = new LinearLayout.LayoutParams(dp(110), dp(40));
            ip.setMargins(dp(8), 0, 0, 0);
            row.addView(importButton, ip);

            // the space between the rows: a ListView does not take margins
            FrameLayout wrap = new FrameLayout(SpineSavesActivity.this);
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

    private Button button(String text) {
        Button b = new Button(this);
        b.setText(text);
        b.setAllCaps(false);
        b.setTextSize(14);
        b.setTextColor(0xffe0e7e9);
        b.setPadding(dp(6), 0, dp(6), 0);
        b.setBackground(new RippleDrawable(ColorStateList.valueOf(0x557f9caa), round(0x33172530, 0xff425761, dp(1), dp(8)), null));
        return b;
    }
}
