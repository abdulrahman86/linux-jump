CC=gcc
CFLAGS+=-I ../include/
LDFLAGS=-L../build
LDLIBS=-ljia

.PHONY: clean

all: buck lu merge mm rx

buck: buck.c

lu: lu.c

merge: merge.c

mm: mm.c

rx: rx.c

clean:
	rm -fr buck lu merge mm rx
