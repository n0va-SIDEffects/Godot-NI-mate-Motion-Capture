// Konfigurationsseite fuer die Pebble-Handy-App (Clay), zweisprachig.
// Die Werte der Auswahlfelder sind Zahlen als Text, die Uhr wandelt sie um.
var TEXT = {
  de: {
    intro: "Einstellungen werden beim Speichern an die Uhr gesendet. Änderungen im Menü auf der Uhr erscheinen hier, sobald die App auf der Uhr läuft.",
    control: "Steuerung", pitch: "Tonhöhe", volume: "Lautstärke",
    axisNote: "Der Beschleunigungssensor sieht nur Neigung (Heben/Senken, Drehen). Links/Rechts geht nur über den Kompass, der ist träge.",
    lift: "Heben/Senken", roll: "Drehen", compass: "Kompass (links/rechts)", full: "Immer voll",
    invPitch: "Tonhöhe umkehren", invVol: "Lautstärke umkehren",
    pitchSens: "Empfindlichkeit Tonhöhe", volSens: "Empfindlichkeit Lautstärke",
    sensNote: "Fein: viel Bewegung für den vollen Bereich. Grob: wenig Bewegung.",
    fine: "Fein", medium: "Mittel", coarse: "Grob",
    notes: "Töne", root: "Tiefster Ton", range: "Umfang", octave: "Oktave", octaves: "Oktaven",
    scale: "Tonleiter", scaleNote: "Alles außer Frei rastet auf die Töne der Tonleiter ein.",
    free: "Frei (stufenlos)", chromatic: "Chromatisch", major: "Dur", minor: "Moll", penta: "Pentatonik",
    glide: "Portamento", short: "Kurz", long: "Lang",
    sound: "Klang", wave: "Wellenform", sine: "Sinus", tri: "Dreieck", square: "Rechteck", saw: "Sägezahn",
    waveAnim: "Wellenanzeige", waveAnimNote: "Die Welle im Hintergrund. Falls der Ton beim Spielen knackt, auf Statisch oder Aus stellen.",
    off: "Aus", static_: "Statisch", animated: "Animiert",
    gate: "Rauschsperre", gateNote: "Schaltet den Lautsprecher bei völliger Stille ab, damit er nicht leer rauscht. Beim nächsten Ton wird er weich wieder eingeblendet.",
    backlight: "Beleuchtung dauerhaft an", backlightNote: "Hält die Displaybeleuchtung an, solange die App läuft. Kostet Akku.",
    maxVol: "Maximale Lautstärke",
    display: "Anzeige", language: "Sprache auf der Uhr", auto: "Automatisch (Systemsprache)",
    support: "Unterstützen",
    supportText: "Die App ist kostenlos und ohne Werbung. Wenn sie dir Freude macht, freue ich mich über einen Kaffee: <a href=\"https://buymeacoffee.com/SIDEffects\" target=\"_blank\">buymeacoffee.com/SIDEffects</a>",
    donate: "☕ Buy me a coffee",
    save: "Speichern"
  },
  en: {
    intro: "Settings are sent to the watch when you save. Changes made in the watch menu appear here as soon as the app runs on the watch.",
    control: "Control", pitch: "Pitch", volume: "Volume",
    axisNote: "The accelerometer only senses tilt (lift, roll). Left/right is only possible via the compass, which is slow.",
    lift: "Lift", roll: "Roll", compass: "Compass (left/right)", full: "Always full",
    invPitch: "Invert pitch", invVol: "Invert volume",
    pitchSens: "Pitch sensitivity", volSens: "Volume sensitivity",
    sensNote: "Fine: more movement for the full range. Coarse: less movement.",
    fine: "Fine", medium: "Medium", coarse: "Coarse",
    notes: "Notes", root: "Lowest note", range: "Range", octave: "octave", octaves: "octaves",
    scale: "Scale", scaleNote: "Anything but Free snaps to the notes of the scale.",
    free: "Free (continuous)", chromatic: "Chromatic", major: "Major", minor: "Minor", penta: "Pentatonic",
    glide: "Portamento", short: "Short", long: "Long",
    sound: "Sound", wave: "Waveform", sine: "Sine", tri: "Triangle", square: "Square", saw: "Sawtooth",
    waveAnim: "Wave display", waveAnimNote: "The wave in the background. If the sound clicks while playing, set this to Static or Off.",
    off: "Off", static_: "Static", animated: "Animated",
    gate: "Noise gate", gateNote: "Switches the speaker off during complete silence so it does not hiss. It fades back in with the next note.",
    backlight: "Backlight always on", backlightNote: "Keeps the display backlight on while the app runs. Uses battery.",
    maxVol: "Maximum volume",
    display: "Display", language: "Language on the watch", auto: "Automatic (system language)",
    support: "Support",
    supportText: "The app is free and has no ads. If you enjoy it, a coffee is much appreciated: <a href=\"https://buymeacoffee.com/SIDEffects\" target=\"_blank\">buymeacoffee.com/SIDEffects</a>",
    donate: "☕ Buy me a coffee",
    save: "Save"
  }
};

module.exports = function(lang) {
  var t = TEXT[lang] || TEXT.en;
  var sens = [ { label: t.fine, value: "0" }, { label: t.medium, value: "1" }, { label: t.coarse, value: "2" } ];
  return [
    { type: "heading", defaultValue: "Theremin" },
    { type: "text", defaultValue: t.intro },
    { type: "section", items: [
      { type: "heading", defaultValue: t.control },
      { type: "select", messageKey: "PitchAxis", label: t.pitch, defaultValue: "0", description: t.axisNote,
        options: [ { label: t.lift, value: "0" }, { label: t.roll, value: "1" }, { label: t.compass, value: "2" } ] },
      { type: "select", messageKey: "VolAxis", label: t.volume, defaultValue: "1",
        options: [ { label: t.lift, value: "0" }, { label: t.roll, value: "1" }, { label: t.compass, value: "2" }, { label: t.full, value: "3" } ] },
      { type: "toggle", messageKey: "InvertPitch", label: t.invPitch, defaultValue: false },
      { type: "toggle", messageKey: "InvertVol", label: t.invVol, defaultValue: false },
      { type: "select", messageKey: "PitchSens", label: t.pitchSens, defaultValue: "1", description: t.sensNote, options: sens },
      { type: "select", messageKey: "VolSens", label: t.volSens, defaultValue: "1", options: sens }
    ] },
    { type: "section", items: [
      { type: "heading", defaultValue: t.notes },
      { type: "select", messageKey: "Root", label: t.root, defaultValue: "1",
        options: [ { label: "C2 (65 Hz)", value: "0" }, { label: "A2 (110 Hz)", value: "1" }, { label: "C3 (131 Hz)", value: "2" }, { label: "A3 (220 Hz)", value: "3" } ] },
      { type: "select", messageKey: "Octaves", label: t.range, defaultValue: "4",
        options: [ { label: "1 " + t.octave, value: "1" }, { label: "2 " + t.octaves, value: "2" }, { label: "3 " + t.octaves, value: "3" }, { label: "4 " + t.octaves, value: "4" } ] },
      { type: "select", messageKey: "Scale", label: t.scale, defaultValue: "0", description: t.scaleNote,
        options: [ { label: t.free, value: "0" }, { label: t.chromatic, value: "1" }, { label: t.major, value: "2" }, { label: t.minor, value: "3" }, { label: t.penta, value: "4" } ] },
      { type: "select", messageKey: "Glide", label: t.glide, defaultValue: "1",
        options: [ { label: t.short, value: "0" }, { label: t.medium, value: "1" }, { label: t.long, value: "2" } ] }
    ] },
    { type: "section", items: [
      { type: "heading", defaultValue: t.sound },
      { type: "select", messageKey: "Wave", label: t.wave, defaultValue: "0",
        options: [ { label: t.sine, value: "0" }, { label: t.tri, value: "1" }, { label: t.square, value: "2" }, { label: t.saw, value: "3" } ] },
      { type: "toggle", messageKey: "Gate", label: t.gate, defaultValue: true, description: t.gateNote },
      { type: "select", messageKey: "Volume", label: t.maxVol, defaultValue: "2",
        options: [ { label: "60 %", value: "0" }, { label: "80 %", value: "1" }, { label: "100 %", value: "2" } ] }
    ] },
    { type: "section", items: [
      { type: "heading", defaultValue: t.display },
      { type: "select", messageKey: "WaveAnim", label: t.waveAnim, defaultValue: "2", description: t.waveAnimNote,
        options: [ { label: t.off, value: "0" }, { label: t.static_, value: "1" }, { label: t.animated, value: "2" } ] },
      { type: "toggle", messageKey: "Backlight", label: t.backlight, defaultValue: false, description: t.backlightNote },
      { type: "select", messageKey: "Language", label: t.language, defaultValue: "0",
        options: [ { label: t.auto, value: "0" }, { label: "Deutsch", value: "1" }, { label: "English", value: "2" } ] }
    ] },
    { type: "submit", defaultValue: t.save },
    { type: "section", items: [
      { type: "heading", defaultValue: t.support },
      { type: "text", defaultValue: t.supportText },
      { type: "button", id: "donate", primary: true, defaultValue: t.donate }
    ] }
  ];
};
