#!/usr/bin/env python3
"""Select the mods a build includes (mods/<id>/, see mods/README.md).

    python3 scripts/apply_mods.py --mods speed-dice[,other] [--out DIR]

For the selected mods it
  - gives every gate in their mod.json an observation-gate number (32..63) and writes dm/host/hle_mods_gen.h with MODGATE_<MOD>_<GATE> defines;
  - copies each mod's host.c to dm/host/hle_mod_<id>.c (the host Makefile takes hle_*.c by wildcard; a mod attaches its callbacks from a constructor, as hle_svc.c does);
  - writes DIR/patches.json, the gates for the rewriter (dm/tools/patch_nso.py reads it through DM_MODS_PATCHES);
  - merges the mods' romfs folders into DIR/romfs, the folder for DM_ROMFS_OVERLAY on the device.
With no --mods it removes what an earlier run left. In GitHub Actions it also exports DM_MODS_PATCHES through $GITHUB_ENV.
"""
import argparse, glob, json, os, re, shutil, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FIRST_GATE, LAST_GATE = 32, 63

def available():
    return sorted(os.path.basename(os.path.dirname(p)) for p in glob.glob(os.path.join(ROOT, 'mods', '*', 'mod.json')))

def ident(s): return re.sub(r'[^0-9A-Za-z]', '_', s).upper()

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--mods', default='', help='comma- or space-separated mod IDs (default: none)')
    ap.add_argument('--out', default=os.path.join(ROOT, 'build', 'mods'), help='where patches.json and romfs/ go')
    a = ap.parse_args()
    want = [m for m in re.split(r'[,\s]+', a.mods.strip()) if m]
    have = available()
    bad = [m for m in want if m not in have]
    if bad: sys.exit(f'[RED] unknown mod {", ".join(bad)}; available: {", ".join(have) or "none"}')
    host = os.path.join(ROOT, 'dm', 'host')
    for old in glob.glob(os.path.join(host, 'hle_mod_*.c')) + [os.path.join(host, 'hle_mods_gen.h')]:
        if os.path.exists(old): os.remove(old)
    shutil.rmtree(a.out, ignore_errors=True)
    if not want:
        print('no mods selected'); return
    os.makedirs(a.out)
    defs, obs, number = [], {}, FIRST_GATE
    for mid in sorted(set(want)):
        d = os.path.join(ROOT, 'mods', mid)
        with open(os.path.join(d, 'mod.json'), encoding='utf-8') as f: mod = json.load(f)
        assert mod['id'] == mid, f'mods/{mid}/mod.json names {mod["id"]}'
        nums = {}
        for g in mod.get('gates', []):
            if number > LAST_GATE: sys.exit(f'[RED] more than {LAST_GATE - FIRST_GATE + 1} gates selected')
            nums[g] = number; defs.append(f'#define MODGATE_{ident(mid)}_{ident(g)} {number}   // {mid}: {g}'); number += 1
        n_rows = 0
        for bid, rows in mod.get('obs', {}).items():
            if bid.startswith('_') or bid.startswith('EXAMPLE'): continue
            if not re.fullmatch(r'[0-9a-f]{32}', bid): sys.exit(f'[RED] {mid}: build ID {bid!r} is not 32 lowercase hex digits (the first 16 bytes of the module ID)')
            for r in rows:
                if r['gate'] not in nums: sys.exit(f'[RED] {mid}: row names the gate {r["gate"]!r}, which mod.json does not declare')
                if not re.fullmatch(r'[0-9a-f]{8}', r['word']): sys.exit(f'[RED] {mid}: word {r["word"]!r} is not 8 hex digits')
                obs.setdefault(bid, []).append([int(r['offset'], 0), nums[r['gate']] | int(r.get('param', 0)) << 8, r['word'], f'mod {mid}: {r.get("reason", r["gate"])}'])
                n_rows += 1
        shutil.copyfile(os.path.join(d, mod['host']), os.path.join(host, f'hle_mod_{ident(mid).lower()}.c'))
        romfs = os.path.join(d, mod.get('romfs', 'romfs'))
        if os.path.isdir(romfs):
            shutil.copytree(romfs, os.path.join(a.out, 'romfs'), dirs_exist_ok=True, ignore=shutil.ignore_patterns('.gitkeep'))
        print(f'mod {mid}: {len(nums)} gates, {n_rows} gate rows for builds {", ".join(sorted({b for b in mod.get("obs", {}) if re.fullmatch(r"[0-9a-f]{32}", b)})) or "none yet"}')
        if not n_rows: print(f'[warn] {mid} has no gate rows: it is built in but nothing will call it until mod.json has rows for your main\'s build ID')
    with open(os.path.join(host, 'hle_mods_gen.h'), 'w', encoding='utf-8') as f:
        f.write('// written by scripts/apply_mods.py; do not edit\n#pragma once\n' + '\n'.join(defs) + '\n')
    pj = os.path.abspath(os.path.join(a.out, 'patches.json'))
    with open(pj, 'w', encoding='utf-8') as f: json.dump({'OBS': obs}, f, indent=1)
    os.makedirs(os.path.join(a.out, 'romfs'), exist_ok=True)
    print(f'DM_MODS_PATCHES={pj}')
    if os.environ.get('GITHUB_ENV'):
        with open(os.environ['GITHUB_ENV'], 'a', encoding='utf-8') as f: f.write(f'DM_MODS_PATCHES={pj}\n')

if __name__ == '__main__': main()
