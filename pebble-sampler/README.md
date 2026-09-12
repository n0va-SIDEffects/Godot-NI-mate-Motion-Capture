# Sampler für die Pebble Time 2

Ein Soundboard für den eingebauten Lautsprecher der Pebble Time 2: echte
Aufnahmen für die Geräusche, synthetisierte Jingles für den Rest. Die App
braucht keine Handy-Verbindung.

## Sounds

Die „realistischen“ Geräusche sind echte, gemeinfreie Aufnahmen von
Wikimedia Commons (Nachweise in `resources/samples/ATTRIBUTION.md`), die
Jingles werden auf der Uhr synthetisiert.

| Sound | Was man hört | Technik |
|---|---|---|
| Zufall | Ein zufälliger Sound aus der Liste | – |
| (Handy-Samples) | Vom Handy geladene Clips, siehe unten | ADPCM im RAM |
| Applaus | Echter Konzertsaal-Applaus | Aufnahme |
| Furz | Pardon. | Aufnahme |
| Rülpser | Wohl bekomm's | Aufnahme |
| Explosion | Kabumm | Aufnahme |
| Trommelwirbel | Wirbel mit Becken-Finale | Aufnahme (gemischt) |
| Ba-Dum-Tss | Zwei Snare-Schläge und Becken | Aufnahme (gemischt) |
| Ka-Ching | Registrierkasse | Aufnahme |
| Katze | Siamkatze, Miau | Aufnahme |
| Tröte | Diesellok-Horn | Aufnahme |
| Sirene | Feuerwehr-Motorsirene | Aufnahme |
| Buzzer | Falsch! | Aufnahme |
| Gong | Vorstellung beginnt | Aufnahme |
| Klospülung | Weg damit | Aufnahme |
| Hahn | Kikeriki | Aufnahme |
| Niesen | Hatschi | Aufnahme |
| Lachen | Ha ha ha | Aufnahme |
| Elefant | Töröö | Aufnahme |
| Pferd | Wiehern | Aufnahme |
| Sad Trombone | „Wah wah wah waaah“ mit Vibrato | PCM-Synthese |
| Level-Up | Schnelles aufsteigendes Arpeggio | Noten |
| Game Over | Zwei absteigende Stimmen | Noten, 2 Tracks |
| Tusch | „Ta-daaa!“ mit Echo-Stimme | Noten, 2 Tracks |
| Grille | Drei Zirp-Gruppen für peinliche Stille | PCM-Synthese |
| Pfiff | Wolf-Pfiff | PCM-Synthese |
| Laser | Pew pew pew | PCM-Synthese |
| Klingel | Ding dong mit Oktav-Schimmer | Noten, 2 Tracks |
| Tetris | Korobeiniki im Game-Boy-Stil, Lead plus Bass | Noten, 2 Tracks |

## Bedienung

| Taste | Aktion |
|---|---|
| Hoch / Runter | Durch die Liste blättern |
| Select | Ausgewählten Sound abspielen (erste Zeile: Zufall) |
| Select lang | Menü: Stopp, Zufalls-Sound, Lautstärke (20–100 %), Schütteln an/aus |
| Touchscreen | Wischen blättert, Antippen spielt ab (Pebble Time 2) |
| Uhr schütteln | Zufälligen Sound abspielen |
| Zurück | App beenden |

## Einstellungen in der Handy-App

In der Pebble-App auf dem Handy hat die App eine Einstellungsseite
(Zahnrad-Symbol): Lautstärke, Schütteln an/aus, Touch-Bedienung an/aus und
bis zu vier **Handy-Samples**. Die Einstellungen werden beim Speichern an
die Uhr geschickt und dort gemerkt.

### Handy-Samples

Die Uhr hat nur 4 KB dauerhaften Speicher pro App, eigene Samples können
also nicht auf der Uhr gespeichert werden. Stattdessen lädt die Handy-App
bei jedem Start der Uhr-App bis zu vier Samples von konfigurierten URLs und
schiebt sie in den Arbeitsspeicher der Uhr (max. 3 Sekunden je Sample). Sie
erscheinen oben in der Liste, zuerst mit Fortschrittsanzeige, dann als
„Handy-Sample“. So geht's:

```bash
python3 tools/import_sample.py meinclip.mp3 --max-seconds 3 --export chef.ima
```

Die `.ima`-Datei unter einer https-Adresse ablegen (Dropbox-Link mit
`dl=1`, GitHub raw, eigener Webspace) und in den Einstellungen Name und URL
eintragen, speichern. Die Übertragung dauert je nach Bluetooth-Verbindung
einige Sekunden pro Sample.

### Aufnehmen mit der Uhr

Geht leider nicht: Das Pebble SDK bietet für das Mikrofon nur die
Diktier-Funktion (Sprache zu Text über das Handy), keinen Zugriff auf
Rohaudio. Eigene Aufnahmen entstehen deshalb am Handy oder Rechner und
kommen als Handy-Sample oder fest eingebautes Sample in die App.

Lautstärke und Schüttel-Option werden gespeichert. Ist der Lautsprecher in den
Uhr-Einstellungen oder per Quiet Time stumm geschaltet, zeigt die Kopfzeile das
an und die Uhr vibriert kurz statt zu spielen.

## Eigene Samples (Suno, Freesound, Aufnahmen)

Manche Geräusche klingen als echte Aufnahme einfach besser. Die App kann
deshalb zusätzlich Audio-Samples aus dem Flash streamen. Der Import geht
mit einem Befehl:

```bash
python3 tools/import_sample.py meinclip.mp3 --name "Applaus" --hint "Echte Menge"
pebble build
```

Das Werkzeug wandelt den Clip per ffmpeg in Mono 16 kHz, entrauscht ihn
(`--denoise`, Standard 8 dB), filtert Bässe unter 150 Hz heraus (die der
Lautsprecher ohnehin nicht wiedergibt), schneidet Stille weg, kürzt auf
`--max-seconds` (Standard 4 s), normalisiert und speichert ihn als IMA-ADPCM
(4 Bit pro Sample, also 8 KB pro Sekunde; die Uhr dekodiert das zu 16 Bit).
Anschließend steht das Sample in `package.json` und `src/c/samples.inc` und
erscheint ganz oben in der Liste. `ffmpeg` sollte installiert sein; ohne
ffmpeg geht nur WAV-Eingabe mit einfacherem Resampler. Mit `--start 2.5`
lässt sich ein Ausschnitt wählen, `--replace` überschreibt ein vorhandenes
Sample, `--color GColorRedARGB8` setzt die Menüfarbe.
`python3 tools/import_sample.py --help` zeigt alle Optionen.

Budget: Eine Pebble-App darf insgesamt 256 KB Ressourcen haben. Die
mitgelieferten Samples belegen davon schon rund 230 KB; für ein neues Sample
muss also ein altes weichen (Zeile in `src/c/samples.inc` und Eintrag in
`package.json` löschen, `.ima` entfernen). Zur Orientierung: 256 KB, das sind
rund 30 Sekunden ADPCM-Audio bei 16 kHz. Das Werkzeug zeigt nach jedem
Import den Füllstand an.

Tipp für Suno: Kurze Prompts wie „sound effect only, no music, crowd
applause, 3 seconds“ liefern brauchbare Clips; der Free Plan erlaubt nur
private Nutzung. Alternativ liefert freesound.org viele CC0-Geräusche.

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
src/c/ima_adpcm.h        IMA-ADPCM-Decoder für die Samples
src/c/main.c             Menü, Action-Menü, Schütteln, Touch, Einstellungen
src/c/phone.c            AppMessage: Einstellungen und Sample-Übertragung vom Handy
src/pkjs/index.js        Handy-Seite: Clay-Einstellungen, Sample-Download und -Übertragung
src/pkjs/config.js       Aufbau der Einstellungsseite
src/c/samples.inc        Liste importierter Samples (vom Import-Werkzeug gepflegt)
tools/import_sample.py   Audio-Clip -> Sample-Ressource
```

Jeder synthetisierte Sound ist eine Funktion, die pro Aufruf ein Sample
liefert. Die Streaming-Pumpe rendert 32-ms-Blöcke in den Speaker-Stream und
füllt per App-Timer nach, damit die Oberfläche flüssig bleibt.

Hinweis zur Klanggestaltung: Der Uhrlautsprecher ist winzig und gibt unter
etwa 300 Hz kaum etwas wieder. Rein synthetische Geräusche wirken darauf
schnell künstlich, tonale Jingles funktionieren dagegen sehr gut. Deshalb
sind die Geräusche echte Aufnahmen und die Jingles synthetisch.

## Neue Sounds hinzufügen

1. In `sounds.c` eine Funktion `static int32_t gen_xyz(Synth *s)` schreiben,
   die ein Sample (±32767) zurückgibt. Helfer wie `osc_saw`, `noise`, `lp1`,
   `ramp`, `ar_env` und `env_decay` sind in `synth.h` dokumentiert.
2. Am Ende der Tabelle `SOUNDS[]` einen Eintrag mit `SYNTH(...)` ergänzen
   (oder `NOTES(...)` / `TRACKS(...)` für Melodien).
3. `pebble build`.
