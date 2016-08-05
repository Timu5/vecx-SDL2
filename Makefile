
CFLAGS := -std=gnu89 -O3 -Wall -Wextra -Wfatal-errors  $(shell sdl2-config --cflags)
LIBS := $(shell sdl2-config --libs) -lSDL2_image
OBJECTS := emu/e6809.o emu/e8910.o emu/e6522.o emu/edac.o emu/vecx.o osint.o 
TARGET := vecx
CLEANFILES := $(TARGET) $(OBJECTS)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

clean:
	$(RM) $(CLEANFILES)

