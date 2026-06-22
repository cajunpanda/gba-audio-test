/* Host harness: renders the real screen.c drawing code into a PPM so the
   on-screen layout can be verified without a GBA. Build with -DSCREEN_HOST. */
#include <stdio.h>
#include <stdlib.h>
#include "../src/gba.h"
#include "../src/screen.h"

#define W 240
#define H 160
static u16 buf[W * H];
volatile u16 *VRAM = buf;

static void dump(const char *path)
{
    FILE *f = fopen(path, "wb");
    fprintf(f, "P6\n%d %d\n255\n", W, H);
    for (int i = 0; i < W * H; i++) {
        u16 p = buf[i];                       /* BGR555 */
        int r = (p & 31) << 3, g = ((p >> 5) & 31) << 3, b = ((p >> 10) & 31) << 3;
        fputc(r, f); fputc(g, f); fputc(b, f);
    }
    fclose(f);
}

int main(int argc, char **argv)
{
    int t = argc > 1 ? atoi(argv[1]) : 0;
    screen_init();
    screen_show_test(t);
    if (argc > 2) screen_status(argv[2]);     /* optional status line */
    dump("/tmp/screen.ppm");
    return 0;
}
