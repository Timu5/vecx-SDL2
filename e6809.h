#ifndef __E6809_H
#define __E6809_H

typedef struct {
	/* index registers */
	uint16_t reg_x;
	uint16_t reg_y;

	/* user stack pointer */
	uint16_t reg_u;

	/* hardware stack pointer */
	uint16_t reg_s;

	/* program counter */
	uint16_t reg_pc;

	/* accumulators */
	uint16_t reg_a;
	uint16_t reg_b;

	/* direct page register */
	uint16_t reg_dp;

	/* condition codes */
	uint16_t reg_cc;

	/* flag to see if interrupts should be handled (sync/cwai). */
	uint16_t irq_status;

	uint16_t *rptr_xyus[4];

} M6809;

extern M6809 CPU;

/* user defined read and write functions */

extern uint8_t (*e6809_read8) (uint16_t address);
extern void (*e6809_write8) (uint16_t address, uint8_t data);

void e6809_reset (void);
uint16_t e6809_sstep (uint16_t irq_i, uint16_t irq_f);

#endif