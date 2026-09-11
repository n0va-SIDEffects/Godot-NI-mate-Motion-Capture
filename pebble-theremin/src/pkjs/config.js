// Konfigurationsseite fuer die Pebble-Handy-App (Clay).
// Die Werte der Auswahlfelder sind Zahlen als Text, die Uhr wandelt sie um.
module.exports = [
  { "type": "heading", "defaultValue": "Theremin" },
  { "type": "text", "defaultValue": "Einstellungen werden beim Speichern an die Uhr gesendet. Änderungen im Menü auf der Uhr erscheinen hier, sobald die App auf der Uhr läuft." },
  {
    "type": "section",
    "items": [
      { "type": "heading", "defaultValue": "Steuerung" },
      { "type": "select", "messageKey": "PitchAxis", "label": "Tonhöhe", "defaultValue": "0",
        "description": "Der Beschleunigungssensor sieht nur Neigung (Heben/Senken, Drehen). Links/Rechts geht nur über den Kompass, der ist träge.",
        "options": [
          { "label": "Heben/Senken", "value": "0" },
          { "label": "Drehen", "value": "1" },
          { "label": "Kompass (links/rechts)", "value": "2" }
        ] },
      { "type": "select", "messageKey": "VolAxis", "label": "Lautstärke", "defaultValue": "1",
        "options": [
          { "label": "Heben/Senken", "value": "0" },
          { "label": "Drehen", "value": "1" },
          { "label": "Kompass (links/rechts)", "value": "2" },
          { "label": "Immer voll", "value": "3" }
        ] },
      { "type": "toggle", "messageKey": "InvertPitch", "label": "Tonhöhe umkehren", "defaultValue": false },
      { "type": "toggle", "messageKey": "InvertVol", "label": "Lautstärke umkehren", "defaultValue": false },
      { "type": "select", "messageKey": "PitchSens", "label": "Empfindlichkeit Tonhöhe", "defaultValue": "1",
        "description": "Fein: viel Bewegung für den vollen Bereich. Grob: wenig Bewegung.",
        "options": [
          { "label": "Fein", "value": "0" }, { "label": "Mittel", "value": "1" }, { "label": "Grob", "value": "2" }
        ] },
      { "type": "select", "messageKey": "VolSens", "label": "Empfindlichkeit Lautstärke", "defaultValue": "1",
        "options": [
          { "label": "Fein", "value": "0" }, { "label": "Mittel", "value": "1" }, { "label": "Grob", "value": "2" }
        ] }
    ]
  },
  {
    "type": "section",
    "items": [
      { "type": "heading", "defaultValue": "Töne" },
      { "type": "select", "messageKey": "Root", "label": "Tiefster Ton", "defaultValue": "1",
        "options": [
          { "label": "C2 (65 Hz)", "value": "0" }, { "label": "A2 (110 Hz)", "value": "1" },
          { "label": "C3 (131 Hz)", "value": "2" }, { "label": "A3 (220 Hz)", "value": "3" }
        ] },
      { "type": "select", "messageKey": "Octaves", "label": "Umfang", "defaultValue": "4",
        "options": [
          { "label": "1 Oktave", "value": "1" }, { "label": "2 Oktaven", "value": "2" },
          { "label": "3 Oktaven", "value": "3" }, { "label": "4 Oktaven", "value": "4" }
        ] },
      { "type": "select", "messageKey": "Scale", "label": "Tonleiter", "defaultValue": "0",
        "description": "Alles außer Frei rastet auf die Töne der Tonleiter ein.",
        "options": [
          { "label": "Frei (stufenlos)", "value": "0" }, { "label": "Chromatisch", "value": "1" },
          { "label": "Dur", "value": "2" }, { "label": "Moll", "value": "3" }, { "label": "Pentatonik", "value": "4" }
        ] },
      { "type": "select", "messageKey": "Glide", "label": "Portamento", "defaultValue": "1",
        "options": [
          { "label": "Kurz", "value": "0" }, { "label": "Mittel", "value": "1" }, { "label": "Lang", "value": "2" }
        ] }
    ]
  },
  {
    "type": "section",
    "items": [
      { "type": "heading", "defaultValue": "Klang" },
      { "type": "select", "messageKey": "Wave", "label": "Wellenform", "defaultValue": "0",
        "options": [
          { "label": "Sinus", "value": "0" }, { "label": "Dreieck", "value": "1" },
          { "label": "Rechteck", "value": "2" }, { "label": "Sägezahn", "value": "3" }
        ] },
      { "type": "select", "messageKey": "Volume", "label": "Maximale Lautstärke", "defaultValue": "2",
        "options": [
          { "label": "60 %", "value": "0" }, { "label": "80 %", "value": "1" }, { "label": "100 %", "value": "2" }
        ] }
    ]
  },
  { "type": "submit", "defaultValue": "Speichern" }
];
