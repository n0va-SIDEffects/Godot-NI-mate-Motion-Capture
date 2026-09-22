# Die erste Messung: was ein Vollbild auf der Uhr wirklich kostet

Diese Seite ist die Anleitung fuer die Messung, die vor jeder weiteren
Designentscheidung steht. Sie dauert am Handgelenk etwa zehn Minuten.

## Warum zuerst

Das Konzeptpapier rechnet mit 30 Bildern pro Sekunde. Dafuer muesste fast jede
Terrainzeile jedes Bild neu geschrieben werden. Zwei Befunde aus den
PebbleOS-Quellen machen diese Annahme wacklig:

1. `graphics_release_frame_buffer` meldet **immer den ganzen Puffer** als
   schmutzig, und der Compositor ruft ohnehin `framebuffer_dirty_all`. Der im
   Konzept geplante Dirty-Row-Trick (Himmel-Caching, HUD-Band) spart
   Rechenzeit, aber **keine Uebertragung**. Als Bandbreitentrick faellt er weg.
2. Die Bildausgabe laeuft auf KernelMain, also auf **hoeherer Prioritaet als
   die App und als der Audio-Nachschub**. Der Audio-Nachschub der Firmware hat
   eine harte Frist von 32 ms ohne jede Reserve; wird sie gerissen, gibt es
   32 ms Stille, und gezaehlt wird das nirgends
   (Einzelheiten in `../schwebung/docs/firmware-befund.md`).

Das heisst: die Bildrate wird nicht vom Rasterizer begrenzt, sondern von der
Panel-Uebertragung, und jedes zusaetzliche Bild pro Sekunde geht spaeter dem
Ton vom Teller. Der Emulator misst das nicht: dort dauert ein Vollbild rund
5,4 ms, das ist die Rechenzeit von QEMU, nicht die Uebertragungszeit des
JDI-Panels.

## Messung A: das Messgeruest (schwebung, Bildschirm PANEL)

Das ist die saubere, vom Spiel unabhaengige Messung.

```bash
cd pebble/schwebung
pebble build
pebble install --cloudpebble       # echte Uhr ueber die Pebble-App, Dev Connect
pebble logs --cloudpebble | tee panel.log
```

Auf der Uhr: **langer Druck auf Select** schaltet den Bildschirm weiter, bis
`PANEL` oben steht. Dann:

1. **Select** startet 300 Bilder Vollbild (alle 228 Zeilen geaendert).
   Warten, bis die Zahl im Log steht.
2. **Select** noch einmal: 300 Bilder mit nur 10 geaenderten Zeilen.

Im Log stehen zwei Zeilen dieser Form:

```
[E1][PANEL] Vollbild:  300 Frames, 9871 ms, 32.9 ms/Frame, 30.3 fps, rend 1.2 ms
[E1][PANEL] 10 Zeilen: 300 Frames, 9840 ms, 32.8 ms/Frame, 30.4 fps, rend 0.1 ms
```

`rend` ist unsere eigene Rasterzeit, `ms/Frame` die Zeit, die das System
tatsaechlich pro Bild braucht. Die Differenz ist die Panel-Uebertragung plus
Compositor.

## Messung B: dieselbe Frage mit dem echten Renderer (bodeneffekt, MESSUNG)

**Ohne Entwicklungsrechner:** `bodeneffekt.pbw` mit der Pebble-App aufs Telefon
und von dort auf die Uhr. Der Messbildschirm zeigt alle Ergebnisse selbst an,
ein Log wird dafuer nicht gebraucht — abfotografieren genuegt.

Mit Toolchain und eingeschalteter Developer Connection geht auch:

```bash
cd pebble/bodeneffekt
pebble build
pebble login                                  # einmalig, fuer die CloudPebble-Verbindung
pebble install --cloudpebble --logs | tee bodeneffekt.log
```

**Back kurz** wechselt zum Bildschirm MESSUNG. Dort:

| Taste | Wirkung |
|---|---|
| Select | Test starten, danach reihum: Vollbild, 10 Zeilen, echte Voxel-Szene |
| Up | Strahlenzahl 200 (1 px je Spalte) <-> 100 (2 px je Spalte) |
| Down | Sichtweite 200 / 160 / 120 Zellen |

Nach jedem Test steht das Ergebnis **auf der Uhr**:

```
MESSUNG  200 St 200 Z
        Bild / rast
Voll  32.9 / 1.2 ms
10Z   32.8 / 0.1 ms
Voxel 34.1 / 8.6 ms
Ziel 25 fps (40ms)
Welt 12ms heap 74k
```

Links die Gesamtzeit je Bild, rechts davon unsere eigene Rasterzeit. Die Zeile
`Ziel` rechnet die Uhr aus der groesseren von Vollbild- und Voxelzeit aus, nach
genau der Tabelle weiter unten. Steht `rast` deutlich unter der Gesamtzeit,
begrenzt die Panel-Uebertragung und nicht der Renderer.

Laeuft ein Log mit, stehen dieselben Zahlen ausfuehrlicher darin:

```
[BE][PANEL] Vollbild:  300 Bilder, ... ms/Bild, ... fps, rast ... ms
[BE][PANEL] 10 Zeilen: ...
[BE][PANEL] Voxel:     ...
[BE][PANEL] Voxel mit 200 Strahlen, Sicht 200 Zellen, 20000 Schritte
```

Die dritte Zeile ist die eigentliche Antwort: so schnell laeuft das Spiel,
wenn es so schnell laufen darf wie es kann.

Dann Up druecken (100 Strahlen) und die Voxel-Variante noch einmal messen,
danach Down (Sichtweite 160, dann 120) und wieder messen. Am Ende stehen
sechs bis neun Zahlen im Log, und daraus folgt die Zielbildrate.

## Auswertung

| Vollbildzeit | Zielbildrate | Folge fuer das Konzept |
|---|---|---|
| unter 28 ms | 30 fps | Konzept haelt, Sichtweite 200 bleibt |
| 28 bis 40 ms | 25 fps (40 ms Raster) | Sichtweite auf 160 zuruecknehmen |
| ueber 40 ms | 20 fps (50 ms Raster) | Sichtweite 120, 100 Strahlen pruefen |

Sind Vollbild und 10 Zeilen **gleich schnell**, ist der Befund aus den Quellen
bestaetigt: es gibt keinen Zeilenvorteil beim Framebuffer-Direktzugriff, und
Himmel-Caching ist reine CPU-Ersparnis.

Liegt `rast` (unsere Rasterzeit) deutlich unter `ms/Bild`, ist nicht der
Renderer die Grenze, sondern die Uebertragung. Dann bringt jede weitere
Optimierung des Rasterizers nichts fuer die Bildrate, wohl aber Luft fuer den
Ton, der spaeter dazukommt.

## Was danach zu aendern ist

`RENDER_TICK_MS` in `src/c/config.h` auf das gemessene Raster setzen (33, 40
oder 50), und `SIGHT_FAR` entsprechend. Beides steht dort mit genau dieser
Begruendung als Kommentar.
