# Der emery-Emulator im kopflosen Container

Gesammelt beim Bauen von Phase 1. Zwei Teile: was noetig war, damit der
Emulator ueberhaupt laeuft, und was er ueber die Uhr verraet.

## Damit er laeuft

**QEMU ohne Bildschirm.** `pebble install --emulator emery` startet QEMU mit
`-display` und `-audio`. Ohne X-Server und ohne Audiogeraet bricht das ab. Ein
Wrapper, der beide Optionen entfernt und `-display none` anhaengt, loest es;
aktiviert wird er ueber `PEBBLE_QEMU_PATH`:

```python
#!/usr/bin/env python3
import os, sys
REAL = "/root/.local/share/pebble-sdk/SDKs/4.33.1/toolchain/bin/qemu-pebble"
args, out, i = sys.argv[1:], [], 0
while i < len(args):
    if args[i] in ("-display", "-audio", "-audiodev", "-soundhw"):
        i += 2
        continue
    out.append(args[i]); i += 1
os.execv(REAL, [REAL] + out + ["-display", "none"])
```

Dazu `SDL_VIDEODRIVER=dummy`, `SDL_AUDIODRIVER=dummy` und die Systempakete
fuer SDL2 (`libsdl2-2.0-0`), die `qemu-pebble` auch ohne Fenster laden will.

**pypkjs bindet auf IPv6.** Der Telefonsimulator oeffnet seinen Websocket mit
`pywsgi.WSGIServer(("", port), ...)`. In einem Container ohne IPv6 scheitert
das mit `OSError: [Errno 97] Address family not supported by protocol`, und
zwar still: Der Prozess laeuft weiter, lauscht aber nicht. `pebble install`
meldet dann nur `[Errno 111] Connection refused`. Eine Zeile in
`site-packages/pypkjs/runner/websocket.py` behebt es:

```python
self.server = pywsgi.WSGIServer(("127.0.0.1", self.port), ...)
```

**Weitere Stolpersteine.** `NO_PROXY=localhost,127.0.0.1` setzen, sonst laufen
die lokalen Verbindungen in den Proxy. Nach einem Absturz bleibt
`/tmp/pb-emulator.json` mit toten Prozessnummern liegen; loeschen. Mehrere
gleichzeitige Verbindungen (`pebble logs` plus `pebble screenshot` plus
`emu-button`) bringen pypkjs gelegentlich durcheinander, dann hilft nur ein
Neustart von QEMU und pypkjs. Ein ungueltiges Argument an `emu-accel` reisst
die Verbindung ebenfalls ab.

**Screenshots ohne pypkjs.** Der QEMU-Monitor kann `screendump datei.ppm`,
was die Log-Verbindung nicht stoert. Achtung: Dieses Bild hat andere Farben
als `pebble screenshot` (Graustufen statt der echten Palette), es taugt zur
Kontrolle, nicht als Beleg.

## Was er ueber die Uhr verraet

**Tasten.** Gemessen mit Zaehlern je Taste in der App:

| Abo | Back | Down, Up, Select |
|---|---|---|
| `window_raw_click_subscribe` | kein einziges Ereignis | Down und Up zuverlaessig |
| `window_single_click_subscribe` | Handler laeuft, App bleibt offen | wie erwartet |
| `window_single_repeating_click_subscribe` | App wird beendet, Handler feuert nie | wie erwartet |

Folge fuer das Konzept: Der linke Flipper kann auf Back nicht gehalten werden.
Der Zangengriff bleibt als Option mit Auto-Release, Standard ist der
Daumen-Modus. Auf der echten Uhr gegenpruefen.

**Bildausgabe.** 300 Frames Vollbild und 300 Frames mit 10 geaenderten Zeilen
kosten dasselbe: 5,5 ms je Frame. Der Framebuffer-Zugriff bringt also keinen
Zeilenvorteil, wie es die PebbleOS-Quellen vorhersagen. Der absolute Wert gilt
nur fuer QEMU.

**Ton unter Last.** Bei 25 fps bleibt der Vorlauf stabil bei 160 bis 175 ms,
Unterlaeufe 0. Waehrend des Panel-Tests, der Frames so schnell wie moeglich
zeichnet, lief der Strom zweimal leer (`Stau`, Neustart) und sammelte
35 Unterlaeufe. Die Reihenfolge der Prioritaeten aus dem Hardware-Befund ist
damit sichtbar.

**Beschleunigung.** `emu-accel tilt-left`, `tilt-right`, `gravity+x` kommen an
und liefern Hochpass-Stoesse und Neigung. Der Sprung des Emulators ist
allerdings ein voller g-Sprung in einem Sample, haerter als jede Handbewegung;
Schwellen lassen sich daran nicht einstellen. `emu-tap` erreichte die App in
keinem Versuch.

**Touch.** Das QEMU-Protokoll von libpebble2 kennt Pakete fuer Tasten,
Beschleunigung, Klaps, Kompass, Batterie, Vibration und Zeitformat, aber
keines fuer Touch. Der Magnetfinger, die Zieh-Geste des Plungers und der
Naehe-Geiger sind im Emulator nicht pruefbar.

**Physik.** Der Stresstest (2000 Substeps, drei Kugeln, bewegte Flipper)
brauchte in QEMU 25 bis 29 Mikrosekunden je Substep bei rund 145
Kollisionstests. Das ist eine Hausnummer fuer die Verhaeltnisse, keine
Aussage ueber den Cortex-M33 der Uhr.
