#ifndef __VECX_H
#define __VECX_H

enum {
	VECTREX_MHZ		= 1500000, /* speed of the vectrex being emulated */
	VECTREX_COLORS  = 128,     /* number of possible colors ... grayscale */

	ALG_MAX_X		= 33000,
	ALG_MAX_Y		= 41000
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
extern unsigned alg_rsh;
extern unsigned alg_xsh;
extern unsigned alg_ysh;
extern unsigned alg_zsh;
extern unsigned alg_jch0;
extern unsigned alg_jch1;
extern unsigned alg_jch2;
extern unsigned alg_jch3;
extern unsigned alg_jsh; 
extern unsigned alg_compare;
extern long alg_dx;
extern long alg_dy;
extern long alg_curr_x;
extern long alg_curr_y;

extern size_t vector_draw_cnt;
extern vector_t vectors[];


void vecx_reset (void);
void vecx_emu (long cycles);

#endif
