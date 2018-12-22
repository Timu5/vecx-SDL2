#ifndef __EDAC_H
#define __EDAC_H

#include "e6522.h"

enum {
	DAC_MAX_X = 33000,
	DAC_MAX_Y = 41000
};


typedef struct
{
	/* analog devices */

	uint16_t rsh;  /* zero ref sample and hold */
	uint16_t xsh;  /* x sample and hold */
	uint16_t ysh;  /* y sample and hold */
	uint16_t zsh;  /* z sample and hold */
	uint16_t jch0;		  /* joystick direction channel 0 */
	uint16_t jch1;		  /* joystick direction channel 1 */
	uint16_t jch2;		  /* joystick direction channel 2 */
	uint16_t jch3;		  /* joystick direction channel 3 */
	uint16_t jsh;  /* joystick sample and hold */

	uint8_t compare;

	int32_t dx;     /* delta x */
	int32_t dy;     /* delta y */
	int32_t curr_x; /* current x position */
	int32_t curr_y; /* current y position */

	uint8_t vectoring; /* are we drawing a vector right now? */
	int32_t vector_x0;
	int32_t vector_y0;
	int32_t vector_x1;
	int32_t vector_y1;
	int32_t vector_dx;
	int32_t vector_dy;
	uint8_t vector_color;

    VIA6522 *VIA;

    void *userdata;
    void(*add_line) (void* userdata, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint8_t color);
} DACVec;

void dac_update(DACVec *DAC);
void dac_sstep(DACVec *DAC);
void dac_reset(DACVec *DAC);

#endif