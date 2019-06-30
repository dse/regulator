CC = clang

CFLAGS_LIBPULSE_SIMPLE  = $(shell pkg-config --cflags libpulse-simple)
LDFLAGS_LIBPULSE_SIMPLE = $(shell pkg-config --libs   libpulse-simple)

CFLAGS  += -g -Wall -Wextra $(CFLAGS_LIBPULSE_SIMPLE)
LDFLAGS += $(LDFLAGS_LIBPULSE_SIMPLE)

SOURCES = $(wildcard *.c)
OBJECTS = $(patsubst %.c, %.o, $(SOURCES))
EXECUTABLES = regulator tester

all: $(EXECUTABLES)

regulator: regulator.o
tester: tester.o

clean:
	rm $(OBJECTS) $(EXECUTABLES)
