# Mods

Each folder is one mod. Every mod in this folder is built into every IPA, and each one is **off until the player turns it on**: the settings screen has a *Mods* section with one switch per mod (it sets the mod's env switch, e.g. `DM_MOD_SPEEDDICE=1`; `Documents/env.txt` wins over the screen). A change takes effect at the next start.

```
mods/<id>/
  mod.json   id, title, launcher (env, label, note of its switch), gates (names), obs (gate rows per build of main), host (C file), romfs (folder)
  host.c     callbacks for the gates (call dm_mod_gate_run from dm/host/hle_modcall.c to run on the guest stack); attaches them from a constructor with gate_obs_set(), and returns from each callback while its env switch is not 1 (read at the first gate hit: the launcher's choices are not in the environment yet when constructors run)
  romfs/     files for the RomFS overlay (Data/StreamingAssets/... as in the game's RomFS)
```

How it works: `scripts/apply_mods.py` numbers the gates (32-63), copies `host.c` into `dm/host/` (built by wildcard), writes `launcher_mods.json` (the Mods switches; `dm/games/settings.py` adds them to the app's `launcher.json` when `DM_MODS_SETTINGS` names the file, for the iOS and Mac builds alike), writes the gate rows the rewriter (`dm/tools/patch_nso.py`, `DM_MODS_PATCHES`) puts into the game's `main`, and merges the romfs folders into `romfs/` for `DM_ROMFS_OVERLAY`. A gate fires only in a build whose ID has rows in `mod.json`.

`dm/games/settings.py` is the one repo file this framework edits for the launcher (a `Mods` section and `mod_rows`).

Add a mod: copy `speed-dice/`, change the id, gates and `host.c`.

On the device: the workflow uploads `mod-romfs`; copy its contents (the `Data/` folder) into the app's `Documents/` and add `DM_ROMFS_OVERLAY=<the folder that contains Data/>` to `env.txt` (the path of `Documents`, not of `Documents/Data`). Then switch the mod on under Mods.
