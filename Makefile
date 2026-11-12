# Compilation du serveur web
CC      = gcc
CFLAGS  = -Wall -Wextra -std=gnu17 -g
SOURCES = $(wildcard src/*.c)

serveur: $(SOURCES)
	$(CC) $(CFLAGS) -o $@ $(SOURCES)

clean:
	rm -f serveur

.PHONY: clean
