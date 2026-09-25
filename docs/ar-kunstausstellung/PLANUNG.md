# AR-Kunstausstellung: echte Bilder zum Leben erwecken

**Status:** Planung, Version 2 (Stand 25.09.2026)
**Ziel:** Besucher:innen richten ihr eigenes Handy auf ein abstraktes Gemälde, und das Bild beginnt leise zu leben: Es atmet, fliesst, schimmert, klingt.

---

## 0. Was feststeht

| Frage | Entscheidung | Konsequenz |
|---|---|---|
| Art der Bilder | **Eher abstrakt** | Die Trackbarkeit ist das Hauptrisiko und muss **zuerst** getestet werden (Abschnitt 3) |
| Künstlerin | **Macht mit** | Sie wird Mitgestalterin der digitalen Ebene, nicht nur „Lieferantin“ (Abschnitt 6) |
| Geräte | **Eigene Handys (BYOD)** | **WebAR**: QR-Code scannen, ohne App, läuft im Browser auf iOS und Android |
| Stimmung | **Poetisch, subtil** | Keine Effekte, die aus dem Bild in den Raum gehen. Alles bleibt **im Bild**, und dafür genügt WebAR völlig |
| Anzahl Bilder | **Noch offen** | Die Architektur muss beliebig skalieren (Abschnitt 5.1) |
| Erstes Beispielwerk | **Relief aus gefalteten, bedruckten Papiermodulen** (kein flaches Gemälde) | Hervorragend trackbar, braucht aber Relief-spezifische Regeln (Abschnitt 4) |
| Prozessvideo | **Gibt es noch keines** | Idee 5 („Das Bild entsteht“) nur, wenn die Künstlerin künftig filmt |

### Technik-Entscheid: WebAR
- **Stack:** MindAR (Image Tracking, Open Source) + three.js, als statische Web-App über HTTPS gehostet.
- **Alternative Engine:** Die Open-Source-8th-Wall-Engine (Image Targets, MIT-Lizenz) als Plan B, falls MindAR bei den abstrakten Bildern schlecht trackt. Es lohnt sich, im Tracking-Test **beide** zu prüfen.
- **Nicht mehr im Rennen:**
  - Unity-App: Installationshürde, für subtile Effekte nicht nötig.
  - Artivive: App-Pflicht, wenig Kontrolle. Als 10-Minuten-Schnelltest aber weiterhin brauchbar.
  - Snapchat: Branding, Datenschutz.

---

## 1. Der Kerngedanke: Das Bild nicht überdecken, sondern **das echte Bild selbst bewegen**

Gerade bei abstrakten Bildern mit Farbflächen fällt jeder Farbunterschied zwischen Kamerabild und Overlay sofort auf. Ein darübergelegtes Video wirkt dann schnell wie ein „Aufkleber“.

**Lösung:** Der Grossteil der Effekte arbeitet **direkt mit dem Live-Kamerabild**. Die App weiss durch das Tracking genau, wo das Gemälde im Kamerabild liegt. Ein Shader verformt, belichtet oder färbt genau diese Pixel sanft um.

**Vorteile:**
- **Perfekte Farbtreue:** Es ist ja das echte Bild, inklusive Raumlicht, Glanz und Weissabgleich des Handys.
- **Winzige Datenmenge:** Ein paar KB Code plus Masken statt vieler MB Video. Das ist ideal für BYOD mit schwachem Empfang.
- **Generativ:** Nie zweimal gleich, kein sichtbarer Loop.
- **Skaliert:** Ein Effekt-Baukasten wird für alle Bilder wiederverwendet (Abschnitt 5.2).

Wo es echte Bewegtbilder braucht, etwa das Entstehen des Bildes (Idee 5), kommen **maskierte Video-Overlays** dazu, die farblich an das Kamerabild angeglichen werden.

---

## 2. Ideen für abstrakte, poetische Bilder

Typ: 🎨 = Shader auf Live-Kamerabild · 🎞 = Video-Overlay · 🔊 = Klang

| # | Idee | Beschreibung | Typ | Aufwand |
|---|---|---|---|---|
| 1 | **Das Bild atmet** | Farbfelder dehnen sich minimal aus und ziehen sich zusammen, sehr langsam, wie ein Atemzug. Die Sättigung pulsiert kaum merklich | 🎨 | klein |
| 2 | **Farbe fliesst weiter** | Pinselspuren und Verläufe bewegen sich entlang ihrer eigenen Richtung weiter, als wäre die Farbe noch nass | 🎨 | mittel |
| 3 | **Wanderndes Streiflicht** | Ein virtuelles Licht streift über die Oberfläche und zeigt die Pastosität und die Pinselstruktur. **Das Licht kommt von dort, wo man steht**, denn die Handyposition ist bekannt: Man hält das Licht in der Hand | 🎨 | mittel (braucht Normal-Map, siehe 5.3) |
| 4 | **Schimmern / Glimmen** | Einzelne Farbbereiche beginnen leicht zu leuchten, wie Glut oder Sonnenlicht auf Wasser | 🎨 | klein |
| 5 | **Das Bild entsteht** | Zeitraffer des echten Malprozesses: Das Bild baut sich Schicht für Schicht auf und endet exakt im fertigen Werk. **Die Künstlerin filmt dafür ihren Malprozess** | 🎞 | mittel |
| 6 | **Zeitschichten** | Frühere Zustände des Bildes (übermalte Schichten, Varianten) scheinen durch. Die Künstlerin fotografiert Zwischenstände | 🎞/🎨 | klein bis mittel |
| 7 | **Handy als Lupe** | Nur in einem weichen Kreis um die Bildmitte des Handys wird das Bild lebendig, man „erforscht“ es mit dem Handy | 🎨 | klein (kombinierbar mit allem) |
| 8 | **Stillhalten wird belohnt** | Das Bild erwacht erst, wenn man ein paar Sekunden ruhig davor steht. Wer hektisch wischt, sieht nichts. Das passt zu einer kontemplativen Ausstellung | 🎨 | klein (Logik-Baustein) |
| 9 | **Das Bild klingt** | Jede Farbzone hat einen Klang. Wo das Handy hinschaut, erklingt diese Zone, und so entsteht ein generatives Hörbild. Möglich wäre eine Zusammenarbeit mit einer Komponist:in oder Musiker:innen des Hauses | 🔊 | mittel |
| 10 | **Die Stimme der Künstlerin** | Ein Satz, ein Gedanke, ein Flüstern. Oder Worte, die als Schrift kurz im Bild auftauchen und wieder zerfallen | 🔊/🎨 | klein |
| 11 | **Tageszeit & Wetter** | Das Bild verhält sich morgens anders als abends, z.B. kühler oder wärmer, schneller oder langsamer. Jeder Besuch ist anders | 🎨 | klein |
| 12 | **Auflösen & Zurückfinden** | Ein Bildteil zerfällt in Farbstaub und setzt sich wieder zusammen | 🎨 | mittel |

**Dramaturgie-Idee:** Jedes Bild bekommt **eine** Eigenschaft, nicht fünf. Das eine Bild atmet, das nächste klingt, das dritte zeigt seine Entstehung. Dazu ein roter Faden, den die Künstlerin vorgibt, z.B. „Zeit“, „Atem“ oder „Erinnerung“.

---

## 3. Das Hauptrisiko: Tracking bei abstrakten Bildern

Image Tracking braucht **viele markante, ungleichmässig verteilte Details**. Abstrakte Kunst ist dafür sehr unterschiedlich gut geeignet:

| Bildtyp | Trackbarkeit |
|---|---|
| Gestisch, pastos, viele Pinselspuren, Linien, Kratzer, Collage | ✅ gut |
| Mischung aus Flächen und Strukturen | 🟡 meist ok, kommt auf die Verteilung an |
| Grosse Farbfelder, weiche Verläufe (à la Rothko) | ❌ schwierig |
| Monochrom, sehr dunkel, glänzend, hinter Glas | ❌ schwierig |
| Wiederholende Muster / Raster | ❌ verwechselbar, instabil |

### Strategie (in dieser Reihenfolge)
1. **Referenzfoto vor Ort unter Ausstellungslicht.** Streiflicht auf pastoser Farbe erzeugt zusätzliche Details, die ein flacher Scan nicht hat.
2. **Jedes Bild früh testen** mit einem Score und einer Heatmap, wo Merkmale gefunden werden. Dazu ein Test mit echtem Handy vor dem Bild, aus verschiedenen Abständen und Winkeln.
3. **Schwache Bilder:** Tracking-Engine vergleichen (MindAR vs. 8th-Wall-Engine), Referenzfoto optimieren (Ausschnitt, Auflösung, Kontrast).
4. **Neue Werke:** Die Künstlerin kann bewusst Struktur einbauen (feine Linien, Kratzspuren, Textur in Flächen), ohne dass es ihre Bildsprache verändert.
5. **Notlösung „Anker daneben“:** Eine von der Künstlerin gestaltete kleine Karte oder ein Bildschild neben dem Werk dient als Tracking-Anker. Einschränkung: Bei WebAR muss dieser Anker im Kamerabild bleiben (kein Welt-Tracking).
6. **Ehrliche Alternative:** Nicht jedes Bild muss AR haben. Ein Werk, das nicht trackt, kann nur „klingen“ (Audio). Das passt zu einer poetischen Ausstellung sogar gut.

---

## 4. Erstes Beispielwerk: Falt-Relief (Werk A)

Das Werk besteht aus Hunderten gefalteter Papiermodule aus bedruckten Blättern (Text- und Programmfragmente, z.B. „Wert“, „Vorlage“, „(guitar)“, „piano“, Daten wie „22.5“/„30.5“), angeordnet in einem Raster und in einem Goldrahmen. Die Farben sind Rot, Schwarz, Weiss und Grau mit Akzenten in Türkis und Gelb. Es ist ein **Relief** mit echter Tiefe.

> Foto und Heatmap sind bewusst **nicht** im Repo (Rechte der Künstlerin).

### 4.1 Tracking-Check (Handyfoto vom 25.09.2026, SIFT-Analyse auf 768 × 1024 px)

| Messung | Ergebnis | Bewertung |
|---|---|---|
| Tracking-Merkmale gesamt | ca. 11 900 | ✅ sehr hoch |
| Abdeckung (8 × 8-Raster, ≥ 15 Merkmale pro Zelle) | **100 %**, min. 41 pro Zelle | ✅ gleichmässig über die ganze Fläche |
| „Zwillinge“ (gleiche Merkmale an anderer Stelle) | ca. 1,6 % | ✅ Das Raster wiederholt sich zwar, der Aufdruck macht aber jedes Modul einzigartig |
| Simuliert 30° schräg | 3205 bestätigte Treffer, Positionsfehler 0,1 px | ✅ |
| Simuliert 45° schräg | 367 Treffer, 0,2 px | 🟡 reicht noch, der Spielraum wird kleiner |
| Simuliert weit weg (35 % Grösse) | 911 Treffer | ✅ |
| Simuliert Unschärfe / dunkler Raum | 422 / 859 Treffer | ✅ |

**Fazit:** Das ist fast der Idealfall für Image Tracking, viel besser als ein typisches abstraktes Gemälde.

**Aber:** Die Simulation behandelt das Werk als flaches Bild. In echt ist es ein Relief. Schräg betrachtet verdecken sich die Falten gegenseitig und die Schatten ändern sich, das Bild sieht also anders aus als das Referenzfoto. Der echte Handytest vor dem Werk bleibt deshalb Pflicht.

### 4.2 Relief-spezifische Regeln
- **Referenzfoto exakt frontal**, gleichmässig ausgeleuchtet, **unter dem finalen Ausstellungslicht**. Das Testfoto ist leicht schräg und oben dunkler, das reicht für den Check, aber nicht als finales Target.
- **Den Goldrahmen aus dem Target zuschneiden:** Er spiegelt, und die Spiegelung ändert sich mit dem Blickwinkel.
- **Mehrere Referenzfotos** (frontal, ca. 25° links, 25° rechts) als Targets für **dasselbe** Werk verwenden. MindAR kann mehrere Targets in einer Datei, und das fängt die Relief-Parallaxe ab.
- **Idealer Betrachtungsbereich:** frontal ±30°. Eine Bodenmarkierung hilft, den richtigen Standort zu finden.
- **Physische Masse** und **Relieftiefe** messen (wie weit stehen die Module vor?).
- Optional ein **3D-Scan** (Photogrammetrie, z.B. RealityScan/Polycam, oder iPhone-LiDAR). Damit sitzen 3D-Elemente mit korrekter Tiefe auf dem Relief.

### 4.3 Ideen speziell für dieses Werk

| # | Idee | Beschreibung | Typ | Aufwand |
|---|---|---|---|---|
| A1 | **Welle über dem Feld** | Eine langsame Welle läuft durch das Raster, und jedes Modul neigt sich minimal, wie Wind über ein Blumenfeld oder Schuppen, die sich aufstellen. Umgesetzt auf dem Live-Kamerabild, pro Rasterzelle. **Empfohlen als erster Prototyp:** braucht keine zusätzlichen Assets und zeigt das Prinzip sofort | 🎨 | klein bis mittel |
| A2 | **Entfalten** | Das Modul in der Bildmitte des Handys faltet sich langsam auf, wird wieder zum flachen, bedruckten Blatt, man kann den Text lesen, und dann faltet es sich zurück. *„Was war das Papier, bevor es Kunst wurde?“* Das ist das poetische Herz des Werks. Dafür braucht es die Faltart von der Künstlerin und ein Scan eines ungefalteten Blatts. Umsetzung als Falt-Animation in Blender (3D, lokal an einem Modul) | 3D/🎞 | mittel bis gross |
| A3 | **Worte steigen auf** | Die aufgedruckten Wörter („Wert“, „Vorlage“ …) lösen sich aus den Falten, treiben kurz im Raum vor dem Relief, formen einen Satz der Künstlerin und sinken zurück. Das Material selbst spricht | 🎨/3D | mittel |
| A4 | **Streiflicht** | Beim Relief besonders stark: Ein virtuelles Licht folgt dem Standort der Betrachter:in, und die Faltenschatten wandern mit. Die Normal-Map entsteht per Photometric Stereo (5.3); das Relief ist dafür der perfekte Fall | 🎨 | mittel |
| A5 | **Rot als Herzschlag** | Nur die roten Fragmente glimmen in einem ruhigen Puls. Technisch sehr einfach, weil Rot sich klar vom Rest abhebt | 🎨 | klein |
| A6 | **Klang der Herkunft** | Falls das Papier aus Konzert- oder Veranstaltungsprogrammen stammt (Gitarre, Piano …): Die Module klingen nach den Konzerten, die darauf angekündigt waren. Wo das Handy hinschaut, hört man ein Fragment | 🔊 | mittel |

**Kombi-Vorschlag:** A1 (Welle) als ruhiger Grundzustand. Wer still steht, erlebt A2: ein einzelnes Modul entfaltet sich (Modifikator „Stillhalten wird belohnt“).

---

## 5. Technik-Konzept

### 5.1 Architektur: ein Link pro Bild
```
QR-Code am Bild ──► https://…/?werk=03 ──► lädt NUR Werk 03
                                            ├─ target.mind      (Tracking-Daten)
                                            ├─ config.json      (Effekt + Parameter)
                                            ├─ maske-*.png      (wo der Effekt wirkt)
                                            ├─ normal.png       (optional, Streiflicht)
                                            └─ prozess.mp4 / klang.mp3 (optional)
```
- **Skaliert auf beliebig viele Bilder**, weil jede Seite nur ein Tracking-Ziel lädt. Das bedeutet schnelle Erkennung und keine Verwechslung.
- **Neue Werke hinzufügen** heisst: Ordner anlegen, Referenzfoto rein, Masken rein, config ausfüllen. Das geht ohne Programmieren.
- **Kleine Datenmenge:** Shader-Effekte brauchen nur wenige hundert KB pro Werk.

### 5.2 Effekt-Baukasten
Einmal gebaute Effekte werden pro Bild nur **parametrisiert**:
- `atmen`, `fliessen`, `streiflicht`, `schimmern`, `aufloesen`, `zeitschichten`, `video`, `klang`
- plus **Modifikatoren**: `lupe`, `stillhalten`, `tageszeit`
- Die **Masken** (Graustufenbilder) definieren, **wo** im Bild der Effekt wirkt. Die **Künstlerin malt die Masken selbst**, etwa in Photoshop oder auf einem Ausdruck mit Pinsel, der dann gescannt wird. So entscheidet sie, wo ihr Bild lebt.

### 5.3 Normal-Map fürs Streiflicht (Idee 3)
Methode aus der Museumsfotografie (Photometric Stereo / RTI):
- Das Bild wird mit **fixer Kamera** und 4–8 Aufnahmen aus **verschiedenen Lichtrichtungen** fotografiert.
- Daraus lässt sich die echte Oberflächenstruktur (Normal-Map) berechnen.
- Das geht mit Equipment aus deiner Abteilung in ca. 30 Minuten pro Bild.

### 5.4 Malprozess-Video (Idee 5)
- Kamera **fix von oben bzw. frontal** auf die Leinwand, konstantes Licht, Intervall- oder Zeitrafferaufnahme.
- In der Postproduktion wird das Video per Perspektivkorrektur auf das finale Referenzfoto ausgerichtet und farblich angeglichen.
- Das letzte Bild muss **exakt** dem fertigen Werk entsprechen. Dann kommt der Übergang zum echten Bild nahtlos.

### 5.5 UX für eigene Handys
- **Einstieg:** kleiner, dezenter QR-Code am Bildschild. Optional zusätzlich ein NFC-Tag, dann genügt Antippen.
- **Onboarding:** ein dunkler Screen, ein Satz („Halte dein Handy ruhig vor das Bild.“), Kamera-Freigabe mit kurzer Begründung. Kein technisches UI, keine Buttons im Bild.
- **Ton:** standardmässig leise bzw. aus, mit dem Hinweis „Mit Kopfhörern erleben“. So spielen nicht zehn Handys gleichzeitig durcheinander.
- **Fallback:** Wenn ein Handy kein WebAR kann (sehr alt, Kamera verweigert), zeigt die Seite ein Video des Effekts, damit niemand leer ausgeht.
- **Datenschutz:** Das Kamerabild bleibt auf dem Handy, es wird nichts hochgeladen. Kein Tracking, höchstens anonyme Zählung pro Werk (z.B. Matomo). Die Kamera braucht HTTPS.
- **Sprache:** DE, optional EN.

### 5.6 Testgeräte
Mindestens: ein aktuelles iPhone, ein älteres iPhone (z.B. SE/8), ein aktuelles Android, ein **günstiges, älteres Android** (Samsung-A-Serie o.ä.). Im Team herumfragen, wer alte Handys in der Schublade hat.

---

## 6. Zusammenarbeit mit der Künstlerin

- **Kick-off-Workshop (2–3 h):** Was bedeutet „lebendig“ für ihre Arbeit? Rhythmus, Geste, Zeit, Material? Welche Werke? Dazu gleich vor Ort 2–3 Bilder mit dem Handy testen.
- **Sie gestaltet mit:**
  - Masken malen (wo lebt das Bild?)
  - Effekt und Tempo pro Bild wählen
  - Malprozess filmen (bei neuen Werken)
  - Zwischenstände fotografieren
  - Stimme oder Text beisteuern
  - evtl. von Hand gemalte Animationselemente
- **Für neue Werke:** Tracking-freundliche Struktur mitdenken (Abschnitt 3).
- **Vereinbarung schriftlich festhalten:**
  - Einverständnis zur digitalen Veränderung (Werkintegrität, Art. 11 URG)
  - Nutzung der Referenzfotos
  - Online-Verfügbarkeit (wie lange? Nach Ausstellungsende abschalten?)
  - Nennung
  - Honorar

---

## 7. Phasenplan

| Phase | Inhalt | Dauer (grob) |
|---|---|---|
| **1: Kick-off mit der Künstlerin** | Workshop, Werkauswahl, 3–5 Referenzfotos machen | 1 Tag |
| **2: Tracking-Check** | Alle Kandidaten-Bilder auf Trackbarkeit prüfen (Score, Heatmap, Handytest vor Ort) | 1–3 Tage |
| **3: Prototyp „Welle“ (Werk A)** | WebAR + Shader auf Live-Kamerabild, Test vor dem echten Relief mit mehreren Handys | 1–2 Wochen |
| **4: Effekt-Baukasten** | Die weiteren Effekte bauen, Konfiguration pro Werk, Onboarding, Fallback | 2–4 Wochen |
| **5: Produktion pro Werk** | Masken (Künstlerin), Parameter, optional Normal-Maps, Prozessvideos, Klang | je nach Anzahl Bilder, ca. 0,5–2 Tage pro Werk |
| **6: Test vor Ort** | Finales Licht, alle Testgeräte, Probe-Publikum (Kolleg:innen, Freundeskreis) | 1 Woche |
| **7: Eröffnung & Betrieb** | QR-Schilder, Hosting, Ansprechperson, Abschalttermin | laufend |

**Aufwand pro Werk:** Nach dem Baukasten ist ein Werk mit einem Shader-Effekt ca. ein halber Tag, mit Prozessvideo oder Klang eher 1–2 Tage.

---

## 8. Noch offene Fragen

1. **Existieren die Bilder schon, oder entstehen (auch) neue Werke für die Ausstellung?** Das ist wichtig für das Prozessvideo und für tracking-freundliche Struktur.
2. **Sind die anderen Werke auch Falt-Reliefs**, oder gibt es auch flache Bilder? Hinter Glas?
3. **Werk A:** Masse (B × H) und Relieftiefe? Gehört der Goldrahmen zum Werk? **Woher stammt das Papier** (Programmhefte? Flyer? Von welchen Veranstaltungen?) Das ist wichtig für A2, A3 und A6. Welche Faltart ist es?
4. **Wann und wo** ist die Ausstellung? Wie ist der Handyempfang dort, gibt es Gäste-WLAN?
5. **Ton gewünscht?** Wenn ja: Stimme der Künstlerin, Klanglandschaft, Musik?
6. **Hosting/Domain:** über das Theater (Datenschutzvorgaben der IT?) oder extern?
7. **Wer baut was?** Die WebAR-App und den Effekt-Baukasten kann ich programmieren. Was übernimmt dein Team bei Foto, Normal-Maps, Video und Sound?

---

## 9. Nächste Schritte

- [x] Erstes Beispielwerk erhalten (Werk A, Falt-Relief), Tracking-Check per Foto: sehr gut (Abschnitt 4.1)
- [ ] Weitere Werke fotografieren und prüfen (Handyfoto frontal reicht für den ersten Check)
- [ ] Von Werk A: **ein sauberes Frontalfoto** (ganzes Werk, möglichst gleichmässiges Licht, ohne Schräglage) plus Masse
- [ ] **Prototyp „Welle“ (A1)** bauen: WebAR-Seite, die das echte Relief im Kamerabild sanft wogen lässt, zum Testen per QR-Code direkt vor dem Werk
- [ ] Kick-off-Termin mit der Künstlerin (Faltart, Herkunft des Papiers, Ideen A1–A6 besprechen)

---

## Anhang: Verworfene Optionen (Version 1)

- **Native Unity-App** (Effekte in den Raum, Leih-iPads): für subtile, bildgebundene Effekte und BYOD nicht nötig.
- **Artivive:** Besucher:innen müssten eine App installieren. Taugt aber als schneller Tracking-Test.
- **Snapchat-Lens:** Snapchat-Pflicht, Branding, Datenschutz.
- **Unreal / Godot:** Image Tracking auf dem Handy nicht zuverlässig bzw. nicht produktionsreif.
- **Adobe Aero:** Seit Ende 2025 eingestellt.

### Quellen
- MindAR: https://github.com/hiukim/mind-ar-js · Doku: https://hiukim.github.io/mind-ar-js-doc/
- 8th Wall Open Source (Image Targets, MIT): https://www.8thwall.com/blog/post/208587408737/8th-wall-open-source
- Adobe Aero End of Support: https://helpx.adobe.com/aero/aero-end-of-support-faq.html
- Artivive Pricing: https://www.artivive.com/pricing
- AR Foundation Image Tracking: https://docs.unity3d.com/Packages/com.unity.xr.arfoundation@6.1/manual/features/image-tracking/artrackedimagemanager.html
- Godot ARCore Plugin (WIP): https://github.com/GodotVR/godot_arcore
- Unreal UE5 Image Tracking: https://forums.unrealengine.com/t/image-tracking-not-working-on-ue5-0-arkit-arcore/791248
- Louvre × Snapchat (2026): https://newsroom.snap.com/the-incredible-unknowns
