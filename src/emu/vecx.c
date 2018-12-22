#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "vecx.h"

#include "e6809.h"
#include "e6522.h"
#include "e8910.h"
#include "edac.h"

//void(*vecx_render) (void);

//uint8_t rom[8192];
//uint8_t cart[32768];
//uint8_t ram[1024];

/* the sound chip registers */

//uint8_t snd_select;

//size_t vector_draw_cnt;
//vector_t vectors[VECTOR_MAX_CNT];

/* update the snd chips internal registers when VIA.ora/VIA.orb changes */

static void snd_update(vecx *vecx)
{
	switch (vecx->VIA.orb & 0x18)
	{
	case 0x00:
		/* the sound chip is disabled */
		break;
	case 0x08:
		/* the sound chip is sending data */
		break;
	case 0x10:
		/* the sound chip is recieving data */
		if (vecx->snd_select != 14) e8910_write(&vecx->PSG, vecx->snd_select, vecx->VIA.ora);
		break;
	case 0x18:
		/* the sound chip is latching an address */
		if ((vecx->VIA.ora & 0xf0) == 0x00) vecx->snd_select = vecx->VIA.ora & 0x0f;
		break;
	}
}

static uint8_t read8_port_a(vecx *vecx)
{
	if ((vecx->VIA.orb & 0x18) == 0x08)
	{
		/* the snd chip is driving port a */
		return e8910_read(&vecx->PSG, vecx->snd_select);
	}
	else
	{
		return vecx->VIA.ora;
	}
}

static uint8_t read8_port_b(vecx *vecx)
{
	/* compare signal is an input so the value does not come from
	* VIA.orb.
	*/
	if (vecx->VIA.acr & 0x80)
	{
		/* timer 1 has control of bit 7 */
		return (uint8_t)((vecx->VIA.orb & 0x5f) | vecx->VIA.t1pb7 | vecx->DAC.compare);
	}
	else
	{
		/* bit 7 is being driven by VIA.orb */
		return (uint8_t)((vecx->VIA.orb & 0xdf) | vecx->DAC.compare);
	}
}

static void write8_port_a(vecx *vecx, uint8_t data)
{
	snd_update(vecx);

	/* output of port a feeds directly into the dac which then
	* feeds the x axis sample and hold.
	*/
    vecx->DAC.xsh = data ^ 0x80;
	dac_update(&vecx->DAC);
}

static void write8_port_b(vecx *vecx, uint8_t data)
{
	(void)data;
	snd_update(vecx);
	dac_update(&vecx->DAC);
}

static uint8_t read8(vecx *vecx, uint16_t address)
{
	uint8_t data = 0xff;

	if ((address & 0xe000) == 0xe000)
	{
		/* rom */
		data = vecx->rom[address & 0x1fff];
	}
	else if ((address & 0xe000) == 0xc000)
	{
		if (address & 0x800)
		{
			/* ram */
			data = vecx->ram[address & 0x3ff];
		}
		else if (address & 0x1000)
		{
			/* io */
			data = via_read(&vecx->VIA, address);
		}
	}
	else if (address < 0x8000)
	{
		/* cartridge */
		data = vecx->cart[address];
	}

	return data;
}

static void write8(vecx *vecx, uint16_t address, uint8_t data)
{
	if ((address & 0xe000) == 0xe000)
	{
		/* rom */
	}
	else if ((address & 0xe000) == 0xc000)
	{
		/* it is possible for both ram and io to be written at the same! */

		if (address & 0x800)
		{
            vecx->ram[address & 0x3ff] = data;
		}

		if (address & 0x1000)
		{
			via_write(&vecx->VIA, address, data);
		}
	}
	else if (address < 0x8000)
	{
		/* cartridge */
	}
}

static void addline(vecx *vecx, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint8_t color)
{
    vecx->vectors[vecx->vector_draw_cnt].x0 = x0;
    vecx->vectors[vecx->vector_draw_cnt].y0 = y0;
    vecx->vectors[vecx->vector_draw_cnt].x1 = x1;
    vecx->vectors[vecx->vector_draw_cnt].y1 = y1;
    vecx->vectors[vecx->vector_draw_cnt].color = color;
    vecx->vector_draw_cnt++;
}

void vecx_input(vecx *vecx, uint8_t key, uint8_t value)
{
	uint8_t psg_io = e8910_read(&vecx->PSG, 14);
	switch (key)
	{
	case VECTREX_PAD1_BUTTON1: e8910_write(&vecx->PSG, 14, value ? psg_io & ~0x01 : psg_io | 0x01); break;
	case VECTREX_PAD1_BUTTON2: e8910_write(&vecx->PSG, 14, value ? psg_io & ~0x02 : psg_io | 0x02); break;
	case VECTREX_PAD1_BUTTON3: e8910_write(&vecx->PSG, 14, value ? psg_io & ~0x04 : psg_io | 0x04); break;
	case VECTREX_PAD1_BUTTON4: e8910_write(&vecx->PSG, 14, value ? psg_io & ~0x08 : psg_io | 0x08); break;

	case VECTREX_PAD2_BUTTON1: e8910_write(&vecx->PSG, 14, value ? psg_io & ~0x10 : psg_io | 0x10); break;
	case VECTREX_PAD2_BUTTON2: e8910_write(&vecx->PSG, 14, value ? psg_io & ~0x20 : psg_io | 0x20); break;
	case VECTREX_PAD2_BUTTON3: e8910_write(&vecx->PSG, 14, value ? psg_io & ~0x40 : psg_io | 0x40); break;
	case VECTREX_PAD2_BUTTON4: e8910_write(&vecx->PSG, 14, value ? psg_io & ~0x80 : psg_io | 0x80); break;

	case VECTREX_PAD1_X: vecx->DAC.jch0 = value; break;
	case VECTREX_PAD1_Y: vecx->DAC.jch1 = value; break;
	case VECTREX_PAD2_X: vecx->DAC.jch2 = value; break;
	case VECTREX_PAD2_Y: vecx->DAC.jch3 = value; break;
	}
}

void vecx_reset(vecx *vecx)
{
	/* ram */
	for (int r = 0; r < 1024; r++)
        vecx->ram[r] = (uint8_t)rand();

	e8910_reset(&vecx->PSG);

    vecx->snd_select = 0;

    vecx->DAC.add_line = addline;
    vecx->DAC.userdata = (void*)vecx;
    vecx->DAC.VIA = &vecx->VIA;

	dac_reset(&vecx->DAC);

    vecx->vector_draw_cnt = 0;
    vecx->fcycles = FCYCLES_INIT;

	vecx->VIA.read8_port_a = read8_port_a;
    vecx->VIA.read8_port_b = read8_port_b;
    vecx->VIA.write8_port_a = write8_port_a;
    vecx->VIA.write8_port_b = write8_port_b;
    vecx->VIA.userdata = (void*)vecx;

	via_reset(&vecx->VIA);

	vecx->CPU.read8 = read8;
    vecx->CPU.write8 = write8;
    vecx->CPU.userdata = (void*)vecx;

	e6809_reset(&vecx->CPU);
}

void vecx_emu(vecx *vecx, int32_t cycles)
{
	while (cycles > 0)
	{
		uint16_t icycles = e6809_sstep(&vecx->CPU, vecx->VIA.ifr & 0x80, 0);

		for (uint16_t c = 0; c < icycles; c++)
		{
			via_sstep0(&vecx->VIA);
			dac_sstep(&vecx->DAC);
			via_sstep1(&vecx->VIA);
		}

		cycles -= (int32_t)icycles;

        vecx->fcycles -= (int32_t)icycles;

		if (vecx->fcycles < 0)
		{

            vecx->fcycles += FCYCLES_INIT;
            vecx->render();

			/* everything that was drawn during this pass
			 * now is being removed.
			 */
            vecx->vector_draw_cnt = 0;
		}
	}
}
