/* Gameboy - OSTWALD EDITION, Retro-Spiele fuer den Geschichtsvortrag
 * Wilhelm-Ostwald-Gymnasium Leipzig, Klasse 5/3
 * Gebaut mit GBDK-2020 fuer den monochromen Game Boy.
 */
#include <gb/gb.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <rand.h>
#include <gbdk/console.h>
#include "tiles.h"
#include "font5x7.h"
#include "logo.h"

/* ---------- kleine Helfer ---------- */

/* ---------- Sound: kleiner Melodie-Spieler, laeuft im VBlank-Interrupt ---------- */
/* Tonhoehe als Gameboy-Registerwert: 2048 - 131072 / Hz */
#define C4 1547
#define E4 1651
#define G4 1714
#define C5 1797
#define E5 1849
#define G5 1881
#define B5 1915
#define C6 1923
#define D6 1936
#define E6 1949
#define G6 1964
#define C7 1985
#define REST 0

typedef struct { uint16_t f; uint8_t len; } Note;
static const Note *volatile snd_ptr;
static volatile uint8_t snd_cnt, snd_env;

static const Note M_BOOT[]   = { {C6, 5}, {C7, 12}, {0, 0} };          /* das klassische "Pling" */
static const Note M_MOVE[]   = { {E6, 3}, {0, 0} };
static const Note M_SELECT[] = { {G5, 4}, {C6, 4}, {G6, 10}, {0, 0} };
static const Note M_COIN[]   = { {B5, 4}, {E6, 14}, {0, 0} };
static const Note M_BATT[]   = { {D6, 3}, {G6, 3}, {C7, 8}, {0, 0} };
static const Note M_LEVEL[]  = { {C5, 5}, {E5, 5}, {G5, 5}, {C6, 5}, {G5, 4}, {C6, 18}, {0, 0} };
static const Note M_START[]  = { {C5, 4}, {E5, 4}, {G5, 4}, {C6, 12}, {0, 0} };
static const Note M_RIGHT[]  = { {C6, 5}, {G6, 16}, {0, 0} };
static const Note M_WRONG[]  = { {E4, 8}, {C4, 24}, {0, 0} };
static const Note M_OVER[]   = { {G5, 9}, {E5, 9}, {C5, 9}, {G4, 9}, {REST, 4}, {C4, 36}, {0, 0} };
static const Note M_WIN[]    = { {C5, 6}, {E5, 6}, {G5, 6}, {C6, 8}, {REST, 4}, {G5, 6}, {C6, 30}, {0, 0} };

static void snd_note(uint16_t f, uint8_t env) {
    if (!f) { NR12_REG = 0x00; NR14_REG = 0x80; return; }
    NR10_REG = 0x00; NR11_REG = 0x80; NR12_REG = env;
    NR13_REG = (uint8_t)(f & 0xFF); NR14_REG = 0x80 | (uint8_t)(f >> 8);
}

static void snd_tick(void) {
    if (!snd_ptr) return;
    if (snd_cnt) { snd_cnt--; return; }
    if (!snd_ptr->len) { snd_ptr = 0; return; }
    snd_note(snd_ptr->f, snd_env);
    snd_cnt = snd_ptr->len - 1;
    snd_ptr++;
}

static void play(const Note *m, uint8_t env) {
    disable_interrupts();
    snd_ptr = m; snd_cnt = 0; snd_env = env;
    enable_interrupts();
}

static void boom(void) {   /* Rauschen auf Kanal 4 */
    NR41_REG = 0x00; NR42_REG = 0xF4; NR43_REG = 0x61; NR44_REG = 0x80;
}

static void wait_frames(uint8_t n) { while (n--) wait_vbl_done(); }

static void wait_release(void) { while (joypad()) wait_vbl_done(); }

static uint8_t wait_press(uint8_t mask) {
    uint8_t keys;
    wait_release();
    while (!(keys = (joypad() & mask))) wait_vbl_done();
    wait_release();
    return keys;
}

static void clear_screen(void) {
    HIDE_SPRITES;
    cls();
    gotoxy(0, 0);
}

static void put(uint8_t x, uint8_t y, uint8_t t) { set_bkg_tile_xy(x, y, t); }

static uint8_t glyph_tile[FONT_COUNT];

/* Direkter Kacheltext vermeidet das automatische Scrollen unten rechts. */
static void text(uint8_t x, uint8_t y, const char *s) {
    while (*s && x < 20) {
        uint8_t c = *s++;
        if (c < FONT_FIRST || c >= FONT_FIRST + FONT_COUNT) c = '?';
        put(x++, y, glyph_tile[c - FONT_FIRST]);
    }
}

/* ================================================================
 * SPIEL 1: QUELLEN-SNAKE
 * Die Schlange frisst Quellen. Jede Quelle zeigt ihre Quellenart.
 * ================================================================ */

#define MAXLEN 120
static uint8_t sx[MAXLEN], sy[MAXLEN], occupied[18][20];
static uint8_t head_slot, tail_slot;

static uint8_t next_slot(uint8_t slot) {
    return slot + 1 == MAXLEN ? 0 : slot + 1;
}
static uint8_t slen, dir, ndir, fx, fy, ftype, score, collected;

static const uint8_t food_tile[4] = { T_COIN, T_LETTER, T_CASS, T_FOTO };
static const char * const food_name[4] = {
    "MUENZE=SACHQUELLE   ",
    "BRIEF=SCHRIFTQUELLE ",
    "KASSETTE=TONQUELLE  ",
    "FOTO=BILDQUELLE     "
};

/* Spielfeld: x 1..18, y 3..16, Wand drumherum */
static void place_food(void) {
    do {
        fx = 1 + (rand() % 18);
        fy = 3 + (rand() % 14);
    } while (occupied[fy][fx]);
    ftype = rand() & 3;
    if (collected != 0x0F) {
        while (collected & (1u << ftype)) ftype = (ftype + 1) & 3;
    }
    put(fx, fy, food_tile[ftype]);
}

/* Level = Zeitalter. Zahl = Bilder pro Schritt (60 Bilder = 1 Sekunde) */
#define NLEVEL 5
static const uint8_t lv_speed[NLEVEL] = { 30, 22, 16, 11, 7 };
static const char * const lv_name[NLEVEL] = {
    "STEINZEIT ", "RITTERZEIT", "DAMPFLOK  ", "GAMEBOY   ", "ZUKUNFT   "
};
static uint8_t level;

static void snake_hud(void) {
    gotoxy(0, 0); printf("L%u %s  %3u", (unsigned)(level + 1), lv_name[level], (unsigned)score);
}

/* Auswahl-Bildschirm fuer Level / Schwierigkeit. Gibt 0..n-1 zurueck. */
static uint8_t choose(const char *title, const char * const *names, uint8_t n,
                      const char *h1, const char *h2) {
    uint8_t sel = 0, i, keys, prev = 0, old;
    clear_screen();
    text(3, 1, title);
    text(1, 3, "WAEHLE DEIN LEVEL:");
    for (i = 0; i < n; i++) {
        gotoxy(3, 5 + i * 2); printf("%u %s", (unsigned)(i + 1), names[i]);
    }
    text(1, 15, h1);
    text(1, 16, h2);
    text(0, 17, "A: SPIEL  B: ZURUECK");
    put(1, 5, T_ARROW);
    wait_release();
    while (1) {
        keys = joypad();
        old = sel;
        if ((keys & J_UP) && !(prev & J_UP) && sel) sel--;
        if ((keys & J_DOWN) && !(prev & J_DOWN) && sel + 1 < n) sel++;
        if (old != sel) {
            put(1, 5 + old * 2, 0);
            put(1, 5 + sel * 2, T_ARROW);
            play(M_MOVE, 0xA1);
        }
        prev = keys;
        if (keys & J_B) { wait_release(); return 0xFF; }
        if (keys & (J_A | J_START)) break;
        wait_vbl_done();
    }
    play(M_START, 0xF2);
    wait_release();
    return sel;
}

static uint8_t pause_game(uint8_t row) {
    text(0, row, "A: WEITER  B: MENUE ");
    return (wait_press(J_A | J_B | J_START) & J_B) != 0;
}

static void game_snake(void) {
    uint8_t i, x, y, speed, tick = 0, keys, eating, growing;
    const char *message = "FRISS DIE QUELLEN!  ";
    level = choose("QUELLEN-SNAKE", lv_name, NLEVEL, "ALLE 5 QUELLEN GEHT", "ES EIN LEVEL HOEHER");
    if (level == 0xFF) return;
    clear_screen();
    for (x = 0; x < 20; x++) { put(x, 2, T_WALL); put(x, 17, T_WALL); }
    for (y = 2; y < 18; y++) { put(0, y, T_WALL); put(19, y, T_WALL); }
    slen = 3; score = 0; collected = 0; dir = ndir = 0; /* 0=rechts 1=links 2=hoch 3=runter */
    memset(occupied, 0, sizeof occupied);
    tail_slot = 0; head_slot = slen - 1;
    for (i = 0; i < slen; i++) {
        sx[i] = 6 + i; sy[i] = 9;
        occupied[9][sx[i]] = 1;
        put(sx[i], 9, i == head_slot ? T_HEAD : T_BODY);
    }
    snake_hud();
    text(0, 1, message);
    place_food();
    speed = lv_speed[level];
    wait_release();

    while (1) {
        keys = joypad();
        if ((keys & J_RIGHT) && dir != 1) ndir = 0;
        else if ((keys & J_LEFT) && dir != 0) ndir = 1;
        else if ((keys & J_UP) && dir != 3) ndir = 2;
        else if ((keys & J_DOWN) && dir != 2) ndir = 3;
        if (keys & J_START) {
            if (pause_game(1)) return;
            text(0, 1, message);
        }
        wait_vbl_done();
        if (++tick < speed) continue;
        tick = 0;
        dir = ndir;

        x = sx[head_slot]; y = sy[head_slot];
        if (dir == 0) x++; else if (dir == 1) x--; else if (dir == 2) y--; else y++;

        eating = x == fx && y == fy;
        growing = eating && slen < MAXLEN;
        if (x == 0 || x == 19 || y == 2 || y == 17 ||
            (occupied[y][x] && (growing || x != sx[tail_slot] || y != sy[tail_slot]))) {
            boom();
            play(M_OVER, 0xF4);
            break;
        }

        if (eating) {
            collected |= 1u << ftype;
            play(M_COIN, 0xF2);
            score++;
            message = food_name[ftype];
            text(0, 1, message);
            if ((score % 5) == 0 && level < NLEVEL - 1) {
                level++;
                speed = lv_speed[level];
                play(M_LEVEL, 0xF2);
            }
            snake_hud();
        }
        if (growing) slen++;
        else {
            occupied[sy[tail_slot]][sx[tail_slot]] = 0;
            put(sx[tail_slot], sy[tail_slot], 0);
            tail_slot = next_slot(tail_slot);
        }
        put(sx[head_slot], sy[head_slot], T_BODY);
        head_slot = next_slot(head_slot);
        sx[head_slot] = x; sy[head_slot] = y; occupied[y][x] = 1;
        put(x, y, T_HEAD);
        if (eating) place_food();
    }

    wait_frames(40);
    clear_screen();
    text(5, 2, "GAME OVER!");
    gotoxy(2, 4); printf("DU HAST %u QUELLEN", (unsigned)score);
    text(2, 5, "GESAMMELT.");
    gotoxy(2, 7); printf("ZEIT: %s", lv_name[level]);
    text(1, 9, "DEINE QUELLEN:");
    if (!collected) text(1, 11, "NOCH KEINE.");
    y = 11;
    for (i = 0; i < 4; i++) {
        if (collected & (1u << i)) {
            put(0, y, food_tile[i]);
            text(1, y++, food_name[i]);
        }
    }
    text(1, 17, "A: ZUM MENUE");
    wait_press(J_START | J_A);
}

/* ================================================================
 * SPIEL 2: BATTERIE-REGEN
 * Der Gameboy faengt Batterien. Hausaufgaben kosten ein Leben.
 * ================================================================ */

#define NOBJ 4
static uint8_t ox[NOBJ], oy[NOBJ], otype[NOBJ], oactive[NOBJ];

static void draw_player(uint8_t px) {
    move_sprite(0, px + 8,  136);
    move_sprite(1, px + 16, 136);
    move_sprite(2, px + 8,  144);
    move_sprite(3, px + 16, 144);
}

/* Schwierigkeit: Fall-Verzoegerung (Bilder pro Pixel), Abstand neuer Dinge,
   Stromverbrauch (Bilder pro Prozent), Hausaufgaben-Chance (1 von x), Leben */
static const char * const rain_name[3] = { "LEICHT", "MITTEL", "SCHWER" };
static const uint8_t rain_fdelay[3] = { 3, 2, 1 };
static const uint8_t rain_spawn[3]  = { 80, 60, 45 };
static const uint8_t rain_drain[3]  = { 16, 11, 8 };
static const uint8_t rain_hw[3]     = { 6, 4, 3 };
static const uint8_t rain_lives[3]  = { 5, 3, 3 };


static void game_rain(void) {
    uint8_t px = 72, i, batt = 0, spawn = 0, keys, ftick = 0;
    uint8_t energy = 100, etick = 0, notice = 0, lost_life = 0;
    uint8_t counters_dirty = 1, energy_dirty = 1, old_px;
    uint8_t d = choose("BATTERIE-REGEN", rain_name, 3, "BATTERIEN FANGEN!", "HEFTEN AUSWEICHEN!");
    uint8_t lives, fdelay, spawnrate;
    if (d == 0xFF) return;
    lives = rain_lives[d];
    fdelay = rain_fdelay[d];
    spawnrate = rain_spawn[d];

    clear_screen();
    text(0, 0, "BATT:     LEBEN:");
    text(0, 1, "STROM:   %");
    for (i = 0; i < 20; i++) put(i, 17, T_WALL);
    set_sprite_tile(0, T_GB_TL); set_sprite_tile(1, T_GB_TR);
    set_sprite_tile(2, T_GB_BL); set_sprite_tile(3, T_GB_BR);
    for (i = 0; i < NOBJ; i++) { oactive[i] = 0; move_sprite(4 + i, 0, 0); }
    draw_player(px);
    SHOW_SPRITES;

    while (lives && energy) {
        keys = joypad();
        if (keys & J_START) {
            HIDE_SPRITES;
            if (pause_game(0)) return;
            SHOW_SPRITES;
            text(0, 0, "BATT:     LEBEN:    ");
            counters_dirty = 1;
        }
        old_px = px;
        if ((keys & J_LEFT) && px > 2) px -= 2;
        if ((keys & J_RIGHT) && px < 142) px += 2;
        if (px != old_px) draw_player(px);
        if (++etick >= rain_drain[d]) {
            etick = 0;
            energy--;
            energy_dirty = 1;
        }

        if (++spawn >= spawnrate) {
            spawn = 0;
            for (i = 0; i < NOBJ; i++) if (!oactive[i]) {
                oactive[i] = 1;
                oy[i] = 24;
                ox[i] = 4 + (rand() % 148);
                otype[i] = (rand() % rain_hw[d]) == 0;
                move_sprite(4 + i, ox[i] + 8, oy[i] + 16);
                set_sprite_tile(4 + i, otype[i] ? T_HOMEW : T_BATT);
                break;
            }
        }

        if (++ftick >= fdelay) ftick = 0;
        for (i = 0; i < NOBJ; i++) if (oactive[i]) {
            if (ftick == 0) oy[i]++;
            if (oy[i] > 112 && oy[i] < 136 && ox[i] + 8 > px && ox[i] < px + 16) {
                oactive[i] = 0; move_sprite(4 + i, 0, 0);
                counters_dirty = 1;
                notice = 24;
                lost_life = otype[i];
                if (otype[i]) {
                    if (lives) lives--;
                    boom();
                    play(M_WRONG, 0xD3);
                } else {
                    if (batt < 255) batt++;
                    play(M_BATT, 0xD2);
                    energy = (energy > 75) ? 100 : energy + 25;
                    energy_dirty = 1;
                    if (batt < 255 && (batt % 5) == 0) {
                        if (spawnrate > 28) spawnrate = spawnrate >= 34 ? spawnrate - 6 : 28;
                        if (fdelay > 1 && (batt % 10) == 0) {
                            fdelay--;
                            play(M_LEVEL, 0xD2);
                        }
                    }
                }
            } else if (oy[i] >= 136) {
                oactive[i] = 0; move_sprite(4 + i, 0, 0);
            } else if (!ftick) {
                move_sprite(4 + i, ox[i] + 8, oy[i] + 16);
            }
        }

        if (counters_dirty) {
            gotoxy(5, 0); printf("%3u", (unsigned)batt);
            gotoxy(16, 0); putchar('0' + lives);
            counters_dirty = 0;
        }
        if (energy_dirty) {
            gotoxy(6, 1); printf("%3u", (unsigned)energy);
            energy_dirty = 0;
        }
        if (notice == 24) text(12, 1, lost_life ? "-1 LEBEN" : "+25 MAX ");
        if (notice && !--notice) text(12, 1, "        ");
        wait_vbl_done();
    }

    HIDE_SPRITES;
    play(M_OVER, 0xD3);
    clear_screen();
    if (!energy) {
        text(2, 2, "BATTERIE LEER!");
        text(1, 5, "DER ORIGINAL-GAME");
        text(1, 6, "BOY BRAUCHT 4 AA.");
    } else {
        text(1, 2, "ZU VIELE HEFTE!");
        text(1, 5, "ERST LERNEN, DANN");
        text(1, 6, "SPIELEN. SAGT MAMA.");
    }
    gotoxy(1, 10); printf("BATTERIEN: %u", (unsigned)batt);
    text(1, 17, "A: ZUM MENUE");
    wait_press(J_START | J_A | J_B);
}

/* ================================================================
 * SPIEL 3: ZEITREISE-QUIZ
 * ================================================================ */

typedef struct {
    const char *q1, *q2, *q3;
    const char *a, *b;
    uint8_t right;         /* 0 = A, 1 = B */
    const char *f1, *f2;   /* Fakt nach der Antwort */
} Question;

static const Question quiz[] = {
    { "WANN KAM DER", "GAME BOY NACH", "EUROPA?",
      "1990", "2010", 0,
      "1989 JAPAN,", "1990 EUROPA." },
    { "WIE VIELE BATTERIEN", "BRAUCHTE DER", "ORIGINAL-GAME BOY?",
      "2 STUECK", "4 STUECK", 1,
      "4 STUECK AA. LEER?", "DANN SPIEL VORBEI!" },
    { "WAS WAR ZUERST?", "", "",
      "SPEZIALKLASSEN", "DER GAME BOY", 0,
      "1985: SPEZ.-KLASSEN", "1989: GAME BOY" },
    { "DIESER GAME BOY IST", "EINE ...", "",
      "SCHRIFTQUELLE", "SACHQUELLE", 1,
      "EIN GEGENSTAND ZUM", "ANFASSEN = SACHE." },
    { "EIN ALTER BRIEF VON", "OMA IST EINE ...", "",
      "SCHRIFTQUELLE", "BILDQUELLE", 0,
      "TEXT VON FRUEHER", "= SCHRIFTQUELLE." },
    { "WIE SAH DAS BILD", "BEIM ORIGINAL AUS?", "",
      "GRUEN-GRAU", "KNALLBUNT", 0,
      "KEIN EIGENES LICHT.", "LAMPE HILFT!" },
    { "WILHELM OSTWALD", "BEKAM 1909 DEN ...", "",
      "OSCAR", "NOBELPREIS", 1,
      "UNSER NAMENSGEBER", "WAR EIN GENIE!" },
    { "TETRIS WURDE", "ERFUNDEN IN DER ...", "",
      "SOWJETUNION", "USA", 0,
      "1984 IN MOSKAU.", "" },
    { "WAS KANNST DU AM", "GERAET ABLESEN?", "",
      "TASTENNAMEN", "KAUFDATUM", 0,
      "A UND B STEHEN", "AUF DEM GEHAEUSE." },
    { "WO FINDEST DU DAS", "KAUFDATUM?", "",
      "KAUFBELEG", "BATTERIEFACH", 0,
      "DER KAUFBELEG IST", "EINE SCHRIFTQUELLE." },
};
#define NQ (sizeof(quiz) / sizeof(quiz[0]))

static void game_quiz(void) {
    uint8_t i, sel, right = 0, keys;
    uint8_t count = NQ;
    for (i = 0; i < count; i++) {
        const Question *q = &quiz[i];
        clear_screen();
        gotoxy(0, 0); printf("ZEITREISE-QUIZ %u/%u", (unsigned)(i + 1), (unsigned)count);
        text(1, 3, q->q1); text(1, 4, q->q2); text(1, 5, q->q3);
        text(2, 9, "A "); text(4, 9, q->a);
        text(2, 11, "B "); text(4, 11, q->b);
        text(0, 16, "DRUECKE A ODER B");
        text(0, 17, "START: MENUE");
        wait_release();
        while (1) {
            keys = joypad();
            if (keys & J_A) { sel = 0; break; }
            if (keys & J_B) { sel = 1; break; }
            if (keys & J_START) { wait_release(); return; }
            wait_vbl_done();
        }
        put(1, sel == 0 ? 9 : 11, T_ARROW);
        wait_release();
        text(0, 16, "                    ");
        text(0, 17, "                    ");
        if (sel == q->right) {
            right++; play(M_RIGHT, 0xF2);
            text(1, 13, "RICHTIG!");
        } else {
            play(M_WRONG, 0xF4);
            text(1, 13, "LEIDER FALSCH!");
        }
        text(1, 14, q->f1);
        text(1, 15, q->f2);
        text(0, 16, "A: WEITER");
        text(0, 17, "START: MENUE");
        if (wait_press(J_A | J_START) & J_START) return;
    }

    clear_screen();
    play(M_WIN, 0xF2);
    text(3, 2, "QUIZ GESCHAFFT!");
    gotoxy(3, 5); printf("%u VON %u RICHTIG", (unsigned)right, (unsigned)count);
    if (right == count)      { text(1, 8, "TITEL:"); text(1, 9, "GESCHICHTS-PROFI!"); }
    else if (right * 10 >= count * 7)  { text(1, 8, "TITEL:"); text(1, 9, "ZEITREISENDE(R)"); }
    else if (right * 10 >= count * 4)  { text(1, 8, "TITEL:"); text(1, 9, "QUELLEN-LEHRLING"); }
    else                  { text(1, 8, "TITEL:"); text(1, 9, "ZURUECK IN DIE"); text(1, 10, "STEINZEIT! :)"); }
    text(1, 17, "A: ZUM MENUE");
    wait_press(J_START | J_A);
}

/* ================================================================
 * TITEL UND MENUE
 * ================================================================ */

#include "archive.h"

static void intro(void) {
    uint8_t x, y;
    clear_screen();
    set_bkg_data(LOGO_BASE, LOGO_W * 2, logodata);
    for (y = 0; y < 2; y++)
        for (x = 0; x < LOGO_W; x++)
            put((20 - LOGO_W) / 2 + x, 7 + y, LOGO_BASE + y * LOGO_W + x);
    text(2, 10, "OSTWALD EDITION");
    text(3, 15, "A ODER START");
    for (y = 48; y > 0; y--) {
        if (joypad() & (J_A | J_START)) goto finished;
        SCY_REG = y;
        wait_vbl_done();
    }
    SCY_REG = 0;
    play(M_BOOT, 0xD2);
    for (y = 0; y < 70; y++) {
        if (joypad() & (J_A | J_START)) break;
        wait_vbl_done();
    }
finished:
    SCY_REG = 0;
    wait_release();
}

static uint8_t menu(void) {
    static const char * const games[] = {
        "OSTWALD-ARCHIV", "QUELLEN-SNAKE", "BATTERIE-REGEN", "ZEITREISE-QUIZ"
    };
    uint8_t sel = 0, keys, i, prev = 0;
    clear_screen();
    for (i = 0; i < 20; i++) { put(i, 0, T_WALL); put(i, 6, T_WALL); }
    text(6, 2, "GAMEBOY");
    text(2, 4, "OSTWALD EDITION");
    for (i = 0; i < 4; i++) text(3, 8 + i * 2, games[i]);
    text(1, 17, "A: WAEHLEN");
    put(1, 8, T_ARROW);
    wait_release();
    while (1) {
        i = sel;
        keys = joypad();
        if ((keys & J_UP) && !(prev & J_UP) && sel) {
            sel--;
            play(M_MOVE, 0xA1);
        }
        if ((keys & J_DOWN) && !(prev & J_DOWN) && sel < 3) {
            sel++;
            play(M_MOVE, 0xA1);
        }
        if (sel != i) {
            put(1, 8 + i * 2, 0);
            put(1, 8 + sel * 2, T_ARROW);
        }
        if (keys & (J_A | J_START)) break;
        prev = keys;
        wait_vbl_done();
    }
    initrand(DIV_REG | ((uint16_t)sel << 8));
    play(M_SELECT, 0xE2);
    wait_release();
    return sel;
}

void main(void) {
    NR52_REG = 0x80; NR50_REG = 0x77; NR51_REG = 0xFF;   /* Ton an, beide Lautsprecher */
    disable_interrupts();
    add_VBL(snd_tick);
    enable_interrupts();
    DISPLAY_ON;
    printf(" ");                     /* Standardschrift laden */
    {   /* Standardschrift durch eigene, schlankere 5x7-Schrift ersetzen */
        uint8_t c;
        for (c = FONT_FIRST; c < FONT_FIRST + FONT_COUNT; c++) {
            gotoxy(0, 0); putchar(c);
            glyph_tile[c - FONT_FIRST] = get_bkg_tile_xy(0, 0);
            set_bkg_data(glyph_tile[c - FONT_FIRST], 1, &font5x7[(uint16_t)(c - FONT_FIRST) * 16]);
        }
        cls();
    }
    set_bkg_data(TILE_BASE, TILE_COUNT, tiledata);
    set_sprite_data(TILE_BASE, TILE_COUNT, tiledata);
    SHOW_BKG;
    intro();
    while (1) {
        uint8_t g = menu();
        if (g == 0) game_archive();
        else if (g == 1) game_snake();
        else if (g == 2) game_rain();
        else game_quiz();
    }
}
