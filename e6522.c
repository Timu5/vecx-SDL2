#include <stdint.h>

#include "e6522.h"
#include "vecx.h"

VIA6522 VIA;

/* update IRQ and bit-7 of the ifr register after making an adjustment to
 * ifr.
 */

static void int_update (void)
{
	if ((VIA.ifr & 0x7f) & (VIA.ier & 0x7f)) {
		VIA.ifr |= 0x80;
	} else {
		VIA.ifr &= 0x7f;
	}
}

uint8_t via_read (uint16_t address)
{
	uint8_t data = 0;
	
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
	return data;
}

void via_write (uint16_t address, uint8_t data)
{
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

/* perform a single cycle worth of via emulation.
 * via_sstep0 is the first postion of the emulation.
 */

void via_sstep0 (void)
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

void via_sstep1 (void)
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

void via_reset (void)
{
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
}