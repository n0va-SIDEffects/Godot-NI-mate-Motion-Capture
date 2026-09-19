# Toolchain: Einrichtung, Kommandos, Fallstricke

Alles in diesem Dokument wurde am **19.09.2026** in einer Linux-Container-Umgebung
(Ubuntu 24.04, ohne Bildschirm, ohne IPv6) tatsächlich durchlaufen. Versionen:
**pebble-tool 5.0.40**, **SDK 4.33.1**.

## 1. Installation

```bash
# uv, falls noch nicht vorhanden
curl -LsSf https://astral.sh/uv/install.sh | sh

uv tool install pebble-tool --python 3.13
export PATH="$HOME/.local/bin:$PATH"        # dauerhaft in .bashrc/.zshrc

pebble sdk install latest                   # lud hier 4.33.1
pebble sdk list                             # "(active)" muss dabeistehen
pebble sdk activate 4.33.1                  # falls nicht
```

**Python 3.13**, nicht 3.14 — pebble-tool unterstützt 3.14 noch nicht. uv holt
sich ein passendes Python selbst, wenn das System ein anderes hat.

Systempakete für den Emulator (Ubuntu 24.04; unter 24.04 heißen zwei Pakete
anders als in älteren Anleitungen):

```bash
sudo apt-get install -y nodejs npm libsdl2-2.0-0 libglib2.0-0t64 \
  libpixman-1-0 zlib1g libsndio7.0 libpng16-16t64
```

Windows hat keine native Unterstützung — WSL mit Ubuntu benutzen.

`pebble login` ist **nur** für Deployment auf echte Hardware über die Cloud und
fürs Veröffentlichen nötig. SDK, Build und Emulator laufen ohne Anmeldung.
Zugangsdaten landen in `~/.local/share/pebble-sdk/oauth_firebase/` und gehören
nicht in die Versionsverwaltung.

## 2. Die Kommandos

| Kommando | Zweck |
|---|---|
| `pebble new-project [--javascript\|--worker\|--alloy\|--simple] NAME` | Projekt anlegen |
| `pebble build` | kompiliert alle `targetPlatforms`, erzeugt `build/<name>.pbw` |
| `pebble clean` | Build-Verzeichnis leeren |
| `pebble install --emulator <plattform>` | in den Emulator installieren und starten |
| `pebble install --phone <IP>` | auf die echte Uhr, Developer Connection aktiv |
| `pebble install --logs` | installieren und gleich Logs mitlesen |
| `pebble logs [--emulator <p>]` | Logstrom (läuft, bis man abbricht) |
| `pebble screenshot --emulator <p> --no-open <datei>.png` | Bildschirmfoto in nativer Auflösung |
| `pebble emu-button --emulator <p> click\|push\|release <taste>` | Tasten |
| `pebble emu-accel --emulator <p> tilt-left\|tilt-right\|…\|none` | Lagesensor |
| `pebble emu-tap`, `emu-battery`, `emu-control` | Tippen, Akkustand, Sensorbrücke |
| `pebble transcribe "text"` | Diktat-Ergebnis einspeisen (auch `--error …`) |
| `pebble send-app-message --emulator <p> --string <nr>=<wert>` | Nachricht an die **JS-Seite** |
| `pebble data-logging list\|download` | Datalogging auslesen |
| `pebble gdb` | Debugger an den Emulator hängen |
| `pebble kill` | Emulatorprozesse beenden |
| `pebble wipe` | simulierten Flash löschen (auch App-Daten!) |
| `pebble sdk install\|list\|activate\|uninstall` | SDK-Verwaltung |
| `pebble login`, `pebble publish` | Konto und Store (siehe `pebble-publish`) |

## 3. Fallstricke

**Jedes Kommando mit `< /dev/null`.** Erstlauf- und Analyse-Rückfragen warten
sonst auf stdin. Symptom: keine Ausgabe, kein `build/`-Verzeichnis, Hänger bis
zum Timeout. Sieht aus wie ein kaputtes SDK, ist nur eine unbeantwortete Frage.

**`pebble sdk install` endet mit Code 1**, wenn die Version schon installiert
ist (`SDK X is already installed.`). In einem Skript unter `set -e` bricht der
Lauf dort ab — als Erfolg behandeln.

**Die SDK-Installation ist nicht atomar.** Ein Abbruch hinterlässt
`~/.local/share/pebble-sdk/SDKs/<version>/` mit `sdk-core`, aber ohne
`toolchain/`. pebble-tool meldet die Version trotzdem als installiert, aber der
ARM-Compiler und die Moddable-Werkzeuge fehlen; `pebble new-project --alloy`
scheitert dann mit *"The currently active SDK does not have Moddable tools."*
Reparatur: `pebble sdk uninstall <version>`, dann neu installieren.

**Installieren aktiviert nicht immer.** Zeigt `pebble sdk list` die Version ohne
`(active)`, verhält sich `pebble new-project`, als wäre kein SDK da →
`pebble sdk activate <version>`.

**Linker-Warnung ignorieren.** `warning: … has a LOAD segment with RWX
permissions` erscheint bei jedem Build und ist harmlos. Alle *anderen*
Warnungen ernst nehmen — das Standard-`wscript` setzt kein `-Werror`, der
Build läuft also auch mit echten Fehlern im Code durch.

**Emulator-Kaltstart.** Der erste `pebble install --emulator …` nach dem
Hochfahren kann mit `[Errno 61] Connection refused` scheitern, weil QEMU und
pypkjs noch nicht bereit sind. Einmal wiederholen.

**`qemu-pebble` dreht dauerhaft nahe 100 % CPU.** Nach jedem Test `pebble kill`.

**Hängender Emulator.** Wenn `screenshot` und `ping` in Timeouts laufen, obwohl
`install` noch funktioniert, ist der simulierte Flash (`qemu_spi_flash.bin`)
beschädigt. `pebble kill` allein reicht nicht: `pebble kill && pebble wipe`,
dann neu installieren. `wipe` löscht auch die gespeicherten App-Daten.

**Zustand des Emulators nachsehen:** `/tmp/pb-emulator.json` enthält PIDs und
Ports von QEMU und pypkjs. Der Pebble-Protokoll-Kanal nimmt genau einen
TCP-Client an — solange pypkjs dranhängt, scheitert jeder eigene Versuch.

## 4. Kopflos betreiben (Server, Container, Cloud)

Der Emulator öffnet ein SDL-Fenster und braucht deshalb ein X-Display. Hier
verifizierter Weg ohne Bildschirm:

```bash
sudo apt-get install -y xvfb libsdl2-2.0-0

# Ein DAUERHAFTES virtuelles Display, nicht xvfb-run:
nohup Xvfb :99 -screen 0 1024x768x24 >/dev/null 2>&1 &
export DISPLAY=:99
export NO_PROXY=localhost,127.0.0.1

pebble install --emulator emery < /dev/null
```

**Nicht `xvfb-run` benutzen.** Es beendet das Display, sobald das aufgerufene
Kommando fertig ist — und nimmt den gerade gestarteten Emulator mit. Das
`install` meldet noch `App install succeeded.`, der nächste `screenshot`
scheitert dann mit `[Errno 111] Connection refused`, was wie ein Netzwerk- oder
Patchproblem aussieht, aber keines ist. Ein einmal gestartetes `Xvfb :99` mit
gesetztem `DISPLAY` löst das.

Auf Hosts **ohne IPv6** (kein `/proc/sys/net/ipv6`) scheitert das mit
`[Errno 111] Connection refused`, obwohl QEMU läuft. Ursache: pypkjs bindet
seinen WebSocket an den leeren Host `("", port)`, gevent löst das nach IPv6 auf,
der Bind scheitert lautlos, und der Port, zu dem pebble-tool dann verbindet,
öffnet nie. Ein Zeichen ändern genügt:

```bash
SP="$HOME/.local/share/uv/tools/pebble-tool/lib/python3.13/site-packages"
sed -i 's/pywsgi.WSGIServer(("", self.port)/pywsgi.WSGIServer(("0.0.0.0", self.port)/' \
  "$SP/pypkjs/runner/websocket.py"
```

`scripts/setup_headless.sh` erledigt Prüfung und Patch idempotent. **Nach jedem
`uv tool install --force pebble-tool` erneut anwenden** — die Neuinstallation
überschreibt die Datei.

Danach läuft der volle Kreislauf: `install` → `emu-button` → `screenshot` →
`kill`. Hier geprüft mit beiden neuen Plattformen: `emery` liefert ein PNG in
200 × 228, `gabbro` eines in 260 × 260.

Für Screenshots im Store-Kontext ergänzt `pebble-publish`
(`scripts/emulator-headless.md`) weitere Kniffe, etwa einen Wrapper, der
SDL-Argumente aus dem QEMU-Aufruf entfernt.

## 5. Aufräumen

```bash
pebble kill < /dev/null
pkill -f "pebble transcribe"      # transcribe beendet sich oft nicht selbst
```

`pebble transcribe` hält die Verbindung offen; bleibt es hängen, verklemmt es
die Brücke zum Emulator, und danach hilft nur noch `kill && wipe`.
