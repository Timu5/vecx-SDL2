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

typedef struct {
	/* the via 6522 registers */
	uint8_t ora;
	uint8_t orb;
	uint8_t ddra;
	uint8_t ddrb;
	uint8_t t1on;  /* is timer 1 on? */
	uint8_t t1int; /* are timer 1 interrupts allowed? */
	uint16_t t1c;
	uint8_t t1ll;
	uint8_t t1lh;
	uint8_t t1pb7; /* timer 1 controlled version of pb7 */
	uint8_t t2on;  /* is timer 2 on? */
	uint8_t t2int; /* are timer 2 interrupts allowed? */
	uint16_t t2c;
	uint8_t t2ll;
	uint8_t sr;
	uint8_t srb;   /* number of bits shifted so far */
	uint8_t src;   /* shift counter */
	uint8_t srclk;
	uint8_t acr;
	uint8_t pcr;
	uint8_t ifr;
	uint8_t ier;
	uint8_t ca2;
	uint8_t cb2h;  /* basic handshake version of cb2 */
	uint8_t cb2s;  /* version of cb2 controlled by the shift register */
} VIA6522;

extern uint8_t rom[8192];
extern uint8_t cart[32768];
extern uint8_t ram[1024];

extern VIA6522 VIA;

extern uint8_t snd_regs[16];
extern unsigned alg_jch0;
extern unsigned alg_jch1;
extern unsigned alg_jch2;
extern unsigned alg_jch3;

extern long vector_draw_cnt;
extern long vector_erse_cnt;
extern vector_t *vectors_draw;
extern vector_t *vectors_erse;

void vecx_reset (void);
void vecx_emu (long cycles);

#endif
