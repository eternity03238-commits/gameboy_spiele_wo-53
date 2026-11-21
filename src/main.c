/* ZEITREISE 5/3 - Retro-Spiele fuer den Geschichtsvortrag
 * Wilhelm-Ostwald-Gymnasium Leipzig, Klasse 5/3
 * Gebaut mit GBDK-2020. Laeuft auf jedem Gameboy (DMG, Pocket, Color, Advance).
 */
#include <gb/gb.h>
#include <stdio.h>
#include <stdint.h>
#include <rand.h>
#include <gbdk/console.h>
#include "tiles.h"

/* ---------- kleine Helfer ---------- */

static void beep(uint8_t hi) {
    NR52_REG = 0x80; NR51_REG = 0x11; NR50_REG = 0x77;
    NR10_REG = 0x00; NR11_REG = 0x80; NR12_REG = 0xF3;
    NR13_REG = hi;   NR14_REG = 0x87;
}
static void boom(void) {
    NR52_REG = 0x80; NR51_REG = 0x88; NR50_REG = 0x77;
    NR41_REG = 0x00; NR42_REG = 0xF4; NR43_REG = 0x61; NR44_REG = 0x80;
}

static void wait_frames(uint8_t n) { while (n--) wait_vbl_done(); }

static void wait_release(void) { while (joypad()) wait_vbl_done(); }

static void wait_press(uint8_t mask) {
    wait_release();
    while (!(joypad() & mask)) wait_vbl_done();
    wait_release();
}

static void clear_screen(void) {
    HIDE_SPRITES;
    cls();
    gotoxy(0, 0);
}

static void put(uint8_t x, uint8_t y, uint8_t t) { set_bkg_tile_xy(x, y, t); }

static void text(uint8_t x, uint8_t y, const char *s) { gotoxy(x, y); printf("%s", s); }

/* ================================================================
 * SPIEL 1: QUELLEN-SNAKE
 * Die Schlange frisst Quellen. Jede Quelle zeigt ihre Quellenart.
 * ================================================================ */

#define MAXLEN 120
static uint8_t sx[MAXLEN], sy[MAXLEN];
static uint8_t slen, dir, ndir, fx, fy, ftype, score;

static const uint8_t food_tile[4] = { T_COIN, T_LETTER, T_CASS, T_FOTO };
static const char * const food_name[4] = {
    "MUENZE=SACHQUELLE   ",
    "BRIEF=SCHRIFTQUELLE ",
    "KASSETTE=TONQUELLE  ",
    "FOTO=BILDQUELLE     "
};

/* Spielfeld: x 1..18, y 3..16, Wand drumherum */
static uint8_t on_snake(uint8_t x, uint8_t y) {
    uint8_t i;
    for (i = 0; i < slen; i++) if (sx[i] == x && sy[i] == y) return 1;
    return 0;
}

static void place_food(void) {
    do {
        fx = 1 + (rand() % 18);
        fy = 3 + (rand() % 14);
    } while (on_snake(fx, fy));
    ftype = rand() & 3;
    put(fx, fy, food_tile[ftype]);
}

static void snake_hud(void) {
    gotoxy(0, 0); printf("QUELLEN-SNAKE  %3u", (unsigned)score);
}

static void game_snake(void) {
    uint8_t i, x, y, speed, tick = 0, keys;
    clear_screen();
    for (x = 0; x < 20; x++) { put(x, 2, T_WALL); put(x, 17, T_WALL); }
    for (y = 2; y < 18; y++) { put(0, y, T_WALL); put(19, y, T_WALL); }
    slen = 3; score = 0; dir = ndir = 0; /* 0=rechts 1=links 2=hoch 3=runter */
    for (i = 0; i < slen; i++) { sx[i] = 8 - i; sy[i] = 9; put(sx[i], sy[i], i ? T_BODY : T_HEAD); }
    snake_hud();
    text(0, 1, "FRISS DIE QUELLEN!  ");
    place_food();
    speed = 10;
    wait_release();

    while (1) {
        keys = joypad();
        if ((keys & J_RIGHT) && dir != 1) ndir = 0;
        else if ((keys & J_LEFT) && dir != 0) ndir = 1;
        else if ((keys & J_UP) && dir != 3) ndir = 2;
        else if ((keys & J_DOWN) && dir != 2) ndir = 3;
        if (keys & J_START) { /* Pause */
            text(6, 9, " PAUSE ");
            wait_press(J_START);
            put(6,9,0);
            text(6, 9, "       ");
            for (i = 0; i < slen; i++) put(sx[i], sy[i], i ? T_BODY : T_HEAD);
            put(fx, fy, food_tile[ftype]);
        }
        wait_vbl_done();
        if (++tick < speed) continue;
        tick = 0;
        dir = ndir;

        x = sx[0]; y = sy[0];
        if (dir == 0) x++; else if (dir == 1) x--; else if (dir == 2) y--; else y++;

        /* Crash? */
        if (x == 0 || x == 19 || y == 2 || y == 17 || on_snake(x, y)) {
            boom();
            break;
        }

        if (x == fx && y == fy) {
            beep(0xC0);
            score++;
            if (slen < MAXLEN) slen++;
            text(0, 1, food_name[ftype]);
            if (speed > 4 && (score % 4) == 0) speed--;
            snake_hud();
        } else {
            set_bkg_tile_xy(sx[slen - 1], sy[slen - 1], 0);
        }
        for (i = slen - 1; i > 0; i--) { sx[i] = sx[i - 1]; sy[i] = sy[i - 1]; }
        sx[0] = x; sy[0] = y;
        put(sx[1], sy[1], T_BODY);
        put(x, y, T_HEAD);
        if (x == fx && y == fy) place_food();
    }

    wait_frames(40);
    clear_screen();
    text(5, 2, "GAME OVER!");
    gotoxy(2, 5); printf("DU HAST %u QUELLEN", (unsigned)score);
    text(2, 6, "GESAMMELT.");
    text(1, 9, "QUELLENARTEN:");
    put(1, 11, T_COIN);   text(3, 11, "SACHQUELLE");
    put(1, 12, T_LETTER); text(3, 12, "SCHRIFTQUELLE");
    put(1, 13, T_CASS);   text(3, 13, "TONQUELLE");
    put(1, 14, T_FOTO);   text(3, 14, "BILDQUELLE");
    text(2, 17, "WEITER MIT START");
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

static void game_rain(void) {
    uint8_t px = 72, i, lives = 3, batt = 0, fall = 1, spawn = 0, spawnrate = 50, keys;
    uint8_t energy = 100, etick = 0;
    clear_screen();
    text(0, 0, "BATTERIE-REGEN");
    text(0, 2, "FANG DIE BATTERIEN!");
    text(0, 3, "KEINE HAUSAUFGABEN!");
    for (i = 0; i < 20; i++) put(i, 17, T_WALL);

    set_sprite_tile(0, T_GB_TL); set_sprite_tile(1, T_GB_TR);
    set_sprite_tile(2, T_GB_BL); set_sprite_tile(3, T_GB_BR);
    for (i = 0; i < NOBJ; i++) { oactive[i] = 0; move_sprite(4 + i, 0, 0); }
    draw_player(px);
    SHOW_SPRITES;
    wait_release();

    while (lives && energy) {
        keys = joypad();
        if ((keys & J_LEFT) && px > 2) px -= 2;
        if ((keys & J_RIGHT) && px < 142) px += 2;
        draw_player(px);

        /* Batterie wird langsam leer - wie beim echten Gameboy */
        if (++etick == 8) { etick = 0; energy--; }

        if (++spawn >= spawnrate) {
            spawn = 0;
            for (i = 0; i < NOBJ; i++) if (!oactive[i]) {
                oactive[i] = 1;
                ox[i] = 4 + (rand() % 148);
                oy[i] = 24;
                otype[i] = ((rand() & 3) == 0) ? 1 : 0; /* 1 = Hausaufgabe */
                set_sprite_tile(4 + i, otype[i] ? T_HOMEW : T_BATT);
                break;
            }
        }

        for (i = 0; i < NOBJ; i++) if (oactive[i]) {
            oy[i] += fall;
            if (oy[i] >= 120 && oy[i] < 136 && ox[i] + 8 > px && ox[i] < px + 16) {
                oactive[i] = 0; move_sprite(4 + i, 0, 0);
                if (otype[i]) { lives--; boom(); }
                else {
                    batt++; beep(0xD0);
                    energy = (energy > 75) ? 100 : energy + 25;
                    if ((batt % 5) == 0) { if (spawnrate > 20) spawnrate -= 6; if (fall < 3 && batt % 10 == 0) fall++; }
                }
            } else if (oy[i] > 136) {
                oactive[i] = 0; move_sprite(4 + i, 0, 0);
            } else {
                move_sprite(4 + i, ox[i] + 8, oy[i] + 16);
            }
        }

        gotoxy(0, 1);
        printf("BATT:%3u LEBEN:%u", (unsigned)batt, (unsigned)lives);
        gotoxy(0, 4);
        printf("STROM:%3u   ", (unsigned)energy);
        wait_vbl_done();
    }

    HIDE_SPRITES;
    wait_frames(30);
    clear_screen();
    if (!energy) {
        text(2, 2, "BATTERIE LEER!");
        text(1, 4, "DER BILDSCHIRM IST");
        text(1, 5, "AUS. 1990 HAETTEST");
        text(1, 6, "DU JETZT 4 NEUE AA");
        text(1, 7, "BATTERIEN GEBRAUCHT");
    } else {
        text(1, 2, "ZU VIELE HAUS-");
        text(1, 3, "AUFGABEN GEFANGEN!");
        text(1, 5, "ERST LERNEN, DANN");
        text(1, 6, "SPIELEN. (SAGT MAMA)");
    }
    gotoxy(1, 10); printf("BATTERIEN: %u", (unsigned)batt);
    gotoxy(1, 11); printf("REICHT FUER %u GB-", (unsigned)(batt / 4));
    text(1, 12, "LADUNGEN (4 = 1 GB)");
    text(2, 17, "WEITER MIT START");
    wait_press(J_START | J_A);
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
    { "WANN KAM DER", "GAMEBOY NACH", "DEUTSCHLAND?",
      "1990", "2010", 0,
      "1989 JAPAN,", "1990 DEUTSCHLAND." },
    { "WIE VIELE BATTERIEN", "BRAUCHT EIN", "GAMEBOY?",
      "2 STUECK", "4 STUECK", 1,
      "4 x AA. LEER?", "DANN SPIEL VORBEI!" },
    { "WAS IST AELTER?", "", "",
      "UNSERE SCHULE", "DER GAMEBOY", 0,
      "WOG GEGRUENDET 1985", "GAMEBOY ERST 1989!" },
    { "EIN GAMEBOY IST", "EINE ...", "",
      "SCHRIFTQUELLE", "SACHQUELLE", 1,
      "EIN GEGENSTAND ZUM", "ANFASSEN = SACHE." },
    { "EIN ALTER BRIEF VON", "OMA IST EINE ...", "",
      "SCHRIFTQUELLE", "BILDQUELLE", 0,
      "TEXT VON FRUEHER", "= SCHRIFTQUELLE." },
    { "WELCHE FARBE HATTE", "DER BILDSCHIRM?", "",
      "GRUEN-GRAU", "KNALLBUNT", 0,
      "UND OHNE LICHT!", "NUR UNTER DER LAMPE" },
    { "WILHELM OSTWALD", "BEKAM 1909 DEN ...", "",
      "OSCAR", "NOBELPREIS", 1,
      "UNSER NAMENSGEBER", "WAR EIN GENIE!" },
    { "TETRIS WURDE", "ERFUNDEN IN DER ...", "",
      "SOWJETUNION", "USA", 0,
      "1984 IN MOSKAU.", "" },
    { "MATHE: GAMEBOY 1989", "HEUTE 2026.", "WIE ALT IST ER?",
      "37 JAHRE", "73 JAHRE", 0,
      "2026 - 1989 = 37", "AELTER ALS PAPA?" },
    { "WER DIE VERGANGEN-", "HEIT NICHT KENNT,", "...",
      "VERSTEHT NIX", "MUSS NACHSITZEN", 0,
      "HELMUT KOHL, 1995.", "(STEHT AUFM BLATT)" },
};
#define NQ (sizeof(quiz) / sizeof(quiz[0]))

static void game_quiz(void) {
    uint8_t i, sel, right = 0, keys;
    for (i = 0; i < NQ; i++) {
        const Question *q = &quiz[i];
        clear_screen();
        gotoxy(0, 0); printf("ZEITREISE-QUIZ %u/%u", (unsigned)(i + 1), (unsigned)NQ);
        text(1, 3, q->q1); text(1, 4, q->q2); text(1, 5, q->q3);
        text(2, 9, "A "); text(4, 9, q->a);
        text(2, 11, "B "); text(4, 11, q->b);
        text(0, 16, "DRUECKE DIE TASTE");
        text(0, 17, "A ODER B!");
        wait_release();
        while (1) {
            keys = joypad();
            if (keys & J_A) { sel = 0; break; }
            if (keys & J_B) { sel = 1; break; }
            wait_vbl_done();
        }
        put(1, sel == 0 ? 9 : 11, T_ARROW);
        wait_release();
        text(0, 16, "                    ");
        text(0, 17, "                    ");
        if (sel == q->right) {
            right++; beep(0xE0);
            text(1, 13, "RICHTIG!");
        } else {
            boom();
            text(1, 13, "LEIDER FALSCH!");
        }
        text(1, 14, q->f1);
        text(1, 15, q->f2);
        text(3, 17, "WEITER MIT A");
        wait_press(J_A | J_START);
    }

    clear_screen();
    text(3, 2, "QUIZ GESCHAFFT!");
    gotoxy(3, 5); printf("%u VON %u RICHTIG", (unsigned)right, (unsigned)NQ);
    if (right == NQ)      { text(1, 8, "TITEL:"); text(1, 9, "GESCHICHTS-PROFI!"); }
    else if (right >= 7)  { text(1, 8, "TITEL:"); text(1, 9, "ZEITREISENDE(R)"); }
    else if (right >= 4)  { text(1, 8, "TITEL:"); text(1, 9, "QUELLEN-LEHRLING"); }
    else                  { text(1, 8, "TITEL:"); text(1, 9, "ZURUECK IN DIE"); text(1, 10, "STEINZEIT! :)"); }
    text(2, 17, "WEITER MIT START");
    wait_press(J_START | J_A);
}

/* ================================================================
 * TITEL UND MENUE
 * ================================================================ */

static void intro(void) {
    uint8_t y;
    clear_screen();
    /* Fallendes Logo wie beim echten Gameboy-Start */
    for (y = 0; y <= 7; y++) {
        text(4, y, "            ");
        text(4, y + 1, "OSTWALD-BOY");
        wait_frames(6);
    }
    beep(0xE8); wait_frames(8); beep(0xF4);
    wait_frames(50);
}

static uint8_t menu(void) {
    uint8_t sel = 0, keys, i, waited = 0;
    clear_screen();
    for (i = 0; i < 20; i++) { put(i, 0, T_WALL); put(i, 6, T_WALL); }
    text(3, 2, "ZEITREISE 5/3");
    text(1, 3, "WILHELM-OSTWALD-");
    text(1, 4, "GYMNASIUM LEIPZIG");
    put(1, 8, T_COIN); put(2, 8, T_LETTER); put(3, 8, T_CASS); put(4, 8, T_FOTO);
    put(15, 8, T_BATT); put(16, 8, T_HOMEW); put(17, 8, T_HEAD);
    text(3, 10, "QUELLEN-SNAKE");
    text(3, 12, "BATTERIE-REGEN");
    text(3, 14, "ZEITREISE-QUIZ");
    text(1, 17, "FACH: GESCHICHTE");
    wait_release();
    while (1) {
        put(1, 10, sel == 0 ? T_ARROW : 0);
        put(1, 12, sel == 1 ? T_ARROW : 0);
        put(1, 14, sel == 2 ? T_ARROW : 0);
        keys = joypad();
        if (keys & J_UP)   { if (sel) sel--; beep(0x90); waited = 12; }
        if (keys & J_DOWN) { if (sel < 2) sel++; beep(0x90); waited = 12; }
        if (keys & (J_A | J_START)) break;
        if (waited) { wait_frames(waited); waited = 0; }
        wait_vbl_done();
    }
    initrand(DIV_REG | ((uint16_t)sel << 8));
    beep(0xF0);
    wait_release();
    return sel;
}

void main(void) {
    DISPLAY_ON;
    printf(" ");                     /* Schrift laden */
    set_bkg_data(TILE_BASE, TILE_COUNT, tiledata);
    set_sprite_data(TILE_BASE, TILE_COUNT, tiledata);
    SHOW_BKG;
    intro();
    while (1) {
        uint8_t g = menu();
        if (g == 0) game_snake();
        else if (g == 1) game_rain();
        else game_quiz();
    }
}
