/* screen.c - Mode 3 bitmap text. Drawn when a screen is attached; has no
   effect on the headless audio path (VCOUNT timing and the APU are untouched). */
#include "gba.h"
#include "font.h"
#include "screen.h"

#define SCRW 240
#define SCRH 160
#ifdef SCREEN_HOST
extern volatile u16 *VRAM;            /* host render harness supplies a buffer */
#else
static volatile u16 *const VRAM = (volatile u16 *)0x06000000;
#endif

#define RGB15(r,g,b) ((u16)((r) | ((g) << 5) | ((b) << 10)))
#define C_BG     RGB15( 1, 2, 5)
#define C_HEAD   RGB15(31,31, 4)
#define C_NAME   RGB15(18,31,31)
#define C_TEXT   RGB15(25,25,28)
#define C_FOOT   RGB15( 8,28,10)
#define C_STAT   RGB15(31,21, 4)
#define C_PAUSE  RGB15(31, 5, 5)

#define COLS 30          /* 240 / 8  */
#define ROWH 12          /* glyph cell height */

static int g_paused;

static void fill(u16 color)
{
    volatile u32 *p = (volatile u32 *)VRAM;
    u32 w = (u32)color | ((u32)color << 16);
    int i;
    for (i = 0; i < (SCRW * SCRH) / 2; i++) p[i] = w;
}

static void fill_row(int row, u16 color)
{
    int y0 = row * ROWH, y, x;
    for (y = y0; y < y0 + ROWH && y < SCRH; y++)
        for (x = 0; x < SCRW; x++)
            VRAM[y * SCRW + x] = color;
}

static void putc_at(int px, int py, char c, u16 fg, u16 bg)
{
    const unsigned char *g;
    int cy, cx;
    if (c < 0x20 || c > 0x7E) c = ' ';
    g = fontdata[c - 0x20];
    for (cy = 0; cy < FONT_H_PX; cy++) {
        unsigned char row = g[cy];
        int y = py + cy;
        if (y < 0 || y >= SCRH) continue;
        for (cx = 0; cx < 8; cx++) {
            int x = px + cx;
            if (x < 0 || x >= SCRW) continue;
            VRAM[y * SCRW + x] = ((row >> (7 - cx)) & 1) ? fg : bg;
        }
    }
}

static void text(int col, int row, const char *s, u16 fg, u16 bg)
{
    int x = col * 8, y = row * ROWH;
    while (*s && col < COLS) {
        putc_at(x, y, *s++, fg, bg);
        x += 8; col++;
    }
}

void screen_init(void)
{
#ifndef SCREEN_HOST
    REG_DISPCNT = 3 | (1 << 10);     /* mode 3, BG2 on */
#endif
    fill(C_BG);
}

/* test name + up to 3 explainer lines, in enum order (see main.c) */
static const char *const NAME[] = {
    "REFERENCE TONE", "STEREO ROUTING", "FREQ SWEEP", "FREQ STEPS",
    "DYNAMIC RANGE", "NOISE  CH4", "WAVE  CH3", "SOUNDBIAS MODES",
    "DIRECTSOUND FIFO", "SILENCE"
};
static const char *const L1[] = {
    "1KHZ SQUARE, BOTH CH", "DUAL TONE, THEN L, THEN R", "80HZ TO 16KHZ STEPPED",
    "100HZ..16KHZ TONES", "1KHZ, VOLUME 15 TO 0", "BROADBAND, BOTH CH",
    "TRIANGLE WAVE ~300HZ", "1KHZ, CYCLING PWM RATE", "SINE 440HZ THEN SWEEP",
    "ALL CHANNELS OFF"
};
static const char *const L2[] = {
    "NOMINAL LEVEL CHECK", "SUMMING = BOTH IN EAR", "BANDWIDTH / ROLLOFF",
    "PER-FREQUENCY RESPONSE", "NOISE FLOOR/LINEARITY", "DISTORTION CHECK",
    "HARMONIC CONTENT", "9/8/7/6-BIT PWM MODES", "DMA/FIFO AUDIO PATH",
    "IDLE NOISE FLOOR"
};

void screen_set_paused(int paused) { g_paused = paused; }

static void draw_paused(void)
{
    /* right of the TEST n/10 row, clear of the full-width header above */
    if (g_paused) text(24, 2, "PAUSED", C_PAUSE, C_BG);
    else          text(24, 2, "      ", C_BG,    C_BG);
}

void screen_show_test(int t)
{
    char num[16];
    int i = 0, n = t + 1;
    const char *p = "TEST ";
    fill(C_BG);
    text(0, 0, "CAJUN PANDA GBA AUDIO TEST", C_HEAD, C_BG);

    /* "TEST n/10" (no leading zero) */
    while (*p) num[i++] = *p++;
    if (n >= 10) num[i++] = (char)('0' + n / 10);   /* /10,%10: const divisor */
    num[i++] = (char)('0' + n % 10);
    num[i++] = '/'; num[i++] = '1'; num[i++] = '0'; num[i] = 0;
    text(0, 2, num, C_TEXT, C_BG);

    text(0, 4, NAME[t], C_NAME, C_BG);
    text(0, 6, L1[t], C_TEXT, C_BG);
    text(0, 7, L2[t], C_TEXT, C_BG);

    text(0, 12, "R:NEXT L:PREV L+R:PAUSE", C_FOOT, C_BG);
    draw_paused();
}

void screen_status(const char *s)
{
    fill_row(9, C_BG);
    text(0, 9, s, C_STAT, C_BG);
}

static int utoa(char *b, unsigned v)
{
    char t[12];
    int n = 0, i = 0;
    if (v == 0) { b[0] = '0'; return 1; }
    while (v) { t[n++] = (char)('0' + v % 10); v /= 10; }   /* /10: const, no libgcc */
    while (n) b[i++] = t[--n];
    return i;
}

void screen_status_num(const char *pre, unsigned val, const char *suf)
{
    char buf[40];
    int i = 0;
    while (*pre && i < 30) buf[i++] = *pre++;
    i += utoa(buf + i, val);
    while (*suf && i < 38) buf[i++] = *suf++;
    buf[i] = 0;
    screen_status(buf);
}
