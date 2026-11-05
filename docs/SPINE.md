# Mods from Spine (Android)

The launcher has a **Spine** button. It opens the database of Gothic mods of [Spine](https://github.com/ClockworkOrigins/Spine)
(Clockwork Origins): the list of the mods, the download into the installation of Gothic or Gothic II, and the play. The list and
the files need no account.

The mods go to the installation that is imported or downloaded already (`Gothic1`, `Gothic2`), so Gothic / Gothic II has to be
there first. A mod for both games uses Gothic II when it is installed.

## What is shown

Mods of the types *total conversion* and *enhancement* for Gothic, Gothic II or both (about 340 of 566 entries; patches, tools,
demos and Gothic 3 are left out). Search, the game, the language, the sort, and *Downloaded* are in the bar. A row has the
size, the languages (`DE EN PL RU`) and the playtime; a tap opens the details.

## The page of a mod

A tap on a row opens the page of the mod (`SpineDetailsActivity`): the screenshots (a tap shows one in full), the description
with its formatting (the HTML of the database goes through `Html.fromHtml`), the features, the changes of the versions, the
rating and the opinions of the players, the size, the playtime and the languages, and the buttons *Check*, *Download*, *Play*,
*Remove*. The data comes from `requestInfoPage`, `getWeightedRating`, `getRatings` and `getReviews`, the pictures from
`{FallbackFileserver-of-mods}<id>/screens/<File>`.

## Ikarus and LeGo

OpenGothic cannot run mods that use the libraries Ikarus and LeGo as a rule (they read and write the memory of the original
program, see [IKARUS.md](IKARUS.md)). The list of Spine has no field that says which mod uses them, so the files themselves are
looked into: the compiled scripts (`GOTHIC.DAT` inside the `.mod`/`.vdf`, or loose) keep the names of all the symbols as text,
and `SpineScan` looks for the names that only the libraries declare (`MEM_ReadInt`, `MEM_WriteInt`, `MEM_InitAll`,
`HookEngine`, `CALL__*`, `LeGo_Init`, `FF_ApplyExtGT`, ...).

**Before the download** (the button *Check*, `SpineInstaller.check`): a VDF archive has its catalog at the start, so
`SpineVdf` reads the header and the catalog, and then only the bytes up to the end of the last `GOTHIC*.DAT` (`MENU.DAT`
is skipped, it has no libraries and is often at the end). That is a part of the main archive: from nothing (the scripts are
loose or in a small archive, textures come after) to about half of it; the rest is not downloaded for the check. The bytes
are kept as `<file>.zpart` and the download goes on from them with an HTTP `Range` request, so nothing is read twice; a pass
is remembered in `spine/checked.txt` (`id<TAB>version`). The check is a button, because for some mods it still means a few
tens of MB; the download itself does the same check on its way, so a mod is never installed without it.

* A mod with one of the libraries is **stopped at once**, deleted, and remembered in `spine/blocked.txt`. It disappears from the
  list; *Unsupported: N* shows them again, greyed out.
* It is a check of names, not of the behaviour: a mod that has the libraries only in a form that has none of these names would
  pass, and a mod could in theory carry the names without using them. In a test on the real server *Gothic 2: Reimagined* was stopped after 255 KB of 5.4 MB, and mods without the names passed (and installed with a correct hash); there is no list of all mods that was checked.
* `SpineInstaller.ALLOW_LIBRARIES = true` switches the check off (for the mod that the emulation of
  `common/game/compatibility` was made for).

## How a mod is stored and played

`spine/<id>/` in the files of the application keeps the mod with the paths it has in the Gothic folder
(`Data/modvdf/X.mod`, `System/X.ini`, `_work/...`). As Spine does on the computer, the files go to the folder of the game
**only for the time of the play** (`SpineActivation`):

1. the archives that `[FILES] VDF=` of the ini lists go to `Data/` (the others stay), the other files go to their place, with the
   case of the folders of the game (`_work/data` → `_work/Data`);
2. a file of the game that a mod replaces is kept as `*.spbak`;
3. `game.txt` gets the folder of the game and the ini (`-game:`); the engine reads it (`InstallDetect::androidModIni`);
4. when the game is over (the launcher or the Spine screen is shown again, and before every start of a game) everything is put
   back. The files are *moved*, not copied, so it takes no time and no space.

What was done is written to `spine/active.txt` before every step, so the folder of the game is also put back after a crash or
when the application was stopped while the mod was played.

`[OVERRIDES]` of the ini (changes of `Gothic.ini` that Spine makes) and packages (voice packs) are not supported yet. Plugins
and programs of Windows (`.dll`, `.exe`) are not downloaded.

## The protocol (for the next person)

* `POST https://clockwork-origins.com:19181/<endpoint>` with a JSON body; every value of the answer is a string.
  `requestAllProjects` `{"Username":"","Password":"","Language":"English"}` → `Projects[]`;
  `requestProjectFiles` `{"Username":"","Password":"","Language":"Deutsch","ProjectID":1}` → `Files[{File,Hash}]`,
  `FallbackFileserver`.
* The server knows only ciphers with the key exchange by RSA (`TLS_RSA_WITH_AES_*`); newer Java versions disable them, so
  `SpineApi` adds them for this host (Android's own TLS has to support them).
* A file is `{FallbackFileserver}{ProjectID}/{File}`. `.z` is a zlib stream; the hash is the SHA-512 of the file without it.
* The ini of a mod is in the list of some languages only, so the first language (the phone, English, Polish, German, Russian) that
  has an ini is taken for the whole mod.
* Games: `GameType` 0 Gothic, 1 Gothic II, 3 both. Mods: `ModType` 0 total conversion, 1 enhancement.
