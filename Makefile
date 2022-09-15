.PHONY: all clean
all: hp_ins

heud: hp_ins.c
	gcc -Wall $(shell pkg-config libevdev --cflags) -o hp_ins hp_ins.c $(shell pkg-config libevdev --libs)

clean:
	@rm -f hp_ins
