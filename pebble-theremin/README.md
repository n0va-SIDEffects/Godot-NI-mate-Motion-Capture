# Theremin für die Pebble Time 2

Ein Theremin als Watch-App für die Pebble Time 2 (SDK-Plattform `emery`).
Die Tonhöhe und die Lautstärke steuerst du über die Neigung des Handgelenks,
der Ton kommt in Echtzeit aus dem Lautsprecher der Uhr.

## Bedienung

| Taste                    | Wirkung                                              |
|--------------------------|------------------------------------------------------|
| SELECT                   | Ton an / aus                                         |
| SELECT lang drücken      | Einstellungen öffnen                                 |
| UP / DOWN                | Wellenform: Sinus, Dreieck, Rechteck, Sägezahn       |
| UP lang drücken          | Nullpunkt neu kalibrieren (Uhr etwa 1 Sekunde ruhig halten) |
| BACK                     | App beenden                                          |

### Steuerachsen

Tonhöhe und Lautstärke lassen sich in den Einstellungen je einer Achse zuordnen:

| Achse          | Sensor                 | Bewegung                                          |
|----------------|------------------------|---------------------------------------------------|
| Heben/Senken   | Beschleunigungssensor  | Hand heben und senken                             |
| Drehen         | Beschleunigungssensor  | Handgelenk drehen, Uhr zu dir oder von dir weg    |
| Kompass        | Magnetkompass          | Arm nach links oder rechts schwenken              |
| Immer voll     | keiner                 | nur für Lautstärke: immer maximale Lautstärke     |

Standard: Tonhöhe über Heben/Senken, Lautstärke über Drehen. Beide Achsen
lassen sich in den Einstellungen umkehren.

**Warum kein Links/Rechts über den Beschleunigungssensor?** Der Sensor misst
die Richtung der Schwerkraft. Heben, Senken und Drehen der Hand verändern
diese Richtung relativ zur Uhr, ein Schwenk des Arms nach links oder rechts
dagegen nicht: Die Uhr dreht sich dabei um die Hochachse, und die Schwerkraft
zeigt weiterhin nach unten. Diese Bewegung ist für den Sensor unsichtbar,
egal wie die Hand gehalten wird. Nur der Kompass (oder ein Gyroskop, das das
SDK nicht freigibt) sieht sie. Der Kompass ist als Option enthalten, reagiert
aber träge und braucht beim ersten Start eine Kalibrierung (8er-Bewegung).

### Einstellungen in der Pebble-Handy-App

In der Pebble-App auf dem Handy hat die Theremin-App eine Einstellungsseite
(Zahnrad bei der App). Sie enthält dieselben Optionen wie das Menü auf der
Uhr. Beim Speichern werden die Werte an die Uhr geschickt, die Uhr vibriert
kurz und übernimmt sie sofort. Umgekehrt schickt die Uhr ihre Einstellungen
beim Start und nach jeder Änderung im Uhr-Menü an das Handy, damit die Seite
dort den echten Stand zeigt. Die Seite ist mit
[Clay](https://github.com/pebble/clay) gebaut und liegt in `src/pkjs/config.js`.

### Einstellungen auf der Uhr (SELECT lang)

| Eintrag           | Werte                                                   |
|-------------------|---------------------------------------------------------|
| Kalibrieren       | Nullpunkt neu setzen                                    |
| Tonhöhe           | Heben/Senken, Drehen, Kompass                           |
| Lautstärke        | Drehen, Heben/Senken, Kompass, Immer voll               |
| Tonhöhe umkehren  | Nein / Ja                                               |
| Lautst. umkehren  | Nein / Ja                                               |
| Tiefster Ton      | C2, A2, C3, A3                                          |
| Umfang            | 1 bis 4 Oktaven                                         |
| Tonleiter         | Frei, Chromatisch, Dur, Moll, Pentatonik (rastet auf Töne ein) |
| Empf. Tonhöhe     | Fein, Mittel, Grob (wie viel Bewegung der volle Bereich braucht) |
| Empf. Lautstärke  | Fein, Mittel, Grob                                      |
| Portamento        | Kurz, Mittel, Lang (Gleiten zwischen Tönen)             |
| Max. Lautstärke   | 60 %, 80 %, 100 %                                       |
| Wellenform        | Sinus, Dreieck, Rechteck, Sägezahn                      |
| Wellenanzeige     | Aus, Statisch, Animiert (Welle im Hintergrund)          |
| Rauschsperre      | An / Aus (Lautsprecher bei völliger Stille abschalten)  |
| Beleuchtung       | Automatisch / Dauerhaft an (solange die App läuft)      |

SELECT auf einem Eintrag schaltet zum nächsten Wert. Alle Einstellungen
werden auf der Uhr gespeichert.

**Kalibrierung:** Beim Start hält man die Uhr etwa eine Sekunde in der
Spielhaltung ruhig ("Ruhig halten..."). Die Uhr vibriert kurz, danach ist diese
Haltung der Nullpunkt: mittlere Tonhöhe (A4) und etwa ein Drittel der
Lautstärke. Rund 15 Grad zu dir kippen ergibt volle Lautstärke, rund 12 Grad
von dir weg Stille. Mit langem SELECT lässt sich der Nullpunkt jederzeit neu
setzen.

Die Fußzeile der App zeigt die Tastenbelegung; die zweite Zeile wird bei
laufender Kalibrierung durch den Status ersetzt.

Die zuletzt gewählte Wellenform wird gespeichert.

## Installation auf der Uhr

1. Die Datei `build/pebble-theremin.pbw` auf das Android-Handy übertragen.
2. Die Datei antippen und mit der Pebble-App öffnen.
3. Die Pebble-App installiert die App per Bluetooth auf der Uhr.

## Selbst bauen

```bash
pip install pebble-tool
pebble sdk install latest
npm install          # holt pebble-clay für die Konfigurationsseite
pebble build
```

Das Ergebnis liegt danach unter `build/pebble-theremin.pbw`.

## Technik

- **Sensor:** Das Pebble-SDK bietet keine Gyroskop-API, deshalb wird der
  Beschleunigungssensor (50 Hz) verwendet. Aus dem Schwerkraftvektor ergibt
  sich der Roll- und Kippwinkel der Uhr.
- **Audio:** Die App öffnet einen rohen PCM-Stream (16 kHz, 16 Bit, mono) über
  die Speaker-API und erzeugt die Wellenform selbst mit einem 32-Bit
  Phasenakkumulator. Ziel-Tonhöhe und Ziel-Lautstärke werden pro Sample
  weich nachgeführt, damit nichts knackt. 16 Bit statt 8 Bit sind wichtig,
  weil bei leisen Tönen sonst nur wenige Quantisierungsstufen übrig bleiben.
- **Latenz:** Die App hält den Stream etwa 120 ms vor der Wiedergabe
  (`LEAD_MS` in `src/c/theremin.c`). Der interne Puffer der Firmware fasst
  deutlich mehr (gemessen: 8 KB plus DMA-Blöcke von 256 Bytes), die App
  begrenzt die Latenz also selbst. Fühlt sich der Ton träge an, kann der Wert
  verkleinert werden; setzt der Ton aus, vergrößern.
- **Rauschsperre:** Der Verstärker des Lautsprechers rauscht leise, sobald
  ein Stream offen ist, auch bei digitaler Stille. Ist die Lautstärke ganz
  zurückgekippt und der Ton eine halbe Sekunde still, schließt die App den
  Stream und schaltet damit den Verstärker ab. Beim nächsten Ton wird der
  Stream neu geöffnet und weich eingeblendet (der Notenname erscheint grau,
  solange die Sperre aktiv ist). Rauschen, das während eines Tons hörbar ist,
  stammt aus dem Verstärker und lässt sich per Software nicht entfernen.
- **Wellenformtabellen:** 16 Bit mit 512 Einträgen pro Periode und linearer
  Interpolation (Fehler unter -80 dB), damit die Tonerzeugung selbst kein
  Körnen oder Zischen beisteuert.
- **Anzeige und Audio:** Ein Bildaufbau blockiert die App für einige
  Millisekunden, in denen die Audio-Pumpe nicht nachfüllen kann. Die
  Hintergrundwelle wird deshalb sparsam gezeichnet (ohne Kantenglättung, in
  4-Pixel-Schritten, etwa 6 Bilder pro Sekunde), und der Vorlauf `LEAD_MS`
  liegt über der Dauer eines Bildaufbaus. Knackt es trotzdem, lässt sich die
  Welle in den Einstellungen auf Statisch oder Aus stellen.
- **Kein Flash-Zugriff beim Spielen:** Einstellungen werden erst beim
  Verlassen der App (oder bei Stille) gespeichert. Ein Schreibzugriff auf den
  Flash-Speicher blockiert die App kurz und würde den Ton unterbrechen.
- **Stream-Uhr:** `time_ms()` der Firmware liefert rund um Sekundengrenzen
  gelegentlich Werte, die um genau eine Sekunde daneben liegen. Die App
  filtert diese Sprünge, sonst gerät die Audio-Taktung aus dem Tritt.
- **Wellenformen:** Rechteck und Sägezahn sind bandbegrenzt (additiv aus
  Harmonischen bis etwa 5 kHz aufgebaut, in sechs Frequenzbändern) und
  etwas leiser als Sinus und Dreieck. Naive Rechteck- und Sägezahnwellen
  haben unendlich viele Obertöne, die bei 16 kHz Abtastrate zurückfalten und
  den kleinen Lautsprecher übersteuern.
- **Mathematik:** Das Pebble-SDK liefert keine `libm`. Wellenformen und
  Halbtonverhältnisse kommen aus generierten Tabellen (`wavetables.h`,
  `semitone_table.h`), alles andere ist Festkomma-Arithmetik.

## Feinjustierung im Code

Die Empfindlichkeitsstufen stehen als Tabellen oben in `src/c/theremin.c`
(`PITCH_RANGE_MG`, `PITCH_RANGE_DEG`, `VOL_RANGE_MG`, `VOL_RANGE_DEG`), der
Audio-Vorlauf als `LEAD_MS`. Die Lautstärke-Faktoren für Rechteck und Sägezahn
stehen im Generator-Skript-Kommentar in `wavetables.h`.
