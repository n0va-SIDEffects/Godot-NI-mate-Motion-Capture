// Physik-Pruefstand auf dem Rechner. Uebersetzt dieselben Quellen wie die App
// (physics.c, table.c) gegen einen Mini-Ersatz fuer pebble.h und prueft, was
// sich auf der Uhr nur schwer beobachten laesst:
//   1. Kein Tunneling, auch bei hohem Tempo und schraeg auf duenne Banden.
//   2. Die Kugel verlaesst die Abschussbahn und kommt nicht zurueck.
//   3. Die Flipper uebertragen Winkelgeschwindigkeit.
//   4. Gleiche Eingaben ergeben Bit fuer Bit dieselbe Bahn (Determinismus).
#include <stdio.h>
#include "../../src/c/physics.h"

static int s_fails;

static void check(const char *name, bool ok, const char *detail) {
  printf("%-46s %s%s%s\n", name, ok ? "ok" : "FEHLER",
         detail && *detail ? "  " : "", detail ? detail : "");
  if (!ok) {
    s_fails++;
  }
}

static void run(World *w, int ms) {
  for (int i = 0; i < ms / PHYS_DT_MS; i++) {
    phys_substep(w);
  }
}

// 1. Tunneling: Kugeln mit extremem Tempo in alle Richtungen schiessen und
// zaehlen, ob eine den Tisch verlaesst.
static void test_tunneling(void) {
  int escapes = 0;
  int lost = 0;
  for (int speed = 200; speed <= 1500; speed += 100) {
    for (int dir = 0; dir < 16; dir++) {
      World w;
      phys_init(&w);
      Ball *b = phys_spawn_lane(&w);
      b->p = vec_make(FX_FROM_INT(91), FX_FROM_INT(60));
      b->in_lane = false;
      int32_t ang = (dir * TRIG_MAX_ANGLE) / 16;
      Vec v = vec_rot(vec_make(FX_FROM_INT(speed), 0), ang);
      b->v = v;
      run(&w, 3000);
      escapes += w.st.escapes;
      // Abgeflossen ist erlaubt (unten offen), ausserhalb des Tisches nicht.
      if (b->alive) {
        int32_t x = FX_TO_INT(b->p.x), y = FX_TO_INT(b->p.y);
        if (x < 0 || x > TABLE_W || y < 0 || y > DRAIN_Y) {
          lost++;
        }
      }
    }
  }
  char d[80];
  snprintf(d, sizeof(d), "Ausbrueche %d, verloren %d", escapes, lost);
  check("Kein Tunneling bei 200 bis 1500 px/s", escapes == 0 && lost == 0, d);
}

// 2. Abschussbahn: Die Kugel muss den Kanal verlassen und darf nicht
// zurueckfallen. Das Einwegtor oben ist genau dafuer da.
static void test_plunger_lane(void) {
  int back_in_lane = 0;
  int never_left = 0;
  for (int speed = PLUNGER_MIN_PX_S; speed <= PLUNGER_MAX_PX_S; speed += 40) {
    World w;
    phys_init(&w);
    Ball *b = phys_spawn_lane(&w);
    b->v = vec_make(0, -FX_FROM_INT(speed));
    bool left = false;
    for (int i = 0; i < 4000 / PHYS_DT_MS; i++) {
      phys_substep(&w);
      if (!b->alive) {
        break;
      }
      int32_t x = FX_TO_INT(b->p.x);
      int32_t y = FX_TO_INT(b->p.y);
      if (x < PLUNGER_LANE_X0 - 6) {
        left = true;
      } else if (left && x > 176 && y > 100) {   // 176 ist rechts der Trennwand,
                                                 // die rechte Outlane liegt davor
        back_in_lane++;
        break;
      }
    }
    if (!left) {
      never_left++;
      printf("   %4d px/s: Kugel bleibt in der Bahn\n", speed);
    }
  }
  char d[80];
  snprintf(d, sizeof(d), "nie heraus %d, zurueckgefallen %d", never_left, back_in_lane);
  check("Abschuss verlaesst die Bahn und bleibt draussen",
        never_left == 0 && back_in_lane == 0, d);
}

// 2a. Derselbe Abschuss, aber mit geneigtem Tisch und der Kugel an der
// Bandenwand. Genau hier fiel auf, dass eine feste Reibung je Kontakt die
// Kugel bremst, sobald sie anliegt: Sie kam nur bis zur halben Hoehe und fiel
// in die Bahn zurueck. Mit Coulomb-Reibung haengt die Bremsung am
// Normalimpuls und damit an der Physik.
static void test_plunger_tilted(void) {
  int stuck = 0;
  int32_t worst = 0;
  for (int gx = -105; gx <= 105; gx += 35) {
    for (int sx = 176; sx <= 190; sx += 2) {
      World w;
      phys_init(&w);
      w.gravity = vec_make(FX_FROM_INT(gx), FX_FROM_INT(GRAVITY_PX_S2));
      Ball *b = phys_spawn_lane(&w);
      b->p = vec_make(FX_FROM_INT(sx), FX_FROM_INT(PLUNGER_REST_Y));
      b->v = vec_make(0, -FX_FROM_INT(PLUNGER_MIN_PX_S));
      int32_t top = 999;
      bool left = false;
      for (int i = 0; i < 4000 / PHYS_DT_MS && b->alive; i++) {
        phys_substep(&w);
        int32_t y = FX_TO_INT(b->p.y);
        if (y < top) {
          top = y;
        }
        if (FX_TO_INT(b->p.x) < PLUNGER_LANE_X0 - 6) {
          left = true;
          break;
        }
      }
      if (!left) {
        stuck++;
        worst = top;
      }
    }
  }
  char d[90];
  snprintf(d, sizeof(d), "%d Faelle stecken geblieben, hoechster Punkt dabei y=%ld",
           stuck, (long)worst);
  check("Abschuss traegt auch an der Bande und bei Neigung", stuck == 0, d);
}

// 2b. Einwegtor: Keine Kugel darf von oben in die Abschussbahn zurueck. Der
// Flug wird an vielen Stellen im oberen Tischbereich und in vielen Richtungen
// gestartet, weil der schwierige Fall die Ecke rechts oben ist, in der Bande,
// Tor und Aussenwand zusammenkommen.
static void test_gate_oneway(void) {
  int back_in = 0;
  int worst_x = 0, worst_y = 0, worst_speed = 0, worst_dir = 0;
  // Startpunkte nur im Tischinneren, links der Abschussbahn. Rechts davon
  // liegt teils schon der Bereich hinter der Aussenwand, und eine Kugel, die
  // dort startet, sagt nichts ueber das Tor aus.
  for (int sx = 30; sx <= 165; sx += 9) {
    for (int sy = 16; sy <= 60; sy += 8) {
      for (int speed = 100; speed <= 1200; speed += 220) {
        for (int dir = 0; dir < 12; dir++) {
          World w;
          phys_init(&w);
          Ball *b = phys_spawn_lane(&w);
          b->p = vec_make(FX_FROM_INT(sx), FX_FROM_INT(sy));
          b->in_lane = false;
          b->v = vec_rot(vec_make(FX_FROM_INT(speed), 0),
                         (dir * TRIG_MAX_ANGLE) / 12);
          for (int i = 0; i < 5000 / PHYS_DT_MS; i++) {
            phys_substep(&w);
            if (!b->alive) {
              break;
            }
            int32_t bx = FX_TO_INT(b->p.x);
            if (bx > 176 && bx < 192 && FX_TO_INT(b->p.y) > 120) {
              back_in++;
              worst_x = sx; worst_y = sy; worst_speed = speed; worst_dir = dir;
              break;
            }
          }
        }
      }
    }
  }
  char d[120];
  snprintf(d, sizeof(d), "%d Rueckkehrer, zuletzt von %d,%d mit %d px/s Richtung %d/12",
           back_in, worst_x, worst_y, worst_speed, worst_dir);
  check("Einwegtor laesst keine Kugel zurueck in die Bahn", back_in == 0, d);
}

// 2c. Spielbarkeit: Eine Kugel, die oben in den Tisch faellt, soll meistens
// bei den Flippern ankommen und nicht auf dem kuerzesten Weg abfliessen. Das
// ist kein Physiktest, sondern ein Tischtest: Ohne Rueckfuehrungen an den
// Seitenwaenden war jeder Ball nach rund einer Sekunde weg.
static void test_reaches_flippers(void) {
  int total = 0, reached = 0;
  int32_t ms_sum = 0;
  for (int sx = 20; sx <= 165; sx += 5) {
    World w;
    phys_init(&w);
    Ball *b = phys_spawn_lane(&w);
    b->p = vec_make(FX_FROM_INT(sx), FX_FROM_INT(30));
    b->v = vec_make(0, 0);
    b->in_lane = false;
    total++;
    bool hit = false;
    int i = 0;
    for (; i < 8000 / PHYS_DT_MS && b->alive; i++) {
      phys_substep(&w);
      int32_t x = FX_TO_INT(b->p.x), y = FX_TO_INT(b->p.y);
      if (y > 176 && x > 30 && x < 150) {
        hit = true;
        break;
      }
    }
    if (hit) {
      reached++;
      ms_sum += i * PHYS_DT_MS;
    }
  }
  char d[90];
  snprintf(d, sizeof(d), "%d von %d erreichen die Flipperzone, im Mittel nach %ld ms",
           reached, total, (long)(reached ? ms_sum / reached : 0));
  check("Kugeln von oben erreichen die Flipper", reached * 4 >= total * 3, d);
}

// 3. Flipper: Eine ruhende Kugel auf dem Flipper muss durch den Schlag
// deutlich schneller werden, als sie durch Schwerkraft werden koennte.
static void test_flipper_transfer(void) {
  World w;
  phys_init(&w);
  Ball *b = phys_spawn_lane(&w);
  // Kugel auf den linken Flipper legen, knapp vor der Spitze
  Vec tip = phys_flipper_tip(&w.table.flip[0]);
  b->p = vec_make(tip.x - FX_FROM_INT(6), tip.y - FX_FROM_INT(6));
  b->v = vec_make(0, 0);
  b->in_lane = false;
  run(&w, 200);                       // liegenbleiben lassen
  fix before = vec_len(b->v);
  phys_set_flipper(&w, 0, true);
  run(&w, 120);                       // Schlag
  fix after = vec_len(b->v);
  char d[80];
  snprintf(d, sizeof(d), "vorher %ld px/s, nachher %ld px/s",
           (long)FX_TO_INT(before), (long)FX_TO_INT(after));
  check("Flipper uebertraegt Winkelgeschwindigkeit",
        after > before + FX_FROM_INT(150) && w.st.contacts_flip > 0, d);
}

// 4. Determinismus: Zwei gleiche Laeufe muessen Bit fuer Bit gleich enden.
// Darauf bauen Replay, Tagestisch und der Regressionstest des Konzepts.
static void test_determinism(void) {
  Vec end[2];
  Vec vel[2];
  for (int k = 0; k < 2; k++) {
    World w;
    phys_init(&w);
    Ball *b = phys_spawn_lane(&w);
    b->v = vec_make(0, -FX_FROM_INT(700));
    for (int i = 0; i < 2000; i++) {
      if (i == 200) phys_set_flipper(&w, 0, true);
      if (i == 260) phys_set_flipper(&w, 0, false);
      if (i == 400) phys_set_flipper(&w, 1, true);
      if (i == 470) phys_set_flipper(&w, 1, false);
      if (i == 600) phys_nudge(&w, FX_FROM_INT(120), 0);
      phys_substep(&w);
    }
    end[k] = w.ball[0].p;
    vel[k] = w.ball[0].v;
  }
  char d[100];
  snprintf(d, sizeof(d), "%ld,%ld gegen %ld,%ld", (long)end[0].x, (long)end[0].y,
           (long)end[1].x, (long)end[1].y);
  check("Gleiche Eingaben, gleiche Bahn (Fixed-Point)",
        end[0].x == end[1].x && end[0].y == end[1].y &&
        vel[0].x == vel[1].x && vel[0].y == vel[1].y, d);
}

// 5. Kosten: Wie viele Kollisionstests kostet ein Substep im Mittel? Das ist
// die Groesse, die sich auf 180 Segmente hochrechnen laesst.
static void test_cost(void) {
  World w;
  phys_init(&w);
  for (int i = 0; i < MAX_BALLS; i++) {
    Ball *b = phys_spawn_lane(&w);
    if (!b) break;
    b->p = vec_make(FX_FROM_INT(40 + i * 40), FX_FROM_INT(40 + i * 20));
    b->v = vec_make(FX_FROM_INT(700 - i * 200), FX_FROM_INT(500 + i * 150));
    b->in_lane = false;
  }
  w.table.flip[0].up = true;
  w.table.flip[1].up = true;
  for (int i = 0; i < 4000; i++) {
    if ((i & 63) == 0) {
      w.table.flip[0].up = !w.table.flip[0].up;
      w.table.flip[1].up = !w.table.flip[1].up;
    }
    for (int k = 0; k < MAX_BALLS; k++) {
      if (!w.ball[k].alive) {
        w.ball[k].alive = true;
        w.ball[k].p = vec_make(FX_FROM_INT(91), FX_FROM_INT(40));
        w.ball[k].v = vec_make(FX_FROM_INT(600), FX_FROM_INT(400));
      }
    }
    phys_substep(&w);
  }
  printf("   Tisch: %u Segmente, %u Kreise, 2 Flipper\n",
         w.table.seg_count, w.table.circ_count);
  printf("   %lu Substeps, %lu Tests, also %lu Tests je Substep bei %d Kugeln\n",
         (unsigned long)w.st.substeps, (unsigned long)w.st.narrow_tests,
         (unsigned long)(w.st.narrow_tests / w.st.substeps), MAX_BALLS);
  printf("   Teilschritte hoechstens %lu, Ausbrueche %lu\n",
         (unsigned long)w.st.splits_max, (unsigned long)w.st.escapes);
  check("Stresstest ohne Ausbrueche", w.st.escapes == 0, "");
}

// 6. Magnetfinger: Ab welchem Abstand traegt der Magnet die Kugel gegen die
// Schwerkraft? Das ist die Hauptkritik der Jury als Zahl. Die Fingerkuppe
// verdeckt rund 40 px Radius; liegt die Traggrenze darunter, muss man die
// Kugel blind halten, liegt sie darueber, sieht man sie am Kuppenrand.
static void test_magnet_reach(void) {
  int carry = -1;
  for (int d = 2; d <= MAG_RADIUS_PX; d++) {
    World w;
    phys_init(&w);
    Ball *b = phys_spawn_lane(&w);
    b->p = vec_make(FX_FROM_INT(91), FX_FROM_INT(80 + d));
    b->v = vec_make(0, 0);
    b->in_lane = false;
    w.mag_on = true;
    w.mag_pos = vec_make(FX_FROM_INT(91), FX_FROM_INT(80));
    w.mag_accel = FX_FROM_INT(MAG_ACCEL_MAX_PX_S2);
    fix y0 = b->p.y;
    run(&w, 150);
    if (b->p.y < y0) {
      carry = d;      // Kugel steigt: der Magnet traegt sie noch
    }
  }
  printf("   Zugkraft: 0 px %d, 20 px %d, 40 px %d, 64 px %d px/s^2 (Schwerkraft %d)\n",
         MAG_ACCEL_MAX_PX_S2,
         (int)((long)MAG_ACCEL_MAX_PX_S2 * MAG_SOFT_PX * MAG_SOFT_PX /
               (20 * 20 + MAG_SOFT_PX * MAG_SOFT_PX)),
         (int)((long)MAG_ACCEL_MAX_PX_S2 * MAG_SOFT_PX * MAG_SOFT_PX /
               (40 * 40 + MAG_SOFT_PX * MAG_SOFT_PX)),
         (int)((long)MAG_ACCEL_MAX_PX_S2 * MAG_SOFT_PX * MAG_SOFT_PX /
               (64 * 64 + MAG_SOFT_PX * MAG_SOFT_PX)),
         GRAVITY_PX_S2);
  char d[110];
  snprintf(d, sizeof(d), "traegt bis %d px, Fingerkuppe verdeckt %d px", carry, FINGER_TIP_R_PX);
  // Die Traggrenze muss ausserhalb der Fingerkuppe liegen, sonst ist der
  // Magnet nur blind bedienbar.
  check("Magnet traegt bis ausserhalb der Fingerkuppe", carry > FINGER_TIP_R_PX, d);
}

int main(void) {
  printf("SILBERKUGEL, Physik-Pruefstand\n\n");
  test_tunneling();
  test_plunger_lane();
  test_plunger_tilted();
  test_gate_oneway();
  test_reaches_flippers();
  test_flipper_transfer();
  test_determinism();
  test_magnet_reach();
  test_cost();
  printf("\n%s\n", s_fails == 0 ? "Alles bestanden." : "FEHLER vorhanden.");
  return s_fails == 0 ? 0 : 1;
}
