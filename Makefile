# Compilation du serveur web
CC      = gcc
CFLAGS  = -Wall -Wextra -std=gnu17 -g -O2
SOURCES = $(wildcard src/*.c)
ENTETES = $(wildcard src/*.h)

serveur: $(SOURCES) $(ENTETES)
	$(CC) $(CFLAGS) -pthread -o $@ $(SOURCES)

clean:
	rm -f serveur

.PHONY: clean
