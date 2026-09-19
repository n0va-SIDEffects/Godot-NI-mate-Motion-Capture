# Kommunikation: AppMessage, Einstellungen, Timeline

## 1. Das Modell

Die Uhr spricht nie direkt mit dem Internet. Dazwischen sitzt **PebbleKit JS**:
JavaScript, das in der Pebble-App auf dem Handy läuft (`src/pkjs/index.js`).
Von dort gehen HTTP-Anfragen raus, und über **AppMessage** — ein
Wörterbuch aus Schlüssel/Wert-Paaren — zurück zur Uhr.

Schlüssel werden in `package.json` unter `messageKeys` deklariert und stehen im
C als `MESSAGE_KEY_<Name>` zur Verfügung. Typen: `uint8/16/32`, `int8/16/32`,
`cstring`, `data`.

## 2. C-Seite

```c
static void inbox_received(DictionaryIterator *iter, void *context) {
  Tuple *t = dict_find(iter, MESSAGE_KEY_Temperature);
  if (t) {                                    // immer auf NULL pruefen
    int32_t grad = t->value->int32;
  }
  Tuple *s = dict_find(iter, MESSAGE_KEY_City);
  if (s) {
    // Zeiger gilt nur waehrend des Callbacks - kopieren!
    strncpy(s_city, s->value->cstring, sizeof(s_city) - 1);
  }
}

static void inbox_dropped(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Inbox verworfen: %d", (int)reason);
}
static void outbox_failed(DictionaryIterator *iter, AppMessageResult reason,
                          void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Senden fehlgeschlagen: %d", (int)reason);
}
static void outbox_sent(DictionaryIterator *iter, void *context) {
  // Erst hier die naechste Nachricht starten.
}

app_message_register_inbox_received(inbox_received);
app_message_register_inbox_dropped(inbox_dropped);
app_message_register_outbox_failed(outbox_failed);
app_message_register_outbox_sent(outbox_sent);
app_message_open(inbox_size, outbox_size);
```

Senden:

```c
DictionaryIterator *out;
AppMessageResult res = app_message_outbox_begin(&out);
if (res == APP_MSG_OK) {
  int wert = 1;
  dict_write_int(out, MESSAGE_KEY_Request, &wert, sizeof(int), true);
  dict_write_cstring(out, MESSAGE_KEY_Name, "abc");
  res = app_message_outbox_send();
}
if (res != APP_MSG_OK) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "outbox: %d", (int)res);
}
```

### Die fünf Regeln

1. **Puffergrößen ausrechnen.** Summe der Schlüssel und der größten Werte plus
   Overhead. Zu kleiner Inbox heißt: Nachricht wird verworfen, in der App
   passiert scheinbar nichts. Obergrenzen liefern
   `app_message_inbox_size_maximum()` und `app_message_outbox_size_maximum()`.
   Größere Puffer kosten Heap, der anderswo fehlt — also ausrechnen, nicht
   maximal wählen.
2. **Immer nur eine Nachricht unterwegs.** Ein zweites `app_message_outbox_send()`
   vor `outbox_sent` liefert `APP_MSG_BUSY`. Entweder mehrere Werte in *eine*
   Nachricht packen (fast immer die bessere Lösung) oder eine Warteschlange
   bauen, die im `outbox_sent`-Callback weiterschiebt.
3. **Alle vier Callbacks registrieren.** Ohne `dropped` und `failed` debuggt man
   blind.
4. **Zeiger aus dem Iterator kopieren.** `cstring` und `data` zeigen in einen
   Puffer, der nach dem Callback ungültig ist.
5. **Auf Verbindungsverlust vorbereitet sein.** `connection_service_subscribe`
   und ein sinnvoller Zustand ohne Handy — veraltete Werte mit Zeitstempel
   sind besser als leere Felder.

## 3. JS-Seite

```javascript
Pebble.addEventListener('ready', function () {
  // Vorher darf die Uhr nichts erwarten.
  holeWetter();
});

Pebble.addEventListener('appmessage', function (e) {
  console.log('Von der Uhr: ' + JSON.stringify(e.payload));
});

function sende(daten) {
  Pebble.sendAppMessage(daten,
    function () { console.log('ok'); },
    function (e) { console.log('fehlgeschlagen: ' + JSON.stringify(e)); });
}
```

`XMLHttpRequest` ist verfügbar, `fetch` nicht überall — im Zweifel `XMLHttpRequest`.
Die JS-Umgebung wird beendet, wenn die App beendet wird; langlebige Zustände
gehören in `localStorage`.

## 4. Einstellungsseite

Voraussetzung in beiden Varianten: `"capabilities": ["configurable"]`, sonst
zeigt die Handy-App kein Zahnrad.

### Variante A: Clay (Standardfall)

```bash
pebble package install @rebble/clay
```

Einstellungen werden als JSON beschrieben, Clay erzeugt daraus die Oberfläche,
kümmert sich um `showConfiguration`/`webviewclosed` und schickt die Werte als
AppMessage. Clay speichert zusätzlich im `localStorage` des Handys.

### Variante B: Selbstgebaute Seite ohne Abhängigkeit

Nützlich, wenn kein `npm install` beim Bauen stattfinden soll oder das Layout
exakt sitzen muss: Die Seite als `data:text/html,…`-URI in `src/pkjs/index.js`
zusammenbauen und öffnen. Beim Speichern navigiert die Seite auf
`pebblejs://close#<JSON, URL-kodiert>`, was `webviewclosed` auslöst.

```javascript
Pebble.addEventListener('showConfiguration', function () {
  Pebble.openURL('data:text/html,' + encodeURIComponent(HTML));
});

Pebble.addEventListener('webviewclosed', function (e) {
  if (!e.response) { return; }              // abgebrochen
  var cfg = JSON.parse(decodeURIComponent(e.response));
  Pebble.sendAppMessage(cfg);
});
```

Beide Wege enden gleich: Die Uhr bekommt eine AppMessage und **sichert sie
sofort mit `persist_write_data`**. Sonst sieht die App beim nächsten Start ohne
Handy falsch aus.

## 5. Testen

- **`pebble send-app-message` erreicht die C-Seite nicht.** Das Kommando
  stellt die Nachricht der **JS-Seite** als `appmessage`-Event zu (ohne
  `--app-uuid` erscheint nur ein generisches „Ping"-Fenster). Ein Zustand, der
  per Einstellung gesetzt wird, lässt sich damit nicht durchschalten. Für
  Screenshots einzelner Zustände kurzzeitig den C-Standardwert ändern
  (`static int s_theme = 2;`), bauen, installieren, knipsen, zurückändern.
- Das Werkzeug überträgt pro Aufruf zuverlässig nur einen Wert.
- Die Schlüsselnummern stehen nach dem Bauen in `build/appinfo.json` unter
  `appKeys`.

## 6. Timeline

Pins in der Zeitleiste kommen entweder aus der App (lokale Pins) oder von einem
eigenen Server über die Public Web API. Abonnements verwaltet die JS-Seite
(`Pebble.timelineSubscribe`). Die Uhr braucht ein Konto und eine Verbindung —
für eine Offline-App ist die Timeline kein Ersatz für eigene Anzeige.

Der **AppGlance** (Text und Symbol im Launcher) ist billiger und oft die
bessere Wahl: `app_glance_reload()` aus der App, per REST oder aus PebbleKit JS.
