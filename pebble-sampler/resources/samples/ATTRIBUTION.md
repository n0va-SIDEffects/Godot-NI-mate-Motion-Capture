# Herkunft der Samples

Alle Aufnahmen stammen von Wikimedia Commons und sind gemeinfrei (CC0 oder
Public Domain). Sie wurden auf Mono 16 kHz gebracht, entrauscht, gekürzt, normalisiert
und als IMA-ADPCM gespeichert. Ba-Dum-Tss ist aus dem Snare- und dem Becken-Sample gemischt.

Der Trommelwirbel stammt aus einer sehr leisen Aufnahme und brauchte vor dem
Import eine kraeftige Aufbereitung:

```bash
ffmpeg -i Drum_Roll_Intro.wav -af \
  "highpass=f=280:poles=2,afftdn=nr=24:nf=-40:tn=1,\
   acompressor=threshold=-24dB:ratio=4:attack=5:release=100:makeup=6,\
   alimiter=limit=0.92" wirbel.wav
python3 tools/import_sample.py wirbel.wav --name "Trommelwirbel" \
  --hint "Und der Gewinner ist" --color GColorDarkCandyAppleRedARGB8 \
  --max-seconds 4.2 --no-compress --denoise 0 --highpass 0 --trim-db -60 --replace
```

Die Schalter `--no-compress --denoise 0 --highpass 0` verhindern, dass das
Import-Werkzeug die schon erledigte Aufbereitung ein zweites Mal anwendet.

| Sample | Quelle (Commons-Datei) | Autor | Lizenz |
|---|---|---|---|
| applaus.ima | [277021 sandermotions applause-2.wav](https://commons.wikimedia.org/wiki/File:277021_sandermotions_applause-2.wav) | Sandermotions | CC0 |
| furz.ima | [Wet fart tummy rumbles.ogg](https://commons.wikimedia.org/wiki/File:Wet_fart_tummy_rumbles.ogg) | natalie (pdsounds) | Public Domain |
| ruelpser.ima | [Burp.ogg](https://commons.wikimedia.org/wiki/File:Burp.ogg) | ezwa (pdsounds) | Public Domain |
| explosion.ima | [Explosion 10.ogg](https://commons.wikimedia.org/wiki/File:Explosion_10.ogg), fuer den Kleinlautsprecher aufbereitet (siehe unten) | tcpp | Public Domain |
| trommelwirbel.ima | [Drum Roll Intro.ogg](https://commons.wikimedia.org/wiki/File:Drum_Roll_Intro.ogg), entrauscht und angehoben (siehe unten) | Iwan Sounds and DIY | CC0 |
| ba_dum_tss.ima | [Snare (1) Sample.wav](https://commons.wikimedia.org/wiki/File:Snare_(1)_Sample.wav) + [CrashCymbalSample.ogg](https://commons.wikimedia.org/wiki/File:CrashCymbalSample.ogg) | UnKnownrNone; RyGuy | CC0; Public Domain |
| ka_ching.ima | [Cash register.ogg](https://commons.wikimedia.org/wiki/File:Cash_register.ogg) | „Me“ | Public Domain |
| katze.ima | [Meow of a Siamese cat - freemaster2.wav](https://commons.wikimedia.org/wiki/File:Meow_of_a_Siamese_cat_-_freemaster2.wav) | freemaster2 | CC0 |
| troete.ima | [Leslie A200-156.ogg](https://commons.wikimedia.org/wiki/File:Leslie_A200-156.ogg) (Diesellok-Horn) | HarveyHenkelmann | CC0 |
| sirene.ima | [Motorsirene - Feuerwehralarm.ogg](https://commons.wikimedia.org/wiki/File:Motorsirene_-_Feuerwehralarm.ogg) | Nallchen | Public Domain |
| buzzer.ima | [Buzzer.wav](https://commons.wikimedia.org/wiki/File:Buzzer.wav) | David Pride | CC0 |
| gong.ima | [Gong or bell vibrant (short).ogg](https://commons.wikimedia.org/wiki/File:Gong_or_bell_vibrant_(short).ogg) | pdsounds | CC0 |
| klospuelung.ima | [Toilet flush.flac](https://commons.wikimedia.org/wiki/File:Toilet_flush.flac) | ᐃᓄᒃᑎᑐᑦ | CC0 |
| hahn.ima | [Small rooster crowing.ogg](https://commons.wikimedia.org/wiki/File:Small_rooster_crowing.ogg) | alys (pdsounds) | Public Domain |
| niesen.ima | [Sneezing.ogg](https://commons.wikimedia.org/wiki/File:Sneezing.ogg) | jc (pdsounds) | Public Domain |
| lachen.ima | [Laughter then bell ring.ogg](https://commons.wikimedia.org/wiki/File:Laughter_then_bell_ring.ogg) (Ausschnitt) | ezwa (pdsounds) | Public Domain |
| elefant.ima | [Elephant voice - trumpeting.ogg](https://commons.wikimedia.org/wiki/File:Elephant_voice_-_trumpeting.ogg) | தகவலுழவன் | CC0 |
| pferd.ima | [Wiehern.ogg](https://commons.wikimedia.org/wiki/File:Wiehern.ogg) | Hü. | Public Domain |

## Explosion: Oberton-Anreicherung

Eine Explosion besteht fast nur aus Tiefbass, den der Uhrlautsprecher gar
nicht wiedergibt: Von der Originalaufnahme ueberlebten nur -9 dB den
Frequenzgang der Uhr. Die eingebaute Fassung erzeugt deshalb aus dem
Bassanteil kuenstlich dessen Obertoene (Saettigung plus Gleichrichtung,
danach auf 520 bis 2600 Hz begrenzt). Das Ohr ergaenzt daraus den fehlenden
Grundton, der Knall wirkt wieder wuchtig. Dazu kommt ein heller
Knall-Transient und ein Hochpass bei 300 Hz, der den ohnehin unhoerbaren
Rest wegnimmt. Ergebnis: nur noch -5,8 dB Verlust, die Spitze kommt
praktisch vollstaendig durch.

Erzeugt wird die Datei mit dem Skript `tools/build_explosion.py`, danach:

```bash
python3 tools/import_sample.py explosion.wav --name "Explosion" \
  --hint "Kabumm!" --color GColorSunsetOrangeARGB8 \
  --max-seconds 2.2 --no-compress --denoise 0 --highpass 0 --trim-db -60 --replace
```
