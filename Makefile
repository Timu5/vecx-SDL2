
CFLAGS := -std=gnu89 -O3 -Wall -Wextra -Wfatal-errors $(shell sdl2-config --cflags)
LIBS := $(shell sdl2-config --libs) -lSDL2_image
OBJECTS := e6809.o e8910.o e6522.o osint.o vecx.o
TARGET := vecx
CLEANFILES := $(TARGET) $(OBJECTS)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

clean:
	$(RM) $(CLEANFILES)

