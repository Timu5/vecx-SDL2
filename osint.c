#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <SDL.h>
#include <SDL_image.h>

#include "osint.h"
#include "e6809.h"
#include "e8910.h"
#include "vecx.h"

enum {
	EMU_TIMER = 20 /* the emulators heart beats at 20 milliseconds */
};

static SDL_Window *window = NULL; 
static SDL_Renderer *renderer = NULL;
static SDL_Texture *overlay = NULL;

static long scl_factor;

void osint_render (void) {
	int v;
	SDL_SetRenderDrawColor (renderer, 0, 0, 0, 0);
	SDL_RenderClear (renderer);

	if (overlay) {
		SDL_RenderCopy (renderer, overlay, NULL, NULL);
	}

	for (v = 0; v < vector_draw_cnt; v++) {
		Uint8 c = vectors_draw[v].color * 256 / VECTREX_COLORS;
		int x0 = vectors_draw[v].x0 / scl_factor;
		int y0 = vectors_draw[v].y0 / scl_factor;
		int x1 = vectors_draw[v].x1 / scl_factor;
		int y1 = vectors_draw[v].y1 / scl_factor;

		SDL_SetRenderDrawColor(renderer, c, c, c, 0);
		if (x0 == x1 && y0 == y1) {
			/* point */
			SDL_RenderDrawPoint(renderer, x0, y0);
		} else {
			SDL_RenderDrawLine(renderer, x0, y0, x1, y1);
		}
	}

	SDL_RenderPresent (renderer);
}

static char *biosfilename = "bios.bin";
static char *cartfilename = NULL;

static void osint_load_bios(void) {
	FILE *f;
	if (!(f = fopen (biosfilename, "rb"))) {
		perror (biosfilename);
		exit (EXIT_FAILURE);
	}
	if (fread(rom, 1, sizeof (rom), f) != sizeof (rom)){
		fprintf (stderr, "Invalid bios length\n");
		exit (EXIT_FAILURE);
	}
	fclose (f);
}

static void osint_load_cart (void) {
	FILE *f;
	memset (cart, 0, sizeof (cart));
	if (cartfilename) {
		if (!(f = fopen (cartfilename, "rb"))) {
			perror (cartfilename);
			exit (EXIT_FAILURE);
		}
		fread (cart, 1, sizeof (cart), f);
		fclose (f);
	}
}

static void osint_load_state (char *name) {
	FILE *f;
	if (!(f = fopen(name, "rb"))) {
		perror(name);
		return;
	}

	void *cpu[] = { &CPU.reg_x, &CPU.reg_y, &CPU.reg_u, &CPU.reg_s, &CPU.reg_pc, &CPU.reg_a, &CPU.reg_b, &CPU.reg_dp, &CPU.reg_cc, &CPU.irq_status };

	for (int i = 0; i < sizeof(cpu) / sizeof(cpu[0]); i++)
		fread(cpu[i], sizeof(uint16_t), 1, f);

	void *via[] = { &VIA.ora, &VIA.orb, &VIA.ddra, &VIA.ddrb, &VIA.t1on, &VIA.t1int, &VIA.t1ll, &VIA.t1lh, &VIA.t1pb7, &VIA.t2on, &VIA.t2int, &VIA.t2ll, &VIA.sr, &VIA.srb, &VIA.src, &VIA.srclk, &VIA.acr, &VIA.pcr, &VIA.ifr, &VIA.ier, &VIA.ca2, &VIA.cb2h, &VIA.cb2s };

	for (int i = 0; i < sizeof(via) / sizeof(via[0]); i++)
		fread(via[i], sizeof(uint8_t), 1, f);
	fread(&VIA.t1c, sizeof(uint16_t), 1, f);
	fread(&VIA.t2c, sizeof(uint16_t), 1, f);

	void *psg0[] = { &PSG.ready, &PSG.EnvelopeA, &PSG.EnvelopeB, &PSG.EnvelopeC, &PSG.OutputA, &PSG.OutputB, &PSG.OutputC, &PSG.OutputN, &PSG.CountEnv, &PSG.Hold, &PSG.Alternate, &PSG.Attack, &PSG.Holding };
	void *psg1[] = { &PSG.lastEnable, &PSG.PeriodA, &PSG.PeriodB, &PSG.PeriodC, &PSG.PeriodN, &PSG.PeriodE, &PSG.CountA, &PSG.CountB, &PSG.CountC, &PSG.CountN, &PSG.CountE, &PSG.VolA, &PSG.VolB, &PSG.VolC, &PSG.VolE, &PSG.RNG };
	
	for (int i = 0; i < sizeof(psg0) / sizeof(psg0[0]); i++)
		fread(psg0[i], sizeof(uint8_t), 1, f);
	for (int i = 0; i < sizeof(psg1) / sizeof(psg1[0]); i++)
		fread(psg1[i], sizeof(uint32_t), 1, f);
	fread(PSG.Regs, sizeof(uint8_t), 16, f);
	fread(PSG.VolTable, sizeof(uint32_t), 32, f);

	fread(ram, sizeof(uint8_t), 1000, f);

	fclose(f);
}

static void osint_save_state(char *name) {
	FILE *f;
	if (!(f = fopen(name, "wb"))) {
		perror(name);
		return;
	}

	void *cpu[] = { &CPU.reg_x, &CPU.reg_y, &CPU.reg_u, &CPU.reg_s, &CPU.reg_pc, &CPU.reg_a, &CPU.reg_b, &CPU.reg_dp, &CPU.reg_cc, &CPU.irq_status };

	for (int i = 0; i < sizeof(cpu) / sizeof(cpu[0]); i++)
		fwrite(cpu[i], sizeof(uint16_t), 1, f);

	void *via[] = { &VIA.ora, &VIA.orb, &VIA.ddra, &VIA.ddrb, &VIA.t1on, &VIA.t1int, &VIA.t1ll, &VIA.t1lh, &VIA.t1pb7, &VIA.t2on, &VIA.t2int, &VIA.t2ll, &VIA.sr, &VIA.srb, &VIA.src, &VIA.srclk, &VIA.acr, &VIA.pcr, &VIA.ifr, &VIA.ier, &VIA.ca2, &VIA.cb2h, &VIA.cb2s };

	for (int i = 0; i < sizeof(via) / sizeof(via[0]); i++)
		fwrite(via[i], sizeof(uint8_t), 1, f);
	fwrite(&VIA.t1c, sizeof(uint16_t), 1, f);
	fwrite(&VIA.t2c, sizeof(uint16_t), 1, f);

	void *psg0[] = { &PSG.ready, &PSG.EnvelopeA, &PSG.EnvelopeB, &PSG.EnvelopeC, &PSG.OutputA, &PSG.OutputB, &PSG.OutputC, &PSG.OutputN, &PSG.CountEnv, &PSG.Hold, &PSG.Alternate, &PSG.Attack, &PSG.Holding };
	void *psg1[] = { &PSG.lastEnable, &PSG.PeriodA, &PSG.PeriodB, &PSG.PeriodC, &PSG.PeriodN, &PSG.PeriodE, &PSG.CountA, &PSG.CountB, &PSG.CountC, &PSG.CountN, &PSG.CountE, &PSG.VolA, &PSG.VolB, &PSG.VolC, &PSG.VolE, &PSG.RNG };

	for (int i = 0; i < sizeof(psg0) / sizeof(psg0[0]); i++)
		fwrite(psg0[i], sizeof(uint8_t), 1, f);
	for (int i = 0; i < sizeof(psg1) / sizeof(psg1[0]); i++)
		fwrite(psg1[i], sizeof(uint32_t), 1, f);
	fwrite(PSG.Regs, sizeof(uint8_t), 16, f);
	fwrite(PSG.VolTable, sizeof(uint32_t), 32, f);

	fwrite(ram, sizeof(uint8_t), 1000, f);

	fclose(f);
}

static void osint_resize (void) {
	int sclx, scly;
	int screenx, screeny;
	
	SDL_GetWindowSize (window, &screenx, &screeny);

	sclx = ALG_MAX_X / screenx;
	scly = ALG_MAX_Y / screeny;

	scl_factor = sclx > scly ? sclx : scly;
	
	SDL_RenderSetLogicalSize (renderer, ALG_MAX_X / scl_factor, ALG_MAX_Y / scl_factor);
}

static int osint_readevents (void) {
	SDL_Event e;
	while (SDL_PollEvent (&e)) {
		switch (e.type) {
			case SDL_QUIT:
				return 1;
				break;
			case SDL_WINDOWEVENT:
				if (e.window.event == SDL_WINDOWEVENT_RESIZED)
					osint_resize ();
				break;
			case SDL_DROPFILE:
				cartfilename = e.drop.file;
				osint_load_cart ();
				vecx_reset ();
				break;
			case SDL_KEYDOWN:
				switch (e.key.keysym.sym) {
					case SDLK_ESCAPE:
						return 1;
					case SDLK_a:
						snd_regs[14] &= ~0x01;
						break;
					case SDLK_s:
						snd_regs[14] &= ~0x02;
						break;
					case SDLK_d:
						snd_regs[14] &= ~0x04;
						break;
					case SDLK_f:
						snd_regs[14] &= ~0x08;
						break;
					case SDLK_LEFT:
						alg_jch0 = 0x00;
						break;
					case SDLK_RIGHT:
						alg_jch0 = 0xff;
						break;
					case SDLK_UP:
						alg_jch1 = 0xff;
						break;
					case SDLK_DOWN:
						alg_jch1 = 0x00;
						break;
					default:
						break;
				}
				break;
			case SDL_KEYUP:
				switch (e.key.keysym.sym) {
					case SDLK_F1:
						osint_load_state("q1.save");
						break;
					case SDLK_F2:
						osint_load_state("q2.save");
						break;
					case SDLK_F3:
						osint_load_state("q3.save");
						break;
					case SDLK_F4:
						osint_load_state("q4.save");
						break;
					case SDLK_F5:
						osint_save_state("q1.save");
						break;
					case SDLK_F6:
						osint_save_state("q2.save");
						break;
					case SDLK_F7:
						osint_save_state("q3.save");
						break;
					case SDLK_F8:
						osint_save_state("q4.save");
						break;
					case SDLK_a:
						snd_regs[14] |= 0x01;
						break;
					case SDLK_s:
						snd_regs[14] |= 0x02;
						break;
					case SDLK_d:
						snd_regs[14] |= 0x04;
						break;
					case SDLK_f:
						snd_regs[14] |= 0x08;
						break;
					case SDLK_LEFT:
						alg_jch0 = 0x80;
						break;
					case SDLK_RIGHT:
						alg_jch0 = 0x80;
						break;
					case SDLK_UP:
						alg_jch1 = 0x80;
						break;
					case SDLK_DOWN:
						alg_jch1 = 0x80;
						break;
					default:
						break;
				}
				break;
			default:
				break;
		}
	}
	return 0;
}

void osint_emuloop (void) {
	Uint32 next_time = SDL_GetTicks () + EMU_TIMER;
	vecx_reset ();
	for (;;) {
		vecx_emu ((VECTREX_MHZ / 1000) * EMU_TIMER);
		if (osint_readevents ()) break;

		{
			Uint32 now = SDL_GetTicks ();
			if (now < next_time)
				SDL_Delay (next_time - now);
			else
				next_time = now;
			next_time += EMU_TIMER;
		}
	}
}

void osint_load_overlay (const char *filename) {
	SDL_Texture *image;
	image = IMG_LoadTexture (renderer, filename);
	if (image) {
		overlay = image;
	} else {
		fprintf (stderr, "IMG_Load: %s\n", IMG_GetError ());
	}
}

int main (int argc, char *argv[]) {			
	if (SDL_Init (SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
		fprintf (stderr, "Failed to initialize SDL: %s\n", SDL_GetError ());
		exit (EXIT_FAILURE);
	}

	window = SDL_CreateWindow ("Vecx", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 330 * 3 / 2, 410 * 3 / 2, SDL_WINDOW_RESIZABLE);
	if (!window) {
		fprintf (stderr, "Failed to create window: %s\n", SDL_GetError ());
		SDL_Quit ();
		exit (EXIT_FAILURE);
	}

	renderer = SDL_CreateRenderer (window, -1, SDL_RENDERER_ACCELERATED);
	if (!renderer) {
		fprintf (stderr, "Failed to create renderer: %s\n", SDL_GetError ());
		SDL_DestroyWindow (window);
		SDL_Quit ();
		exit (EXIT_FAILURE);
	}

	if (argc > 1)
		cartfilename = argv[1];
	if (argc > 2)
		osint_load_overlay (argv[2]);

	osint_resize();
	osint_load_bios();
	osint_load_cart();
	e8910_init_sound ();

	osint_emuloop ();

	e8910_done_sound ();
	SDL_DestroyRenderer (renderer);
	SDL_DestroyWindow (window);
	SDL_Quit ();

	return 0;
}
