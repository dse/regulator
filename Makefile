CC = clang

LIBRARIES = libpulse-simple sndfile

CFLAGS  += -g -Wall -Wextra $(shell pkg-config --cflags $(LIBRARIES))
LDFLAGS += $(shell pkg-config --libs $(LIBRARIES))

SOURCES = $(wildcard *.c)
OBJECTS = $(patsubst %.c, %.o, $(SOURCES))
EXECUTABLES = regulator

all: $(EXECUTABLES)

regulator: regulator.o

clean:
	rm $(OBJECTS) $(EXECUTABLES) || true
