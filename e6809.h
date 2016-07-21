#ifndef __E6809_H
#define __E6809_H

/* user defined read and write functions */

extern uint16_t (*e6809_read8) (uint16_t address);
extern void (*e6809_write8) (uint16_t address, uint16_t data);

void e6809_reset (void);
uint16_t e6809_sstep (uint16_t irq_i, uint16_t irq_f);

#endif