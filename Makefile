
CFLAGS := -std=c99 -O3 -Wall -Wextra -Wfatal-errors  $(shell sdl2-config --cflags)
LIBS := $(shell sdl2-config --libs) -lSDL2_image
OBJECTS := src/emu/e6809.o src/emu/e8910.o src/emu/e6522.o src/emu/edac.o src/emu/vecx.o src/ser.o src/main.o 
TARGET := vecx
CLEANFILES := $(TARGET) $(OBJECTS)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

clean:
	$(RM) $(CLEANFILES)

