# Compilation du serveur web
CC      = gcc
CFLAGS  = -Wall -Wextra -std=gnu17 -g -O2
SOURCES = $(wildcard src/*.c)
ENTETES = $(wildcard src/*.h)

serveur: $(SOURCES) $(ENTETES)
	$(CC) $(CFLAGS) -pthread -o $@ $(SOURCES)

# Lance les tests automatiques
test: serveur
	./tests/tests.sh

# Relance les tests avec les détecteurs d'erreurs mémoire (AddressSanitizer et UBSan)
verifier: $(SOURCES) $(ENTETES)
	$(CC) $(CFLAGS) -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -pthread -o serveur-verif $(SOURCES)
	SERVEUR=./serveur-verif ./tests/tests.sh

clean:
	rm -f serveur serveur-verif

.PHONY: test verifier clean
