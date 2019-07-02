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

test: $(EXECUTABLES)
	@./regulator --ticks-per-hour=12000 --file=sample-data/westclox-facedown.wav
	@./regulator --ticks-per-hour=12000 --file=sample-data/westclox-upright.wav
	@./regulator --ticks-per-hour=18000 --file=sample-data/acs1-facedown-1.wav
	@./regulator --ticks-per-hour=18000 --file=sample-data/acs1-facedown-2.wav
	@./regulator --ticks-per-hour=18000 --file=sample-data/acs1-upright-1.wav
	@./regulator --ticks-per-hour=18000 --file=sample-data/acs1-upright-2.wav
