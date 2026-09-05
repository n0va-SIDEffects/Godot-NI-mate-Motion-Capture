# HELO Remote – Pebble Time 2 App für den AJA HELO

Steuert und überwacht einen **AJA HELO** (H.264 Streaming-/Recording-Encoder) direkt vom
Handgelenk: Aufnahme und Stream starten/stoppen, Status, Laufzeit, freier Speicher und
Gerätetemperatur auf einen Blick.

Zielplattform ist die **Pebble Time 2** (`emery`, 200×228 Farbdisplay). Die App baut
zusätzlich für Pebble Time (`basalt`) und Pebble 2 / Core 2 Duo (`diorite`).

> **Status:** Der Code wurde per Syntax-Check und gegen den mitgelieferten HELO-Simulator
> getestet (`tools/helo-simulator.js`), aber noch **nicht auf echter Hardware** (Uhr + HELO).
> Siehe [Bekannte Unsicherheiten](#bekannte-unsicherheiten).

---

## Funktionsweise

```
 Pebble Time 2  <-- Bluetooth / AppMessage -->  Pebble-App am Telefon (PebbleKit JS)  <-- HTTP (WLAN) -->  AJA HELO
   main.c                                            index.js                                     REST API :80
```

Die Uhr spricht nie direkt mit dem HELO. Der JavaScript-Teil in der Pebble-App auf dem Telefon
pollt die HELO-REST-API und schickt einen kompakten Status an die Uhr. Tastendrücke auf der Uhr
werden als Befehl ans Telefon geschickt und dort in `eParamID_ReplicatorCommand`-Requests übersetzt.

**Voraussetzung:** Das Telefon muss den HELO per HTTP erreichen (gleiches WLAN/VLAN, kein HTTPS).

### Verwendete HELO-REST-Parameter

| Parameter | Zweck |
|---|---|
| `eParamID_ReplicatorCommand` | `1` Rec Start · `2` Rec Stop · `3` Stream Start · `4` Stream Stop |
| `eParamID_ReplicatorRecordState` | `0` Init · `1` Idle · `2` Recording · `3/4` Failed · `5` Shutdown |
| `eParamID_ReplicatorStreamState` | analog für den Stream |
| `eParamID_RecordingDuration` / `eParamID_StreamingDuration` | Laufzeit (Timecode) |
| `eParamID_CurrentMediaAvailable` | freier Speicher in % |
| `eParamID_Temperature` | Gerätetemperatur in °C |
| `eParamID_SysName` | Gerätename für die Kopfzeile (optional, Fallback: IP) |
| `POST /authenticator/login` | Login, falls am HELO die Benutzer-Authentifizierung aktiv ist |

Requests: `http://<helo>/config?action=get&paramid=<id>` bzw. `...action=set&paramid=<id>&value=<n>`.

---

## Bedienung auf der Uhr

| Taste | Aktion |
|---|---|
| **Oben** | Aufnahme starten. Läuft sie: stoppen – **zweiter Druck innerhalb von 4 s** bestätigt. |
| **Mitte** | Status sofort aktualisieren (bricht eine offene Stopp-Bestätigung ab). |
| **Unten** | Stream starten / stoppen (gleiche Bestätigungslogik). |
| **Zurück** | App beenden. |

Anzeige:

- **Kopfzeile:** Gerätename, Verbindungspunkt (grün = OK, rot = HELO nicht erreichbar,
  gelb = Passwort falsch/fehlt, grau = noch keine Daten vom Telefon).
- **REC-Karte:** rot wenn Aufnahme läuft, orange bei Fehler, grau im Leerlauf; Laufzeit groß.
- **STREAM-Karte:** blau wenn live, sonst wie oben.
- **Fußzeile:** Balken „Medien xx % frei“ (rot < 10 %, orange < 25 %), Temperatur, Meldungen.
- **Aktionsleiste rechts:** zeigt je nach Zustand Start- oder Stopp-Symbol.

Die Uhr vibriert kurz bei Start, doppelt bei Stopp und lang bei einem Fehlerzustand
(abschaltbar in den Einstellungen).

---

## Einstellungen (Zahnrad in der Pebble-App)

Die Konfigurationsseite wird mit [Clay](https://github.com/pebble/clay) erzeugt:

- **IP-Adresse / Hostname** des HELO (Pflicht)
- **HTTP-Port** (Standard 80)
- **Passwort** – nur wenn am HELO *User Authentication* eingeschaltet ist
- **Abfrage-Intervall** 1–15 s (Standard 3 s)
- **Vibrieren bei Start/Stopp**

---

## Bauen und installieren

Der Pebble-SDK-Workflow von Core Devices (Stand 2026):

```bash
# 1. Werkzeug installieren (einmalig)
uv tool install "pebble-tool" --python 3.13     # oder: pip install pebble-tool
pebble sdk install latest
pebble sdk activate <version>                   # z.B. 4.17 – Ausgabe von "pebble sdk list"

# 2. Bauen
cd pebble-helo-remote
npm install            # holt pebble-clay
pebble build           # -> build/helo-remote.pbw

# 3. Installieren
pebble install --emulator emery        # Emulator Pebble Time 2
pebble install --phone <IP-des-Telefons>   # echte Uhr (Developer Connection in der Pebble-App aktivieren)
```

Logs des JS-Teils (hilfreich beim Einrichten): `pebble logs --phone <IP>`.

### Ohne Hardware testen

```bash
node tools/helo-simulator.js 8080                    # ohne Passwort
node tools/helo-simulator.js 8080 --password geheim  # mit Login
```

Automatischer Test des JS-Teils gegen den Simulator (startet ihn selbst):

```bash
node tools/test-pkjs.js                      # ohne Auth
SIM_PASSWORD=geheim node tools/test-pkjs.js  # inkl. Login-Flow
```

Danach in den App-Einstellungen die IP des Rechners und Port `8080` eintragen. Der Simulator
führt eine kleine Zustandsmaschine (Rec/Stream, Laufzeit, 73 % Speicher, 47 °C) und loggt alle
Requests.

---

## Projektstruktur

```
pebble-helo-remote/
├── package.json              Pebble-Projekt (Plattformen, messageKeys, Clay-Abhängigkeit)
├── wscript                   Standard-Buildskript des Pebble-SDK
├── src/c/main.c              Watch-App: UI, Tasten, AppMessage
├── src/pkjs/index.js         Phone-Seite: HELO-REST-Polling, Befehle, Auth, Clay
├── src/pkjs/config.js        Clay-Konfigurationsseite
├── resources/images/menu_icon.png
├── tools/helo-simulator.js   HELO-REST-Simulator für Tests
└── tools/test-pkjs.js        Integrationstest Phone-Seite gegen den Simulator
```

### Nachrichtenprotokoll (AppMessage)

Uhr → Telefon: `CMD` = 0 Refresh, 1 Rec Start, 2 Rec Stop, 3 Stream Start, 4 Stream Stop.

Telefon → Uhr: `CONN` (0 unbekannt, 1 OK, 2 offline, 3 Auth-Fehler, 4 keine IP konfiguriert),
`REC_STATE`, `REC_NAME`, `REC_DUR`, `STREAM_STATE`, `STREAM_NAME`, `STREAM_DUR`, `MEDIA_PCT`,
`TEMP_C`, `SYS_NAME`, `MESSAGE`, `VIBRATE`.

---

## Bekannte Unsicherheiten

- **Nicht auf Hardware getestet.** Der Build-Server des Pebble-SDK war aus der Entwicklungsumgebung
  nicht erreichbar, daher wurde der C-Code nur gegen einen API-Stub syntaxgeprüft und der JS-Teil
  gegen den Simulator. Erster echter Test: `pebble build` ausführen und Emulator starten.
- `eParamID_SysName` ist aus der Ki-Pro-API übernommen; liefert der HELO ihn nicht, zeigt die
  Kopfzeile einfach die IP.
- `value_name` der Zustände wird am HELO als Enum-Name geliefert (z. B. `eRRSRecording`). Die
  App übersetzt die bekannten Zahlenwerte 0–5 in deutsche Kurzlabels und zeigt sonst den
  bereinigten Enum-Namen.
- Bei aktivierter Authentifizierung hängt das Cookie-Handling von der JS-Laufzeit der
  Pebble-App ab (iOS/Android). Die App versucht beides: Cookie automatisch und manuell mitzusenden.
- HELO **Plus** nutzt dieselben Replicator-Parameter; Multi-Stream-Laufzeiten
  (`eParamID_Stream1_Duration`, `eParamID_Stream2_Duration`) sind noch nicht eingebunden.

## Ideen für später

- Aufnahme-/Streaming-Profil (`eParamID_RecordingProfileSel`, `eParamID_StreamingProfileSel`) per
  Long-Press wählen
- Mehrere HELOs (Bühne / Probebühne) umschalten
- Timeline-Pins bei Start/Stopp

## Lizenz

Wie das umgebende Repository (siehe `../LICENSE.md`).
