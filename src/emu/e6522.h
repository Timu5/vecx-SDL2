#ifndef __E6522_H
#define __E6522_H

typedef struct
{
	/* the via 6522 registers */
	uint8_t ora, orb;
	uint8_t ddra, ddrb;
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

    void *userdata;
    uint8_t(*read8_port_a) (void *userdata);
    uint8_t(*read8_port_b) (void *userdata);
    void(*write8_port_a) (void *userdata, uint8_t data);
    void(*write8_port_b) (void *userdata, uint8_t data);
} VIA6522;

uint8_t via_read(VIA6522 *VIA, uint16_t address);
void via_write(VIA6522 *VIA, uint16_t address, uint8_t data);
void via_sstep0(VIA6522 *VIA);
void via_sstep1(VIA6522 *VIA);
void via_reset(VIA6522 *VIA);

#endif
