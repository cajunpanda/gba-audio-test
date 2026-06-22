/* gba.h - minimal GBA register definitions used by the audio test ROM. */
#ifndef GBA_H
#define GBA_H

typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;
typedef signed   char  s8;
typedef signed   short s16;
typedef signed   int   s32;

#define vu16 volatile u16
#define vu32 volatile u32

#define REG(off) (*(volatile u16 *)(0x04000000 + (off)))
#define REG32(off) (*(volatile u32 *)(0x04000000 + (off)))

/* Display / timing */
#define REG_DISPCNT   REG(0x0000)
#define REG_DISPSTAT  REG(0x0004)
#define REG_VCOUNT    REG(0x0006)

/* Keypad (0 = pressed) */
#define REG_KEYINPUT  REG(0x0130)
#define KEY_R   (1 << 8)
#define KEY_L   (1 << 9)

/* --- Sound: PSG channels --- */
#define REG_SOUND1CNT_L REG(0x0060)  /* ch1 sweep */
#define REG_SOUND1CNT_H REG(0x0062)  /* ch1 duty/len/envelope */
#define REG_SOUND1CNT_X REG(0x0064)  /* ch1 freq/control */
#define REG_SOUND2CNT_L REG(0x0068)  /* ch2 duty/len/envelope */
#define REG_SOUND2CNT_H REG(0x006C)  /* ch2 freq/control */
#define REG_SOUND3CNT_L REG(0x0070)  /* ch3 wave enable */
#define REG_SOUND3CNT_H REG(0x0072)  /* ch3 len/volume */
#define REG_SOUND3CNT_X REG(0x0074)  /* ch3 freq/control */
#define REG_SOUND4CNT_L REG(0x0078)  /* ch4 len/envelope */
#define REG_SOUND4CNT_H REG(0x007C)  /* ch4 freq/control */
#define REG_WAVE_RAM    ((volatile u16 *)0x04000090)

/* --- Sound: mixer / control --- */
#define REG_SOUNDCNT_L  REG(0x0080)  /* DMG vol + L/R channel enables */
#define REG_SOUNDCNT_H  REG(0x0082)  /* DMG ratio + DirectSound control */
#define REG_SOUNDCNT_X  REG(0x0084)  /* master enable */
#define REG_SOUNDBIAS   REG(0x0088)  /* bias level + sampling/bit-rate mode */
#define REG_FIFO_A      REG32(0x00A0)
#define REG_FIFO_B      REG32(0x00A4)

/* SOUNDCNT_X */
#define SND_ENABLE      (1 << 7)

/* SOUNDCNT_H bits */
#define DMG_VOL_100     (2 << 0)   /* PSG output ratio 100% */
#define DSA_VOL_100     (1 << 2)
#define DSB_VOL_100     (1 << 3)
#define DSA_R_ENABLE    (1 << 8)
#define DSA_L_ENABLE    (1 << 9)
#define DSA_TIMER0      (0 << 10)
#define DSA_RESET       (1 << 11)
#define DSB_R_ENABLE    (1 << 12)
#define DSB_L_ENABLE    (1 << 13)
#define DSB_TIMER0      (0 << 14)
#define DSB_RESET       (1 << 15)

/* SOUNDCNT_L: bits 0-2 right master vol, 4-6 left master vol,
   8-11 right channel enables (ch1..4), 12-15 left channel enables. */
#define PSG_R_VOL(v)    ((v) << 0)
#define PSG_L_VOL(v)    ((v) << 4)
#define PSG_R_EN(ch)    (1 << (8  + ((ch) - 1)))
#define PSG_L_EN(ch)    (1 << (12 + ((ch) - 1)))

/* SOUNDBIAS sampling/amplitude mode (bits 15-14) over default bias 0x100 */
#define BIAS_BASE       (0x100 << 1)            /* default bias level field */
#define BIAS_9BIT_32K   (BIAS_BASE | (0 << 14)) /* 9-bit, 32.768 kHz */
#define BIAS_8BIT_65K   (BIAS_BASE | (1 << 14)) /* 8-bit, 65.536 kHz */
#define BIAS_7BIT_131K  (BIAS_BASE | (2 << 14)) /* 7-bit, 131.072 kHz */
#define BIAS_6BIT_262K  (BIAS_BASE | (3 << 14)) /* 6-bit, 262.144 kHz */

/* PSG note control bits */
#define SND_RESTART     (1 << 15)  /* in CNT_X / CNT_H freq registers */
#define SND_LEN_FLAG    (1 << 14)

/* Timers */
#define REG_TM0CNT_L  REG(0x0100)
#define REG_TM0CNT_H  REG(0x0102)
#define TM_ENABLE     (1 << 7)
#define TM_FREQ_1     0           /* 16.78 MHz tick */

/* DMA1 (DirectSound FIFO A) */
#define REG_DMA1SAD   REG32(0x00BC)
#define REG_DMA1DAD   REG32(0x00C0)
#define REG_DMA1CNT_L REG(0x00C4)
#define REG_DMA1CNT_H REG(0x00C6)
#define DMA_DEST_FIXED (2 << 5)
#define DMA_SRC_INC    (0 << 7)
#define DMA_REPEAT     (1 << 9)
#define DMA_32BIT      (1 << 10)
#define DMA_SPECIAL    (3 << 12)   /* sound FIFO timing */
#define DMA_ENABLE     (1 << 15)

/* CPU clock for timer reload math (Hz) */
#define CPU_HZ 16777216

#endif /* GBA_H */
