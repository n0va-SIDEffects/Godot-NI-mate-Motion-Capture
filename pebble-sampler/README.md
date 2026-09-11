# Sampler für die Pebble Time 2

Ein Soundboard für den eingebauten Lautsprecher der Pebble Time 2. Alle Sounds
werden direkt auf der Uhr synthetisiert (kein einziges Audio-Asset), die App
ist deshalb winzig (~17 KB) und braucht keine Handy-Verbindung.

## Sounds

| Sound | Was man hört | Technik |
|---|---|---|
| Zufall | Ein zufälliger Sound aus der Liste | – |
| Applaus | Klatschende Menge mit An- und Abschwellen | PCM-Synthese |
| Tusch | Dreistimmige Fanfare „Ta-daaa!“ | Noten, 3 Tracks |
| Furz | Wabernder Brummton mit Flattern und Sputtern | PCM-Synthese |
| Rülpser | Kurz, tief, gurgelnd | PCM-Synthese |
| Fall | Lotusflöte abwärts, dann dumpfer Aufschlag | PCM-Synthese |
| Sad Trombone | „Wah wah wah waaah“ mit Vibrato am Ende | PCM-Synthese |
| Trommelwirbel | Beschleunigender Wirbel mit Crescendo und Becken | PCM-Synthese |
| Ba-Dum-Tss | Zwei Toms und ein Becken | PCM-Synthese |
| Boing | Sprungfeder mit abklingendem Wobbeln | PCM-Synthese |
| Buzzer | Verstimmtes „EHHH“ für falsche Antworten | PCM-Synthese |
| Ka-Ching | Klick plus Glockenpartials | PCM-Synthese |
| Level-Up | Schnelles aufsteigendes Arpeggio | Noten |
| Game Over | Zwei absteigende Stimmen | Noten, 2 Tracks |
| Lachen | Sechs absteigende „Ha“-Silben | PCM-Synthese |
| Grille | Drei Zirp-Gruppen für peinliche Stille | PCM-Synthese |
| Explosion | Übersteuertes Rauschen, das dunkler wird | PCM-Synthese |
| Sirene | Langsam pendelnder Sweep | PCM-Synthese |
| Katze | „Miau“ mit Formantverlauf | PCM-Synthese |
| Tröte | Airhorn, drei Stöße | PCM-Synthese |
| Pfiff | Wolf-Pfiff, hoch, Pause, hoch-runter | PCM-Synthese |
| Kuckuck | Kuckucksuhr | Noten |
| Laser | Pew pew pew | PCM-Synthese |
| Dun Dun Duuun | Dramatischer Sting, d-Moll nach E-vermindert | Noten, 3 Tracks |
| Klingel | Ding dong mit Oktav-Schimmer | Noten, 2 Tracks |
| Zauber | Zwei verschränkte Glitzer-Läufe | Noten, 2 Tracks |

## Bedienung

| Taste | Aktion |
|---|---|
| Hoch / Runter | Durch die Liste blättern |
| Select | Ausgewählten Sound abspielen (erste Zeile: Zufall) |
| Select lang | Menü: Stopp, Zufalls-Sound, Lautstärke (20–100 %), Schütteln an/aus |
| Uhr schütteln | Zufälligen Sound abspielen |
| Zurück | App beenden |

Lautstärke und Schüttel-Option werden gespeichert. Ist der Lautsprecher in den
Uhr-Einstellungen oder per Quiet Time stumm geschaltet, zeigt die Kopfzeile das
an und die Uhr vibriert kurz statt zu spielen.

## Bauen und installieren

Voraussetzung ist das aktuelle Pebble SDK (4.9 oder neuer, wegen der
Speaker-API). Installation laut https://developer.repebble.com/sdk/:

```bash
uv tool install pebble-tool
pebble sdk install latest
```

Dann im Ordner `pebble-sampler`:

```bash
pebble build
pebble install --phone <IP-der-Pebble-App>      # per Handy auf die Uhr
pebble install --emulator emery                 # oder im Emulator (ohne Ton)
```

Die fertige Datei liegt danach unter `build/pebble-sampler.pbw` und kann auch
direkt über die Pebble-App auf dem Handy installiert werden.

Die App ist auf die Plattform `emery` (Pebble Time 2) eingestellt. Für die
Pebble 2 Duo (`flint`, hat ebenfalls einen Lautsprecher) reicht es, den Namen
in `package.json` unter `targetPlatforms` zu ergänzen. Die UI-Farben fallen
dort automatisch auf Schwarz-Weiß zurück.

## Aufbau

```
src/c/synth.h, synth.c   Fixed-Point-Synthesizer (16 kHz, 16 Bit, ohne Floats)
src/c/sounds.c           Sound-Bank: Generatoren und Notensequenzen
src/c/player.c           Streaming-Pumpe für PCM plus Noten/Track-Wiedergabe
src/c/main.c             Menü, Action-Menü, Schütteln, Einstellungen
```

Jeder synthetisierte Sound ist eine Funktion, die pro Aufruf ein Sample
liefert. Die Streaming-Pumpe rendert 32-ms-Blöcke in den Speaker-Stream und
füllt per App-Timer nach, damit die Oberfläche flüssig bleibt.

Hinweis zur Klanggestaltung: Der Uhrlautsprecher ist winzig und gibt unter
etwa 300 Hz kaum etwas wieder. Die tiefen Sounds (Furz, Rülpser, Toms) leben
deshalb bewusst von Sägezahn- und Rechteck-Obertönen sowie Rauschanteilen.

## Neue Sounds hinzufügen

1. In `sounds.c` eine Funktion `static int32_t gen_xyz(Synth *s)` schreiben,
   die ein Sample (±32767) zurückgibt. Helfer wie `osc_saw`, `noise`, `lp1`,
   `ramp`, `ar_env` und `env_decay` sind in `synth.h` dokumentiert.
2. Am Ende der Tabelle `SOUNDS[]` einen Eintrag mit `SYNTH(...)` ergänzen
   (oder `NOTES(...)` / `TRACKS(...)` für Melodien).
3. `pebble build`.
