#ifndef __E8910_H
#define __E8910_H

typedef struct {
	uint8_t ready;
	uint8_t Regs[16];
	int32_t lastEnable;
	int32_t PeriodA, PeriodB, PeriodC, PeriodN, PeriodE;
	int32_t CountA, CountB, CountC, CountN, CountE;
	uint32_t VolA, VolB, VolC, VolE;
	uint8_t EnvelopeA, EnvelopeB, EnvelopeC;
	uint8_t OutputA, OutputB, OutputC, OutputN;
	int8_t CountEnv;
	uint8_t Hold, Alternate, Attack, Holding;
	int32_t RNG;
	uint32_t VolTable[32];

} AY8910;

extern AY8910 PSG;

void e8910_init_sound (void);
void e8910_done_sound (void);
uint8_t e8910_read (uint8_t r);
void e8910_write (uint8_t r, uint8_t v);

#endif
