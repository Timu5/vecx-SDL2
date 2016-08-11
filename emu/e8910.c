#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <SDL.h>

#include "e8910.h"

/***************************************************************************

  ay8910.c


  Emulation of the AY-3-8910 / YM2149 sound chip.

  Based on various code snippets by Ville Hallik, Michael Cuddy,
  Tatsuyuki Satoh, Fabrice Frances, Nicola Salmoria.

***************************************************************************/

#define STEP2 length
#define STEP  2

AY8910 PSG;

uint16_t vol_table[32] = { 0, 23, 27, 33, 39, 46, 55, 65, 77, 92, 109, 129, 154, 183, 217, 258, 307,365, 434, 516, 613, 728, 865, 1029, 1223, 1453, 1727, 2052, 2439, 2899, 3446, 4095 };

enum
{
	SOUND_FREQ = 22050,
	SOUND_SAMPLE = 1024,
	TUNEA = 1, /* tuning muliplayer */
	TUNEB = 2, /* tuning divider */

	MAX_OUTPUT = 0x0fff,

	/* register id's */
	AY_AFINE = 0,
	AY_ACOARSE = 1,
	AY_BFINE = 2,
	AY_BCOARSE = 3,
	AY_CFINE = 4,
	AY_CCOARSE = 5,
	AY_NOISEPER = 6,
	AY_ENABLE = 7,
	AY_AVOL = 8,
	AY_BVOL = 9,
	AY_CVOL = 10,
	AY_EFINE = 11,
	AY_ECOARSE = 12,
	AY_ESHAPE = 13,

	AY_PORTA = 14,
	AY_PORTB = 15
};

void e8910_write(uint8_t r, uint8_t v)
{
	int32_t old;

	PSG.regs[r] = v;

	/* A note about the period of tones, noise and envelope: for speed reasons,*/
	/* we count down from the period to 0, but careful studies of the chip     */
	/* output prove that it instead counts up from 0 until the counter becomes */
	/* greater or equal to the period. This is an important difference when the*/
	/* program is rapidly changing the period to modulate the sound.           */
	/* To compensate for the difference, when the period is changed we adjust  */
	/* our internal counter.                                                   */
	/* Also, note that period = 0 is the same as period = 1. This is mentioned */
	/* in the YM2203 data sheets. However, this does NOT apply to the Envelope */
	/* period. In that case, period = 0 is half as period = 1. */
	switch (r)
	{
	case AY_AFINE:
	case AY_ACOARSE:
		PSG.regs[AY_ACOARSE] &= 0x0f;
		old = PSG.per_a;
		PSG.per_a = ((PSG.regs[AY_AFINE] + 256 * PSG.regs[AY_ACOARSE]) * TUNEA) / TUNEB;
		if (PSG.per_a == 0) PSG.per_a = 1;
		PSG.cnt_a += PSG.per_a - old;
		if (PSG.cnt_a <= 0) PSG.cnt_a = 1;
		break;
	case AY_BFINE:
	case AY_BCOARSE:
		PSG.regs[AY_BCOARSE] &= 0x0f;
		old = PSG.per_b;
		PSG.per_b = ((PSG.regs[AY_BFINE] + 256 * PSG.regs[AY_BCOARSE]) * TUNEA) / TUNEB;
		if (PSG.per_b == 0) PSG.per_b = 1;
		PSG.cnt_b += PSG.per_b - old;
		if (PSG.cnt_b <= 0) PSG.cnt_b = 1;
		break;
	case AY_CFINE:
	case AY_CCOARSE:
		PSG.regs[AY_CCOARSE] &= 0x0f;
		old = PSG.per_c;
		PSG.per_c = ((PSG.regs[AY_CFINE] + 256 * PSG.regs[AY_CCOARSE]) * TUNEA) / TUNEB;
		if (PSG.per_c == 0) PSG.per_c = 1;
		PSG.cnt_c += PSG.per_c - old;
		if (PSG.cnt_c <= 0) PSG.cnt_c = 1;
		break;
	case AY_NOISEPER:
		PSG.regs[AY_NOISEPER] &= 0x1f;
		old = PSG.per_n;
		PSG.per_n = (PSG.regs[AY_NOISEPER] * TUNEA) / TUNEB;
		if (PSG.per_n == 0) PSG.per_n = 1;
		PSG.cnt_n += PSG.per_n - old;
		if (PSG.cnt_n <= 0) PSG.cnt_n = 1;
		break;
	case AY_ENABLE:
		break;
	case AY_AVOL:
		PSG.regs[AY_AVOL] &= 0x1f;
		PSG.env_a = PSG.regs[AY_AVOL] & 0x10;
		PSG.vol_a = PSG.env_a ? PSG.vol_e : vol_table[PSG.regs[AY_AVOL] ? PSG.regs[AY_AVOL] * 2 + 1 : 0];
		break;
	case AY_BVOL:
		PSG.regs[AY_BVOL] &= 0x1f;
		PSG.env_b = PSG.regs[AY_BVOL] & 0x10;
		PSG.vol_b = PSG.env_b ? PSG.vol_e : vol_table[PSG.regs[AY_BVOL] ? PSG.regs[AY_BVOL] * 2 + 1 : 0];
		break;
	case AY_CVOL:
		PSG.regs[AY_CVOL] &= 0x1f;
		PSG.env_c = PSG.regs[AY_CVOL] & 0x10;
		PSG.vol_c = PSG.env_c ? PSG.vol_e : vol_table[PSG.regs[AY_CVOL] ? PSG.regs[AY_CVOL] * 2 + 1 : 0];
		break;
	case AY_EFINE:
	case AY_ECOARSE:
		old = PSG.per_e;
		PSG.per_e = ((PSG.regs[AY_EFINE] + 256 * PSG.regs[AY_ECOARSE])) * 1;
		if (PSG.per_e == 0) PSG.per_e = 1;
		PSG.cnt_e += PSG.per_e - old;
		if (PSG.cnt_e <= 0) PSG.cnt_e = 1;
		break;
	case AY_ESHAPE:
		/* envelope shapes:
		C AtAlH
		0 0 x x  \___

		0 1 x x  /___

		1 0 0 0  \\\\

		1 0 0 1  \___

		1 0 1 0  \/\/
				  ___
		1 0 1 1  \

		1 1 0 0  ////
				  ___
		1 1 0 1  /

		1 1 1 0  /\/\

		1 1 1 1  /___

		The envelope counter on the AY-3-8910 has 16 steps. On the YM2149 it
		has twice the steps, happening twice as fast. Since the end result is
		just a smoother curve, we always use the YM2149 behaviour.
		*/
		PSG.regs[AY_ESHAPE] &= 0x0f;
		PSG.attack = (PSG.regs[AY_ESHAPE] & 0x04) ? 0x1f : 0x00;
		if ((PSG.regs[AY_ESHAPE] & 0x08) == 0)
		{
			/* if Continue = 0, map the shape to the equivalent one which has Continue = 1 */
			PSG.hold = 1;
			PSG.alternate = PSG.attack;
		}
		else
		{
			PSG.hold = PSG.regs[AY_ESHAPE] & 0x01;
			PSG.alternate = PSG.regs[AY_ESHAPE] & 0x02;
		}
		PSG.cnt_e = PSG.per_e;
		PSG.cnt_env = 0x1f;
		PSG.holding = 0;
		PSG.vol_e = vol_table[PSG.cnt_env ^ PSG.attack];
		if (PSG.env_a) PSG.vol_a = PSG.vol_e;
		if (PSG.env_b) PSG.vol_b = PSG.vol_e;
		if (PSG.env_c) PSG.vol_c = PSG.vol_e;
		break;
	case AY_PORTA:
		break;
	case AY_PORTB:
		break;
	}
}

static void e8910_callback(void *userdata, uint8_t *stream, int length)
{
	(void)userdata;

	int outn;
	uint8_t* buf1 = stream;
	static uint16_t last_vol = 128;

	length = length * 2;

	/* The 8910 has three outputs, each output is the mix of one of the three */
	/* tone generators and of the (single) noise generator. The two are mixed */
	/* BEFORE going into the DAC. The formula to mix each channel is: */
	/* (ToneOn | ToneDisable) & (NoiseOn | NoiseDisable). */
	/* Note that this means that if both tone and noise are disabled, the output */
	/* is 1, not 0, and can be modulated changing the volume. */


	/* If the channels are disabled, set their output to 1, and increase the */
	/* counter, if necessary, so they will not be inverted during this update. */
	/* Setting the output to 1 is necessary because a disabled channel is locked */
	/* into the ON state (see above); and it has no effect if the volume is 0. */
	/* If the volume is 0, increase the counter, but don't touch the output. */
	if (PSG.regs[AY_ENABLE] & 0x01)
	{
		if (PSG.cnt_a <= STEP2) PSG.cnt_a += STEP2;
		PSG.out_a = 1;
	}
	else if (PSG.regs[AY_AVOL] == 0)
	{
		/* note that I do count += length, NOT count = length + 1. You might think */
		/* it's the same since the volume is 0, but doing the latter could cause */
		/* interferencies when the program is rapidly modulating the volume. */
		if (PSG.cnt_a <= STEP2) PSG.cnt_a += STEP2;
	}

	if (PSG.regs[AY_ENABLE] & 0x02)
	{
		if (PSG.cnt_b <= STEP2) PSG.cnt_b += STEP2;
		PSG.out_b = 1;
	}
	else if (PSG.regs[AY_BVOL] == 0)
	{
		if (PSG.cnt_b <= STEP2) PSG.cnt_b += STEP2;
	}

	if (PSG.regs[AY_ENABLE] & 0x04)
	{
		if (PSG.cnt_c <= STEP2) PSG.cnt_c += STEP2;
		PSG.out_c = 1;
	}
	else if (PSG.regs[AY_CVOL] == 0)
	{
		if (PSG.cnt_c <= STEP2) PSG.cnt_c += STEP2;
	}

	/* for the noise channel we must not touch out_n - it's also not necessary */
	/* since we use outn. */
	if ((PSG.regs[AY_ENABLE] & 0x38) == 0x38)	/* all off */
		if (PSG.cnt_n <= STEP2) PSG.cnt_n += STEP2;

	outn = (PSG.out_n | PSG.regs[AY_ENABLE]);

	/* buffering loop */
	while (length > 0)
	{
		unsigned vol;
		int left = 2;
		/* vol_a, vol_b and vol_c keep track of how long each square wave stays */
		/* in the 1 position during the sample period. */

		int vol_a, vol_b, vol_c;
		vol_a = vol_b = vol_c = 0;

		do
		{
			int nextevent;

			if (PSG.cnt_n < left) nextevent = PSG.cnt_n;
			else nextevent = left;

			if (outn & 0x08)
			{
				if (PSG.out_a) vol_a += PSG.cnt_a;
				PSG.cnt_a -= nextevent;
				/* per_a is the half period of the square wave. Here, in each */
				/* loop I add per_a twice, so that at the end of the loop the */
				/* square wave is in the same status (0 or 1) it was at the start. */
				/* vol_a is also incremented by per_a, since the wave has been 1 */
				/* exactly half of the time, regardless of the initial position. */
				/* If we exit the loop in the middle, out_a has to be inverted */
				/* and vol_a incremented only if the exit status of the square */
				/* wave is 1. */
				while (PSG.cnt_a <= 0)
				{
					PSG.cnt_a += PSG.per_a;
					if (PSG.cnt_a > 0)
					{
						PSG.out_a ^= 1;
						if (PSG.out_a) vol_a += PSG.per_a;
						break;
					}
					PSG.cnt_a += PSG.per_a;
					vol_a += PSG.per_a;
				}
				if (PSG.out_a) vol_a -= PSG.cnt_a;
			}
			else
			{
				PSG.cnt_a -= nextevent;
				while (PSG.cnt_a <= 0)
				{
					PSG.cnt_a += PSG.per_a;
					if (PSG.cnt_a > 0)
					{
						PSG.out_a ^= 1;
						break;
					}
					PSG.cnt_a += PSG.per_a;
				}
			}

			if (outn & 0x10)
			{
				if (PSG.out_b) vol_b += PSG.cnt_b;
				PSG.cnt_b -= nextevent;
				while (PSG.cnt_b <= 0)
				{
					PSG.cnt_b += PSG.per_b;
					if (PSG.cnt_b > 0)
					{
						PSG.out_b ^= 1;
						if (PSG.out_b) vol_b += PSG.per_b;
						break;
					}
					PSG.cnt_b += PSG.per_b;
					vol_b += PSG.per_b;
				}
				if (PSG.out_b) vol_b -= PSG.cnt_b;
			}
			else
			{
				PSG.cnt_b -= nextevent;
				while (PSG.cnt_b <= 0)
				{
					PSG.cnt_b += PSG.per_b;
					if (PSG.cnt_b > 0)
					{
						PSG.out_b ^= 1;
						break;
					}
					PSG.cnt_b += PSG.per_b;
				}
			}

			if (outn & 0x20)
			{
				if (PSG.out_c) vol_c += PSG.cnt_c;
				PSG.cnt_c -= nextevent;
				while (PSG.cnt_c <= 0)
				{
					PSG.cnt_c += PSG.per_c;
					if (PSG.cnt_c > 0)
					{
						PSG.out_c ^= 1;
						if (PSG.out_c) vol_c += PSG.per_c;
						break;
					}
					PSG.cnt_c += PSG.per_c;
					vol_c += PSG.per_c;
				}
				if (PSG.out_c) vol_c -= PSG.cnt_c;
			}
			else
			{
				PSG.cnt_c -= nextevent;
				while (PSG.cnt_c <= 0)
				{
					PSG.cnt_c += PSG.per_c;
					if (PSG.cnt_c > 0)
					{
						PSG.out_c ^= 1;
						break;
					}
					PSG.cnt_c += PSG.per_c;
				}
			}

			PSG.cnt_n -= nextevent;
			if (PSG.cnt_n <= 0)
			{
				/* Is noise output going to change? */
				if ((PSG.RNG + 1) & 2)	/* (bit0^bit1)? */ {
					PSG.out_n = ~PSG.out_n;
					outn = (PSG.out_n | PSG.regs[AY_ENABLE]);
				}

				/* The Random Number Generator of the 8910 is a 17-bit shift */
				/* register. The input to the shift register is bit0 XOR bit3 */
				/* (bit0 is the output). This was verified on AY-3-8910 and YM2149 chips. */

				/* The following is a fast way to compute bit17 = bit0^bit3. */
				/* Instead of doing all the logic operations, we only check */
				/* bit0, relying on the fact that after three shifts of the */
				/* register, what now is bit3 will become bit0, and will */
				/* invert, if necessary, bit14, which previously was bit17. */
				if (PSG.RNG & 1) PSG.RNG ^= 0x24000; /* This version is called the "Galois configuration". */
				PSG.RNG >>= 1;
				PSG.cnt_n += PSG.per_n;
			}

			left -= nextevent;
		} while (left > 0);

		/* update envelope */
		if (PSG.holding == 0)
		{
			PSG.cnt_e -= STEP;
			if (PSG.cnt_e <= 0)
			{
				do
				{
					PSG.cnt_env--;
					PSG.cnt_e += PSG.per_e;
				} while (PSG.cnt_e <= 0);

				/* check envelope current position */
				if (PSG.cnt_env < 0)
				{
					if (PSG.hold)
					{
						if (PSG.alternate)
							PSG.attack ^= 0x1f;
						PSG.holding = 1;
						PSG.cnt_env = 0;
					}
					else
					{
						/* if cnt_env has looped an odd number of times (usually 1), */
						/* invert the output. */
						if (PSG.alternate && (PSG.cnt_env & 0x20))
							PSG.attack ^= 0x1f;

						PSG.cnt_env &= 0x1f;
					}
				}

				PSG.vol_e = vol_table[PSG.cnt_env ^ PSG.attack];
				/* reload volume */
				if (PSG.env_a) PSG.vol_a = PSG.vol_e;
				if (PSG.env_b) PSG.vol_b = PSG.vol_e;
				if (PSG.env_c) PSG.vol_c = PSG.vol_e;
			}
		}

		vol = ((vol_a * PSG.vol_a + vol_b * PSG.vol_b + vol_c * PSG.vol_c) / (3 * STEP)) >> 6;
		vol = ((last_vol + (128 + vol)) / (1 + 1));
		if (--length & 1) *(buf1++) = vol;
		last_vol = vol;
	}
}

void e8910_reset(void)
{
	for (uint8_t r = 0; r < 16; r++)
		e8910_write(r, 0);

	/* input buttons */
	e8910_write(14, 0xff);
}

void e8910_init(void)
{
	// SDL audio stuff
	SDL_AudioSpec reqSpec;
	SDL_AudioSpec givenSpec;

	PSG.RNG = 1;
	PSG.out_a = 0;
	PSG.out_b = 0;
	PSG.out_c = 0;
	PSG.out_n = 0xff;

	// set up audio buffering
	reqSpec.freq = SOUND_FREQ;            // Audio frequency in samples per second
	reqSpec.format = AUDIO_U8;          // Audio data format
	reqSpec.channels = 1;            // Number of channels: 1 mono, 2 stereo
	reqSpec.samples = SOUND_SAMPLE;            // Audio buffer size in samples
	reqSpec.callback = e8910_callback;      // Callback function for filling the audio buffer
	reqSpec.userdata = NULL;
	/* Open the audio device */
	if (SDL_OpenAudio(&reqSpec, &givenSpec) < 0)
	{
		fprintf(stderr, "Couldn't open audio: %s\n", SDL_GetError());
		exit(-1);
	}

	// Start playing audio
	SDL_PauseAudio(0);
}

void e8910_done(void)
{
	SDL_CloseAudio();
}
