# Theremin für die Pebble Time 2

Ein Theremin als Watch-App für die Pebble Time 2 (SDK-Plattform `emery`).
Die Tonhöhe und die Lautstärke steuerst du über die Neigung des Handgelenks,
der Ton kommt in Echtzeit aus dem Lautsprecher der Uhr.

## Bedienung

| Geste / Taste            | Wirkung                                              |
|--------------------------|------------------------------------------------------|
| Handgelenk links/rechts rollen | Tonhöhe, 4 Oktaven von A2 (110 Hz) bis A6 (1760 Hz) |
| Uhr zu dir kippen        | lauter                                               |
| Uhr von dir weg kippen   | leiser bis stumm                                     |
| SELECT                   | Ton an / aus                                         |
| SELECT lang drücken      | Nullpunkt neu kalibrieren (Uhr etwa 1 Sekunde ruhig halten) |
| UP / DOWN                | Wellenform: Sinus, Dreieck, Rechteck, Sägezahn       |
| BACK                     | App beenden                                          |

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
- **Mathematik:** Das Pebble-SDK liefert keine `libm`. Sinus und Halbton-
  verhältnisse kommen aus generierten Tabellen (`sine_table.h`,
  `semitone_table.h`), alles andere ist Festkomma-Arithmetik.

## Feinjustierung

Die wichtigsten Stellschrauben stehen oben in `src/c/theremin.c`:

| Konstante          | Bedeutung                                              |
|--------------------|--------------------------------------------------------|
| `TILT_PITCH_RANGE` | Rollwinkel (milli-g), der den vollen Tonbereich abdeckt |
| `TILT_VOL_MIN/MAX` | Kippbereich für stumm bis maximale Lautstärke          |
| `F_MIN_HZ`, `OCTAVES` | Tonbereich                                          |
| `LEAD_MS`          | Audio-Vorlauf, bestimmt die Latenz                     |
| `STREAM_VOLUME`    | Grundlautstärke des Lautsprechers (0-100)              |
