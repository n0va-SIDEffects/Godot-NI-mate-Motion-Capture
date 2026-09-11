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

Standard: Tonhöhe über Heben/Senken, Lautstärke über Kompass. Beide Achsen
lassen sich in den Einstellungen umkehren.

**Kompass-Hinweis:** Der Beschleunigungssensor sieht eine Drehung um die
Hochachse nicht, deshalb läuft "links/rechts" über den Kompass. Der braucht
beim ersten Start eine Kalibrierung (Uhr in einer 8er-Bewegung schwenken,
die App zeigt das unten an) und reagiert etwas träger als die
Beschleunigungsachsen. In Räumen mit viel Stahl oder starken Magnetfeldern
kann er unruhig werden.

### Einstellungen (SELECT lang)

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

SELECT auf einem Eintrag schaltet zum nächsten Wert. Alle Einstellungen
werden auf der Uhr gespeichert.

**Kalibrierung:** Beim Start hält man die Uhr etwa eine Sekunde in der
Spielhaltung ruhig ("Ruhig halten..."). Die Uhr vibriert kurz, danach ist diese
Haltung der Nullpunkt: mittlere Tonhöhe (A4) und etwa ein Drittel der
Lautstärke. Rund 15 Grad zu dir kippen ergibt volle Lautstärke, rund 12 Grad
von dir weg Stille. Mit langem SELECT lässt sich der Nullpunkt jederzeit neu
setzen.

Die zuletzt gewählte Wellenform wird gespeichert.

## Installation auf der Uhr

1. Die Datei `build/pebble-theremin.pbw` auf das Android-Handy übertragen.
2. Die Datei antippen und mit der Pebble-App öffnen.
3. Die Pebble-App installiert die App per Bluetooth auf der Uhr.

## Selbst bauen

```bash
pip install pebble-tool
pebble sdk install latest
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
- **Stream-Uhr:** `time_ms()` der Firmware liefert rund um Sekundengrenzen
  gelegentlich Werte, die um genau eine Sekunde daneben liegen. Die App
  filtert diese Sprünge, sonst gerät die Audio-Taktung aus dem Tritt.
- **Wellenformen:** Rechteck und Sägezahn sind bandbegrenzt (additiv aus
  Harmonischen bis etwa 7 kHz aufgebaut, in sechs Frequenzbändern) und
  bewusst leiser als Sinus und Dreieck. Naive Rechteck- und Sägezahnwellen
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
