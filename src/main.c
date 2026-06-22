/* main.c - Cajun Panda's GBA Audio Test ROM.
 *
 * A headless, auto-cycling suite of audio tests for exercising the Game Boy
 * Advance audio path: stereo routing, frequency response, dynamic range, the
 * PSG channels, the SOUNDBIAS PWM bit-rate modes, and the DirectSound FIFO/DMA
 * path. It runs with no screen attached; each test is announced by a count of
 * beeps (N beeps = test number N). When a display is present it shows an
 * optional on-screen explainer.
 *
 * Input is limited to the shoulder buttons so it works on a bare board with no
 * face buttons wired:
 *   R  = next test        L  = previous test
 *   hold L+R (~0.7s)      = pause (loops the current test) / resume
 *
 * Everything is register-level and division-free at runtime so it builds with
 * a stock arm-none-eabi GCC (no devkitARM or libgcc multilib needed).
 */
#include "gba.h"
#include "sintab.h"
#include "screen.h"

/* Freestanding fallbacks: -O2 GCC may emit calls to these even under
   -ffreestanding. Provide them so the -nostdlib link always resolves. */
void *memcpy(void *d, const void *s, unsigned n)
{
    unsigned char *dp = d; const unsigned char *sp = s;
    while (n--) *dp++ = *sp++;
    return d;
}
void *memset(void *d, int c, unsigned n)
{
    unsigned char *dp = d;
    while (n--) *dp++ = (unsigned char)c;
    return d;
}

/* ---- frequency register helpers (constant-folded at compile time) ---- */
#define RSQ(f) ((u16)(2048 - 131072 / (f)))   /* square ch1/ch2 */
#define RWV(f) ((u16)(2048 -  65536 / (f)))   /* wave ch3        */

#define CH1 1
#define CH2 2
#define CH3 3
#define CH4 4
#define M1 (1<<0)
#define M2 (1<<1)
#define M3 (1<<2)
#define M4 (1<<3)

/* ---- low-level timing ---- */
static void vsync(void)
{
    while (REG_VCOUNT >= 160) {}
    while (REG_VCOUNT <  160) {}
}

/* ---- routing / silence ---- */
static inline void route(u16 lmask, u16 rmask)
{
    REG_SOUNDCNT_L = (7) | (7 << 4) | (rmask << 8) | (lmask << 12);
}

static void psg_off(void)
{
    REG_SOUND1CNT_H = 0; REG_SOUND1CNT_X = 0;
    REG_SOUND2CNT_L = 0; REG_SOUND2CNT_H = 0;
    REG_SOUND3CNT_L = 0; REG_SOUND3CNT_H = 0; REG_SOUND3CNT_X = 0;
    REG_SOUND4CNT_L = 0; REG_SOUND4CNT_H = 0;
    REG_SOUNDCNT_L  = 0;
}

/* ---- PSG note helpers ---- */
static void ch1_note(u16 rate, u8 vol, u8 duty)
{
    REG_SOUND1CNT_L = 0;                        /* sweep off */
    REG_SOUND1CNT_H = ((u16)vol << 12) | ((u16)duty << 6);
    REG_SOUND1CNT_X = (rate & 0x7FF) | SND_RESTART;
}

static void ch2_note(u16 rate, u8 vol, u8 duty)
{
    REG_SOUND2CNT_L = ((u16)vol << 12) | ((u16)duty << 6);
    REG_SOUND2CNT_H = (rate & 0x7FF) | SND_RESTART;
}

static void ch4_noise(u8 vol)
{
    REG_SOUND4CNT_L = ((u16)vol << 12);
    REG_SOUND4CNT_H = SND_RESTART | (2 << 4);  /* mid clock, broadband */
}

static void ch3_wave_load(void)
{
    /* 32-sample 4-bit triangle, loaded into both wave banks. */
    static const u16 tri[8] = {
        0x0123, 0x4567, 0x89AB, 0xCDEF,
        0xFEDC, 0xBA98, 0x7654, 0x3210
    };
    int i;
    REG_SOUND3CNT_L = (1 << 6);                /* play-bank=1 -> writes hit bank0 */
    for (i = 0; i < 8; i++) REG_WAVE_RAM[i] = tri[i];
    REG_SOUND3CNT_L = (0 << 6);                /* play-bank=0 -> writes hit bank1 */
    for (i = 0; i < 8; i++) REG_WAVE_RAM[i] = tri[i];
    REG_SOUND3CNT_L = (1 << 7);                /* enable, play bank0, single bank */
}

static void ch3_note(u16 rate)
{
    REG_SOUND3CNT_H = (1 << 13);               /* volume 100% */
    REG_SOUND3CNT_X = (rate & 0x7FF) | SND_RESTART;
}

/* ---- DirectSound (FIFO A) sine engine ---- */
#define DS_BUF 304
static signed char ds_buf[DS_BUF] __attribute__((aligned(4)));
static u32 ds_phase;       /* 8.16-ish phase, index = (phase>>16)&0xFF       */
static u32 ds_inc;         /* per-sample phase step = freqHz << 10           */
static int ds_active;
static u32 ds_acc;         /* fractional samples-per-frame accumulator (5/16) */

static void ds_init(void)
{
    ds_phase = 0; ds_acc = 0;
    REG_DMA1CNT_H = 0;                  /* ensure DMA is fully off first */
    /* sample rate 16384 Hz: timer reload = 65536 - CPU/rate = 65536-1024 */
    REG_TM0CNT_L = (u16)(65536 - (CPU_HZ / 16384));
    REG_TM0CNT_H = TM_ENABLE | TM_FREQ_1;
    /* DMA dest (FIFO_A) and word count are set once and never touched again */
    REG_DMA1DAD   = (u32)0x040000A0;
    REG_DMA1CNT_L = 4;
    REG_SOUNDCNT_H = DMG_VOL_100 | DSA_VOL_100 | DSA_L_ENABLE | DSA_R_ENABLE | DSA_RESET;
    ds_active = 1;
}

static void ds_stop(void)
{
    REG_DMA1CNT_H = 0;
    REG_TM0CNT_H  = 0;
    REG_SOUNDCNT_H = DMG_VOL_100;
    ds_active = 0;
}

static void ds_frame(void)
{
    /* exact samples this frame: 280896 cyc / 1024 cyc-per-sample = 274.3125 */
    int n = 274;
    ds_acc += 5;                       /* 0.3125 == 5/16 */
    if (ds_acc >= 16) { ds_acc -= 16; n = 275; }

    {
        int i;
        for (i = 0; i < n; i++) {
            ds_buf[i] = sintab[(ds_phase >> 16) & 0xFF];
            ds_phase += ds_inc;
        }
    }
    /* Restart FIFO DMA from the freshly generated buffer (phase is preserved
       so the waveform stays continuous across frames). The disable->enable
       needs a settle gap: re-enabling a repeat-mode FIFO DMA too quickly can
       mis-latch the control bits on real hardware, leaving a runaway DMA that
       starves the CPU and hangs the ROM (mGBA doesn't model this). */
    REG_DMA1CNT_H = 0;
    __asm__ volatile("nop; nop; nop; nop; nop; nop; nop; nop");
    REG_DMA1SAD   = (u32)ds_buf;
    REG_DMA1CNT_H = DMA_ENABLE | DMA_SPECIAL | DMA_32BIT | DMA_REPEAT |
                    DMA_DEST_FIXED | DMA_SRC_INC;
}

/* ---- sweep / step frequency tables (Hz + matching square rate) ---- */
static const u16 sweepHz[] = {
    80,110,150,200,280,380,520,700,950,1300,1800,2400,3200,4300,5800,7800,
    10000,13000,16000
};
static const u16 sweepRate[] = {
    RSQ(80),RSQ(110),RSQ(150),RSQ(200),RSQ(280),RSQ(380),RSQ(520),RSQ(700),
    RSQ(950),RSQ(1300),RSQ(1800),RSQ(2400),RSQ(3200),RSQ(4300),RSQ(5800),
    RSQ(7800),RSQ(10000),RSQ(13000),RSQ(16000)
};
#define NSWEEP (int)(sizeof(sweepHz)/sizeof(sweepHz[0]))

static const u16 stepHz[]   = {100,250,500,1000,2000,4000,8000,12000,16000};
static const u16 stepRate[] = {
    RSQ(100),RSQ(250),RSQ(500),RSQ(1000),RSQ(2000),RSQ(4000),RSQ(8000),
    RSQ(12000),RSQ(16000)
};
#define NSTEP (int)(sizeof(stepHz)/sizeof(stepHz[0]))

static const u16 biasModes[] = {
    BIAS_9BIT_32K, BIAS_8BIT_65K, BIAS_7BIT_131K, BIAS_6BIT_262K
};

/* ---- test catalogue ---- */
enum {
    T_REF, T_STEREO, T_SWEEP, T_STEPS, T_DYN, T_NOISE, T_WAVE,
    T_BIAS, T_DSOUND, T_SILENCE, T_COUNT
};
/* per-test run length in frames (~60 fps) */
static const u16 dur[T_COUNT] = {
    240, 480, 360, 360, 352, 240, 240, 400, 480, 240
};

/* ---- framework state ---- */
enum { PH_ANNOUNCE, PH_RUN, PH_GAP };
#define LOOP_GAP 40          /* silent frames between loops when paused (~0.7s) */
static int   cur;
static int   phase;          /* PH_ANNOUNCE, then PH_RUN, then PH_GAP loop when paused */
static int   pframe;         /* frame index within current phase */
static int   announceLen;
static int   paused;
static int   sub, subTimer;  /* per-test step counters (division-free) */

static void silence_all(void)
{
    ds_stop();
    psg_off();
    REG_SOUNDCNT_H = DMG_VOL_100;
    REG_SOUNDBIAS  = BIAS_9BIT_32K;   /* restore default; test 8 leaves 6-bit */
}

static void enter_test(int i)
{
    if (i < 0)        i = T_COUNT - 1;
    if (i >= T_COUNT) i = 0;
    cur = i;
    silence_all();
    screen_show_test(i);
    phase       = PH_ANNOUNCE;
    pframe      = 0;
    announceLen = (i + 1) * 16 + 24;   /* (N beeps * 16f) + tail gap */
    sub = 0; subTimer = 0;
}

/* beep count = cur+1, on ch2 both channels at ~1500 Hz */
static void announce_tick(int f)
{
    int cell = f >> 4;          /* 16 frames per beep cell */
    int pos  = f & 15;
    if (cell <= cur) {
        if (pos == 0) { route(M2, M2); ch2_note(RSQ(1500), 12, 2); }
        else if (pos == 7) { REG_SOUND2CNT_L = 0; REG_SOUNDCNT_L = 0; }
    } else {
        REG_SOUNDCNT_L = 0;
    }
}

static void test_setup(int t)
{
    switch (t) {
    case T_REF:                                   /* 1 kHz reference, both */
        route(M2, M2); ch2_note(RSQ(1000), 12, 2);
        break;
    case T_STEREO:                                /* alternate L / R       */
        break;                                    /* driven in update      */
    case T_SWEEP:
        break;
    case T_STEPS:
        break;
    case T_DYN:                                   /* 1 kHz, volume ramp    */
        route(M2, M2);
        break;
    case T_NOISE:                                 /* broadband noise both  */
        route(M4, M4); ch4_noise(12);
        break;
    case T_WAVE:                                  /* wave-RAM triangle     */
        ch3_wave_load(); route(M3, M3); ch3_note(RWV(300));
        break;
    case T_BIAS:                                  /* 1 kHz, cycle bit-rate */
        route(M2, M2); ch2_note(RSQ(1000), 12, 2);
        break;
    case T_DSOUND:                                /* FIFO sine + sweep     */
        psg_off();
        ds_inc = (u32)440 << 10;
        ds_init();
        break;
    case T_SILENCE:                               /* noise-floor window    */
        silence_all();
        break;
    }
}

static void test_update(int t, int f)
{
    switch (t) {
    case T_STEREO: {
        /* seg 0: both at once (low-left + high-right); a summed path makes
           both tones audible in both ears. seg 1: left only. seg 2: right only. */
        int seg = sub % 3;
        if (subTimer == 0) {
            if (seg == 0) {
                route(M1, M2);                       /* ch1->L, ch2->R */
                ch1_note(RSQ(330), 12, 2);
                ch2_note(RSQ(1320), 12, 2);
                screen_status("DUAL  L=330  R=1320 HZ");
            } else if (seg == 1) {
                REG_SOUND1CNT_H = 0;                  /* silence ch1 */
                route(M2, 0); ch2_note(RSQ(440), 12, 2);
                screen_status_num("LEFT ONLY ", 440, " HZ");
            } else {
                route(0, M2); ch2_note(RSQ(880), 12, 2);
                screen_status_num("RIGHT ONLY ", 880, " HZ");
            }
        }
        {
            int lim = (seg == 0) ? 150 : 90;         /* dual segment a bit longer */
            if (++subTimer >= lim) { subTimer = 0; sub++; }
        }
        break;
    }

    case T_SWEEP:
        if (subTimer == 0 && sub < NSWEEP) {
            route(M2, M2); ch2_note(sweepRate[sub], 12, 2);
            screen_status_num("SWEEP ", sweepHz[sub], " HZ");
        }
        if (++subTimer >= 16) { subTimer = 0; if (sub < NSWEEP - 1) sub++; }
        break;

    case T_STEPS:
        if (subTimer == 0 && sub < NSTEP) {
            route(M2, M2); ch2_note(stepRate[sub], 12, 2);
            screen_status_num("TONE ", stepHz[sub], " HZ");
        }
        if (++subTimer >= 40) { subTimer = 0; if (sub < NSTEP - 1) sub++; }
        break;

    case T_DYN: {
        int vol = 15 - sub;
        if (vol < 0) vol = 0;
        if (subTimer == 0) { ch2_note(RSQ(1000), (u8)vol, 2);
                             screen_status_num("VOLUME ", (unsigned)vol, " / 15"); }
        if (++subTimer >= 22) { subTimer = 0; if (sub < 15) sub++; }
        break;
    }

    case T_BIAS:
        if (subTimer == 0 && sub < 4) {
            static const char *const bn[4] = {
                "9-BIT  32.768 KHZ PWM", "8-BIT  65.536 KHZ PWM",
                "7-BIT 131.072 KHZ PWM", "6-BIT 262.144 KHZ PWM"
            };
            REG_SOUNDBIAS = biasModes[sub];
            ch2_note(RSQ(1000), 12, 2);     /* retrigger marks each segment */
            screen_status(bn[sub]);
        }
        if (++subTimer >= 100) { subTimer = 0; if (sub < 3) sub++; }
        break;

    case T_DSOUND: {
        /* hold 440 Hz for ~120 frames, then sweep the table */
        if (f == 0) screen_status_num("SINE ", 440, " HZ");
        if (f >= 120) {
            if (subTimer == 0 && sub < NSWEEP) {
                ds_inc = (u32)sweepHz[sub] << 10;
                screen_status_num("SWEEP ", sweepHz[sub], " HZ");
            }
            if (++subTimer >= 16) { subTimer = 0; if (sub < NSWEEP - 1) sub++; }
        }
        break;
    }

    default:
        break;
    }
    (void)f;
}

/* ---- input ---- */
static u16 prevKeys;
static int prevBoth;
static int bothSeen;          /* a L+R chord happened in this press episode */

static void handle_input(void)
{
    u16 keys = (u16)(~REG_KEYINPUT) & (KEY_L | KEY_R);
    int both = (keys & KEY_L) && (keys & KEY_R);

    /* L+R together toggles pause immediately, regardless of which landed
       first (it fires on the frame both are down). */
    if (both && !prevBoth) {
        paused = !paused;
        screen_set_paused(paused);
        screen_show_test(cur);
    }
    if (both) bothSeen = 1;

    /* Single-button nav fires on release, only if no chord happened this
       episode, so pressing the two buttons a frame apart can't skip a test. */
    if (!bothSeen) {
        if ((prevKeys & KEY_R) && !(keys & KEY_R))      enter_test(cur + 1);
        else if ((prevKeys & KEY_L) && !(keys & KEY_L)) enter_test(cur - 1);
    }

    if (keys == 0) bothSeen = 0;          /* reset once everything is released */
    prevBoth = both;
    prevKeys = keys;
}

/* ---- init ---- */
static void sound_init(void)
{
    REG_SOUNDCNT_X = SND_ENABLE;          /* master power */
    REG_SOUNDCNT_H = DMG_VOL_100;         /* PSG ratio 100%, DirectSound off */
    REG_SOUNDCNT_L = 0;
    REG_SOUNDBIAS  = BIAS_9BIT_32K;       /* GBA default sampling mode */
}

static void boot_chime(void)
{
    static const u16 notes[3] = { RSQ(523), RSQ(659), RSQ(1047) };
    int i, f;
    route(M2, M2);
    for (i = 0; i < 3; i++) {
        ch2_note(notes[i], 12, 2);
        for (f = 0; f < 11; f++) vsync();
    }
    REG_SOUND2CNT_L = 0;
    REG_SOUNDCNT_L = 0;
    for (f = 0; f < 10; f++) vsync();
}

int main(void)
{
    screen_init();                        /* Mode 3; ignored if no screen */
    sound_init();
    boot_chime();
#ifndef START_TEST
#define START_TEST T_REF
#endif
    enter_test(START_TEST);
#ifdef START_PAUSED
    paused = 1; screen_set_paused(1);
#endif

    for (;;) {
        vsync();
        handle_input();

        switch (phase) {
        case PH_ANNOUNCE:
            announce_tick(pframe);
            if (++pframe >= announceLen) {
                phase = PH_RUN; pframe = 0;
                sub = 0; subTimer = 0;
                REG_SOUNDCNT_L = 0;
                test_setup(cur);
            }
            break;

        case PH_RUN:
            test_update(cur, pframe);
            if (ds_active) ds_frame();
            if (++pframe >= dur[cur]) {
                if (paused) {             /* loop this test after a short gap */
                    silence_all();
                    phase = PH_GAP; pframe = 0;
                } else {
                    enter_test(cur + 1);
                }
            }
            break;

        case PH_GAP:                      /* brief silence, then replay (no beeps) */
            if (++pframe >= LOOP_GAP) {
                phase = PH_RUN; pframe = 0;
                sub = 0; subTimer = 0;
                test_setup(cur);
            }
            break;
        }
    }
    return 0;
}
