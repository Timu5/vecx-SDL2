#ifndef __E6809_H
#define __E6809_H

/* user defined read and write functions */

extern uint8_t (*e6809_read8) (uint16_t address);
extern void (*e6809_write8) (uint16_t address, uint8_t data);

void e6809_reset (void);
unsigned e6809_sstep (unsigned irq_i, unsigned irq_f);

#endif