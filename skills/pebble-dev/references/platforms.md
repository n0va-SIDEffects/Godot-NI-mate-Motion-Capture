# Plattformen, Defines und Projekt-Metadaten

## 1. Die Hardwarefamilie

Offizielle Tabelle von `developer.repebble.com/guides/tools-and-resources/hardware-information/`,
Stand September 2026. Spaltenweise gelesen, damit nichts verrutscht:

| | aplite | basalt | chalk | diorite | flint | emery | gabbro |
|---|---|---|---|---|---|---|---|
| Uhr | Classic, Steel | Time, Time Steel | Time Round | Pebble 2 | Pebble 2 Duo | Pebble Time 2 | Pebble Round 2 |
| Hersteller | Pebble Technology | Pebble Technology | Pebble Technology | Pebble Technology | Core Devices | Core Devices | Core Devices |
| SOC | STM32F205RE | STM32F411 | STM32F411 | STM32F411 | nRF52840 | SiFli SF32LB52J | SiFli SF32LB52J |
| CPU | Cortex-M3 64 MHz | Cortex-M4 100 MHz | Cortex-M4 100 MHz | Cortex-M4 100 MHz | Cortex-M4 64 MHz | Star-MC1 240 MHz | Star-MC1 240 MHz |
| Auflösung | 144 × 168 | 144 × 168 | 180 × 180 | 144 × 168 | 144 × 168 | 200 × 228 | 260 × 260 |
| Form | eckig | eckig | **rund** | eckig | eckig | eckig | **rund** |
| Farben | 2 | 64 | 64 | 2 | 2 | 64 | 64 |
| App (Code + Heap) | 24 k | 64 k | 64 k | 64 k | 64 k | 128 k | 128 k |
| Ressourcen max | 96 k | 256 k | 256 k | 256 k | 256 k | 256 k | 256 k |
| Touch | nein | nein | nein | nein | nein | **ja** | **ja** |
| Lautsprecher | nein | nein | nein | nein | **ja** | **ja** | nein |
| Mikrofon | nein | ja | ja | ja | ja | ja (2×, ENC) | ja (2×, ENC) |
| Herzfrequenz | nein | nein | nein | ja (außer SE) | nein | ja | nein |
| Sensoren | Accel, Kompass | Accel, Kompass | Accel, Kompass | Accel | 6-Achs-IMU, Kompass, Barometer | 6-Achs-IMU, Kompass | 3-Achs-IMU, Kompass |
| Backlight | weiß | weiß | weiß | weiß | weiß | **RGB** | weiß |
| Vibration | ERM | ERM | ERM | ERM | LRA | LRA | LRA |
| Tasten | 4 | 4 | 4 | 4 | 4 | 4 | 4 |
| Akku (Herstellerangabe) | ~7 T | ~7–10 T | ~2 T | ~7 T | ~30 T | ~30 T | ~14 T |

Zwei Punkte, die regelmäßig für Verwirrung sorgen:

- **`flint` (Pebble 2 Duo) hat laut dieser Tabelle einen Lautsprecher**, ist
  aber schwarz-weiß. Wer eine Audio-App nur auf `emery` beschränkt, sollte das
  bewusst tun und nicht aus der Annahme heraus, `emery` sei die einzige Uhr mit
  Lautsprecher.
- **`emery` ist eckig** (200 × 228), **`gabbro` ist rund** (260 × 260). Die
  beiden werden ständig verwechselt, weil beide neu sind und dieselbe CPU haben.

Welche Ressourcen- und Speichergrenzen die Toolchain wirklich durchsetzt, sagt
der Build selbst (siehe Abschnitt 5) — und der weicht an einer Stelle von der
Tabelle ab: Für `aplite` meldete SDK 4.33.1 eine Ressourcengrenze von 128 KB,
nicht 96 k. Im Zweifel gilt, was der Build sagt.

## 2. Compile-Zeit-Defines

**Auf Fähigkeiten prüfen, nicht auf Modelle.** Die `PBL_PLATFORM_*`-Defines gibt
es (`PBL_PLATFORM_APLITE`, `_BASALT`, `_CHALK`, `_DIORITE`, `_FLINT`, `_EMERY`,
`_GABBRO`), aber die Dokumentation rät ausdrücklich davon ab, sie zur
Fallunterscheidung zu benutzen: neue Hardware bricht solchen Code, eine
Fähigkeitsabfrage überlebt sie.

| Define | Kurzmakro | Bedeutung |
|---|---|---|
| `PBL_COLOR` | `PBL_IF_COLOR_ELSE(a, b)` | 64 Farben |
| `PBL_BW` | `PBL_IF_BW_ELSE(a, b)` | nur Schwarz-Weiß |
| `PBL_RECT` | `PBL_IF_RECT_ELSE(a, b)` | eckiges Display |
| `PBL_ROUND` | `PBL_IF_ROUND_ELSE(a, b)` | rundes Display |
| `PBL_MICROPHONE` | `PBL_IF_MICROPHONE_ELSE(a, b)` | Diktierfunktion |
| `PBL_HEALTH` | `PBL_IF_HEALTH_ELSE(a, b)` | Health-Service |
| `PBL_SMARTSTRAP` | `PBL_IF_SMARTSTRAP_ELSE(a, b)` | Smartstrap-Port |
| `PBL_TOUCH` | — | Touchscreen |
| `PBL_SPEAKER` | — | Lautsprecher |
| `PBL_RGB_BACKLIGHT` | — | farbige Hintergrundbeleuchtung |
| `PBL_DISPLAY_WIDTH` / `PBL_DISPLAY_HEIGHT` | — | Pixelmaße |
| `PBL_API_EXISTS(fn)` | — | prüft, ob eine Funktion im SDK existiert |

```c
// Ein einzelner Wert: Makro.
window_set_background_color(win, PBL_IF_COLOR_ELSE(GColorJaegerGreen, GColorBlack));

// Ganze Blöcke: Praeprozessor.
#if defined(PBL_MICROPHONE)
  s_dictation = dictation_session_create(0, dictation_cb, NULL);
#endif

// Neue API vorsichtig benutzen:
#if PBL_API_EXISTS(health_service_peek_current_value)
  HealthValue steps = health_service_sum_today(HealthMetricStepCount);
#endif
```

Auf der JS-Seite genauso defensiv:

```javascript
if (Pebble.getActiveWatchInfo) {
  var info = Pebble.getActiveWatchInfo();   // info.platform: 'emery', 'gabbro', ...
}
```

## 3. Ressourcen pro Plattform

Dateinamen bekommen Suffixe, das Build-System wählt automatisch und packt nur
die passende Variante ins `.pbw`:

```
resources/images/logo~color.png
resources/images/logo~bw.png
resources/images/logo~round.png
resources/images/logo~emery.png
```

Im Code bleibt es ein einziger Bezeichner: `RESOURCE_ID_LOGO`.

Das spart nicht nur Platz, es ist auch der saubere Weg für runde Displays:
dort braucht ein Bild oft einen anderen Zuschnitt, keinen skalierten.

## 4. package.json

Das Gerüst, das `pebble new-project` erzeugt (SDK 4.33.1), mit den Feldern, auf
die es ankommt:

```json
{
  "name": "meine-app",
  "author": "SIDE effect's",
  "version": "1.0.0",
  "keywords": ["pebble-app"],
  "private": true,
  "dependencies": {},
  "pebble": {
    "displayName": "Meine App",
    "uuid": "18b5b0c4-e7ee-4664-8c65-646bb092f915",
    "sdkVersion": "3",
    "enableMultiJS": true,
    "targetPlatforms": ["emery", "gabbro"],
    "watchapp": { "watchface": false },
    "capabilities": ["configurable"],
    "messageKeys": ["Accent", "ShowSeconds"],
    "resources": {
      "media": [
        { "type": "bitmap", "name": "LOGO",      "file": "images/logo.png" },
        { "type": "bitmap", "name": "MENU_ICON", "file": "images/icon25.png", "menuIcon": true },
        { "type": "font",   "name": "FONT_MONO_20", "file": "fonts/mono.ttf" }
      ]
    }
  }
}
```

| Feld | Achtung |
|---|---|
| `uuid` | darf sich über Versionen **nie** ändern, sonst gilt ein Update als neue App |
| `version` | für den Store gelten eigene Regeln — siehe `pebble-publish` |
| `displayName` | Name im Launcher; `name` ist nur der npm-Bezeichner |
| `targetPlatforms` | nur eintragen, was getestet ist; fehlende Hardware-API bricht das Linken |
| `messageKeys` | werden im C als `MESSAGE_KEY_<Name>` verfügbar |
| `capabilities` | `"configurable"` ist Pflicht für das Zahnrad in der Handy-App, dazu `"location"`, `"health"` |
| `menuIcon` | genau **ein** Medieneintrag, PNG 25 × 25, sonst generisches Launcher-Symbol |
| `resources.media` | maximal 256 Einträge |
| `watchapp.hiddenApp` | schließt `onlyShownOnCommunication` aus |

## 5. Was der Build über den Speicher sagt

`pebble build` gibt pro Plattform einen Bericht aus. Für das unveränderte
Gerüst aus `assets/main.c` (SDK 4.33.1, gemessen):

```
EMERY APP MEMORY USAGE
Total size of resources:        4092 bytes / 256.0KB
Total footprint in RAM:         1942 bytes / 128.0KB
Free RAM available (heap):      129130 bytes
```

| Plattform | Footprint | Freier Heap |
|---|---|---|
| aplite | 1.930 B von 24 k | 22.646 B |
| basalt, chalk, diorite, flint | 1.942 B von 64 k | 63.594 B |
| emery, gabbro | 1.942 B von 128 k | 129.130 B |

**„Free RAM available" ist die Zahl, die zählt.** Sie schrumpft mit jeder Zeile
Code, und was übrig bleibt, teilen sich alle Bilder, Puffer und Fenster zur
Laufzeit. Den Wert nach größeren Änderungen mitlesen — ein plötzlicher Sprung
nach unten ist meist ein versehentlich statisch angelegtes Array.
