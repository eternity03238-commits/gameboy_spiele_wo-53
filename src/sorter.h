/* ================================================================
 * SPIEL 4: QUELLEN-SORTIERER
 * Quellenpaare fallen ins Regal. 3 gleiche Quellenarten nebeneinander
 * oder uebereinander verschwinden. Lerneffekt: Quellenarten sortieren.
 * ================================================================ */
#define SW 6            /* Regalbreite in Feldern */
#define SH 12           /* Regalhoehe in Feldern */
#define SX 2            /* linke Kante auf dem Bildschirm (Kacheln) */
#define SY 4            /* obere Kante auf dem Bildschirm (Kacheln) */

static uint8_t sbox[SH][SW];     /* 0 = leer, 1..4 = Quellenart + 1 */
static uint8_t smark[SH][SW];
static uint8_t s_x, s_y, s_o, s_sub, s_a, s_b, s_na, s_nb;
static uint8_t s_level, s_cleared;
static uint16_t s_score;

static const char * const s_speed_name[3] = { "LANGSAM", "MITTEL", "SCHNELL" };
static const uint8_t s_start_level[3] = { 0, 2, 4 };
/* Fallgeschwindigkeit in 1/16 Pixel pro Bild; 128 = ein Feld */
static const uint8_t s_rate[9] = { 2, 3, 4, 5, 6, 8, 10, 12, 15 };
static const char * const s_plural[4] = {
    "SACHQUELLEN!", "SCHRIFTQUELLEN!", "TONQUELLEN!", "BILDQUELLEN!"
};
/* Lage des zweiten Teils: 0 rechts, 1 unten, 2 links, 3 oben */
static const int8_t s_dx[4] = { 1, 0, -1, 0 };
static const int8_t s_dy[4] = { 0, 1, 0, -1 };

static void s_cell(uint8_t x, uint8_t y) {
    put(SX + x, SY + y, sbox[y][x] ? food_tile[sbox[y][x] - 1] : 0);
}

static uint8_t s_free(int8_t x, int8_t y) {
    if (x < 0 || x >= SW || y >= SH) return 0;
    if (y < 0) return 0;
    return !sbox[(uint8_t)y][(uint8_t)x];
}

static uint8_t s_fits(int8_t x, int8_t y, uint8_t o) {
    return s_free(x, y) && s_free(x + s_dx[o], y + s_dy[o]);
}

static void s_draw_piece(void) {
    uint8_t px = (SX + s_x) * 8 + 8;
    uint8_t py = (SY + s_y) * 8 + 16 + (s_sub >> 4);
    move_sprite(0, px, py);
    move_sprite(1, px + s_dx[s_o] * 8, py + s_dy[s_o] * 8);
}

static void s_panel(void) {
    gotoxy(11, 9);  printf("%5u", (unsigned)s_score);
    gotoxy(11, 12); printf("%u ", (unsigned)(s_level + 1));
}

static void s_preview(void) {
    put(12, 5, food_tile[s_na]);
    put(13, 5, food_tile[s_nb]);
}

static uint8_t s_spawn(void) {
    s_x = 2; s_y = 0; s_o = 0; s_sub = 0;
    s_a = s_na; s_b = s_nb;
    s_na = rand() & 3; s_nb = rand() & 3;
    s_preview();
    if (!s_fits(s_x, s_y, s_o)) return 0;
    set_sprite_tile(0, food_tile[s_a]);
    set_sprite_tile(1, food_tile[s_b]);
    s_draw_piece();
    SHOW_SPRITES;
    return 1;
}

static void s_rotate(void) {
    uint8_t o = (s_o + 1) & 3;
    if (s_fits(s_x, s_y, o)) { s_o = o; return; }
    if (s_fits(s_x - 1, s_y, o)) { s_x--; s_o = o; return; }
    if (s_fits(s_x + 1, s_y, o)) { s_x++; s_o = o; }
}

/* Lose Teile fallen nach unten */
static void s_gravity(void) {
    uint8_t x, y, dest;
    for (x = 0; x < SW; x++) {
        dest = SH;
        for (y = SH; y-- > 0;) {
            if (sbox[y][x]) {
                dest--;
                if (dest != y) { sbox[dest][x] = sbox[y][x]; sbox[y][x] = 0; s_cell(x, y); s_cell(x, dest); }
            }
        }
    }
}

/* Markiert alle Reihen aus 3+ gleichen Quellen. Gibt Anzahl zurueck. */
static uint8_t s_find(uint8_t *type) {
    uint8_t x, y, n = 0, run, k, v;
    memset(smark, 0, sizeof smark);
    for (y = 0; y < SH; y++) {
        for (x = 0; x < SW; x += run) {
            v = sbox[y][x]; run = 1;
            while (x + run < SW && v && sbox[y][x + run] == v) run++;
            if (v && run >= 3) { for (k = 0; k < run; k++) smark[y][x + k] = 1; *type = v - 1; }
        }
    }
    for (x = 0; x < SW; x++) {
        for (y = 0; y < SH; y += run) {
            v = sbox[y][x]; run = 1;
            while (y + run < SH && v && sbox[y + run][x] == v) run++;
            if (v && run >= 3) { for (k = 0; k < run; k++) smark[y + k][x] = 1; *type = v - 1; }
        }
    }
    for (y = 0; y < SH; y++) for (x = 0; x < SW; x++) n += smark[y][x];
    return n;
}

static void s_land(void) {
    uint8_t n, type = 0, chain = 1, x, y, blink;
    int8_t bx = s_x + s_dx[s_o], by = s_y + s_dy[s_o];
    HIDE_SPRITES;
    sbox[s_y][s_x] = s_a + 1; s_cell(s_x, s_y);
    sbox[(uint8_t)by][(uint8_t)bx] = s_b + 1; s_cell(bx, by);
    s_gravity();
    while ((n = s_find(&type))) {
        play(chain > 1 ? M_LEVEL : M_COIN, 0xF2);
        gotoxy(0, 1); printf("%u %s", (unsigned)n, s_plural[type]);
        for (blink = 0; blink < 6; blink++) {
            for (y = 0; y < SH; y++) for (x = 0; x < SW; x++)
                if (smark[y][x]) put(SX + x, SY + y, (blink & 1) ? food_tile[sbox[y][x] - 1] : 0);
            wait_frames(6);
        }
        for (y = 0; y < SH; y++) for (x = 0; x < SW; x++)
            if (smark[y][x]) { sbox[y][x] = 0; s_cell(x, y); }
        s_score += (uint16_t)n * 10 * chain;
        s_cleared += n;
        if (s_cleared >= 12 && s_level < 8) {
            s_cleared -= 12; s_level++;
            text(0, 2, "LEVEL UP!");
        }
        s_panel();
        s_gravity();
        chain++;
        wait_frames(8);
    }
}

static void game_sort(void) {
    uint8_t d, x, y, keys, prev = 0, pressed, speed;
    d = choose("QUELLEN-SORTIERER", "WIE SCHNELL?", s_speed_name, 3,
               "3 GLEICHE QUELLEN", "VERSCHWINDEN!");
    if (d == 0xFF) return;
    clear_screen();
    memset(sbox, 0, sizeof sbox);
    s_level = s_start_level[d]; s_cleared = 0; s_score = 0;
    text(0, 0, "QUELLEN-SORTIERER");
    for (y = SY; y < SY + SH; y++) { put(SX - 1, y, T_WALL); put(SX + SW, y, T_WALL); }
    for (x = SX - 1; x <= SX + SW; x++) put(x, SY + SH, T_WALL);
    text(11, 4, "NAECHSTES");
    text(11, 8, "PUNKTE");
    text(11, 11, "LEVEL");
    text(11, 14, "A DREHEN");
    text(11, 15, "B RUNTER");
    text(11, 16, "START=");
    text(11, 17, "PAUSE");
    s_panel();
    s_na = rand() & 3; s_nb = rand() & 3;
    s_spawn();
    wait_release();
    for (;;) {
        wait_vbl_done();
        keys = joypad();
        pressed = keys & ~prev;
        prev = keys;
        if ((pressed & J_START) && pause_game()) return;
        if ((pressed & J_LEFT)  && s_fits(s_x - 1, s_y, s_o) && (!s_sub || s_fits(s_x - 1, s_y + 1, s_o))) s_x--;
        if ((pressed & J_RIGHT) && s_fits(s_x + 1, s_y, s_o) && (!s_sub || s_fits(s_x + 1, s_y + 1, s_o))) s_x++;
        if (pressed & J_A) {
            uint8_t oo = s_o, ox_ = s_x;
            s_rotate();
            if (s_sub && !s_fits(s_x, s_y + 1, s_o)) { s_o = oo; s_x = ox_; }
        }
        if (pressed & J_B) {
            while (s_fits(s_x, s_y + 1, s_o)) s_y++;
            s_sub = 0;
        } else if (s_fits(s_x, s_y + 1, s_o)) {
            speed = (keys & J_DOWN) ? 48 : s_rate[s_level];
            s_sub += speed;
            if (s_sub >= 128) { s_sub -= 128; s_y++; }
            if (s_sub && !s_fits(s_x, s_y + 1, s_o)) s_sub = 0;
            s_draw_piece();
            continue;
        }
        /* gelandet */
        if (s_sub) { s_sub = 0; s_draw_piece(); continue; }
        s_land();
        text(0, 1, "                    ");
        text(0, 2, "          ");
        if (!s_spawn()) break;
    }
    HIDE_SPRITES;
    play(M_OVER, 0xF4);
    wait_frames(40);
    clear_screen();
    text(1, 3, "DAS REGAL IST VOLL!");
    gotoxy(1, 6); printf("PUNKTE: %u", (unsigned)s_score);
    gotoxy(1, 8); printf("LEVEL:  %u", (unsigned)(s_level + 1));
    text(1, 11, "3 GLEICHE QUELLEN");
    text(1, 12, "= 1 SAMMLUNG.");
    text(1, 17, "A: ZUM MENUE");
    wait_press(J_START | J_A);
}
