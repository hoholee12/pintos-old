.PHONY: default
.DEFAULT_GOAL: default

CFLAGS=-g -Wno-format
SOURCES=$(wildcard *.c)
OBJECTS=$(patsubst %.c, %.o, $(SOURCES))

default: testlib

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

testlib: $(OBJECTS)
	$(CC) -o $@ $(OBJECTS)

clean:
	rm *.o testlib