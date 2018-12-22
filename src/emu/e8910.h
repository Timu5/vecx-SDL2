#ifndef __E8910_H
#define __E8910_H

typedef struct
{
	uint8_t regs[16];
	int32_t per_a, per_b, per_c, per_n, per_e; /* Period */
	int32_t cnt_a, cnt_b, cnt_c, cnt_n, cnt_e; /* count */
	uint32_t vol_a, vol_b, vol_c, vol_e;
	uint8_t env_a, env_b, env_c; /* Envelope */
	uint8_t out_a, out_b, out_c, out_n;
	int8_t cnt_env;
	uint8_t hold, alternate, attack, holding;
	int32_t RNG;

} AY8910;

void e8910_reset(AY8910 *PSG);
void e8910_init(AY8910 *PSG);
void e8910_done(AY8910 *PSG);
uint8_t e8910_read(AY8910 *PSG, uint8_t r);
void e8910_write(AY8910 *PSG, uint8_t r, uint8_t v);

#endif
