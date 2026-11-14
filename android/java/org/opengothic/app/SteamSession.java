package org.opengothic.app;

import in.dragonbra.javasteam.depotdownloader.DepotDownloader;
import in.dragonbra.javasteam.depotdownloader.IDownloadListener;
import in.dragonbra.javasteam.depotdownloader.data.AppItem;
import in.dragonbra.javasteam.depotdownloader.data.DownloadItem;
import in.dragonbra.javasteam.enums.EResult;
import in.dragonbra.javasteam.steam.authentication.AuthPollResult;
import in.dragonbra.javasteam.steam.authentication.AuthSessionDetails;
import in.dragonbra.javasteam.steam.authentication.AuthenticationException;
import in.dragonbra.javasteam.steam.authentication.CredentialsAuthSession;
import in.dragonbra.javasteam.steam.authentication.IAuthenticator;
import in.dragonbra.javasteam.steam.handlers.steamapps.License;
import in.dragonbra.javasteam.steam.handlers.steamapps.PICSProductInfo;
import in.dragonbra.javasteam.steam.handlers.steamapps.PICSRequest;
import in.dragonbra.javasteam.steam.handlers.steamapps.SteamApps;
import in.dragonbra.javasteam.steam.handlers.steamapps.callback.LicenseListCallback;
import in.dragonbra.javasteam.steam.handlers.steamapps.callback.PICSProductInfoCallback;
import in.dragonbra.javasteam.steam.handlers.steamuser.LogOnDetails;
import in.dragonbra.javasteam.steam.handlers.steamuser.SteamUser;
import in.dragonbra.javasteam.steam.handlers.steamuser.callback.LoggedOffCallback;
import in.dragonbra.javasteam.steam.handlers.steamuser.callback.LoggedOnCallback;
import in.dragonbra.javasteam.steam.steamclient.AsyncJobFailedException;
import in.dragonbra.javasteam.steam.steamclient.SteamClient;
import in.dragonbra.javasteam.steam.steamclient.callbackmgr.CallbackManager;
import in.dragonbra.javasteam.steam.steamclient.callbacks.ConnectedCallback;
import in.dragonbra.javasteam.steam.steamclient.callbacks.DisconnectedCallback;
import in.dragonbra.javasteam.steam.steamclient.configuration.SteamConfiguration;
import in.dragonbra.javasteam.types.KeyValue;

import android.util.Log;

import org.bouncycastle.jce.provider.BouncyCastleProvider;

import java.io.Closeable;
import java.io.IOException;
import java.security.SecureRandom;
import java.security.Security;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.CancellationException;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * One connection to Steam: the login (the password and Steam Guard, or the session that was kept), the games of the account
 * and the download of a game. It is the client of the game itself, as the program of Steam is, not the web site:
 * only a session of this kind is allowed to download the content. Every method blocks, so they belong to a thread of their own.
 * The library is JavaSteam (MIT), it needs Android 8 (java.time).
 */
final class SteamSession implements Closeable {
    private static final String TAG = "OpenGothic/Steam";

    static {
        // JavaSteam asks for the provider "BC" by name (SHA-1, AES of the content). Android has a "BC" of its own with the
        // algorithms taken out and a provider of a name that is taken is not added, so the old one has to go first.
        // The provider goes to the END of the list: JavaSteam finds it by the name. At the first place it would take the
        // keystores and the certificates of the whole application, and the TLS of everything else (Spine, the update) failed
        // with "NoSuchAlgorithmException: Error constructing implementation (algorithm: Default ..." until the application was closed.
        Security.removeProvider("BC");
        Security.addProvider(new BouncyCastleProvider());
    }

    /** What the player has to be asked for. The answers are given from any thread. */
    interface Prompt {
        /** The code of Steam Guard from the app of Steam or from the e-mail. Complete it with null to give up. */
        void code(boolean email, String emailHint, boolean previousWasWrong, CompletableFuture<String> answer);

        /** Steam waits until the login is approved in the app of Steam: only to tell the player. */
        void approve();
    }

    interface Progress {
        void status(String text);

        /** The bytes that are done and the size of the whole game (0 when it is not known). */
        void bytes(long done, long total);
    }

    static final class Failure extends Exception {
        /** The session has ended or the password is not right: the player has to sign in again. */
        final boolean signIn;

        Failure(String message, boolean signIn) {
            super(message);
            this.signIn = signIn;
        }
    }

    static final class Login {
        String account;
        String refreshToken;
        String steamId;
    }

    private final SteamClient client;
    private final CallbackManager manager;
    private final SteamUser user;
    private final SteamApps apps;
    private final List<Closeable> subscriptions = new ArrayList<>();

    private volatile boolean running = true;
    private volatile boolean closing;
    private Thread pump;

    private volatile CompletableFuture<Void> connected = new CompletableFuture<>();
    private volatile CompletableFuture<LoggedOnCallback> loggedOn = new CompletableFuture<>();
    private volatile CompletableFuture<List<License>> licenses = new CompletableFuture<>();
    private volatile CompletableFuture<Void> picsDone;
    private final Map<Integer, PICSProductInfo> packages = new java.util.concurrent.ConcurrentHashMap<>();
    private volatile DepotDownloader downloader;

    /**
     * How many times the download ran out of memory in this process: every time the next try takes fewer chunks at the same time.
     * The library keeps the chunks in the memory (up to 64 packed and 64 unpacked ones wait between its stages, its buffers can not be
     * changed), so the only thing to turn down is the number of the chunks that are loaded and unpacked at the same time.
     */
    static volatile int memoryStrikes = 0;

    private static final java.util.ArrayDeque<String> memorySamples = new java.util.ArrayDeque<>();

    private static void sampleMemory(long seconds, long done, long total) {
        Runtime rt = Runtime.getRuntime();
        long used = (rt.totalMemory() - rt.freeMemory()) / (1024 * 1024);
        long max  = rt.maxMemory() / (1024 * 1024);
        synchronized (memorySamples) {
            memorySamples.addLast(seconds + " s: heap " + used + " of " + max + " MB, downloaded " + (done / (1024 * 1024)) + " of " + (total / (1024 * 1024)) + " MB");
            while (memorySamples.size() > 60)
                memorySamples.removeFirst();
        }
    }

    /** The last samples of the memory of the download, for the file of the error. */
    static String memoryReport() {
        StringBuilder b = new StringBuilder("memory strikes " + memoryStrikes + ", the last samples:\n");
        synchronized (memorySamples) {
            for (String s : memorySamples)
                b.append("  ").append(s).append('\n');
        }
        return b.toString();
    }
    SteamSession() {
        client = new SteamClient(SteamConfiguration.create(builder -> { }));
        try {
            // a job of the library fails after ten seconds without an answer: a slow network (a phone far from the servers of
            // Steam) is not a failure. The calls here have the time limits of their own.
            client.getJobManager$javasteam().setTimeoutsEnabled(false);
        } catch (Throwable e) {
            Log.w(TAG, "the time limit of the jobs stays: " + e);
        }
        manager = new CallbackManager(client);
        user = client.getHandler(SteamUser.class);
        apps = client.getHandler(SteamApps.class);

        subscriptions.add(manager.subscribe(ConnectedCallback.class, c -> connected.complete(null)));
        subscriptions.add(manager.subscribe(DisconnectedCallback.class, this::onDisconnected));
        subscriptions.add(manager.subscribe(LoggedOnCallback.class, c -> loggedOn.complete(c)));
        subscriptions.add(manager.subscribe(LoggedOffCallback.class, c -> {
            // only a token that is no good means a new login; being replaced by another session of the account is not that
            EResult r = c.getResult();
            boolean again = r == EResult.AccessDenied || r == EResult.InvalidPassword || r == EResult.Expired
                    || r == EResult.InvalidLoginAuthCode || r == EResult.AccountDisabled;
            Log.w(TAG, "logged off: " + r);
            failAll(new Failure(again ? "The session has ended, sign in again." : "Steam has ended the session: " + r + ".", again));
        }));
        subscriptions.add(manager.subscribe(LicenseListCallback.class, c -> {
            if (c.getResult() == EResult.OK)
                licenses.complete(c.getLicenseList());
            else
                licenses.completeExceptionally(new Failure("Steam did not give the list of the licenses.", false));
        }));
        subscriptions.add(manager.subscribe(PICSProductInfoCallback.class, this::onProductInfo));

        pump = new Thread(() -> {
            while (running)
                manager.runWaitCallbacks(1000L);
        }, "steam-callbacks");
        pump.setDaemon(true);
        pump.start();
    }

    // ---- the login

    void connect() throws Failure {
        connected = new CompletableFuture<>();
        client.connect();
        await(connected, 30, "Steam does not answer, check the connection.");
    }

    /** The password goes to Steam encrypted, the library does it. Steam Guard is asked for through the prompt. */
    Login login(String account, String password, String deviceName, final Prompt prompt) throws Failure {
        AuthSessionDetails details = new AuthSessionDetails();
        details.username = account;
        details.password = password;
        details.deviceFriendlyName = deviceName;
        details.persistentSession = true;
        details.authenticator = new IAuthenticator() {
            @Override
            public CompletableFuture<String> getDeviceCode(boolean previousCodeWasIncorrect) {
                CompletableFuture<String> answer = new CompletableFuture<>();
                prompt.code(false, null, previousCodeWasIncorrect, answer);
                return answer;
            }

            @Override
            public CompletableFuture<String> getEmailCode(String email, boolean previousCodeWasIncorrect) {
                CompletableFuture<String> answer = new CompletableFuture<>();
                prompt.code(true, email, previousCodeWasIncorrect, answer);
                return answer;
            }

            @Override
            public CompletableFuture<Boolean> acceptDeviceConfirmation() {
                prompt.approve();
                // the library waits for the approval of the app of Steam
                return CompletableFuture.completedFuture(Boolean.TRUE);
            }
        };

        AuthPollResult result;
        try {
            // the first call goes to a server that may be slow or may have just been lost: one more try
            CredentialsAuthSession auth;
            for (int attempt = 0; ; ++attempt) {
                try {
                    auth = client.getAuthentication().beginAuthSessionViaCredentials(details).get(90, TimeUnit.SECONDS);
                    break;
                } catch (ExecutionException e) {
                    if (attempt > 0 || closing || !(e.getCause() instanceof AsyncJobFailedException))
                        throw e;
                    Log.w(TAG, "the login did not get an answer, trying again");
                }
            }
            result = auth.pollingWaitForResult().get();
        } catch (TimeoutException e) {
            throw new Failure("Steam did not answer in time. Check the connection and try again.", false);
        } catch (AuthenticationException e) {
            throw explain(e);
        } catch (ExecutionException e) {
            throw explain(e.getCause() != null ? e.getCause() : e);
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
            throw new Failure("The login was interrupted.", false);
        } catch (CancellationException e) {
            throw new Failure("The login was cancelled.", false);
        }

        Login login = new Login();
        login.account = result.getAccountName();
        login.refreshToken = result.getRefreshToken();
        logOn(login.account, login.refreshToken);
        login.steamId = Long.toString(client.getSteamID().convertToUInt64());
        return login;
    }

    /** The session that was kept: no password and no code. */
    Login resume(String account, String refreshToken) throws Failure {
        logOn(account, refreshToken);
        Login login = new Login();
        login.account = account;
        login.refreshToken = refreshToken;
        login.steamId = Long.toString(client.getSteamID().convertToUInt64());
        return login;
    }

    private void logOn(String account, String refreshToken) throws Failure {
        loggedOn = new CompletableFuture<>();
        licenses = new CompletableFuture<>();
        LogOnDetails details = new LogOnDetails();
        details.setUsername(account);
        details.setAccessToken(refreshToken);
        details.setShouldRememberPassword(true);
        // every session has a number of its own, so the screen and a download can be signed in at the same time
        details.setLoginID(new SecureRandom().nextInt(Integer.MAX_VALUE - 1) + 1);
        user.logOn(details);

        LoggedOnCallback r = await(loggedOn, 30, "Steam did not answer the login.");
        Log.i(TAG, "logon: " + r.getResult());
        if (r.getResult() != EResult.OK) {
            boolean again = r.getResult() == EResult.AccessDenied || r.getResult() == EResult.InvalidPassword
                    || r.getResult() == EResult.Expired || r.getResult() == EResult.InvalidLoginAuthCode;
            throw new Failure(again ? "The session has ended, sign in again." : "Steam refused the login: " + r.getResult() + ".", again);
        }
    }

    // ---- the games

    /** Which of the apps the account owns: the licenses are packages, a package lists its apps. */
    Set<Integer> owned(Set<Integer> wanted) throws Failure {
        List<License> list = await(licenses, 30, "Steam did not give the list of the licenses.");
        List<PICSRequest> requests = new ArrayList<>();
        for (License l : list)
            requests.add(new PICSRequest(l.getPackageID(), l.getAccessToken()));

        Set<Integer> ret = new HashSet<>();
        if (requests.isEmpty())
            return ret;

        // the answer comes in parts, every part is a callback
        packages.clear();
        picsDone = new CompletableFuture<>();
        apps.picsGetProductInfo(Collections.<PICSRequest>emptyList(), requests);
        await(picsDone, 60, "Steam did not give the games of the account.");

        for (PICSProductInfo info : packages.values()) {
            KeyValue ids = info.getKeyValues().get("appids");
            if (ids == null)
                continue;
            for (KeyValue id : ids.getChildren()) {
                Integer app = id.asInteger(0);
                if (wanted.contains(app))
                    ret.add(app);
            }
        }
        return ret;
    }

    private void onProductInfo(PICSProductInfoCallback c) {
        packages.putAll(c.getPackages());
        if (!c.isResponsePending() && picsDone != null)
            picsDone.complete(null);
    }

    // ---- the download

    /**
     * The files of the Windows version of the game go to the folder, in the language that was chosen (the name that Steam
     * gives it, "polish"). An interrupted download goes on from what is there.
     */
    void download(int appId, java.io.File dir, boolean verify, String language, final Progress progress) throws Failure {
        List<License> list = await(licenses, 30, "Steam did not give the list of the licenses.");
        final CompletableFuture<Void> done = new CompletableFuture<>();
        final Failure[] failed = {null};

        // the library tells the part of a depot, never the part of the game: the game is the sum of its depots
        final Map<Integer, Long> sizes = new java.util.HashMap<>(depotSizes(appId, language));
        final Map<Integer, Float> parts = new java.util.HashMap<>();
        final Object lock = new Object();
        final Runnable report = () -> {
            long total = 0, got = 0;
            synchronized (lock) {
                for (Map.Entry<Integer, Long> e : sizes.entrySet()) {
                    total += e.getValue();
                    Float p = parts.get(e.getKey());
                    got += p != null ? (long) (p * e.getValue()) : 0;
                }
            }
            progress.bytes(got, total);
        };

        IDownloadListener listener = new IDownloadListener() {
            @Override
            public void onItemAdded(DownloadItem item) {
            }

            @Override
            public void onDownloadStarted(DownloadItem item) {
                progress.status("Downloading...");
            }

            @Override
            public void onDownloadCompleted(DownloadItem item) {
                done.complete(null);
            }

            @Override
            public void onDownloadFailed(DownloadItem item, Throwable error) {
                failed[0] = new Failure("The download failed: " + String.valueOf(error.getMessage()), false);
                done.complete(null);
            }

            @Override
            public void onStatusUpdate(String message) {
                progress.status(message);
            }

            @Override
            public void onFileCompleted(int depotId, String fileName, float depotPercentComplete) {
                part(depotId, depotPercentComplete, 0);
            }

            @Override
            public void onChunkCompleted(int depotId, float depotPercentComplete, long compressedBytes, long uncompressedBytes) {
                part(depotId, depotPercentComplete, uncompressedBytes);
            }

            @Override
            public void onDepotCompleted(int depotId, long compressedBytes, long uncompressedBytes) {
                part(depotId, 1.f, uncompressedBytes);
            }

            private void part(int depotId, float percent, long bytesOfDepot) {
                synchronized (lock) {
                    parts.put(depotId, Math.min(1.f, percent));
                    // a depot that the sizes of the app did not list: its size is what it has done so far over its part
                    if (!sizes.containsKey(depotId) && percent > 0.01f && bytesOfDepot > 0)
                        sizes.put(depotId, (long) (bytesOfDepot / percent));
                }
                report.run();
            }
        };

        // the arguments: debug, LAN cache, chunks loaded, chunks unpacked and files written at the same time, the Windows build.
        // A chunk is a request to a server of Steam and about two megabytes in the memory (the packed one and the unpacked one):
        // a few of them at the same time are slow, too many fill the memory of the application. The number follows the memory.
        // The chunks in flight are only a part of the memory of the pipeline (see memoryStrikes): 8 at most (4 for a heap of 256 MB),
        // half of it after a failure for the lack of memory, and so on. Unpacking 2 chunks at the same time and writing 1 file at a time
        // is enough for the speed of the network.
        int megabytes = (int) (Runtime.getRuntime().maxMemory() / (1024 * 1024));
        int strikes = Math.min(memoryStrikes, 3);
        int chunks = Math.max(2, Math.max(3, Math.min(8, megabytes / 64)) >> strikes);
        int cores = Runtime.getRuntime().availableProcessors();
        int unpack = strikes >= 2 ? 1 : Math.max(1, Math.min(2, cores));
        Log.i(TAG, "memory " + megabytes + " MB, " + chunks + " chunks loaded and " + unpack + " unpacked at the same time (memory strikes " + strikes + ")");
        DepotDownloader d = new DepotDownloader(client, list, false, false, chunks, unpack, 1, true, null);
        downloader = d;
        try {
            d.addListener(listener);
            // the game, the files of Windows and every architecture, in the language that the player has chosen:
            // what has no language (the world, the textures) comes with it
            d.add(new AppItem(appId, false, dir.getAbsolutePath(), null, null,
                    false, "windows", true, null, false, language, false,
                    Collections.<Integer>emptyList(), Collections.<Long>emptyList(), verify, false));
            d.finishAdding();
            final long started = System.currentTimeMillis();
            while (!done.isDone() && running) {
                try {
                    done.get(1, TimeUnit.SECONDS);
                } catch (TimeoutException ignored) {
                    // the download goes on
                    long total = 0, got = 0;
                    synchronized (lock) {
                        for (Map.Entry<Integer, Long> e : sizes.entrySet()) {
                            total += e.getValue();
                            Float p = parts.get(e.getKey());
                            got += p != null ? (long) (p * e.getValue()) : 0;
                        }
                    }
                    sampleMemory((System.currentTimeMillis() - started) / 1000, got, total);
                } catch (ExecutionException | InterruptedException e) {
                    break;
                }
            }
            if (!running && !done.isDone())
                throw new Failure("The download was stopped.", false);
            if (failed[0] != null)
                throw failed[0];
        } finally {
            downloader = null;
            try {
                d.close();
            } catch (Exception ignored) {
            }
        }
    }

    /**
     * The sizes of the depots that this download takes, from the information of the app: the Windows ones and those of the chosen
     * language (or of none). It is a guess of the library's own choice, an empty answer only means that the whole is learned on the way.
     */
    private Map<Integer, Long> depotSizes(int appId, String language) {
        Map<Integer, Long> ret = new java.util.HashMap<>();
        try {
            Long token = apps.picsGetAccessTokens(appId).toFuture().get(30, TimeUnit.SECONDS).getAppTokens().get(appId);
            PICSRequest request = new PICSRequest(appId, token != null ? token : 0L);
            for (PICSProductInfoCallback c : apps.picsGetProductInfo(request).toFuture().get(30, TimeUnit.SECONDS).getResults()) {
                PICSProductInfo info = c.getApps().get(appId);
                if (info == null)
                    continue;
                for (KeyValue depot : info.getKeyValues().get("depots").getChildren()) {
                    int id;
                    try {
                        id = Integer.parseInt(depot.getName());
                    } catch (NumberFormatException e) {
                        continue;   // "branches" and the like
                    }
                    KeyValue config = depot.get("config");
                    String os = config.get("oslist").getValue();
                    String lang = config.get("language").getValue();
                    String low = config.get("lowviolence").getValue();
                    if (os != null && !os.isEmpty() && !os.contains("windows"))
                        continue;
                    if (lang != null && !lang.isEmpty() && !lang.equals(language))
                        continue;
                    if ("1".equals(low))
                        continue;
                    long size = depot.get("manifests").get("public").get("size").asLong(0);
                    if (size > 0)
                        ret.put(id, size);
                }
            }
        } catch (Exception e) {
            Log.w(TAG, "sizes of the depots: " + e);
        }
        Log.i(TAG, "depots of " + appId + ": " + ret);
        return ret;
    }

    /** Stops what is going on, the files stay for the next try. */
    void cancel() {
        running = false;
        DepotDownloader d = downloader;
        if (d != null) {
            try {
                d.close();
            } catch (Exception ignored) {
            }
        }
        failAll(new Failure("Stopped.", false));
    }

    boolean isCancelled() {
        return !running;
    }

    @Override
    public void close() {
        closing = true;
        running = false;
        try {
            if (client.isConnected())
                user.logOff();
        } catch (Exception ignored) {
        }
        try {
            client.disconnect();
        } catch (Exception ignored) {
        }
        for (Closeable c : subscriptions) {
            try {
                c.close();
            } catch (IOException ignored) {
            }
        }
        subscriptions.clear();
    }

    // ---- details

    private void onDisconnected(DisconnectedCallback c) {
        if (!closing && !c.isUserInitiated())
            failAll(new Failure("The connection to Steam was lost.", false));
    }

    private void failAll(Failure f) {
        connected.completeExceptionally(f);
        loggedOn.completeExceptionally(f);
        licenses.completeExceptionally(f);
        CompletableFuture<Void> p = picsDone;
        if (p != null)
            p.completeExceptionally(f);
    }

    private <T> T await(CompletableFuture<T> future, int seconds, String timeout) throws Failure {
        try {
            return future.get(seconds, TimeUnit.SECONDS);
        } catch (TimeoutException e) {
            throw new Failure(timeout, false);
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
            throw new Failure("Interrupted.", false);
        } catch (ExecutionException e) {
            throw explain(e.getCause() != null ? e.getCause() : e);
        } catch (CancellationException e) {
            throw new Failure("Stopped.", false);
        }
    }

    private static Failure explain(Throwable e) {
        if (e instanceof Failure)
            return (Failure) e;
        if (e instanceof AuthenticationException) {
            EResult r = ((AuthenticationException) e).getResult();
            if (r == EResult.InvalidPassword)
                return new Failure("Wrong account name or password.", false);
            if (r == EResult.RateLimitExceeded || r == EResult.AccountLoginDeniedThrottle)
                return new Failure("Too many tries, wait a while before the next one.", false);
            if (r == EResult.TwoFactorCodeMismatch || r == EResult.InvalidLoginAuthCode)
                return new Failure("The Steam Guard code is wrong.", false);
            return new Failure("Steam refused the login" + (r != null ? ": " + r : "") + ".", false);
        }
        Log.w(TAG, "failure", e);
        if (e instanceof AsyncJobFailedException)
            return new Failure("Steam did not answer in time or the connection was lost. Check the connection and try again.", false);
        String m = e.getMessage();
        return new Failure(m != null && !m.isEmpty() ? m : e.getClass().getSimpleName(), false);
    }
}
