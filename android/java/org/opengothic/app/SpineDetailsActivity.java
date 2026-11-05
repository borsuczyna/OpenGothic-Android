package org.opengothic.app;

import android.app.Activity;
import android.app.Dialog;
import android.content.res.ColorStateList;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.RippleDrawable;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.text.Html;
import android.text.SpannableStringBuilder;
import android.text.method.LinkMovementMethod;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.HorizontalScrollView;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.zip.InflaterInputStream;

/**
 * The page of a mod of Spine: screenshots, the description (it is HTML), features, changes, the opinions of the players, and
 * whether the scripts need Ikarus or LeGo, which can be checked here before anything is downloaded.
 */
public final class SpineDetailsActivity extends Activity {
    private static final int GOLD = 0xffe3cf9f;
    private static final int CREAM = 0xfff1e7cd;
    private static final int GREY = 0xff9fb2c0;
    private static final int TEXT = 0xffc8d4da;
    private static final int GREEN = 0xff8fd18f;
    private static final int RED = 0xffe0906a;
    private static final String SCREENS = "https://clockwork-origins.de/Gothic/downloads/mods/";

    private final Handler ui = new Handler(Looper.getMainLooper());
    private ExecutorService pool;
    private boolean destroyed;
    private boolean resumed;

    private File external;
    private SpineStore store;
    private SpineApi.Project project;
    private SpineApi.Info info;

    private ImageView preview;
    private TextView noShots;
    private LinearLayout thumbRow;
    private final List<File> shotFiles = new ArrayList<>();
    private final List<ImageView> thumbViews = new ArrayList<>();
    private int shot = -1;

    private TextView ratingView;
    private TextView compat;
    private Button primary;
    private Button check;
    private Button remove;
    private LinearLayout content;
    private TextView description;

    @Override
    public void onCreate(Bundle state) {
        super.onCreate(state);
        external = getExternalFilesDir(null);
        project = SpineActions.project(getIntent());
        if (external == null || project.id == 0) {
            finish();
            return;
        }
        store = new SpineStore(external);
        pool = Executors.newFixedThreadPool(3);
        SystemUi.hideBars(this);
        setContentView(buildUi());
        refreshStatus();
        loadRemote();
        ui.postDelayed(this::tick, 600);
    }

    @Override
    protected void onResume() {
        super.onResume();
        resumed = true;
        SystemUi.hideBars(this);
        if (store != null)
            refreshStatus();
    }

    @Override
    protected void onPause() {
        resumed = false;
        super.onPause();
    }

    @Override
    protected void onDestroy() {
        destroyed = true;
        ui.removeCallbacksAndMessages(null);
        if (pool != null)
            pool.shutdownNow();
        super.onDestroy();
    }

    private void tick() {
        if (destroyed)
            return;
        if (resumed && SpineActions.report(this))
            refreshStatus();
        else if (SpineDownloadService.running() != null)
            refreshStatus();
        ui.postDelayed(this::tick, 600);
    }

    // ---- the data

    private void loadRemote() {
        final String language = SpineApi.languageName(Locale.getDefault().getLanguage());
        pool.execute(() -> {
            try {
                final SpineApi.Info i = SpineApi.info(project.id, language);
                ui.post(() -> {
                    if (!destroyed)
                        showInfo(i);
                });
            } catch (IOException e) {
                ui.post(() -> {
                    if (!destroyed)
                        description.setText(Texts.t("The page of the mod could not be loaded: ", "Nie udało się wczytać strony moda: ")
                                + e.getMessage());
                });
            }
        });
        pool.execute(() -> {
            try {
                final SpineApi.Rating r = SpineApi.rating(project.id);
                ui.post(() -> {
                    if (!destroyed)
                        showRating(r);
                });
            } catch (IOException ignored) {
                // no rating then
            }
        });
        pool.execute(() -> {
            try {
                final List<SpineApi.Review> r = SpineApi.reviews(project.id);
                ui.post(() -> {
                    if (!destroyed)
                        showReviews(r);
                });
            } catch (IOException ignored) {
                // no opinions then
            }
        });
    }

    private void showInfo(SpineApi.Info i) {
        info = i;
        String html = i.description.replaceAll("(?i)<img[^>]*>", "").trim();
        if (html.isEmpty()) {
            description.setText(Texts.t("The author has not written a description.", "Autor nie napisał opisu."));
        } else {
            description.setText(trim(Html.fromHtml(html, Html.FROM_HTML_MODE_COMPACT)));
        }

        if (!i.features.isEmpty()) {
            StringBuilder b = new StringBuilder();
            for (String f : i.features)
                b.append("•  ").append(f).append('\n');
            addSection(Texts.t("Features", "Cechy"), body(b.toString().trim()));
        }
        int shown = 0;
        LinearLayout changes = new LinearLayout(this);
        changes.setOrientation(LinearLayout.VERTICAL);
        for (SpineApi.Change c : i.history) {
            if (c.text.trim().isEmpty() || shown >= 4)
                continue;
            changes.addView(label("v" + c.version, 13, GOLD));
            TextView t = body("");
            t.setText(trim(Html.fromHtml(c.text.replaceAll("(?i)<img[^>]*>", ""), Html.FROM_HTML_MODE_COMPACT)));
            changes.addView(t);
            ++shown;
        }
        if (shown > 0)
            addSection(Texts.t("Changes", "Zmiany"), changes);

        if (!i.installAllowed)
            refreshStatus();
        loadShots(i.screenshots);
    }

    private void showRating(SpineApi.Rating r) {
        if (r.count <= 0 && r.percent <= 0) {
            ratingView.setText(Texts.t("No ratings yet", "Brak ocen"));
            return;
        }
        double stars = r.percent / 20.0;
        StringBuilder b = new StringBuilder();
        for (int s = 1; s <= 5; ++s)
            b.append(stars >= s - 0.25 ? '★' : '☆');
        ratingView.setText(b + String.format(Locale.US, "  %.1f", stars) + "  ·  " + r.count
                + Texts.t(" ratings", " ocen"));
    }

    private void showReviews(List<SpineApi.Review> reviews) {
        if (reviews.isEmpty())
            return;
        LinearLayout list = new LinearLayout(this);
        list.setOrientation(LinearLayout.VERTICAL);
        int n = 0;
        for (SpineApi.Review r : reviews) {
            if (++n > 6)
                break;
            StringBuilder stars = new StringBuilder();
            for (int s = 1; s <= 5; ++s)
                stars.append(s <= r.rating ? '★' : '☆');
            TextView head = label(stars + "  " + r.user, 13, GOLD);
            head.setPadding(0, dp(6), 0, 0);
            list.addView(head);
            String text = r.text.length() > 500 ? r.text.substring(0, 500) + "..." : r.text;
            list.addView(body(text));
        }
        addSection(Texts.t("Opinions", "Opinie"), list);
    }

    // ---- the pictures

    private void loadShots(final List<SpineApi.Entry> shots) {
        if (shots.isEmpty()) {
            noShots.setVisibility(View.VISIBLE);
            return;
        }
        for (int i = 0; i < shots.size(); ++i) {
            final int index = i;
            final SpineApi.Entry entry = shots.get(i);
            shotFiles.add(null);
            ImageView thumb = new ImageView(this);
            thumb.setScaleType(ImageView.ScaleType.CENTER_CROP);
            thumb.setBackground(round(0x33172530, 0xff425761, dp(1), dp(6)));
            thumb.setOnClickListener(v -> select(index));
            LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(dp(110), dp(66));
            lp.setMargins(0, 0, dp(8), 0);
            thumbRow.addView(thumb, lp);
            thumbViews.add(thumb);
            pool.execute(() -> {
                try {
                    final File file = imageFile(entry);
                    final Bitmap small = decode(file, 220);
                    ui.post(() -> {
                        if (destroyed || small == null)
                            return;
                        shotFiles.set(index, file);
                        thumbViews.get(index).setImageBitmap(small);
                        if (shot < 0)
                            select(index);
                    });
                } catch (IOException ignored) {
                    // the picture stays empty
                }
            });
        }
    }

    private void select(final int index) {
        final File file = index < shotFiles.size() ? shotFiles.get(index) : null;
        if (file == null)
            return;
        shot = index;
        for (int i = 0; i < thumbViews.size(); ++i)
            thumbViews.get(i).setAlpha(i == index ? 1.f : 0.55f);
        pool.execute(() -> {
            final Bitmap big = decode(file, 1600);
            ui.post(() -> {
                if (!destroyed && shot == index && big != null)
                    preview.setImageBitmap(big);
            });
        });
    }

    /** The picture is downloaded once (zlib, like the files of the mods) and kept in the cache of the application. */
    private File imageFile(SpineApi.Entry e) throws IOException {
        String name = e.file.endsWith(".z") ? e.file.substring(0, e.file.length() - 2) : e.file;
        name = name.replaceAll("[^A-Za-z0-9._-]", "_");
        File dir = new File(getCacheDir(), "spine/" + project.id);
        if (!dir.isDirectory() && !dir.mkdirs())
            throw new IOException("Cannot create the cache");
        File file = new File(dir, name);
        if (file.isFile() && file.length() > 0)
            return file;
        File part = new File(dir, name + ".part");
        HttpURLConnection c = (HttpURLConnection) new URL(SCREENS + project.id + "/screens/" + SpineInstaller.encode(e.file)).openConnection();
        try {
            c.setConnectTimeout(15000);
            c.setReadTimeout(30000);
            c.setRequestProperty("User-Agent", "OpenGothic-Android");
            if (c.getResponseCode() != 200)
                throw new IOException("The server answered " + c.getResponseCode());
            InputStream in = e.file.endsWith(".z") ? new InflaterInputStream(c.getInputStream()) : c.getInputStream();
            long total = 0;
            try (InputStream source = in; OutputStream out = new FileOutputStream(part)) {
                byte[] buf = new byte[32768];
                int n;
                while ((n = source.read(buf)) > 0) {
                    total += n;
                    if (total > 30L * 1024 * 1024)
                        throw new IOException("The picture is too big");
                    out.write(buf, 0, n);
                }
            }
            if (!part.renameTo(file))
                throw new IOException("Cannot save the picture");
        } finally {
            c.disconnect();
            if (part.exists())
                //noinspection ResultOfMethodCallIgnored
                part.delete();
        }
        return file;
    }

    private static Bitmap decode(File file, int maxSide) {
        BitmapFactory.Options o = new BitmapFactory.Options();
        o.inJustDecodeBounds = true;
        BitmapFactory.decodeFile(file.getPath(), o);
        if (o.outWidth <= 0 || o.outHeight <= 0)
            return null;
        int sample = 1;
        while (Math.max(o.outWidth, o.outHeight) / (sample * 2) >= maxSide)
            sample *= 2;
        o.inJustDecodeBounds = false;
        o.inSampleSize = sample;
        return BitmapFactory.decodeFile(file.getPath(), o);
    }

    /** A picture on the whole screen: the left third goes back, the right third forward, the middle closes. */
    private void fullscreen() {
        if (shot < 0)
            return;
        final Dialog d = new Dialog(this, android.R.style.Theme_Black_NoTitleBar_Fullscreen);
        final ImageView view = new ImageView(this);
        view.setScaleType(ImageView.ScaleType.FIT_CENTER);
        view.setBackgroundColor(0xff000000);
        final int[] current = {shot};
        final Runnable load = () -> {
            final int index = current[0];
            final File file = shotFiles.get(index);
            if (file == null)
                return;
            pool.execute(() -> {
                final Bitmap big = decode(file, 2048);
                ui.post(() -> {
                    if (current[0] == index && big != null && d.isShowing())
                        view.setImageBitmap(big);
                });
            });
        };
        view.setOnTouchListener((v, event) -> {
            if (event.getAction() == MotionEvent.ACTION_UP) {
                float x = event.getX() / Math.max(1, v.getWidth());
                int count = shotFiles.size();
                if (x < 0.33f) {
                    current[0] = (current[0] + count - 1) % count;
                    load.run();
                } else if (x > 0.66f) {
                    current[0] = (current[0] + 1) % count;
                    load.run();
                } else {
                    d.dismiss();
                }
            }
            return true;
        });
        d.setContentView(view);
        d.setOnDismissListener(x -> select(current[0]));
        d.show();
        load.run();
    }

    // ---- the state

    private void refreshStatus() {
        SpineStore.Mod mod = store.get(project.id);
        String blockedBy = store.blocked().get(project.id);
        boolean checked = SpineApi.versionOf(project).equals(store.checked().get(project.id));
        SpineDownloadService.State s = SpineDownloadService.state;
        final boolean busy = s != null && s.id == project.id && s.running();
        boolean needsInfo = info != null && !info.installAllowed;

        String text;
        int color = TEXT;
        if (mod != null) {
            text = Texts.t("Downloaded. Scripts without Ikarus and LeGo.", "Pobrany. Skrypty bez Ikarusa i LeGo.");
            color = GREEN;
        } else if (blockedBy != null && !SpineInstaller.ALLOW_LIBRARIES) {
            text = "✗  " + Texts.t("Needs " + blockedBy + ", which OpenGothic cannot run. This mod will not work.",
                    "Wymaga " + blockedBy + ", którego OpenGothic nie uruchomi. Ten mod nie zadziała.");
            color = RED;
        } else if (busy && s.check) {
            text = Texts.t("Checking the scripts... ", "Sprawdzanie skryptów... ") + Downloads.size(s.bytes);
        } else if (busy) {
            text = Texts.t("Downloading... ", "Pobieranie... ") + (s.percent() >= 0 ? s.percent() + "%" : Downloads.size(s.bytes));
        } else if (checked) {
            text = "✓  " + Texts.t("The scripts are checked: no Ikarus and no LeGo.", "Skrypty sprawdzone: brak Ikarusa i LeGo.");
            color = GREEN;
        } else {
            text = Texts.t("Not checked yet. Mods with Ikarus or LeGo do not run in OpenGothic. The check reads only the scripts "
                    + "of the mod, not all of it, and the download goes on from there.",
                    "Jeszcze nie sprawdzono. Mody z Ikarusem lub LeGo nie działają w OpenGothic. Sprawdzenie czyta tylko skrypty moda, "
                    + "nie całość, a pobieranie potem idzie dalej od tego miejsca.");
        }
        compat.setText(text);
        compat.setTextColor(color);

        // the main button
        primary.setEnabled(true);
        primary.setAlpha(1.f);
        primary.setBackground(buttonShape(false));
        primary.setTextColor(0xffe0e7e9);
        if (busy) {
            primary.setText(Texts.t("Stop", "Zatrzymaj"));
            primary.setOnClickListener(v -> {
                SpineDownloadService.State cur = SpineDownloadService.state;
                if (cur != null)
                    cur.cancelled = true;
            });
        } else if (mod != null) {
            primary.setText(Texts.t("Play", "Graj"));
            primary.setBackground(buttonShape(true));
            primary.setTextColor(0xff171c20);
            primary.setOnClickListener(v -> SpineActions.play(this, store, mod));
        } else if (blockedBy != null && !SpineInstaller.ALLOW_LIBRARIES) {
            disable(primary, Texts.t("Unsupported", "Nieobsługiwany"));
        } else if (needsInfo) {
            disable(primary, Texts.t("Not available", "Niedostępny"));
        } else if (SpineActions.gameFor(this, external, project.gameType) < 0) {
            disable(primary, Texts.t("Needs ", "Wymaga ") + SpineActions.needs(project.gameType));
        } else {
            primary.setText(Texts.t("Download", "Pobierz") + (project.size > 0 ? "  (" + Downloads.size(project.size) + ")" : ""));
            primary.setBackground(buttonShape(true));
            primary.setTextColor(0xff171c20);
            primary.setOnClickListener(v -> {
                if (SpineActions.install(this, project))
                    refreshStatus();
            });
        }

        boolean canCheck = !busy && mod == null && blockedBy == null && !checked && !needsInfo
                && SpineActions.gameFor(this, external, project.gameType) >= 0;
        check.setVisibility(canCheck ? View.VISIBLE : View.GONE);
        check.setOnClickListener(v -> {
            if (SpineActions.check(this, project))
                refreshStatus();
        });
        remove.setVisibility(mod != null && !busy ? View.VISIBLE : View.GONE);
        if (mod != null)
            remove.setOnClickListener(v -> SpineActions.remove(this, store, mod, this::refreshStatus));
    }

    private void disable(Button b, String text) {
        b.setText(text);
        b.setEnabled(false);
        b.setAlpha(0.5f);
        b.setOnClickListener(null);
    }

    // ---- the screen

    private View buildUi() {
        LinearLayout page = new LinearLayout(this);
        page.setOrientation(LinearLayout.HORIZONTAL);
        page.setPadding(dp(24), dp(14), dp(24), dp(14));
        page.setBackground(new GradientDrawable(GradientDrawable.Orientation.TL_BR,
                new int[] {0xff050b12, 0xff0e2130, 0xff060c13}));

        // the pictures
        LinearLayout left = new LinearLayout(this);
        left.setOrientation(LinearLayout.VERTICAL);
        FrameLayout frame = new FrameLayout(this);
        frame.setBackground(round(0xff000000, 0xff425761, dp(1), dp(10)));
        preview = new ImageView(this);
        preview.setScaleType(ImageView.ScaleType.FIT_CENTER);
        preview.setOnClickListener(v -> fullscreen());
        frame.addView(preview, new FrameLayout.LayoutParams(-1, -1));
        noShots = label(Texts.t("No pictures", "Brak zdjęć"), 14, GREY);
        noShots.setGravity(Gravity.CENTER);
        noShots.setVisibility(View.GONE);
        frame.addView(noShots, new FrameLayout.LayoutParams(-1, -1));
        left.addView(frame, new LinearLayout.LayoutParams(-1, 0, 1.f));
        HorizontalScrollView strip = new HorizontalScrollView(this);
        strip.setHorizontalScrollBarEnabled(false);
        thumbRow = new LinearLayout(this);
        thumbRow.setOrientation(LinearLayout.HORIZONTAL);
        strip.addView(thumbRow);
        LinearLayout.LayoutParams sp = new LinearLayout.LayoutParams(-1, dp(74));
        sp.setMargins(0, dp(8), 0, 0);
        left.addView(strip, sp);
        page.addView(left, new LinearLayout.LayoutParams(0, -1, 5.f));

        // the text
        LinearLayout right = new LinearLayout(this);
        right.setOrientation(LinearLayout.VERTICAL);
        LinearLayout.LayoutParams rp = new LinearLayout.LayoutParams(0, -1, 6.f);
        rp.setMargins(dp(20), 0, 0, 0);
        page.addView(right, rp);

        LinearLayout top = new LinearLayout(this);
        top.setGravity(Gravity.CENTER_VERTICAL);
        TextView title = label(project.name, 24, CREAM);
        title.setTypeface(Typeface.create("serif", Typeface.NORMAL));
        top.addView(title, new LinearLayout.LayoutParams(0, -2, 1.f));
        Button back = button(Texts.t("Back", "Wróć"));
        back.setOnClickListener(v -> finish());
        top.addView(back, new LinearLayout.LayoutParams(dp(90), dp(40)));
        right.addView(top);

        ScrollView scroll = new ScrollView(this);
        content = new LinearLayout(this);
        content.setOrientation(LinearLayout.VERTICAL);
        scroll.addView(content);
        right.addView(scroll, new LinearLayout.LayoutParams(-1, 0, 1.f));

        StringBuilder meta = new StringBuilder();
        meta.append(project.gameType == SpineApi.GOTHIC ? "Gothic" : project.gameType == SpineApi.GOTHIC2 ? "Gothic II" : "Gothic / II");
        if (!SpineApi.versionOf(project).equals("0.0"))
            meta.append("  ·  v").append(SpineApi.versionOf(project));
        if (!project.team.isEmpty())
            meta.append("  ·  ").append(project.team);
        content.addView(label(meta.toString(), 13, GREY));

        StringBuilder facts = new StringBuilder();
        if (project.size > 0)
            facts.append(Downloads.size(project.size));
        if (project.avgMinutes > 0)
            facts.append(facts.length() > 0 ? "  ·  " : "").append(project.avgMinutes / 60).append(" h ")
                    .append(project.avgMinutes % 60).append(" min");
        if (!SpineApi.languageTags(project).isEmpty())
            facts.append(facts.length() > 0 ? "  ·  " : "").append(SpineApi.languageTags(project));
        if (facts.length() > 0)
            content.addView(label(facts.toString(), 13, GREY));
        ratingView = label("", 14, GOLD);
        content.addView(ratingView);

        compat = label("", 13, TEXT);
        compat.setPadding(dp(12), dp(8), dp(12), dp(8));
        compat.setBackground(round(0x33172530, 0xff425761, dp(1), dp(8)));
        LinearLayout.LayoutParams cp = new LinearLayout.LayoutParams(-1, -2);
        cp.setMargins(0, dp(10), 0, dp(8));
        content.addView(compat, cp);

        LinearLayout actions = new LinearLayout(this);
        actions.setOrientation(LinearLayout.HORIZONTAL);
        primary = button("");
        actions.addView(primary, new LinearLayout.LayoutParams(0, dp(44), 1.f));
        check = button(Texts.t("Check scripts", "Sprawdź zgodność"));
        LinearLayout.LayoutParams chp = new LinearLayout.LayoutParams(0, dp(44), 1.f);
        chp.setMargins(dp(8), 0, 0, 0);
        actions.addView(check, chp);
        remove = button(Texts.t("Remove", "Usuń"));
        LinearLayout.LayoutParams rmp = new LinearLayout.LayoutParams(0, dp(44), 0.7f);
        rmp.setMargins(dp(8), 0, 0, 0);
        actions.addView(remove, rmp);
        content.addView(actions);

        description = body(Texts.t("Loading...", "Wczytywanie..."));
        description.setMovementMethod(LinkMovementMethod.getInstance());
        description.setLinkTextColor(GOLD);
        addSection(Texts.t("Description", "Opis"), description);
        return page;
    }

    private void addSection(String title, View view) {
        TextView head = label(title, 15, GOLD);
        head.setTypeface(Typeface.create("serif", Typeface.BOLD));
        head.setPadding(0, dp(14), 0, dp(4));
        content.addView(head);
        content.addView(view);
    }

    private TextView body(String text) {
        TextView t = label(text, 14, TEXT);
        t.setLineSpacing(0, 1.12f);
        return t;
    }

    private static CharSequence trim(CharSequence s) {
        SpannableStringBuilder b = new SpannableStringBuilder(s);
        while (b.length() > 0 && Character.isWhitespace(b.charAt(b.length() - 1)))
            b.delete(b.length() - 1, b.length());
        return b;
    }

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
