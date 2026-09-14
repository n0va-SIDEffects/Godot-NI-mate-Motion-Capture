# SILBERKUGEL, Phase 1: Physik-Prototyp fuer die Pebble Time 2

Baubares Pebble-Projekt (C, SDK 4.33.1, Plattform `emery`) zu Phase 1 des
Konzeptpapiers `KONZEPT.md`. Es ist noch kein Spiel, sondern der Beweis, dass
das Spiel tragen kann. Zwei Fragen soll diese Phase beantworten:

1. Traegt die Physik bei der noetigen Bildrate?
2. Ist der Magnetfinger am Handgelenk spielbar, obwohl die Fingerkuppe rund
   ein Drittel der Bildbreite verdeckt? Das ist die Hauptkritik der Jury.

Was schon laeuft: Fixed-Point-Physik (Q20.12) mit festem Zeitschritt, ein
grauer Testtisch mit Banden, Bumper, Pfosten, Slingshots, Rueckfuehrungen,
Abschussbahn mit Einwegtor und zwei Outlanes, zwei rotierende Flipper mit
Winkelgeschwindigkeits-Uebertrag, Plunger als Zieh-Geste mit LRA-Ratsche und
als Taste, Magnetfinger ueber den Touch-Dienst mit Naehe-Geiger und Griff,
Nudge und Neigung aus dem Beschleunigungssensor mit Vibrationsmaske, ein
Tonstrom als Last, und ueberall Messwerte im Sekundenlog.

## Bauen und installieren

```bash
uv tool install pebble-tool        # einmalig
pebble sdk install latest          # einmalig (getestet mit 4.33.1)
pebble build
pebble install --emulator emery
pebble logs --emulator emery | tee p1.log
python3 tools/analyze_logs.py p1.log
```

Physik ohne Uhr und ohne Emulator pruefen (laeuft in zwei Sekunden durch):

```bash
make -C tools/hosttest run
```

Im kopflosen Container braucht der Emulator zwei Handgriffe, beide in
`docs/emulator-befund.md` beschrieben: einen QEMU-Wrapper ueber
`PEBBLE_QEMU_PATH`, der `-display` und `-audio` entfernt, und eine Zeile in
pypkjs, die den Websocket auf IPv4 bindet.

## Bedienung

Standard ist der **Daumen-Modus**, nicht der Zangengriff des Konzepts. Grund
ist eine Messung, kein Geschmack: Die Back-Taste liefert keine Rohevents
(siehe Befunde), damit laesst sich der linke Flipper nicht halten.

| Taste | SPIEL und MAGNET (Daumen-Modus) | MESS und PANEL |
|---|---|---|
| Up | linker Flipper (Halten moeglich) | lang: Bildschirm wechseln |
| Down | rechter Flipper (Halten moeglich) | Belegung umschalten, 2x Ton, 3x Log |
| Select | Magnetgriff; solange die Kugel in der Bahn liegt: Plunger | Aktion des Bildschirms |
| Back | Bildschirm wechseln; lang: App verlassen | App verlassen |
| Finger | Magnet, Plunger-Zug in der Abschussbahn | |

Im **Zangengriff** (im MESS-Bildschirm mit Down umschaltbar) liegt der linke
Flipper wie im Konzept auf Back, der rechte auf Down, der Plunger auf Up und
der Bildschirmwechsel auf langem Up. Der linke Flipper schlaegt dort erst beim
Loslassen der Taste und faellt nach 170 ms von selbst zurueck; Kugel fangen und
Post-Pass gehen damit nicht.

Vier Bildschirme:

- **SPIEL** Flipper auf dem grauen Testtisch. Select startet eine neue Kugel,
  wenn keine im Spiel ist.
- **MAGNET** Uebung zur Jury-Frage: Die Kugel soll mit dem Magnetfinger im
  Zielkreis gehalten werden, zwei Sekunden zaehlen als Treffer. Gemessen
  werden Ziele je Versuch, beste Haltezeit und der Anteil der Zeit, in der die
  Kugel unter der Fingerkuppe lag, also unsichtbar war. Select setzt die
  Zaehler zurueck.
- **PANEL** Vollbildzeit: 300 Frames so schnell wie moeglich, einmal mit allen
  228 Zeilen, einmal mit 10. Select startet, danach wechselt Select die
  Variante.
- **MESS** Zahlen und Umschalter. Select faehrt den Physik-Stresstest
  (2000 Substeps mit drei Kugeln und bewegten Flippern).

## Die Messungen und was sie zeigen muessen

### 1. Bildrate und Zeichenzeit (jeder Bildschirm)

Das Sekundenlog nennt `fps`, `rend` (Zeit im eigenen Rasterizer) und `gap`
(groesster Abstand zweier Spiel-Ticks, also wie lange der App-Task stand).
Ziel sind 20 bis 25 fps bei stehendem Ton. Der Hardware-Befund raet von
30 fps ab, weil die Bildausgabe auf hoeherer Prioritaet laeuft als der
Tonnachschub.

### 2. Vollbildzeit (PANEL)

Beide Varianten messen dasselbe Bild, einmal ganz und einmal 10 Zeilen. Sind
die Werte gleich, gibt es keinen Zeilenvorteil bei Framebuffer-Zugriff, und
der im Konzept geplante Dirty-Rect-Renderer spart nichts. Genau das sagen die
PebbleOS-Quellen voraus.

### 3. Tonnachschub unter Last (jeder Bildschirm)

`q` ist der geschaetzte Vorlauf im Systempuffer, `ur` sind Unterlaeufe,
`stau` Neustarts des Stroms. Der Ton traegt in Phase 1 nur einen Dauerton als
Last; er beantwortet die Frage, ob Physik, Rendern und Ton zusammen
durchhalten. Im MESS-Bildschirm laesst er sich mit Doppelklick auf Down
abschalten, um den Unterschied zu hoeren und zu messen.

### 4. Physikkosten (MESS, Select)

Der Stresstest rechnet 2000 Substeps mit drei Kugeln und bewegten Flippern und
gibt die Zeit je Substep aus, dazu Kontakte, Kollisionstests, Teilschritte und
Ausbrueche. Die Zahl der Tests je Substep ist die Groesse, mit der sich der
spaetere Tisch aus dem Konzept (rund 180 Segmente statt 22) hochrechnen laesst.

### 5. Magnetfinger (MAGNET)

Die einzige Messung, die zwingend die echte Uhr braucht, weil der Emulator
keinen Touch kennt. Interessant sind: Ziele je Versuch, beste Haltezeit und
`verdeckt`, der Anteil der Spielzeit, in dem die Kugel unter der Fingerkuppe
lag. Der Auswerter rechnet daraus den Verdeckungsanteil aus.

### 6. Tasten (jeder Bildschirm)

Die Zeile `[P1b]` zaehlt Rohevents je Taste. Sie beantwortet das erste Risiko
des Konzepts: ob Back als Flipperknopf taugt.

## Befunde, die das Konzept aendern

Alle im Emulator gemessen, die Quellen stehen dabei. Was die Uhr braucht, ist
als solches gekennzeichnet.

- **Back liefert keine Rohevents.** Mit `window_raw_click_subscribe` auf
  `BUTTON_ID_BACK` kommt kein einziges Down oder Up an (gemessen: `back=0/0`
  bei gleichzeitig `down=3/3` auf der Down-Taste). Ohne einen gewoehnlichen
  Click-Handler auf Back verlaesst das System ausserdem die App. Und
  `window_single_repeating_click_subscribe` auf Back faengt den Systemgriff
  gar nicht erst ab: Die App wird beendet, der Handler feuert nie. Damit ist
  der Zangengriff des Konzepts nur eingeschraenkt moeglich, und der
  Daumen-Modus wird zum Standard. **Auf der Uhr gegenpruefen**, ob sich Back
  dort ebenso verhaelt.
- **Kein Zeilenvorteil beim Zeichnen.** 300 Frames Vollbild: 5,5 ms je Frame.
  300 Frames mit nur 10 geaenderten Zeilen: ebenfalls 5,5 ms. Die Erwartung
  aus den PebbleOS-Quellen ist damit im Emulator bestaetigt; der Dirty-Rect-
  Renderer aus dem Konzept kann entfallen. Die absolute Zahl gilt nur fuer
  QEMU, auf der Uhr misst dieselbe Taste den echten Wert.
- **Vollgas-Rendern bricht den Ton.** Waehrend des Panel-Tests (Frames so
  schnell wie moeglich) lief der Tonstrom zweimal leer und musste neu
  geoeffnet werden, 35 Unterlaeufe. Bei den normalen 25 fps bleibt der Vorlauf
  stabil bei 160 bis 175 ms und `ur` bei 0. Das ist genau die Reihenfolge aus
  dem Hardware-Befund: Die Bildausgabe hat Vorrang vor dem Tonnachschub.
- **Der Magnet des Konzepts traegt nur unter der Fingerkuppe.** Mit den Werten
  des Konzeptpapiers (weicher Kern 14 px, Spitze 2400 px/s²) haelt der Magnet
  die Kugel gegen die Schwerkraft nur bis 21 px Abstand, waehrend die
  Fingerkuppe rund 40 px verdeckt: Tragen waere ausschliesslich blind
  moeglich, genau die Kritik der Jury. Mit einem weichen Kern in Kuppengroesse
  (40 px) und flacherer Spitze (1600 px/s², rund das 2,3-Fache der
  Schwerkraft) liegt die Traggrenze bei 45 px, also am sichtbaren Rand der
  Kuppe. Nachgerechnet und geprueft in `tools/hosttest`.
- **Reibung je Kontakt ist falsch.** Ein fester Prozentsatz je Kontakt bremst
  eine an der Bande anliegende Kugel mit der Substep-Rate statt mit der
  Physik: Die abgeschossene Kugel kam nur bis zur halben Tischhoehe und fiel
  in die Abschussbahn zurueck. Jetzt begrenzt der Normalimpuls die Reibung
  (Coulomb), und der Abschuss traegt auch bei geneigtem Tisch.
- **Ein Testtisch ohne Rueckfuehrungen ist unspielbar.** Ohne die beiden
  Schraegen an den Seitenwaenden rollt jede Kugel aus dem oberen Bogen
  geradewegs in die Outlane; ein Ball war nach rund einer Sekunde weg. Mit
  ihnen erreichen alle 30 Startpunkte des Pruefstands die Flipperzone, im
  Mittel nach 1,1 s.
- **Der Emulator kennt keinen Touch.** Im QEMU-Protokoll gibt es Tasten,
  Beschleunigung, Klaps, Kompass und Batterie, aber kein Touch-Paket. Alles
  zum Magnetfinger, zur Zieh-Geste des Plungers und zum Naehe-Geiger muss auf
  der Uhr gemessen werden.
- **Beschleunigung und Klaps im Emulator.** `pebble emu-accel tilt-left`,
  `tilt-right` und `gravity+x` kommen an und erzeugen Hochpass-Stoesse und
  Neigung wie erwartet. `pebble emu-tap` erreichte die App in keinem Versuch;
  ob der Tap-Dienst auf der Uhr traegt, ist offen.

## Physik-Pruefstand

`tools/hosttest` uebersetzt `physics.c` und `table.c` mit einem Mini-Ersatz
fuer `pebble.h` auf dem Rechner und prueft in zwei Sekunden, was auf der Uhr
Stunden kostet:

```
Kein Tunneling bei 200 bis 1500 px/s             16 Richtungen, 14 Tempi
Abschuss verlaesst die Bahn und bleibt draussen  jedes Tempo von 520 bis 980 px/s
Abschuss traegt auch an der Bande und bei Neigung sieben Neigungen, acht Startpunkte
Einwegtor laesst keine Kugel zurueck in die Bahn rund 6000 Fluege
Kugeln von oben erreichen die Flipper            30 Startpunkte
Flipper uebertraegt Winkelgeschwindigkeit        55 px/s werden zu 835 px/s
Gleiche Eingaben, gleiche Bahn (Fixed-Point)     zwei Laeufe, bitgleich
Magnet traegt bis ausserhalb der Fingerkuppe     Traggrenze 45 px, Kuppe 40 px
```

Der Determinismus-Test ist mehr als eine Formalie: Auf ihm beruhen Replay,
Tagestisch und Ghost-Duell aus dem Konzept. Faellt er, ist irgendwo eine
Gleitkommazahl oder ein uninitialisierter Wert in die Physik geraten.

## Aufbau

| Datei | Inhalt |
|---|---|
| `src/c/fixed.h` | Q20.12 in int32: Multiplikation, Division, Wurzel, Vektoren, Drehung |
| `src/c/table.c` | grauer Testtisch als Segment- und Kreisliste, Flipperdrehpunkte |
| `src/c/physics.c` | fester Zeitschritt, Teilschritte gegen Tunneling, Kontakte mit Coulomb-Reibung, Flipper als rotierende Kapsel, Magnetfeld |
| `src/c/render.c` | Framebuffer-Direktzugriff: Banden, Scheiben, Kapseln, Kugel mit Spur, Magnetring, Umriss der Fingerkuppe, Panel-Test |
| `src/c/input.c` | Touch als Rohereignis: Magnetposition, Zieh-Geste des Plungers mit Ratsche, Wurfgeschwindigkeit aus den letzten drei Positionen |
| `src/c/nudge.c` | Beschleunigung: Hochpass als Stoss, Tiefpass als Neigung, Tilt-Bob, Klaps, Vibrationsmaske |
| `src/c/haptics.c` | LRA-Einzelimpulse ohne `vibes_cancel`, Naehe-Geiger, Zaehlung verworfener Impulse |
| `src/c/audio.c` | PCM-Strom 16 kHz/16 Bit mit Fuellstandsbuchhaltung, aus `pebble/schwebung` uebernommen |
| `src/c/tone.c` | Dauerton als Last, kein Spielklang |
| `src/c/e1clock.c` | monotone Millisekundenuhr, aus `pebble/schwebung` uebernommen |
| `src/c/game.c` | Ballfluss, Plunger, Magnetgriff, Ladung, Magnet-Uebung, Statistik |
| `src/c/main.c` | Fenster, Bildschirme, Tasten, Timer, Sekundenlog |
| `tools/hosttest/` | Physik-Pruefstand auf dem Rechner |
| `tools/analyze_logs.py` | wertet `pebble logs` aus |

## Was als Naechstes auf die Uhr muss

1. Back gegenpruefen: Verhaelt sich die echte Uhr wie der Emulator, bleibt es
   beim Daumen-Modus.
2. Magnet-Uebung spielen und `verdeckt`, Haltezeit und Ziele ablesen. Erst
   diese Zahlen beantworten die Jury-Kritik.
3. Vollbildzeit im PANEL-Bildschirm messen. Sie entscheidet ueber 20, 25 oder
   30 fps.
4. Physik-Stresstest im MESS-Bildschirm: die Zeit je Substep auf echter
   Hardware, hochgerechnet auf 180 Segmente.
5. LRA-Trennschaerfe: Ob der Motor Impulse von 8 bis 15 ms in schneller Folge
   ueberhaupt trennt, sagt keine Software, sondern das Handgelenk.
