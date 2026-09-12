#!/usr/bin/env python3
"""Schnürt den Release-Ordner store/release/ und optional das ZIP.

Aufruf aus dem Projektordner:  python3 store/make_release.py [--zip]
Erwartet: build/*.pbw (pebble build), store/icon/icon_{80,144,48}.png,
store/banner/banner_720x320_{en,de}.png, store/screenshots_<plattform>/,
store/release/description_en.txt + beschreibung_de.txt, store/RELEASE_NOTES.md,
store/VEROEFFENTLICHEN.md.
"""
import glob, json, os, shutil, sys, zipfile

pk = json.load(open('package.json'))
name = pk['pebble'].get('displayName', pk['name']).replace(' ', '')
ver = pk['version']
if ver.count('.') != 1:
    sys.exit(f"version '{ver}' muss Major.Minor sein (Store-Regel)")
rel = 'store/release'
os.makedirs(rel, exist_ok=True)
pbw = glob.glob('build/*.pbw')
if not pbw:
    sys.exit('kein build/*.pbw, erst pebble build')
for old in glob.glob(f'{rel}/*.pbw'):
    os.remove(old)
shutil.copy(pbw[0], f'{rel}/{name}-{ver}.pbw')
for src in ('store/icon/icon_80.png', 'store/icon/icon_144.png', 'store/icon/icon_48.png',
            'store/banner/banner_720x320_en.png', 'store/banner/banner_720x320_de.png',
            'store/RELEASE_NOTES.md'):
    shutil.copy(src, rel) if os.path.exists(src) else print('fehlt:', src)
for d in sorted(glob.glob('store/screenshots_*')):
    dst = os.path.join(rel, os.path.basename(d))
    shutil.rmtree(dst, ignore_errors=True)
    shutil.copytree(d, dst)
print('Release-Ordner:', sorted(os.listdir(rel)))
if '--zip' in sys.argv:
    z = f'store/{name}_Store_Paket.zip'
    with zipfile.ZipFile(z, 'w', zipfile.ZIP_DEFLATED) as zf:
        for root, _, files in os.walk(rel):
            for f in files:
                p = os.path.join(root, f)
                zf.write(p, os.path.relpath(p, 'store'))
        zf.write('store/VEROEFFENTLICHEN.md', 'VEROEFFENTLICHEN.md')
    print('ZIP:', z, os.path.getsize(z), 'Bytes')
