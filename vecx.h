#ifndef __VECX_H
#define __VECX_H

enum {
	VECTREX_MHZ		= 1500000, /* speed of the vectrex being emulated */
	VECTREX_COLORS  = 128,     /* number of possible colors ... grayscale */
};

typedef struct vector_type {
	long x0, y0; /* start coordinate */
	long x1, y1; /* end coordinate */

				 /* color [0, VECTREX_COLORS - 1], if color = VECTREX_COLORS, then this is
				 * an invalid entry and must be ignored.
				 */
	unsigned char color;
} vector_t;

extern uint8_t rom[8192];
extern uint8_t cart[32768];
extern uint8_t ram[1024];

extern uint8_t snd_regs[16];
extern uint8_t snd_select;

extern size_t vector_draw_cnt;
extern vector_t vectors[];


void vecx_reset (void);
void vecx_emu (long cycles);

#endif
