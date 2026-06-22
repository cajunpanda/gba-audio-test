/* screen.h - optional on-screen explainer (Mode 3). Audio runs regardless. */
#ifndef SCREEN_H
#define SCREEN_H

void screen_init(void);
void screen_show_test(int t);                 /* full redraw for a test */
void screen_status(const char *s);            /* dynamic status line */
void screen_status_num(const char *pre, unsigned val, const char *suf);
void screen_set_paused(int paused);

#endif
