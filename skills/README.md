# Skills

Claude-Skills, die in diesem Repo abgelegt sind, damit sie sich von überall
herunterladen lassen.

## pebble-dev

Professionelle Entwicklung von Pebble-Watchapps und Watchfaces: Toolchain,
Projektaufbau, C-API, Speicherbudget, Plattformunterschiede, AppMessage,
Konfigurationsseiten, kopfloser Emulator und Absturzanalyse. Ergänzt die
bestehenden Skills `pebble-audio` (Tonausgabe) und `pebble-publish` (Store).

```
pebble-dev/
  SKILL.md
  references/   toolchain.md, platforms.md, c-api.md, communication.md,
                debugging.md, alloy.md
  assets/       main.c (geprüftes App-Gerüst), zwei Emulator-Screenshots
  scripts/      check_project.py, setup_headless.sh
```

**Installieren** (Ordner an den Ort kopieren, an dem Claude seine Skills sucht):

```bash
cp -r skills/pebble-dev ~/.claude/skills/
```

Oder unter Claude auf claude.ai den Ordner als Skill hochladen. Danach greift
der Skill automatisch, sobald es um Pebble-Entwicklung geht.

**Prüfskript einzeln benutzen**, ganz ohne Claude:

```bash
python3 skills/pebble-dev/scripts/check_project.py ~/pfad/zur/pebble-app
```
