#ifndef __VECX_H
#define __VECX_H

#include "e6522.h"
#include "e6809.h"
#include "e8910.h"
#include "edac.h"

enum
{
	VECTREX_MHZ = 1500000, /* speed of the vectrex being emulated */
	VECTREX_COLORS = 128,     /* number of possible colors ... grayscale */

    VECTREX_PDECAY = 30,      /* phosphor decay rate */

    /* number of 6809 cycles before a frame redraw */
    FCYCLES_INIT = VECTREX_MHZ / VECTREX_PDECAY,

    /* max number of possible vectors that maybe on the screen at one time.
    * one only needs VECTREX_MHZ / VECTREX_PDECAY but we need to also store
    * deleted vectors in a single table
    */
    VECTOR_MAX_CNT = VECTREX_MHZ / VECTREX_PDECAY,

	VECTREX_PAD1_BUTTON1 = 0,
	VECTREX_PAD1_BUTTON2 = 1,
	VECTREX_PAD1_BUTTON3 = 2,
	VECTREX_PAD1_BUTTON4 = 3,
	VECTREX_PAD1_X = 4,
	VECTREX_PAD1_Y = 5,

	VECTREX_PAD2_BUTTON1 = 6,
	VECTREX_PAD2_BUTTON2 = 7,
	VECTREX_PAD2_BUTTON3 = 8,
	VECTREX_PAD2_BUTTON4 = 9,
	VECTREX_PAD2_X = 10,
	VECTREX_PAD2_Y = 11,
};

typedef struct vector_type
{
	int32_t x0, y0; /* start coordinate */
	int32_t x1, y1; /* end coordinate */

				 /* color [0, VECTREX_COLORS - 1], if color = VECTREX_COLORS, then this is
				 * an invalid entry and must be ignored.
				 */
	uint8_t color;
} vector_t;


typedef struct
{
    M6809 CPU;
    VIA6522 VIA;
    AY8910 PSG;
    DACVec DAC;

    uint8_t rom[8192];
    uint8_t cart[51200];
    uint8_t ram[1024];

    int32_t fcycles;

    uint8_t snd_select;

    size_t vector_draw_cnt;
    vector_t vectors[VECTOR_MAX_CNT];

    void(*render) (void);

} vecx;

void vecx_input(vecx *vecx, uint8_t key, uint8_t value);
void vecx_reset(vecx *vecx);
void vecx_emu(vecx *vecx, int32_t cycles);

#endif
