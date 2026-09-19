# Testen und Debuggen

## 1. Logs

```c
APP_LOG(APP_LOG_LEVEL_DEBUG, "Index jetzt %d", i);
// Ausgabe: [INFO    ] D main.c:20 Index jetzt 0
```

Stufen: `APP_LOG_LEVEL_ERROR`, `_WARNING`, `_INFO`, `_DEBUG`, `_DEBUG_VERBOSE`.
`console.log()` aus PebbleKit JS erscheint im selben Strom.

```bash
pebble logs --emulator emery         # laeuft, bis man abbricht
pebble logs --phone 192.168.1.25     # echte Uhr
pebble install --logs                # installieren und gleich mitlesen
```

**Logging kostet.** Es läuft über dieselbe Bluetooth-Verbindung wie AppMessage,
bremst sie aus und zieht Strom. Nicht in Timer-Callbacks, nicht in engen
Schleifen, im Release abschalten oder hinter einen Schalter legen:

```c
#define LOG(...) do { if (DEBUG) APP_LOG(APP_LOG_LEVEL_DEBUG, __VA_ARGS__); } while (0)
```

## 2. Abstürze lesen

Ein Absturz sieht im Log so aus:

```
[INFO] E ault_handling.c:77 App fault! {f23aecb8-…} PC: 0x8016716 LR: 0x8016713
```

`PC` ist die Stelle, an der es knallte, `LR` die Rücksprungadresse, also meist
der Aufrufer. Mit der `.elf`-Datei aus `build/<plattform>/` wird daraus eine
Zeilennummer. Der zuverlässige Weg:

```bash
pebble gdb --emulator emery          # laedt Symbole selbst
(gdb) info symbol 0x…
(gdb) bt
```

Wenn nur das Log vorliegt, geht es auch direkt über die Binutils der Toolchain
(hier mit SDK 4.33.1 geprüft):

```bash
TC=~/.local/share/pebble-sdk/SDKs/current/toolchain/arm-none-eabi/bin
$TC/arm-none-eabi-addr2line -f -e build/emery/pebble-app.elf 0x1b4
# update_time
# /pfad/src/c/main.c:48
```

Die Adressen aus dem Log enthalten die Ladeadresse der App; gesucht ist der
**Versatz innerhalb der App**. `pebble gdb` nimmt einem diese Rechnung ab —
deshalb im Zweifel damit arbeiten.

Symbolübersicht und Speicherfresser im Code:

```bash
$TC/arm-none-eabi-nm -S --size-sort build/emery/pebble-app.elf | tail -20
```

`pebble analyze-size` gibt es ebenfalls, lieferte in dieser Umgebung aber nur
Nullen. Verlässlich ist der Speicherbericht, den `pebble build` ohnehin
ausgibt.

### Die drei üblichen Ursachen

1. **`NULL`-Zeiger** — ein `*_create()` ist fehlgeschlagen (meist Speichermangel)
   oder wurde vergessen. Rückgabewerte prüfen.
2. **Zugriff hinter dem Array-Ende** — Schleifengrenze passt nicht zur Größe.
   Größe einmal als Konstante definieren und überall verwenden oder
   `ARRAY_LENGTH()` benutzen.
3. **Zeiger auf Zerstörtes** — ein Layer wurde im `window_unload` freigegeben,
   ein Callback benutzt ihn weiter. Zeiger nach dem `destroy` auf `NULL` setzen
   und vor Gebrauch prüfen.

## 3. Speicherlecks finden

Beim Beenden schreibt das System eine Heap-Statistik ins Log. Vorgehen:

1. App starten, beenden, Wert notieren.
2. Zehnmal wiederholen.
3. Wächst der belegte Anteil, fehlt ein `*_destroy` — fast immer in
   `window_unload`.

Zusätzlich im Code messen:

```c
APP_LOG(APP_LOG_LEVEL_INFO, "Heap frei: %u", (unsigned)heap_bytes_free());
```

An zwei Stellen aufrufen (nach `init`, nach dem Öffnen des verdächtigen
Fensters) und die Differenz ansehen.

## 4. Emulator steuern

Alle Kommandos brauchen `--emulator <plattform>` und `< /dev/null`.

| Kommando | Zweck |
|---|---|
| `emu-button click\|push\|release <taste>` | Tasten; `back`, `up`, `select`, `down` |
| `emu-accel tilt-left\|tilt-right\|tilt-forward\|tilt-back\|none` | Lage |
| `emu-tap` | Tippen auf das Gehäuse |
| `emu-battery --percent 20 [--charging]` | Akkustand |
| `emu-bt-connection --connected no` | Verbindung trennen |
| `emu-compass --heading 90` | Kompass |
| `emu-set-time`, `emu-time-format 24h` | Uhrzeit und Format |
| `emu-set-timeline-quick-view on` | verdeckten Bereich testen |
| `emu-set-content-size large` | große Systemschrift testen |
| `emu-steps`, `emu-sleep`, `emu-heart-rate` | Health-Werte (Puls nur `emery`) |
| `emu-app-config` | Einstellungsseite öffnen |
| `transcribe "text"` | Diktat-Ergebnis einspeisen |

**Langes Drücken** braucht zwei Aufrufe:

```bash
pebble emu-button --emulator emery push select < /dev/null
sleep 1
pebble emu-button --emulator emery release select < /dev/null
```

`click --duration` erzeugt **kein** echtes Halten — ein
`window_long_click_subscribe`-Handler feuert damit nie.

**`emu-set-timeline-quick-view on` und `emu-set-content-size large` sind die
zwei am häufigsten vergessenen Tests.** Beide zerlegen Layouts, die mit festen
Pixelwerten gebaut wurden.

## 5. Screenshots

```bash
pebble screenshot --emulator emery --no-open shot.png < /dev/null
```

Liefert die native Auflösung (geprüft: `emery` 200 × 228, `gabbro` 260 × 260).

**Das Bild ansehen, nicht nur erzeugen.** Der häufigste stille Fehler ist ein
Screenshot des Watchface statt der App: Nach einem `install` kehrt der Emulator
unter Umständen zum Zifferblatt zurück, und man fotografiert „Install an app to
continue". Also direkt nach dem `install` knipsen und das Ergebnis prüfen.

## 6. Was der Emulator nicht kann

| | Emulator | Echte Uhr |
|---|---|---|
| Abstürze, Layout, Logik | ja | ja |
| Speicherdruck | nein, viel großzügiger | **hier zeigt es sich** |
| Ton | **nein** | ja (siehe `pebble-audio`) |
| Sensoren | nur simuliert | echt |
| Bluetooth-Timing, AppMessage-Durchsatz | grob | echt |
| Akkuverhalten | nein | **nur hier messbar** |

Auf die Uhr kommt die App per `pebble install --phone <IP>` bei aktivierter
Developer Connection in der Handy-App, oder als `.pbw` per Sideload.

## 7. Komfort: Editor mit Codeverständnis

```bash
pebble compile-commands < /dev/null
```

erzeugt `compile_commands.json`. Damit kennen clangd, VS Code und CLion die
SDK-Includes und Plattform-Defines — Autovervollständigung und Sprung zur
Definition funktionieren, und `PBL_*`-Zweige werden korrekt ausgewertet.
