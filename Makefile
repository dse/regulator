LIBRARIES = libpulse-simple sndfile

# _ISOC99_SOURCE for roundf
# _GNU_SOURCE for strdup
CFLAGS += -g -Wall -Wextra -std=c99 -D_GNU_SOURCE -D_ISOC99_SOURCE \
	$(shell pkg-config --cflags $(LIBRARIES))
LDLIBS += $(shell pkg-config --libs $(LIBRARIES)) -lm

SOURCES = $(wildcard *.c)
OBJECTS = $(patsubst %.c, %.o, $(SOURCES))
EXECUTABLES = regulator

all: $(EXECUTABLES)

regulator: regulator.o regulator_main.o regulator_sndfile.o regulator_pulseaudio.o

clean:
	rm $(OBJECTS) $(EXECUTABLES) >/dev/null 2>/dev/null || true

test: $(EXECUTABLES)
	@./regulator --ticks-per-hour=12000 --file=sample-data/westclox-facedown.wav
	@./regulator --ticks-per-hour=12000 --file=sample-data/westclox-upright.wav
	@./regulator --ticks-per-hour=18000 --file=sample-data/acs1-facedown-1.wav
	@./regulator --ticks-per-hour=18000 --file=sample-data/acs1-facedown-2.wav
	@./regulator --ticks-per-hour=18000 --file=sample-data/acs1-upright-1.wav
	@./regulator --ticks-per-hour=18000 --file=sample-data/acs1-upright-2.wav
test-debug: $(EXECUTABLES)
	@./regulator -D -D -D --ticks-per-hour=12000 --file=sample-data/westclox-facedown.wav
	@./regulator -D -D -D --ticks-per-hour=12000 --file=sample-data/westclox-upright.wav
	@./regulator -D -D -D --ticks-per-hour=18000 --file=sample-data/acs1-facedown-1.wav
	@./regulator -D -D -D --ticks-per-hour=18000 --file=sample-data/acs1-facedown-2.wav
	@./regulator -D -D -D --ticks-per-hour=18000 --file=sample-data/acs1-upright-1.wav
	@./regulator -D -D -D --ticks-per-hour=18000 --file=sample-data/acs1-upright-2.wav
test-A-B: $(EXECUTABLES)
	@make clean && make DEFS=-DAB_TESTING_GOOD test-debug >& good.out || true
	@make clean && make DEFS=-DAB_TESTING_BAD  test-debug >& bad.out  || true
	diff -y good.out bad.out
