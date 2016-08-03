#include <stdio.h>
#include <stdint.h>

#include "e6809.h"
#include "e6522.h"
#include "vecx.h"
#include "osint.h"
#include "e8910.h"

uint8_t rom[8192];
uint8_t cart[32768];
uint8_t ram[1024];

/* the sound chip registers */

uint8_t snd_regs[16];
uint8_t snd_select;

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

	VECTOR_MAX_CNT		= VECTREX_MHZ / VECTREX_PDECAY,
};

unsigned alg_vectoring; /* are we drawing a vector right now? */
long alg_vector_x0;
long alg_vector_y0;
long alg_vector_x1;
long alg_vector_y1;
long alg_vector_dx;
long alg_vector_dy;
unsigned char alg_vector_color;

size_t vector_draw_cnt;
vector_t vectors[VECTOR_MAX_CNT];

long fcycles;

/* update the snd chips internal registers when VIA.ora/VIA.orb changes */

void snd_update (void)
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

void alg_update (void)
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

uint8_t read8_port_a()
{
	if ((VIA.orb & 0x18) == 0x08) {
		/* the snd chip is driving port a */

		return snd_regs[snd_select];
	}
	else {
		return VIA.ora;
	}
}

uint8_t read8_port_b()
{
	/* compare signal is an input so the value does not come from
	* VIA.orb.
	*/

	if (VIA.acr & 0x80) {
		/* timer 1 has control of bit 7 */

		return (uint8_t)((VIA.orb & 0x5f) | VIA.t1pb7 | alg_compare);
	} else {
		/* bit 7 is being driven by VIA.orb */

		return (uint8_t)((VIA.orb & 0xdf) | alg_compare);
	}
}

void write8_port_a (uint8_t data)
{
	snd_update();

	/* output of port a feeds directly into the dac which then
	* feeds the x axis sample and hold.
	*/

	alg_xsh = data ^ 0x80;
	alg_update();
}

void write8_port_b (uint8_t data)
{
	snd_update();
	alg_update();
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
			
			data = via_read(address);
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
			via_write(address, data);
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
	fcycles = FCYCLES_INIT;

	via_read8_port_a = read8_port_a;
	via_read8_port_b = read8_port_b;
	via_write8_port_a = write8_port_a;
	via_write8_port_b = write8_port_b;

	via_reset();

	e6809_read8 = read8;
	e6809_write8 = write8;

	e6809_reset ();
}

static __inline void alg_addline (long x0, long y0, long x1, long y1, unsigned char color)
{
		vectors[vector_draw_cnt].x0 = x0;
		vectors[vector_draw_cnt].y0 = y0;
		vectors[vector_draw_cnt].x1 = x1;
		vectors[vector_draw_cnt].y1 = y1;
		vectors[vector_draw_cnt].color = color;
		vector_draw_cnt++;
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

			/* everything that was drawn during this pass now enters
			 * the erase list for the next pass.
			 */
			vector_draw_cnt = 0;
		}
	}
}
