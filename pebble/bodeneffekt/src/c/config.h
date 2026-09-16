#pragma once
#include <pebble.h>

// BODENEFFEKT, Phase 1: Voxel-Space-Renderer, prozedurale Welt, Distanz-Nebel,
// Roll ueber Horizontverschiebung, und Messung von Anfang an.
// Plattform emery (Pebble Time 2, 200 x 228, 8 Bit, 240 MHz).
//
// Einheiten: Weltkoordinaten in Zellen (die Karte ist 128 x 128 Zellen breit
// und kachelt), Hoehen in denselben Zellen-Einheiten 0..255, Zeiten in
// Millisekunden, Winkel im Pebble-TRIG-Raster (TRIG_MAX_ANGLE = 65536).
// Festkomma: Weltposition 16.16, Hoehen und Tiefen 24.8.

#define BE_VERSION "0.1"

// ---------------------------------------------------------------- Display
#define SCR_W 200
#define SCR_H 228
#define SCR_CX 100
#define HUD_H 28                      // unteres Band, wird abgedunkelt statt gefuellt
#define HUD_Y0 (SCR_H - HUD_H)

// ---------------------------------------------------------------- Karte
#define MAP_BITS 7
#define MAP_SIZE (1 << MAP_BITS)      // 128
#define MAP_MASK (MAP_SIZE - 1)
#define MAP_CELLS (MAP_SIZE * MAP_SIZE)

// Wasserspiegel und Canyon: die Strecke soll immer eine fliegbare Schlucht haben
#define WATER_LEVEL 44
#define CANYON_FLOOR 46
#define CANYON_HALFWIDTH 26
#define CANYON_AMPLITUDE 34           // seitlicher Ausschlag des Talwegs

// ---------------------------------------------------------------- Renderer
// SCALE_H und das Sichtfeld haengen zusammen: fuer quadratische Pixel gilt
// tan(halbes Sichtfeld) = (SCR_W / 2) / SCALE_H. Mit 180 sind das 58 Grad.
#define SCALE_H 180
#define FOV_NUM SCR_CX                // Zaehler von tan(halbes Sichtfeld)
#define FOV_DEN SCALE_H

// Strahlen je Bild: 200 (ein Pixel je Spalte) oder als Rueckfallebene 100
// mit doppelter Spaltenbreite. Comanche hat dasselbe getan.
#define RAYS_FULL 200
#define RAYS_HALF 100

#define RAY_Z_START 4                 // naeher als 4 Zellen wird nichts abgetastet
#define RAY_DZ_START 256              // 1,0 Zellen in 24.8
#define RAY_DZ_GROW 5                 // +0,02 Zellen pro Schritt in 24.8
#define RAY_MAX_STEPS 160             // reicht fuer Sichtweite 260

// Sichtweiten (Nebeldichte) als gemeinsame Stellschraube fuer Schwierigkeit
// und Rechenbudget. Index 0 ist der Standard.
#define SIGHT_COUNT 3
#define SIGHT_FAR 200
#define SIGHT_MID 160
#define SIGHT_NEAR 120

#define FOG_LEVELS 16                 // Nebelstufen der 2D-Tabelle
#define PAL_COLORS 64                 // gueltige GColor8-Werte

#define HORIZON_BASE 104              // Bildzeile des Horizonts bei Nicklage 0
#define ROLL_MAX_DEG 15               // Lesbarkeit auf 1,5 Zoll
#define ROLL_RATE_DEG_S 45            // wie schnell sich der Roll der Eingabe naehert

// Verfolgerkamera. Die Zahlen sind keine Geschmacksfrage, sondern folgen aus
// der Bildaufteilung: der Bodenschatten muss genau im Bodeneffekt-Fenster
// (2 bis 12 Zellen ueber Grund) zwischen Rumpf und Bildunterkante wandern.
// Bildzeile des Bodens = Horizont + (agl + CAM_UP) * SCALE_H / CAM_BACK.
// Mit 32 und 8 liegt der Schatten bei 2 Zellen elf Pixel unter dem Rumpf und
// verlaesst das Bild bei rund 11 Zellen, also genau am oberen Rand des
// Bodeneffekt-Fensters: der verschwindende Schatten ist die Ansage, dass der
// Boost nicht mehr laedt. Weiter zurueck (44 Zellen) waere der Schatten laenger
// sichtbar, aber in einer 26 Zellen breiten Schlucht fuellt dann die Wand vor
// der Kamera das halbe Bild.
#define CAM_BACK_CELLS 32
#define CAM_UP_CELLS 8
#define GLIDER_ROW (HORIZON_BASE + (CAM_UP_CELLS * SCALE_H) / CAM_BACK_CELLS)

// Sonne
#define SUN_RADIUS 15
#define SUN_GLOW_R 22

// ---------------------------------------------------------------- Flug
#define SPEED_CELLS_S 25              // Grundtempo in Zellen pro Sekunde
#define TURN_DEG_S_PER_ROLL 130       // Kursaenderung pro Sekunde bei vollem Roll
#define ALT_MIN_AGL 128               // 0,5 Zellen ueber Grund in 24.8: harte Untergrenze
#define ALT_MAX 220                   // Maximalhoehe, gegen den Blick auf die Kachelwiederholung
#define GROUND_HIT_AGL (2 * 256)      // unter 2 Zellen ueber Grund = Bodenkontakt
#define EFFECT_LO_AGL (2 * 256)       // Bodeneffekt-Fenster, untere Grenze
#define EFFECT_HI_AGL (12 * 256)      // obere Grenze
#define HIT_FLASH_MS 150

// Profil A (Fingerstick): Hoehenrate und Rollkommando aus dem Versatz
#define STICK_DEAD_PX 6
#define STICK_SAT_PX 40
#define STICK_CLIMB_CELLS_S 22        // volle Auslenkung = 22 Zellen pro Sekunde
#define PRECISION_PCT 50              // Select gehalten halbiert die Empfindlichkeit
#define TRIM_STEP 256                 // Up/Down trimmen um 1 Zelle/s (24.8)
#define TRIM_MAX (6 * 256)            // hoechstens 6 Zellen pro Sekunde

// Profil B (Tasten, Flappy-Hoehenmodell)
#define FLAP_ACC_CELLS_S2 90          // Steigbeschleunigung, solange Select liegt
#define FLAP_GRAV_CELLS_S2 60         // Sinkbeschleunigung
#define FLAP_VZ_MAX_CELLS_S 26
#define BTN_ROLL_STEP 70              // Rollkommando pro Tastendruck, 0..256

// ---------------------------------------------------------------- Zeiten
#define GAME_TICK_MS 20
#define RENDER_TICK_MS 33             // Startwert; die Messung setzt das Ziel
#define LOG_TICK_MS 1000
#define PANEL_TEST_FRAMES 300
#define PANEL_VARIANTS 3              // 0 Vollbild, 1 zehn Zeilen, 2 echte Voxel-Szene
