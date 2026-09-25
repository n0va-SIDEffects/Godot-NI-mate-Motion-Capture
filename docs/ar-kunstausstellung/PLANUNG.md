# AR-Kunstausstellung: echte Bilder zum Leben erwecken

**Status:** Ideensammlung und Planung (Stand 25.09.2026)
**Ziel:** Besucher:innen richten ihr Handy (oder ein Leihgerät) auf ein echtes Gemälde, und das Bild beginnt sich zu bewegen, zu klingen oder über den Rahmen hinauszuwachsen.

---

## 1. Wie das technisch funktioniert

Das Gemälde selbst ist der Marker (*Image Tracking*, auch *Natural Feature Tracking*).

1. Von jedem Bild gibt es ein **Referenzfoto**. Daraus berechnet die Software markante Merkmale, etwa Kanten, Kontraste und Details.
2. Die Kamera erkennt diese Merkmale live im Raum und berechnet **Position, Winkel und Abstand** des Bildes (Pose).
3. Auf genau diese Fläche wird der digitale Inhalt gelegt: ein Video, eine 3D-Szene, Partikel oder Sound.
4. Der „Magic Moment“ entsteht, wenn der Inhalt **exakt wie das Original beginnt** und erst dann lebendig wird.

---

## 2. Die grosse Grundsatzentscheidung: Wie kommt die AR aufs Handy?

| Weg | Wie | + | − | Eignung |
|---|---|---|---|---|
| **A: Fertige Plattform (Artivive)** | Artivive-App, Bild + Video hochladen | In Stunden startklar, speziell für Kunst gemacht, Free-Plan zum Testen (Pro ca. 14 €/Monat) | Besucher:innen müssen die Artivive-App installieren, wenig kreative Kontrolle, im Kern nur Video-Overlay | **Perfekt für einen schnellen Test**, für die finale Ausstellung eher begrenzt |
| **B: WebAR (MindAR + three.js oder 8th-Wall-Engine)** | QR-Code scannen, die AR startet im Browser | **Keine App-Installation**, läuft auf iOS und Android, volle kreative Kontrolle, Open Source und ohne Lizenzkosten, selbst gehostet | Tracking etwas weniger stabil als nativ, Inhalt verschwindet, sobald das Bild aus dem Sichtfeld ist, Performance auf alten Handys | **Meine Empfehlung als Hauptweg**, wenn die Effekte „im Rahmen“ bleiben |
| **C: Native App (Unity + AR Foundation)** | Eigene App mit ARKit/ARCore | Bestes Tracking, echte 3D-Welt, Inhalte können **aus dem Bild in den Raum** fliegen, Occlusion und Raumklang | App-Store-Veröffentlichung, Installationshürde, deutlich mehr Entwicklungsaufwand | Wenn es spektakulär sein soll, am besten **mit Leih-iPads** |
| **D: Snapchat-Lens (Lens Studio)** | Lens mit Marker-Tracking | Sehr gutes Tracking, kostenlos. Der Louvre macht das seit Feb. 2026 mit sechs Werken | Braucht Snapchat, Snap-Branding, Datenschutzfragen, Plattformabhängigkeit | Eher für ein junges Publikum und Marketing |

**Nicht empfehlenswert (Stand heute):**
- **Adobe Aero** wurde im Nov./Dez. 2025 eingestellt.
- **8th Wall als gehostete Plattform** gibt es seit 28.02.2026 nicht mehr. Die Engine inkl. *Image Targets* ist aber als Open Source (MIT) unter 8thwall.org verfügbar und damit eine gute Option für Weg B.
- **Unreal Engine (Handheld AR):** Image Tracking gilt laut Community-Forum seit UE5 als unzuverlässig.
- **Godot:** Das ARCore-Plugin für Godot 4 ist noch Work-in-Progress, und es gibt kein ausgereiftes Image Tracking für iOS. Für diese Ausstellung ist Godot deshalb nicht produktionsreif. Das Motion-Capture-Wissen aus diesem Repo lässt sich aber für die Inhalte nutzen (siehe Idee 10).

### Empfehlung in einem Satz
**Zuerst mit Artivive oder einem MindAR-Test die Trackbarkeit der echten Bilder prüfen. Danach mit WebAR (MindAR/8th-Wall-Engine + three.js) produzieren, und nur für einzelne Highlight-Effekte, die in den Raum gehen sollen, eine Unity-App auf Leih-iPads.**

---

## 3. Ideen-Pool: Was kann ein Bild „tun“?

Legende Technik: 🌐 = geht mit WebAR · 📱 = braucht eine native App (Welt-Tracking)

### Stufe 1: Das Bild atmet (subtil, poetisch)
1. **Cinemagraph-Effekt** 🌐: Nur einzelne Elemente bewegen sich, etwa Wolken, Wasser, Kerzenflamme, Haare im Wind oder blinzelnde Augen. Der Rest bleibt still. Das wirkt oft am magischsten.
2. **2.5D-Parallax** 🌐: Das Bild wird in Ebenen zerlegt (Vorder-, Mittel- und Hintergrund). Beim Bewegen des Handys verschieben sich die Ebenen, und das Bild bekommt echte Tiefe.
3. **Tageszeit / Jahreszeiten** 🌐: Die Szene wechselt langsam von Tag zu Nacht oder vom Sommer in den Winter.
4. **Klanglandschaft** 🌐: Das Bild bekommt seinen Sound, etwa Meeresrauschen, Stimmengewirr oder Vogelgezwitscher.

### Stufe 2: Das Bild erzählt
5. **Fenster in eine andere Welt (Portal)** 🌐: Das Bild wird zum Fenster. Dahinter liegt eine echte 3D-Szene (Blender-Rekonstruktion), in die man je nach Blickwinkel hineinschaut. Weil die Pose bekannt ist, funktioniert die Perspektive automatisch korrekt. Das ist das gleiche Prinzip wie Off-Axis-Projection im Disguise-Umfeld.
6. **Entstehungsgeschichte** 🌐: Unterzeichnung, Röntgenbild, verworfene Varianten (Pentimenti) oder ein Zeitraffer, wie das Bild Schicht für Schicht entstanden ist.
7. **Die Künstler:in spricht** 🌐: Die Künstler:in erscheint neben dem Bild oder die Stimme führt durch Details. Das ist ein Audioguide mit Bild.
8. **Versteckte Details / Taschenlampe** 🌐: Das Handy wirkt wie eine Lampe oder Lupe und legt im Bild eine verborgene Ebene frei, etwa Symbole, Texte oder eine zweite Geschichte.

### Stufe 3: Das Bild bricht aus (spektakulär)
9. **Aus dem Rahmen heraus** 📱: Vögel fliegen aus dem Bild in den Raum, Farbe tropft über die Wand auf den Boden, Nebel quillt heraus, Figuren steigen aus dem Rahmen.
10. **Theater-Crossover: Schauspieler:innen animieren die Figuren** 🌐/📱: Figuren im Bild bewegen sich mit echter Motion-Capture von Ensemblemitgliedern (NI mate/Kinect → Blender, also die Pipeline aus diesem Repo). Denkbar ist auch eine Verbindung zu einer laufenden Produktion am Haus.
11. **Bilder reden miteinander** 🌐: Eine Figur verlässt Bild A und taucht in Bild B wieder auf. So entsteht eine Geschichte oder Schnitzeljagd quer durch die Ausstellung, mit „Sammelmoment“ am Ende.
12. **Interaktion** 🌐: Man tippt auf Elemente, neigt das Handy (Wasser schwappt) oder pustet ins Mikrofon (Wind bewegt die Szene).

### Bonus: Hybrid mit Projektion (ganz dein Revier)
13. **Ein „Teaser-Bild“ per Projection Mapping** statt AR: Ein Werk wird für alle sichtbar per Projektion lebendig. Das ist der Einstieg in die Ausstellung, der ohne Handy funktioniert und barrierearm ist. Er macht Lust auf die AR-Bilder.

**Tipp zur Dramaturgie:** Nicht jedes Bild braucht die gleiche Stufe. Eine gute Mischung wäre z.B. 60 % subtil (Stufe 1), 30 % erzählend und 1–2 echte „Wow“-Bilder.

---

## 4. Content-Pipeline (mit deinen Tools)

```
Gemälde ──► Referenzfoto ──► Ebenen & Tiefe ──► Animation ──► Export ──► AR-Build
            (vor Ort,         (Photoshop,       (AE, Blender,  (MP4/WebM,
             Ausst.-Licht)     Depth-Map)        KI-Video)      glTF, Audio)
```

1. **Referenzfoto**: frontal, perspektivisch entzerrt, farbtreu, ohne Rahmen zugeschnitten. **Am besten vor Ort unter der finalen Ausstellungsbeleuchtung** fotografieren. Physische Masse des Bildes notieren (Breite × Höhe in cm).
2. **Ebenen & Tiefe**: In Photoshop die Figuren freistellen und die verdeckten Bereiche mit Generative Fill auffüllen. Für die Tiefenkarte eignen sich z.B. Depth Anything oder Marigold.
3. **Animation**:
   - **After Effects**: Puppet Pins, Displacement, Partikel, 2.5D-Kamera.
   - **Blender**: Camera Projection (Bild auf 3D-Geometrie projizieren, im Prinzip Projection Mapping), Rigging, Mocap.
   - **KI-Video (Image-to-Video)**, z.B. MiniMax H3 mit **First/Last-Frame = Originalgemälde**. So startet und endet das Video exakt auf dem echten Bild, und es entsteht ein nahtloser Loop mit perfektem Übergang.
4. **Export**:
   - WebAR: MP4 (H.264) für Vollflächen-Overlays. Für Transparenz „stacked alpha“ (Farbe oben, Alpha unten, Shader setzt es zusammen), weil das auf iOS und Android gleichermassen funktioniert.
   - Echtzeit-3D: glTF/GLB, Texturen komprimiert.
   - Audio: kurze Loops, AAC/Opus.

**Grobe Aufwandsschätzung pro Bild** (sehr grob, hängt stark vom Motiv ab):
- Cinemagraph / subtile Animation: ca. 1–3 Tage
- 2.5D-Parallax mit Sound: ca. 2–4 Tage
- 3D-Portal oder „Aus dem Rahmen“: ca. 1–2 Wochen

---

## 5. Knackpunkte bei echten Bildern (unbedingt früh testen!)

### Trackbarkeit
- **Gut:** detailreiche, kontrastreiche Bilder mit viel Struktur.
- **Schlecht:** monochrome Flächen, sehr dunkle Bilder, minimalistische oder abstrakte Farbfelder, sich wiederholende Muster.
- **Heikel:** Serien mit ähnlichen Motiven, weil die Bilder verwechselt werden können.
- **Test-Tools:** Das ARCore-Tool `arcoreimg` bewertet Bilder mit 0–100 Punkten (≥ 75 empfohlen). Der MindAR-Compiler zeigt die gefundenen Merkmale an.

### Licht & Oberfläche
- **Glas im Rahmen** spiegelt und stört das Tracking stark. Deshalb entspiegeltes Museumsglas oder kein Glas verwenden.
- **Glänzender Firnis und harte Spots** erzeugen Hotspots. Das Licht mit der Lichtabteilung abstimmen.
- Das Licht zwischen Referenzfoto und Ausstellung **nicht mehr ändern**.

### Der Look (damit es nicht „aufgeklebt“ wirkt)
- Kamerabild und gerenderter Inhalt unterscheiden sich in Farbe, Helligkeit und Rauschen.
  - **Lösung 1:** **Nur die bewegten Bereiche maskiert überlagern.** Der Rest bleibt echtes Kamerabild, und kleine Tracking-Wackler fallen kaum auf.
  - **Lösung 2:** Overlay farblich an das Kamerabild angleichen, leichtes Grain hinzufügen und die Ränder weich einblenden.
  - **Profi-Trick:** Das **Live-Kamerabild selbst als Textur** verwenden und verformen. Dann stimmt die Farbe automatisch.
- Beim Erkennen **sanft einblenden**, damit das Bild „erwacht“ statt umzuschalten.
- Gegen Zittern hilft ein Glättungsfilter (z.B. One-Euro-Filter) auf der Pose.

### Raum & Besucherfluss
- AR-Nutzer:innen bleiben länger stehen und blockieren die Sicht. **Bodenmarkierung für den optimalen Abstand** und Platz vor den AR-Bildern einplanen.
- Grosse Bilder brauchen mehr Abstand, damit die Kamera das ganze Bild erfasst.
- **Ton:** Viele Handys mit Lautsprecher gleichzeitig ergeben Chaos. Besser Kopfhörer empfehlen oder leise Sounds und Untertitel einsetzen.

### Infrastruktur
- **WLAN/Mobilfunk** im Ausstellungsraum prüfen (Keller, dicke Mauern?). Die Inhalte vorab laden, als PWA cachen oder ein lokales WLAN einrichten.
- **Geräte:** Bei BYOD (eigene Handys) läuft es von der Performance her auch auf alten Android-Geräten? Leihgeräte (z.B. iPads) im Kiosk-Modus bzw. mit „Geführtem Zugriff“ brauchen Ladestation, Diebstahlschutz und Reinigung.
- Akku und Wärme: AR ist Schwerarbeit fürs Handy.

### Rechtliches (kein Rechtsrat, aber wichtig)
- **Urheberrecht:** Bei lebenden (und bis 70 Jahre nach dem Tod) Künstler:innen braucht es deren **Einverständnis**, denn das Werk wird digital verändert. In der Schweiz betrifft das das Recht auf Werkintegrität (Art. 11 URG). Am schönsten ist es, die Künstler:innen **als Mitgestaltende** einzubeziehen.
- **Datenschutz:** WebAR verarbeitet das Kamerabild lokal im Browser, ohne Upload. Das sollte man auch so kommunizieren. Analytics nur datenschutzfreundlich einsetzen (z.B. Matomo/Plausible).

### Barrierefreiheit
- Untertitel, Audiodeskription, grosse Schrift.
- Alternative für Menschen ohne Smartphone: Leihgeräte oder Projektions-Teaser (Idee 13).

---

## 6. Phasenplan

| Phase | Inhalt | Dauer (grob) |
|---|---|---|
| **0: Klärung** | Offene Fragen beantworten (siehe unten), Budget, Team, Termin | 1 Woche |
| **1: Tracking-Test** | 3–5 echte Bilder fotografieren, mit Artivive (gratis) **und** einem MindAR-Test prüfen: Welche Bilder tracken gut? | 1–2 Tage |
| **2: Proof of Concept** | **Ein** Bild komplett durchziehen: Referenzfoto → Animation → WebAR → Test vor Ort unter echtem Licht | 1–2 Wochen |
| **3: Konzept & Dramaturgie** | Welches Bild bekommt welche Idee? Rote Linie durch die Ausstellung | 1 Woche |
| **4: Content-Produktion** | Animationen pro Bild, Sound | je nach Anzahl Bilder |
| **5: App/Web-Build** | Onboarding („Richte dein Handy auf ein Bild“), Scan-Hinweise, Inhalte, Offline-Caching, Analytics | 2–4 Wochen (parallel zu 4) |
| **6: Test vor Ort** | Verschiedene Geräte (auch alte Androids!), Licht, Besucher-Test mit Kolleg:innen | 1 Woche |
| **7: Eröffnung & Betrieb** | QR-Codes/Beschilderung, Support-Person, Leihgeräte-Management, Auswertung | laufend |

---

## 7. Offene Fragen an Mike

1. **Wie viele Bilder** und welche Art? Malerei, Fotografie, Grafik? Gegenständlich oder abstrakt?
2. **Wer sind die Künstler:innen?** Leben sie noch und sind sie beteiligt, oder sind es gemeinfreie Werke?
3. **Wann und wo?** Eröffnungstermin, Ausstellungsort (im Theater? Galerie?), Laufzeit?
4. **Handys der Besucher:innen oder Leihgeräte?** Oder beides?
5. **Budget und Team:** Machst du das mit deiner Abteilung selbst, oder gibt es externe Unterstützung für die Programmierung?
6. **Welche Wirkung ist gewünscht:** eher poetisch und subtil oder spektakulär mit Wow-Effekt?
7. **Zielgruppe:** Kunstpublikum, Familien, Schulklassen, Theaterpublikum?
8. Gibt es eine **Verbindung zu einer Produktion** am Haus?

---

## 8. Mögliche nächste Schritte

- [ ] Fragen aus Abschnitt 7 klären
- [ ] 3–5 Referenzfotos von echten Bildern machen (möglichst vor Ort, frontal, gutes Licht)
- [ ] **WebAR-Test-Prototyp** (MindAR + three.js): Bild hochladen, Video hochladen, per QR-Code aufs Handy, direkt vor dem Gemälde testen. Das kann ich bauen.
- [ ] Ein Bild als Proof of Concept auswählen

---

### Quellen
- 8th Wall Open Source: https://www.8thwall.com/blog/post/208587408737/8th-wall-open-source
- Adobe Aero End of Support: https://helpx.adobe.com/aero/aero-end-of-support-faq.html
- Artivive Pricing: https://www.artivive.com/pricing
- MindAR: https://github.com/hiukim/mind-ar-js
- AR Foundation Image Tracking (Limits ARKit ≤ 100 / ARCore ≤ 1000 Bilder pro Library): https://docs.unity3d.com/Packages/com.unity.xr.arfoundation@6.1/manual/features/image-tracking/artrackedimagemanager.html
- Godot ARCore Plugin (WIP): https://github.com/GodotVR/godot_arcore
- Unreal UE5 Image Tracking Probleme: https://forums.unrealengine.com/t/image-tracking-not-working-on-ue5-0-arkit-arcore/791248
- Louvre × Snapchat (2026): https://newsroom.snap.com/the-incredible-unknowns
- Lens Studio Marker Tracking: https://developers.snap.com/lens-studio/features/ar-tracking/world/marker-tracking
