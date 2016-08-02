#ifndef __E6522_H
#define __E6522_H

typedef struct {
	/* the via 6522 registers */
	uint8_t ora;
	uint8_t orb;
	uint8_t ddra;
	uint8_t ddrb;
	uint8_t t1on;  /* is timer 1 on? */
	uint8_t t1int; /* are timer 1 interrupts allowed? */
	uint16_t t1c;
	uint8_t t1ll;
	uint8_t t1lh;
	uint8_t t1pb7; /* timer 1 controlled version of pb7 */
	uint8_t t2on;  /* is timer 2 on? */
	uint8_t t2int; /* are timer 2 interrupts allowed? */
	uint16_t t2c;
	uint8_t t2ll;
	uint8_t sr;
	uint8_t srb;   /* number of bits shifted so far */
	uint8_t src;   /* shift counter */
	uint8_t srclk;
	uint8_t acr;
	uint8_t pcr;
	uint8_t ifr;
	uint8_t ier;
	uint8_t ca2;
	uint8_t cb2h;  /* basic handshake version of cb2 */
	uint8_t cb2s;  /* version of cb2 controlled by the shift register */
} VIA6522;

extern VIA6522 VIA;

uint8_t via_read(uint16_t address);
void via_write(uint16_t address, uint8_t data);
void via_sstep0 (void);
void via_sstep1 (void);
void via_reset (void);

#endif
