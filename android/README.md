# Android

An ARM64 APK using Tempest's Android backend. Requires Android 7 (API 24) and Vulkan 1.1. Android window destruction closes the application, including when backgrounding or locking destroys its surface, so save the game before switching applications.

The application has a launcher (`android/java`) that imports the game files and starts the native `GameActivity`. Controls are drawn and handled by the engine, see `common/utils/androidinput.cpp`.

## Build

Install JDK 17, Gradle 8.9, CMake 3.22.1 or newer, Ninja and the Android SDK with SDK 35, build-tools 35.0.0, NDK 27.0.12077973 and SDK CMake 3.22.1. Set `JAVA_HOME`, `ANDROID_HOME` and `VULKAN_SDK`. Put Gradle, Ninja and the host Vulkan SDK's `glslangValidator` on `PATH`. Vulkan headers come from the host SDK; Android links the NDK's Vulkan loader.

Clone this fork with `git clone --recursive https://github.com/borsuczyna/OpenGothic-Android.git` and enter `OpenGothic-Android`, or run `git submodule update --init --recursive` in an existing checkout. Replace `/path/to/ndk` below with the NDK installation directory. The same commands work in PowerShell and a Linux shell.

```sh
cmake -S . -B build/android -G Ninja -DCMAKE_TOOLCHAIN_FILE=/path/to/ndk/build/cmake/android.toolchain.cmake -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-24 -DCMAKE_BUILD_TYPE=Release
cmake --build build/android --target OpenGothic-apk
```

APK: `build/android/OpenGothic-apk/build/outputs/apk/release/OpenGothic-apk-release.apk`. Use `-DCMAKE_BUILD_TYPE=Debug` for a debuggable APK under `outputs/apk/debug/OpenGothic-apk-debug.apk`. Desktop builds do not invoke the APK helper or require Android tools.

`android/build-apk.sh` builds a signed APK with the tools of the Android SDK only, without Gradle. See the comment in the script for its options.

## Launcher

The first screen is the login to Steam (`SteamLoginActivity`) when nobody is signed in; "Continue without an account" goes to the library and the games are imported from a ZIP then. The library (`LauncherActivity`) has a card for Gothic, Gothic II and The Chronicles of Myrtana: **Play** when the game is installed, **Download** when the account owns it, **Import** otherwise. The games live next to each other (`Gothic1`, `Gothic2`, `Myrtana` in the files of the application), the one that is played is written to `game.txt` for the engine. The covers are in `android/launcher-assets/covers` (assets of the APK).

The launcher asks GitHub for the release of the repository at the start (`UpdateCheck.java`): when the version in the name of the release is higher than the `VERSION_NAME` of the application (`add_android_apk` in `CMakeLists.txt`), "Update available" shows in the header and opens the page of the release. Raise the version there with every release.

The update installs itself (`AppUpdate.java`): the launcher offers it once for every version (the chip stays for later), downloads the first `.apk` of the release straight into a session of the system installer and commits it. Android always asks the player to confirm the installation, and asks once to allow the launcher to install packages ("install unknown apps"). Attach the APK to the release; it has to be **signed with the same key** as the installed one, or the system refuses it with the reason.

## Steam

The login is the one of the client of Steam, with the library [JavaSteam](https://github.com/Longi94/JavaSteam) (MIT); `SteamSession.java` is the only place that uses it, the launcher reaches it by names (`SteamBridge`). The password goes to Steam only (the library encrypts it) and is not kept. The session is a refresh token, kept encrypted with a key of the Android Keystore (`SteamStore`); signing out deletes it. A download asks for the language of the game, then it is a foreground service with a notification (`SteamDownloadService`); it takes the Windows files of that language into `<folder>.download` and moves them when the game is whole, and it goes on from what is on the disk after an interruption.

The libraries come with the Gradle build (`DEPENDENCIES` in `CMakeLists.txt`). `build-apk.sh` has none: it leaves the Steam classes out and the launcher works without Steam. JavaSteam needs Android 8 (`java.time`): on Android 7 the launcher has no Steam.

## Game files

Use legally owned installations of Gothic, Gothic II: Night of the Raven and The Chronicles of Myrtana, any of them or all. No game files are included in the APK. The launcher keeps a folder for each (`Gothic1`, `Gothic2`, `Myrtana` in the files of the application), chooses one and writes its name to `game.txt`, the engine starts that one.

Make a ZIP of the installation: `Data`, `_work` and `System` have to be at the root of the archive, or in a single folder in it. Choose **Import game files** in the launcher; the files are copied to the app-specific storage, no storage permissions are needed.

Alternatively, copy the installation root with adb. `Data`, `_work` and `System` must be directly inside `Gothic2`.

```sh
adb install -r build/android/OpenGothic-apk/build/outputs/apk/release/OpenGothic-apk-release.apk
adb shell mkdir -p /sdcard/Android/data/org.opengothic.mobile/files/Gothic2
adb push "/path/to/Gothic II/." /sdcard/Android/data/org.opengothic.mobile/files/Gothic2/
adb logcat -s app AndroidRuntime DEBUG
```

Logs, saves and writable settings use the app's internal files directory. Uninstalling removes both directories. Ray tracing and mesh shading are disabled for this Android build.

With a debug APK, read the file log using `adb shell run-as org.opengothic.mobile cat files/log.txt`.

## Touch controls

- Left stick moves the hero, dragging the right side of the screen rotates the camera.
- Actions that are not possible at the moment (attack without a weapon, spells without mana, sneaking before it is learned, ...) are hidden or dimmed.
- Menus are scrolled with swipes. A selection is confirmed only with an explicit button, never by a gesture.
- Inventory has item categories, direct selection and use/drop/transfer buttons.
- The gear in the main menu opens mobile settings: button size, 3D resolution, shadows, SSAO, FPS limit, joystick deadzone and camera sensitivity. They are stored in the `[MOBILE]` section of the ini file; the Android defaults are 50% resolution, 256 shadow map, no SSAO and 30 FPS. Existing saved preferences are preserved. 3D resolution cycles through 25%, 50%, 75% and 100%; 25% uses one sixteenth of the native pixel count while the interface stays at full resolution. Shadow quality cycles through off, 128, 256, 512, 1024 and 2048.

Android retains the renderer's frustum and HiZ occlusion culling. It also skips static object details smaller than two internal render pixels and shadow casters smaller than one shadow texel. Terrain, skinned characters, morph animations and water are exempt from the extra size culling. Vegetation outside the camera and both active shadow views skips CPU wind matrix updates and uploads; it resumes at the current animation time on reentry. Fog LUTs use half the width and height (one quarter of the scattering rays and storage), keeping the depth samples and distance unchanged. The lower fog resolution and extra size culling are Android-specific.

Particle simulation continues normally, but Android uploads and draws only live particles that intersect the camera or an active shadow view. Decals keep their static upload path, trails keep their full segments, and dynamic particle/trail buffers reuse their capacity as counts change. The point-light vertex shader also tests near-plane intersections using projected box extents instead of transforming eight corners per vertex.

The Gothic visibility options now affect rendering immediately, without restarting or resizing the window. `[PERFORMANCE] sightValue` uses the menu's 20% steps (0 = 20%, 4 = 100%, 14 = 300%); this engine maps 100% to a 250-metre landscape far plane. `[ENGINE] zVobFarClipZScale` controls a separate object range (100 metres at 1x), capped by landscape visibility. Vanilla multiplier values and Myrtana's indexed percentage choices are handled separately using the loaded menu's labels. Model bounds, including scale and wind, keep large objects visible while any part remains in range. The object limit is applied to the HiZ, main and conventional shadow passes; Android also avoids wind/morph uploads for objects outside the active views or range. AI, collision and animation events continue normally. Per-zone Vob far-plane overrides remain unsupported.

CPU instance uploads wait on a completion condition instead of spinning, wakeups use a predicate, and unchanged bone matrices no longer mark GPU blocks dirty. Disabled conventional shadow views skip their global-buffer uploads and copy dispatches. Freed cluster ranges use the smallest suitable hole to reduce fragmentation.
Changing unrelated settings no longer reallocates shadow maps; existing maps are reused until their size or rendering method requires a different allocation.

The original Gothic graphics sliders are also applied by the renderer. Texture quality (`INTERNAL/texDetailIndex`, 0..1) selects full, half, quarter or one-eighth world texture dimensions. Existing compressed mipmaps stay compressed; fonts, menus and document images keep their original resolution. Changes reload cached world textures and refresh descriptors before recording the next frame; this can briefly pause rendering. BC textures without smaller mipmaps retain their original dimensions. Model detail (`PERFORMANCE/modelDetail`, 0..1) controls distance-based progressive LOD thresholds, with object radius taken into account. MRM attachments and skinned models use two reduced levels from their native wedge collapse maps; nearby objects retain full detail, and higher quality moves transitions farther away. Depth and conventional shadow passes select the same geometry. Morph meshes, terrain and assets without valid collapse maps retain full detail. Collision and ray-traced geometry retain the original mesh. LODs add shared mesh storage at loading time and avoid reloading models when the slider changes.

The state machine of the controls has no Android dependencies and is tested on the host:

```sh
c++ -std=c++17 -I common android/tests/controls_test.cpp common/ui/mobilecontrolsmodel.cpp -o controls_test && ./controls_test
```

## Saves

The load and save screens of the touch UI list every `save_slot_N.sav` of the game (there is no limit of the slots), the newest first. A new save is named after the journal topic that has changed last, the name can be changed on the screen (the keyboard of the screen has capital and Polish letters), a save can be renamed, overwritten and deleted. The list shows the chapter and the level of the hero, saves made before this have only the date and the play time. The options are one screen too: the categories on the left, the chosen menu on the right. The launcher can export all saves to a ZIP and import that ZIP later. Import offers two modes: overwrite files whose slot names match, or add every imported save into a new, unused slot so existing saves remain untouched.

## Icons

The icons of the controls are 128x128 RGBA PNG files in `android/assets/touch-icons`. They are embedded into the binary by `android/assets/CMakeLists.txt` and decoded at startup. `IconFiles` in `common/utils/androidinput.cpp` lists them in the order of `AndroidInput::Icon`.
