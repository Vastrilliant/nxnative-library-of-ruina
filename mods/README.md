# Mods

Each folder is one optional mod, chosen per build with the **mods** input of the *Build IPA* workflow (`speed-dice`, or several separated by commas). A mod that is not chosen leaves no trace in the build.

```
mods/<id>/
  mod.json   id, title, gates (names), obs (gate rows per build of main), host (C file), romfs (folder)
  host.c     callbacks for the gates (call dm_mod_gate_run from dm/host/hle_modcall.c to run on the guest stack); attaches them from a constructor with gate_obs_set()
  romfs/     files for the RomFS overlay (Data/StreamingAssets/... as in the game's RomFS)
```

How it works: `scripts/apply_mods.py` numbers the gates (32-63), copies `host.c` into `dm/host/` (built by wildcard), writes the gate rows the rewriter (`dm/tools/patch_nso.py`, `DM_MODS_PATCHES`) puts into the game's `main`, and merges the romfs folders into `romfs/` for `DM_ROMFS_OVERLAY`. A gate fires only in a build whose ID has rows in `mod.json`.

Add a mod: copy `speed-dice/`, change the id, gates and `host.c`.

On the device: the workflow uploads `mod-romfs` when mods are chosen; copy its contents into the app's `Documents/` and add `DM_ROMFS_OVERLAY=<that folder's path>` to `env.txt`.
