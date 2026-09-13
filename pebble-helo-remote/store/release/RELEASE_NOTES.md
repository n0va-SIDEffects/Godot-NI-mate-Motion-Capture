# Release notes

## 1.2

### Short version (paste into the portal's release notes field)

Timeline pins. Every recording and every stream now shows up as a pin in
the watch timeline, with start time and, once stopped, the duration. Can be
switched off in the settings. Needs the app installed from the appstore.

### One-line version (if the field is tight)

New: a timeline pin per recording and stream, with duration once stopped (optional).

### Full version

**New: Timeline pins.** When a recording or stream starts, the phone side
creates a pin in the watch timeline ("HELO recording" / "HELO stream" with
the device name and start time). When it stops, the same pin is updated with
the end time and duration in minutes. Open pins survive a restart of the
phone app. The feature is on by default and can be switched off under
Timeline in the settings; the timeline server can be changed there too.
Pins pushed through the timeline web API can take up to 15 minutes to
appear on the watch.

## 1.1

### Short version (paste into the portal's release notes field)

Seven languages. The watch and the settings page now speak English, German,
French, Spanish, Italian, Portuguese and Dutch, following the watch language
automatically; a fixed language can be chosen in the settings. Layout fixes
on Pebble Time and Pebble 2 so run times and the media bar are no longer
clipped.

### One-line version (if the field is tight)

New: seven languages (EN, DE, FR, ES, IT, PT, NL) following the watch language, plus layout fixes on 144 px displays.

### Full version

**New: Seven languages.** All texts on the watch (status, hints, media line)
and the whole settings page in the Pebble phone app are available in English,
German, French, Spanish, Italian, Portuguese and Dutch. The language follows
the watch by default and can be fixed under Display in the settings.

**Fixed: small displays.** On Pebble Time and Pebble 2 the run time digits
and the media line were clipped; they now use a smaller digit font and a
shorter label.

## 1.0

First release. Start and stop recording and streaming on an AJA HELO from
the watch, with live status: state, run time, free media and temperature.
Stopping asks for a second press so nothing ends by accident. Settings
(address, port, password, polling interval) live in the Pebble phone app.
