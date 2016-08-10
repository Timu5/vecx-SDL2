#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <SDL.h>
#include <SDL_image.h>

#include "emu/e6809.h"
#include "emu/e8910.h"
#include "emu/e6522.h"
#include "emu/edac.h"
#include "emu/vecx.h"

enum {
	EMU_TIMER		= 20, /* the emulators heart beats at 20 milliseconds */

	DEFAULT_WIDTH	= 495,
	DEFAULT_HEIGHT	= 615
};

static SDL_Window *window = NULL; 
static SDL_Renderer *renderer = NULL;
static SDL_Texture *overlay = NULL;
static SDL_Texture *buffer = NULL;
static SDL_Texture *buffer2 = NULL;

static int32_t scl_factor;

static void quit(void);

/* command line arguments */
static char *bios_filename = "bios.bin";
static char *cart_filename = NULL;
static char *overlay_filename = NULL;
static char fullscreen = 0;

static void render (void) {
	size_t v;

	SDL_SetRenderTarget(renderer, buffer);
	{
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 128);
		SDL_RenderFillRect(renderer, NULL);

		for (v = 0; v < vector_draw_cnt; v++) {
			Uint8 c = vectors[v].color * 256 / VECTREX_COLORS;
			int x0 = vectors[v].x0 / scl_factor;
			int y0 = vectors[v].y0 / scl_factor;
			int x1 = vectors[v].x1 / scl_factor;
			int y1 = vectors[v].y1 / scl_factor;

			SDL_SetRenderDrawColor(renderer, 255, 255, 255, c);
			if (x0 == x1 && y0 == y1) {
				/* point */
				SDL_RenderDrawPoint(renderer, x0, y0);
				SDL_RenderDrawPoint(renderer, x0 + 1, y0);
				SDL_RenderDrawPoint(renderer, x0, y0 + 1);
				SDL_RenderDrawPoint(renderer, x0 + 1, y0 + 1);
			}
			else {
				SDL_RenderDrawLine(renderer, x0, y0, x1, y1);
				SDL_RenderDrawLine(renderer, x0 + 1, y0 + 1, x1 + 1, y1 + 1);
			}
		}
	}

	SDL_SetRenderTarget(renderer, buffer2);
	{
		SDL_RenderCopy(renderer, buffer, NULL, NULL);
	}

	SDL_SetRenderTarget(renderer, NULL);
	{
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
		SDL_RenderClear(renderer);

		SDL_RenderCopy(renderer, buffer, NULL, NULL);
		SDL_RenderCopy(renderer, buffer2, NULL, NULL);

		if (overlay) {
			SDL_RenderCopy(renderer, overlay, NULL, NULL);
		}
	}
	SDL_RenderPresent (renderer);
}

static void load_bios(void) {
	FILE *f;
	if (!(f = fopen (bios_filename, "rb"))) {
		perror (bios_filename);
		quit();
	}
	if (fread(rom, 1, sizeof (rom), f) != sizeof (rom)){
		fprintf (stderr, "Invalid bios length\n");
		quit();
	}
	fclose (f);
}

static void load_cart (void) {
	FILE *f;
	memset (cart, 0, sizeof (cart));
	if (cart_filename) {
		if (!(f = fopen (cart_filename, "rb"))) {
			perror (cart_filename);
		}
		fread (cart, 1, sizeof (cart), f);
		fclose (f);
	}
}

static void load_state (char *name) {
}

static void save_state (char *name) {
}

static void resize (void) {
	int sclx, scly;
	int screenx, screeny;
	int width, height;
	
	SDL_GetWindowSize (window, &screenx, &screeny);

	sclx = DAC_MAX_X / screenx;
	scly = DAC_MAX_Y / screeny;

	scl_factor = sclx > scly ? sclx : scly;
	width = DAC_MAX_X / scl_factor;
	height = DAC_MAX_Y / scl_factor;
	
	SDL_RenderSetLogicalSize (renderer, width, height);
	
	SDL_DestroyTexture(buffer);
	SDL_DestroyTexture(buffer2);
	
	buffer = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, width, height);
	buffer2 = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, width / 2, height / 2);
	
	SDL_SetTextureBlendMode(buffer2, SDL_BLENDMODE_BLEND);
	SDL_SetTextureAlphaMod(buffer2, 128);
}

static int readevents (void) {
	SDL_Event e;
	while (SDL_PollEvent (&e)) {
		switch (e.type) {
			case SDL_QUIT:
				return 1;
				break;
			case SDL_WINDOWEVENT:
				if (e.window.event == SDL_WINDOWEVENT_RESIZED)
					resize ();
				break;
			case SDL_DROPFILE:
				cart_filename = e.drop.file;
				load_cart ();
				vecx_reset ();
				break;
			case SDL_KEYDOWN:
				switch (e.key.keysym.sym) {
					case SDLK_ESCAPE: return 1;
					case SDLK_a: vecx_input(VECTREX_PAD1_BUTTON1, 1); break;
					case SDLK_s: vecx_input(VECTREX_PAD1_BUTTON2, 1); break;
					case SDLK_d: vecx_input(VECTREX_PAD1_BUTTON3, 1); break;
					case SDLK_f: vecx_input(VECTREX_PAD1_BUTTON4, 1); break;
					case SDLK_LEFT: vecx_input(VECTREX_PAD1_X, 0x00); break;
					case SDLK_RIGHT: vecx_input(VECTREX_PAD1_X, 0xff); break;
					case SDLK_UP: vecx_input(VECTREX_PAD1_Y, 0xff); break;
					case SDLK_DOWN: vecx_input(VECTREX_PAD1_Y, 0x00); break;
				}
				break;
			case SDL_KEYUP:
				switch (e.key.keysym.sym) {
					case SDLK_F1: load_state ("q.save"); break;
					case SDLK_F5: save_state ("q.save"); break;
					case SDLK_r: load_cart(); vecx_reset (); break;

					case SDLK_a: vecx_input(VECTREX_PAD1_BUTTON1, 0); break;
					case SDLK_s: vecx_input(VECTREX_PAD1_BUTTON2, 0); break;
					case SDLK_d: vecx_input(VECTREX_PAD1_BUTTON3, 0); break;
					case SDLK_f: vecx_input(VECTREX_PAD1_BUTTON4, 0); break;
					case SDLK_LEFT: vecx_input(VECTREX_PAD1_X, 0x80); break;
					case SDLK_RIGHT: vecx_input(VECTREX_PAD1_X, 0x80); break;
					case SDLK_UP: vecx_input(VECTREX_PAD1_Y, 0x80); break;
					case SDLK_DOWN: vecx_input(VECTREX_PAD1_Y, 0x80); break;
				}
				break;
		}
	}
	return 0;
}

static void emuloop (void) {
	Uint32 next_time = SDL_GetTicks () + EMU_TIMER;
	vecx_reset ();
	for (;;) {
		vecx_emu ((VECTREX_MHZ / 1000) * EMU_TIMER);
		if (readevents ()) break;

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

static void load_overlay () {
	SDL_Texture *image;
	if (overlay_filename)
	{
		image = IMG_LoadTexture(renderer, overlay_filename);
		if (image) {
			overlay = image;
			SDL_SetTextureBlendMode(image, SDL_BLENDMODE_BLEND);
			SDL_SetTextureAlphaMod(image, 128);
		}
		else {
			fprintf(stderr, "IMG_Load: %s\n", IMG_GetError());
		}
	}
}

static int init(void)
{
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
		fprintf(stderr, "Failed to initialize SDL: %s\n", SDL_GetError());
		return 0;
	}

	window = SDL_CreateWindow("Vecx", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, DEFAULT_WIDTH, DEFAULT_HEIGHT, SDL_WINDOW_RESIZABLE);
	if (!window) {
		fprintf(stderr, "Failed to create window: %s\n", SDL_GetError());
		return 0;
	}

	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE);
	if (!renderer) {
		fprintf(stderr, "Failed to create renderer: %s\n", SDL_GetError());
		return 0;
	}

	SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

	if (fullscreen)
		SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);

	return 1;
}

static void quit (void)
{
	if (renderer)
		SDL_DestroyRenderer(renderer);
	if (window)
		SDL_DestroyWindow(window);
	SDL_Quit();
	
	exit(0);
}

void parse_args (int argc, char* argv[])
{
	int i;
	for (i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0)
		{
			puts("Usage: vecx [options] [cart_file]");
			puts("Options:");
			puts("  --help            Display this help message");
			puts("  --bios <file>     Load bios file");
			puts("  --overlay <file>  Load overlay file");
			puts("  --fullscreen      Launch in fullscreen mode");
			exit(0);
		}
		else if (strcmp(argv[i], "--bios") == 0 || strcmp(argv[i], "-b") == 0)
		{
			bios_filename = argv[++i];
		}
		else if (strcmp(argv[i], "--overlay") == 0 || strcmp(argv[i], "-o") == 0)
		{
			overlay_filename = argv[++i];
		}
		else if (strcmp(argv[i], "--fullscreen") == 0 || strcmp(argv[i], "-f") == 0)
		{
			fullscreen = 1;
		}
		else if(i == argc - 1)
		{
			cart_filename = argv[i];
		}
		else
		{
			printf("Unkown flag: %s\n", argv[i]);
			exit(0);
		}
	}
}

int main (int argc, char *argv[]) {
	parse_args(argc, argv);
	
	if (!init())
		quit();

	resize ();
	load_bios ();
	load_cart ();
	load_overlay();
	e8910_init ();
	vecx_render = render;

	emuloop ();

	e8910_done ();
	quit();

	return 0;
}
