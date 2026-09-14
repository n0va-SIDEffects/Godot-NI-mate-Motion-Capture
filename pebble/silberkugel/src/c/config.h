#pragma once
#include <pebble.h>

// SILBERKUGEL, Phase 1: Physik-Prototyp fuer die Pebble Time 2 (emery).
// Alle Laengen in Pixeln, alle Zeiten in Millisekunden, alle Geschwindigkeiten
// in Pixeln pro Sekunde.

#define P1_VERSION "0.1"

// ---------------------------------------------------------------- Bildschirm
#define SCR_W 200
#define SCR_H 228
#define HUD_H 14                  // schmale Textzeile oben, ueberlagert das Spielfeld

// Der Tisch passt vollstaendig auf den Bildschirm. Das Konzept sieht 200x400
// mit Kamera vor; fuer Phase 1 waere das nur Streuung in der Messung, weil das
// Scrollen laut Hardware-Befund ohnehin keinen Uebertragungsvorteil kostet
// oder bringt (der Compositor meldet immer das ganze Bild als schmutzig).
#define TABLE_W SCR_W
#define TABLE_H SCR_H
#define DRAIN_Y 228               // ab hier gilt die Kugel als abgeflossen

// ------------------------------------------------------------------- Zeitraster
#define PHYS_HZ 200               // fester Zeitschritt, 5 ms
#define PHYS_DT_MS (1000 / PHYS_HZ)
#define PHYS_MAX_SUBSTEPS 12      // Nachholgrenze: max. 60 ms Physik pro Tick
#define GAME_TICK_MS 20
#define RENDER_TICK_MS 40         // 25 fps Ziel (der Hardware-Befund raet zu 20 bis 25)
#define LOG_TICK_MS 1000
#define PANEL_TEST_FRAMES 300
#define PHYS_BENCH_STEPS 2000     // Stresstest: so viele Substeps am Stueck

// ------------------------------------------------------------------- Physik
#define BALL_R_PX 3               // 6 px Durchmesser wie im Konzept
#define FLIPPER_R_PX 4            // Kapselradius des Flippers
#define MAX_BALLS 3

// Grundtempo des Tisches. Ein echter Flipper steht schraeg, die Kugel sieht
// also nur einen Bruchteil der Erdbeschleunigung; auf einem 228 px hohen
// Tisch entscheidet dieser Wert allein, wie hektisch das Spiel wirkt. Nach
// dem ersten Spieltest von 700 auf 500 gesenkt: Bei 700 legte die Kugel im
// freien Fall ueber den Tisch rund 530 px/s zurueck, also 21 Pixel je Bild,
// bei 500 sind es 450 px/s und 18 Pixel. Feinjustage im MESS-Bildschirm mit
// kurzem Druck auf Up, die Stufe steht im HUD und im Log.
#define GRAVITY_PX_S2 500
#define VEL_MAX_PX_S 1200         // Sicherheitsklemme, verhindert Ausreisser
#define VEL_SLEEP_PX_S 6          // darunter gilt die Kugel als ruhend (Rollreibung)

// Restitution in Prozent
#define REST_WALL_PCT 42
#define REST_SLING_PCT 60
#define REST_POST_PCT 55
#define REST_FLIPPER_PCT 28
// Reibung nach Coulomb: Der tangentiale Impuls ist hoechstens dieser Anteil
// des Normalimpulses. Ein fester Prozentsatz je Kontakt waere falsch, weil
// eine an der Bande anliegende Kugel in jedem Substep einen Kontakt hat und
// damit von der Substep-Rate gebremst wuerde statt von der Physik: Im
// Pruefstand kam eine abgeschossene Kugel deshalb nur bis zur halben Hoehe.
#define FRICTION_MU_PCT 12
#define ROLL_DAMP_PER_S_PCT 6     // leichte Dauerdaempfung, sonst rollt die Kugel ewig

#define BUMPER_KICK_PX_S 340      // aktiver Stoss des Bumpers zusaetzlich zum Abprall
#define SLING_KICK_PX_S 210       // Gummi der Slingshots
#define BUMPER_COOLDOWN_MS 120    // sonst zaehlt ein Kontakt mehrfach

// Anti-Tunneling: kein Teilschritt weiter als das hier, damit eine 6 px grosse
// Kugel keine unendlich duenne Wand ueberspringen kann. Bei 1500 px/s und 5 ms
// waeren es 7,5 px, also teilt der Schritt sich dann von selbst in sieben.
#define MAX_STEP_Q4 20            // 20/16 px = 1,25 px

// ------------------------------------------------------------------- Flipper
#define FLIPPER_LEN_PX 46
#define FLIPPER_SWING_DEG 30      // Ruhe +30 Grad, aktiv -30 Grad
// Hubzeit des Flippers. Sie allein bestimmt, wie hart er schlaegt: Der
// Uebertrag kommt aus der Winkelgeschwindigkeit. Zusammen mit dem gesenkten
// Grundtempo etwas verlangsamt, damit ein Schlag die Kugel nicht quer ueber
// den ganzen Tisch schiesst.
#define FLIPPER_UP_MS 78          // Zeit von Ruhe bis Anschlag oben (Konzept: rund 60 ms)
#define FLIPPER_DOWN_MS 85        // zurueck faellt er langsamer (Feder statt Spule)
#define FLIPPER_L_PIVOT_X 44
#define FLIPPER_L_PIVOT_Y 196
#define FLIPPER_R_PIVOT_X 138
#define FLIPPER_R_PIVOT_Y 196
// Zangengriff: Auf Back gibt es weder Rohevents noch wiederholende Klicks
// (beides im Emulator gemessen, siehe docs/emulator-befund.md), nur den
// gewoehnlichen Klick beim Loslassen. Der linke Flipper bleibt danach so lange
// oben und faellt dann von selbst zurueck.
#define BACK_HOLD_MS 170

// ------------------------------------------------------------------- Plunger
#define PLUNGER_X 183
#define PLUNGER_REST_Y 214
#define PLUNGER_PULL_MAX_PX 60
#define PLUNGER_TICK_PX 10        // pro 10 px Zugweg ein Ratschenimpuls
#define PLUNGER_TICKS_MAX 6
#define PLUNGER_SKILL_TICKS 4     // Skill-Shot aus dem Konzept
// Untergrenze: Die Abschussbahn ist rund 160 px hoch, dafuer braucht es
// mindestens sqrt(2*g*h), bei 500 px/s^2 also 400 px/s. Darunter rollt die
// Kugel zurueck und der Spieler sitzt fest. Gemessen mit tools/hosttest,
// das jeden Wert des Bereichs einzeln durchprobiert.
#define PLUNGER_MIN_PX_S 440
#define PLUNGER_MAX_PX_S 820
#define PLUNGER_LANE_X0 168       // Zone, in der eine Zieh-Geste als Plunger gilt
#define PLUNGER_LANE_Y0 120

// ------------------------------------------------------------------- Magnetfinger
#define MAG_RADIUS_PX 64
// Die Auslegung des Konzepts (r0 = 14 px, Spitze 2400 px/s^2) traegt die Kugel
// nur bis 21 px Abstand, also vollstaendig unter der Fingerkuppe: Der Spieler
// muesste blind halten, genau die Hauptkritik der Jury. Ein weicher Kern von
// Kuppengroesse mit flacherer Spitze verschiebt die Traggrenze nach aussen,
// auf rund 45 px, also an den sichtbaren Rand der Kuppe. Nachgerechnet und
// gemessen in tools/hosttest (Test "Magnet traegt bis ausserhalb der Kuppe").
#define MAG_SOFT_PX 40            // r0 in 1/(r^2 + r0^2)
#define MAG_ACCEL_MAX_PX_S2 1150  // Spitze, rund das 2,3-Fache der Schwerkraft
#define MAG_DEAD_Y 172            // unterste 56 px: kein Magnet, kein Endlos-Save
#define MAG_CHARGE_MAX 100
#define MAG_DRAIN_PER_S 25
#define MAG_RECHARGE_PER_S 6      // im Prototyp laedt der Magnet langsam von selbst nach
#define MAG_GRAB_DIST_PX 12       // Select plus Finger ueber der Kugel = fangen
#define MAG_GRAB_COST 100         // der Notwurf kostet die volle Ladung

// Fingerkuppe: rund 10 mm bei 202 ppi. Das ist die Zahl, um die es der Jury
// geht, und die Verdeckungsstatistik misst genau damit.
#define FINGER_TIP_R_PX 40

// LRA-Geigerzaehler: Tickabstand kodiert die Entfernung unter der Kuppe
#define GEIGER_SLOW_MS 400
#define GEIGER_FAST_MS 60
#define GEIGER_PULSE_MS 10

// ------------------------------------------------------------------- Haptik
#define LRA_FLIPPER_MS 15
#define LRA_BUMPER_MS 10
#define LRA_SLING_MS 8
#define LRA_RATCHET_MS 8
#define LRA_DRAIN_MS 400
#define LRA_MIN_GAP_MS 40         // Ratenbegrenzung aus dem Konzept
#define LRA_GUARD_MS 20           // Nachlauf, in dem der Motor als belegt gilt

// ------------------------------------------------------------------- Beschleunigung
#define ACCEL_RATE_HZ 50
#define ACCEL_BATCH 2             // 25 Rueckrufe pro Sekunde
#define NUDGE_HP_WIN_MS 400       // Fenster des gleitenden Mittels (Hochpass)
#define NUDGE_THRESH_MG 150
#define NUDGE_IMPULSE_PX_S 260    // Stossstaerke bei doppelter Schwelle
#define NUDGE_IMPULSE_MAX_PX_S 420 // Deckel: ein Schlag aufs Gehaeuse ist kein Katapult
#define TILT_BOB_PER_NUDGE_DIV 8   // Anteil des Stossbetrags, der in den Bob geht
#define TILT_BOB_NUDGE_MAX 300     // ... hoechstens so viel pro Stoss
#define NUDGE_COOLDOWN_MS 140
#define TILT_LEAN_MAX_MG 150      // Neigung verschiebt die Schwerkraft um hoechstens das
#define TILT_LEAN_GAIN_PCT 15     // ... und zwar um 15 Prozent der Schwerkraft
#define TILT_BOB_WARN 600
#define TILT_BOB_MAX 1000
#define TILT_BOB_DECAY_PCT_S 30
#define TILT_TAP_COST 250
#define TAP_KICK_PX_S 300
#define VIB_MASK_MS 60            // nach jedem Vibrationsaufruf keine Stossauswertung
#define BUTTON_MASK_MS 80         // Tastendruecke loesen im Accel ebenfalls Spitzen aus

// ------------------------------------------------------------------- Ton (nur als Last)
// Phase 1 macht keine Musik. Der Stream laeuft trotzdem mit, weil die Frage
// lautet, ob Physik plus Rendern plus Ton zusammen durchhalten. Werte und
// Buchhaltung stammen aus pebble/schwebung.
#define AUDIO_RATE_HZ 16000
#define AUDIO_BYTES_PER_MS 32
#define AUDIO_BLOCK_SAMPLES 256
#define AUDIO_BLOCK_BYTES (AUDIO_BLOCK_SAMPLES * 2)
#define AUDIO_TICK_MS 8
#define AUDIO_TARGET_QUEUE_MS 160
#define AUDIO_MAX_BLOCKS_PER_TICK 6
#define AUDIO_VOLUME 70
#define AUDIO_TOPUP_MS 130
#define AUDIO_RING_BYTES 8192
#define AUDIO_STALL_MS 300
#define AUDIO_LEAD_MIN_MS 24
#define AUDIO_LEAD_MAX_MS 240
#define AUDIO_LEAD_STEP_MS 16
#define TONE_HZ 220               // leiser Dauerton als Last, kein Spielklang

// ------------------------------------------------------------------- Touch
#define TOUCH_STALE_MS 400        // ohne Ereignis gilt der Finger als abgehoben

// ------------------------------------------------------------------- Tempo
// Vier Stufen fuer den Spieltest am Handgelenk, umschaltbar im MESS-Bildschirm.
// Die Stufe skaliert Schwerkraft, Tischneigung und Magnetkraft gemeinsam:
// Nur so bleibt die Traggrenze des Magneten dort, wo sie hingehoert, naemlich
// knapp ausserhalb der Fingerkuppe.
#define SPEED_STEPS 4
#define SPEED_DEFAULT_IDX 2       // 100 Prozent
