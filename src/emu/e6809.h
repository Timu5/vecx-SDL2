#ifndef __E6809_H
#define __E6809_H

typedef struct
{
	uint16_t reg_x, reg_y; /* index registers */
	uint16_t reg_u; /* user stack pointer */
	uint16_t reg_s; /* hardware stack pointer */
	uint16_t reg_pc; /* program counter */
	uint8_t reg_a, reg_b; /* accumulators */
	uint8_t reg_dp; /* direct page register */
	uint8_t reg_cc; /* condition codes */
	uint16_t irq_status; /* flag to see if interrupts should be handled (sync/cwai). */
	uint16_t *rptr_xyus[4];

    void *userdata;
    /* user defined read and write functions */
    uint8_t(*read8) (void *userdata, uint16_t address);
    void(*write8) (void *userdata, uint16_t address, uint8_t data);
} M6809;


void e6809_reset(M6809 *CPU);
uint16_t e6809_sstep(M6809 *CPU, uint16_t irq_i, uint16_t irq_f);

#endif