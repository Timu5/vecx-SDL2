#include <stdio.h>
#include <stdint.h>

#include "e6809.h"
#include "vecx.h"
#include "osint.h"
#include "e8910.h"

uint8_t rom[8192];
uint8_t cart[32768];
uint8_t ram[1024];

/* the sound chip registers */

uint8_t snd_regs[16];
uint8_t snd_select;

VIA6522 VIA;

/* analog devices */

unsigned alg_rsh;  /* zero ref sample and hold */
unsigned alg_xsh;  /* x sample and hold */
unsigned alg_ysh;  /* y sample and hold */
unsigned alg_zsh;  /* z sample and hold */
unsigned alg_jch0;		  /* joystick direction channel 0 */
unsigned alg_jch1;		  /* joystick direction channel 1 */
unsigned alg_jch2;		  /* joystick direction channel 2 */
unsigned alg_jch3;		  /* joystick direction channel 3 */
unsigned alg_jsh;  /* joystick sample and hold */

unsigned alg_compare;

long alg_dx;     /* delta x */
long alg_dy;     /* delta y */
long alg_curr_x; /* current x position */
long alg_curr_y; /* current y position */

enum {
	VECTREX_PDECAY	= 30,      /* phosphor decay rate */

	/* number of 6809 cycles before a frame redraw */

	FCYCLES_INIT    = VECTREX_MHZ / VECTREX_PDECAY,

	/* max number of possible vectors that maybe on the screen at one time.
	 * one only needs VECTREX_MHZ / VECTREX_PDECAY but we need to also store
	 * deleted vectors in a single table
	 */

	VECTOR_CNT		= VECTREX_MHZ / VECTREX_PDECAY,

	VECTOR_HASH     = 65521
};

unsigned alg_vectoring; /* are we drawing a vector right now? */
long alg_vector_x0;
long alg_vector_y0;
long alg_vector_x1;
long alg_vector_y1;
long alg_vector_dx;
long alg_vector_dy;
unsigned char alg_vector_color;

long vector_draw_cnt;
long vector_erse_cnt;
vector_t vectors_set[2 * VECTOR_CNT];
vector_t *vectors_draw;
vector_t *vectors_erse;

long vector_hash[VECTOR_HASH];

long fcycles;

/* update the snd chips internal registers when VIA.ora/VIA.orb changes */

static __inline void snd_update (void)
{
	switch (VIA.orb & 0x18) {
	case 0x00:
		/* the sound chip is disabled */
		break;
	case 0x08:
		/* the sound chip is sending data */
		break;
	case 0x10:
		/* the sound chip is recieving data */

		if (snd_select != 14) {
			snd_regs[snd_select] = VIA.ora;
			e8910_write(snd_select, VIA.ora);
		}

		break;
	case 0x18:
		/* the sound chip is latching an address */

		if ((VIA.ora & 0xf0) == 0x00) {
			snd_select = VIA.ora & 0x0f;
		}

		break;
	}
}

/* update the various analog values when orb is written. */

static __inline void alg_update (void)
{
	switch (VIA.orb & 0x06) {
	case 0x00:
		alg_jsh = alg_jch0;

		if ((VIA.orb & 0x01) == 0x00) {
			/* demultiplexor is on */
			alg_ysh = alg_xsh;
		}

		break;
	case 0x02:
		alg_jsh = alg_jch1;

		if ((VIA.orb & 0x01) == 0x00) {
			/* demultiplexor is on */
			alg_rsh = alg_xsh;
		}

		break;
	case 0x04:
		alg_jsh = alg_jch2;

		if ((VIA.orb & 0x01) == 0x00) {
			/* demultiplexor is on */

			if (alg_xsh > 0x80) {
				alg_zsh = alg_xsh - 0x80;
			} else {
				alg_zsh = 0;
			}
		}

		break;
	case 0x06:
		/* sound output line */
		alg_jsh = alg_jch3;
		break;
	}

	/* compare the current joystick direction with a reference */

	if (alg_jsh > alg_xsh) {
		alg_compare = 0x20;
	} else {
		alg_compare = 0;
	}

	/* compute the new "deltas" */

	alg_dx = (long) alg_xsh - (long) alg_rsh;
	alg_dy = (long) alg_rsh - (long) alg_ysh;
}

/* update IRQ and bit-7 of the ifr register after making an adjustment to
 * ifr.
 */

static __inline void int_update (void)
{
	if ((VIA.ifr & 0x7f) & (VIA.ier & 0x7f)) {
		VIA.ifr |= 0x80;
	} else {
		VIA.ifr &= 0x7f;
	}
}

static uint8_t read8 (uint16_t address)
{
	uint8_t data = 0xff;

	if ((address & 0xe000) == 0xe000) {
		/* rom */

		data = rom[address & 0x1fff];
	} else if ((address & 0xe000) == 0xc000) {
		if (address & 0x800) {
			/* ram */

			data = ram[address & 0x3ff];
		} else if (address & 0x1000) {
			/* io */

			switch (address & 0xf) {
			case 0x0:
				/* compare signal is an input so the value does not come from
				 * VIA.orb.
				 */

				if (VIA.acr & 0x80) {
					/* timer 1 has control of bit 7 */

					data = (unsigned char) ((VIA.orb & 0x5f) | VIA.t1pb7 | alg_compare);
				} else {
					/* bit 7 is being driven by VIA.orb */

					data = (unsigned char) ((VIA.orb & 0xdf) | alg_compare);
				}

				break;
			case 0x1:
				/* register 1 also performs handshakes if necessary */

				if ((VIA.pcr & 0x0e) == 0x08) {
					/* if ca2 is in pulse mode or handshake mode, then it
					 * goes low whenever ira is read.
					 */

					VIA.ca2 = 0;
				}

				/* fall through */

			case 0xf:
				if ((VIA.orb & 0x18) == 0x08) {
					/* the snd chip is driving port a */

					data = (unsigned char) snd_regs[snd_select];
				} else {
					data = (unsigned char) VIA.ora;
				}

				break;
			case 0x2:
				data = (unsigned char) VIA.ddrb;
				break;
			case 0x3:
				data = (unsigned char) VIA.ddra;
				break;
			case 0x4:
				/* T1 low order counter */

				data = (unsigned char) VIA.t1c;
				VIA.ifr &= 0xbf; /* remove timer 1 interrupt flag */

				VIA.t1on = 0; /* timer 1 is stopped */
				VIA.t1int = 0;
				VIA.t1pb7 = 0x80;

				int_update ();

				break;
			case 0x5:
				/* T1 high order counter */

				data = (unsigned char) (VIA.t1c >> 8);

				break;
			case 0x6:
				/* T1 low order latch */

				data = (unsigned char) VIA.t1ll;
				break;
			case 0x7:
				/* T1 high order latch */

				data = (unsigned char) VIA.t1lh;
				break;
			case 0x8:
				/* T2 low order counter */

				data = (unsigned char) VIA.t2c;
				VIA.ifr &= 0xdf; /* remove timer 2 interrupt flag */

				VIA.t2on = 0; /* timer 2 is stopped */
				VIA.t2int = 0;

				int_update ();

				break;
			case 0x9:
				/* T2 high order counter */

				data = (unsigned char) (VIA.t2c >> 8);
				break;
			case 0xa:
				data = (unsigned char) VIA.sr;
				VIA.ifr &= 0xfb; /* remove shift register interrupt flag */
				VIA.srb = 0;
				VIA.srclk = 1;

				int_update ();

				break;
			case 0xb:
				data = (unsigned char) VIA.acr;
				break;
			case 0xc:
				data = (unsigned char) VIA.pcr;
				break;
			case 0xd:
				/* interrupt flag register */

				data = (unsigned char) VIA.ifr;
				break;
			case 0xe:
				/* interrupt enable register */

				data = (unsigned char) (VIA.ier | 0x80);
				break;
			}
		}
	} else if (address < 0x8000) {
		/* cartridge */

		data = cart[address];
	}

	return data;
}

static void write8 (uint16_t address, uint8_t data)
{
	if ((address & 0xe000) == 0xe000) {
		/* rom */
	} else if ((address & 0xe000) == 0xc000) {
		/* it is possible for both ram and io to be written at the same! */

		if (address & 0x800) {
			ram[address & 0x3ff] = data;
		}

		if (address & 0x1000) {
			switch (address & 0xf) {
			case 0x0:
				VIA.orb = data;

				snd_update ();

				alg_update ();

				if ((VIA.pcr & 0xe0) == 0x80) {
					/* if cb2 is in pulse mode or handshake mode, then it
					 * goes low whenever orb is written.
					 */

					VIA.cb2h = 0;
				}

				break;
			case 0x1:
				/* register 1 also performs handshakes if necessary */

				if ((VIA.pcr & 0x0e) == 0x08) {
					/* if ca2 is in pulse mode or handshake mode, then it
					 * goes low whenever ora is written.
					 */

					VIA.ca2 = 0;
				}

				/* fall through */

			case 0xf:
				VIA.ora = data;

				snd_update ();

				/* output of port a feeds directly into the dac which then
				 * feeds the x axis sample and hold.
				 */

				alg_xsh = data ^ 0x80;

				alg_update ();

				break;
			case 0x2:
				VIA.ddrb = data;
				break;
			case 0x3:
				VIA.ddra = data;
				break;
			case 0x4:
				/* T1 low order counter */

				VIA.t1ll = data;

				break;
			case 0x5:
				/* T1 high order counter */

				VIA.t1lh = data;
				VIA.t1c = (VIA.t1lh << 8) | VIA.t1ll;
				VIA.ifr &= 0xbf; /* remove timer 1 interrupt flag */

				VIA.t1on = 1; /* timer 1 starts running */
				VIA.t1int = 1;
				VIA.t1pb7 = 0;

				int_update ();

				break;
			case 0x6:
				/* T1 low order latch */

				VIA.t1ll = data;
				break;
			case 0x7:
				/* T1 high order latch */

				VIA.t1lh = data;
				break;
			case 0x8:
				/* T2 low order latch */

				VIA.t2ll = data;
				break;
			case 0x9:
				/* T2 high order latch/counter */

				VIA.t2c = (data << 8) | VIA.t2ll;
				VIA.ifr &= 0xdf;

				VIA.t2on = 1; /* timer 2 starts running */
				VIA.t2int = 1;

				int_update ();

				break;
			case 0xa:
				VIA.sr = data;
				VIA.ifr &= 0xfb; /* remove shift register interrupt flag */
				VIA.srb = 0;
				VIA.srclk = 1;

				int_update ();

				break;
			case 0xb:
				VIA.acr = data;
				break;
			case 0xc:
				VIA.pcr = data;


				if ((VIA.pcr & 0x0e) == 0x0c) {
					/* ca2 is outputting low */

					VIA.ca2 = 0;
				} else {
					/* ca2 is disabled or in pulse mode or is
					 * outputting high.
					 */

					VIA.ca2 = 1;
				}

				if ((VIA.pcr & 0xe0) == 0xc0) {
					/* cb2 is outputting low */

					VIA.cb2h = 0;
				} else {
					/* cb2 is disabled or is in pulse mode or is
					 * outputting high.
					 */

					VIA.cb2h = 1;
				}

				break;
			case 0xd:
				/* interrupt flag register */

				VIA.ifr &= ~(data & 0x7f);
				int_update ();

				break;
			case 0xe:
				/* interrupt enable register */

				if (data & 0x80) {
					VIA.ier |= data & 0x7f;
				} else {
					VIA.ier &= ~(data & 0x7f);
				}

				int_update ();

				break;
			}
		}
	} else if (address < 0x8000) {
		/* cartridge */
	}
}

void vecx_reset (void)
{
	unsigned r;

	/* ram */

	for (r = 0; r < 1024; r++) {
		ram[r] = r & 0xff;
	}

	for (r = 0; r < 16; r++) {
		snd_regs[r] = 0;
		e8910_write(r, 0);
	}

	/* input buttons */

	snd_regs[14] = 0xff;
	e8910_write(14, 0xff);

	snd_select = 0;

	VIA.ora = 0;
	VIA.orb = 0;
	VIA.ddra = 0;
	VIA.ddrb = 0;
	VIA.t1on = 0;
	VIA.t1int = 0;
	VIA.t1c = 0;
	VIA.t1ll = 0;
	VIA.t1lh = 0;
	VIA.t1pb7 = 0x80;
	VIA.t2on = 0;
	VIA.t2int = 0;
	VIA.t2c = 0;
	VIA.t2ll = 0;
	VIA.sr = 0;
	VIA.srb = 8;
	VIA.src = 0;
	VIA.srclk = 0;
	VIA.acr = 0;
	VIA.pcr = 0;
	VIA.ifr = 0;
	VIA.ier = 0;
	VIA.ca2 = 1;
	VIA.cb2h = 1;
	VIA.cb2s = 0;

	alg_rsh = 128;
	alg_xsh = 128;
	alg_ysh = 128;
	alg_zsh = 0;
	alg_jch0 = 128;
	alg_jch1 = 128;
	alg_jch2 = 128;
	alg_jch3 = 128;
	alg_jsh = 128;

	alg_compare = 0; /* check this */

	alg_dx = 0;
	alg_dy = 0;
	alg_curr_x = ALG_MAX_X / 2;
	alg_curr_y = ALG_MAX_Y / 2;

	alg_vectoring = 0;

	vector_draw_cnt = 0;
	vector_erse_cnt = 0;
	vectors_draw = vectors_set;
	vectors_erse = vectors_set + VECTOR_CNT;

	fcycles = FCYCLES_INIT;

	e6809_read8 = read8;
	e6809_write8 = write8;

	e6809_reset ();
}

/* perform a single cycle worth of via emulation.
 * via_sstep0 is the first postion of the emulation.
 */

static __inline void via_sstep0 (void)
{
	unsigned t2shift;

	if (VIA.t1on) {
		VIA.t1c--;

		if ((VIA.t1c & 0xffff) == 0xffff) {
			/* counter just rolled over */

			if (VIA.acr & 0x40) {
				/* continuous interrupt mode */

				VIA.ifr |= 0x40;
				int_update ();
				VIA.t1pb7 = 0x80 - VIA.t1pb7;

				/* reload counter */

				VIA.t1c = (VIA.t1lh << 8) | VIA.t1ll;
			} else {
				/* one shot mode */

				if (VIA.t1int) {
					VIA.ifr |= 0x40;
					int_update ();
					VIA.t1pb7 = 0x80;
					VIA.t1int = 0;
				}
			}
		}
	}

	if (VIA.t2on && (VIA.acr & 0x20) == 0x00) {
		VIA.t2c--;

		if ((VIA.t2c & 0xffff) == 0xffff) {
			/* one shot mode */

			if (VIA.t2int) {
				VIA.ifr |= 0x20;
				int_update ();
				VIA.t2int = 0;
			}
		}
	}

	/* shift counter */

	VIA.src--;

	if ((VIA.src & 0xff) == 0xff) {
		VIA.src = VIA.t2ll;

		if (VIA.srclk) {
			t2shift = 1;
			VIA.srclk = 0;
		} else {
			t2shift = 0;
			VIA.srclk = 1;
		}
	} else {
		t2shift = 0;
	}

	if (VIA.srb < 8) {
		switch (VIA.acr & 0x1c) {
		case 0x00:
			/* disabled */
			break;
		case 0x04:
			/* shift in under control of t2 */

			if (t2shift) {
				/* shifting in 0s since cb2 is always an output */

				VIA.sr <<= 1;
				VIA.srb++;
			}

			break;
		case 0x08:
			/* shift in under system clk control */

			VIA.sr <<= 1;
			VIA.srb++;

			break;
		case 0x0c:
			/* shift in under cb1 control */
			break;
		case 0x10:
			/* shift out under t2 control (free run) */

			if (t2shift) {
				VIA.cb2s = (VIA.sr >> 7) & 1;

				VIA.sr <<= 1;
				VIA.sr |= VIA.cb2s;
			}

			break;
		case 0x14:
			/* shift out under t2 control */

			if (t2shift) {
				VIA.cb2s = (VIA.sr >> 7) & 1;

				VIA.sr <<= 1;
				VIA.sr |= VIA.cb2s;
				VIA.srb++;
			}

			break;
		case 0x18:
			/* shift out under system clock control */

			VIA.cb2s = (VIA.sr >> 7) & 1;

			VIA.sr <<= 1;
			VIA.sr |= VIA.cb2s;
			VIA.srb++;

			break;
		case 0x1c:
			/* shift out under cb1 control */
			break;
		}

		if (VIA.srb == 8) {
			VIA.ifr |= 0x04;
			int_update ();
		}
	}
}

/* perform the second part of the via emulation */

static __inline void via_sstep1 (void)
{
	if ((VIA.pcr & 0x0e) == 0x0a) {
		/* if ca2 is in pulse mode, then make sure
		 * it gets restored to '1' after the pulse.
		 */

		VIA.ca2 = 1;
	}

	if ((VIA.pcr & 0xe0) == 0xa0) {
		/* if cb2 is in pulse mode, then make sure
		 * it gets restored to '1' after the pulse.
		 */

		VIA.cb2h = 1;
	}
}

static __inline void alg_addline (long x0, long y0, long x1, long y1, unsigned char color)
{
	unsigned long key;
	long index;

	key = (unsigned long) x0;
	key = key * 31 + (unsigned long) y0;
	key = key * 31 + (unsigned long) x1;
	key = key * 31 + (unsigned long) y1;
	key %= VECTOR_HASH;

	/* first check if the line to be drawn is in the current draw list.
	 * if it is, then it is not added again.
	 */

	index = vector_hash[key];

	if (index >= 0 && index < vector_draw_cnt &&
		x0 == vectors_draw[index].x0 &&
		y0 == vectors_draw[index].y0 &&
		x1 == vectors_draw[index].x1 &&
		y1 == vectors_draw[index].y1) {
		vectors_draw[index].color = color;
	} else {
		/* missed on the draw list, now check if the line to be drawn is in
		 * the erase list ... if it is, "invalidate" it on the erase list.
		 */

		if (index >= 0 && index < vector_erse_cnt &&
			x0 == vectors_erse[index].x0 &&
			y0 == vectors_erse[index].y0 &&
			x1 == vectors_erse[index].x1 &&
			y1 == vectors_erse[index].y1) {
			vectors_erse[index].color = VECTREX_COLORS;
		}

		vectors_draw[vector_draw_cnt].x0 = x0;
		vectors_draw[vector_draw_cnt].y0 = y0;
		vectors_draw[vector_draw_cnt].x1 = x1;
		vectors_draw[vector_draw_cnt].y1 = y1;
		vectors_draw[vector_draw_cnt].color = color;
		vector_hash[key] = vector_draw_cnt;
		vector_draw_cnt++;
	}
}

/* perform a single cycle worth of analog emulation */

static __inline void alg_sstep (void)
{
	long sig_dx, sig_dy;
	unsigned sig_ramp;
	unsigned sig_blank;

	if ((VIA.acr & 0x10) == 0x10) {
		sig_blank = VIA.cb2s;
	} else {
		sig_blank = VIA.cb2h;
	}

	if (VIA.ca2 == 0) {
		/* need to force the current point to the 'orgin' so just
		 * calculate distance to origin and use that as dx,dy.
		 */

		sig_dx = ALG_MAX_X / 2 - alg_curr_x;
		sig_dy = ALG_MAX_Y / 2 - alg_curr_y;
	} else {
		if (VIA.acr & 0x80) {
			sig_ramp = VIA.t1pb7;
		} else {
			sig_ramp = VIA.orb & 0x80;
		}

		if (sig_ramp == 0) {
			sig_dx = alg_dx;
			sig_dy = alg_dy;
		} else {
			sig_dx = 0;
			sig_dy = 0;
		}
	}

	if (alg_vectoring == 0) {
		if (sig_blank == 1 &&
			alg_curr_x >= 0 && alg_curr_x < ALG_MAX_X &&
			alg_curr_y >= 0 && alg_curr_y < ALG_MAX_Y) {

			/* start a new vector */

			alg_vectoring = 1;
			alg_vector_x0 = alg_curr_x;
			alg_vector_y0 = alg_curr_y;
			alg_vector_x1 = alg_curr_x;
			alg_vector_y1 = alg_curr_y;
			alg_vector_dx = sig_dx;
			alg_vector_dy = sig_dy;
			alg_vector_color = (unsigned char) alg_zsh;
		}
	} else {
		/* already drawing a vector ... check if we need to turn it off */

		if (sig_blank == 0) {
			/* blank just went on, vectoring turns off, and we've got a
			 * new line.
			 */

			alg_vectoring = 0;

			alg_addline (alg_vector_x0, alg_vector_y0,
						 alg_vector_x1, alg_vector_y1,
						 alg_vector_color);
		} else if (sig_dx != alg_vector_dx ||
				   sig_dy != alg_vector_dy ||
				   (unsigned char) alg_zsh != alg_vector_color) {

			/* the parameters of the vectoring processing has changed.
			 * so end the current line.
			 */

			alg_addline (alg_vector_x0, alg_vector_y0,
						 alg_vector_x1, alg_vector_y1,
						 alg_vector_color);

			/* we continue vectoring with a new set of parameters if the
			 * current point is not out of limits.
			 */

			if (alg_curr_x >= 0 && alg_curr_x < ALG_MAX_X &&
				alg_curr_y >= 0 && alg_curr_y < ALG_MAX_Y) {
				alg_vector_x0 = alg_curr_x;
				alg_vector_y0 = alg_curr_y;
				alg_vector_x1 = alg_curr_x;
				alg_vector_y1 = alg_curr_y;
				alg_vector_dx = sig_dx;
				alg_vector_dy = sig_dy;
				alg_vector_color = (unsigned char) alg_zsh;
			} else {
				alg_vectoring = 0;
			}
		}
	}

	alg_curr_x += sig_dx;
	alg_curr_y += sig_dy;

	if (alg_vectoring == 1 &&
		alg_curr_x >= 0 && alg_curr_x < ALG_MAX_X &&
		alg_curr_y >= 0 && alg_curr_y < ALG_MAX_Y) {

		/* we're vectoring ... current point is still within limits so
		 * extend the current vector.
		 */

		alg_vector_x1 = alg_curr_x;
		alg_vector_y1 = alg_curr_y;
	}
}

void vecx_emu (long cycles)
{
	unsigned c, icycles;

	while (cycles > 0) {
		icycles = e6809_sstep (VIA.ifr & 0x80, 0);

		for (c = 0; c < icycles; c++) {
			via_sstep0 ();
			alg_sstep ();
			via_sstep1 ();
		}

		cycles -= (long) icycles;

		fcycles -= (long) icycles;

		if (fcycles < 0) {
			vector_t *tmp;

			fcycles += FCYCLES_INIT;
			osint_render ();

			/* everything that was drawn during this pass now now enters
			 * the erase list for the next pass.
			 */

			vector_erse_cnt = vector_draw_cnt;
			vector_draw_cnt = 0;

			tmp = vectors_erse;
			vectors_erse = vectors_draw;
			vectors_draw = tmp;
		}
	}
}
