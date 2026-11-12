/*
 * serveur-web-c : un serveur HTTP écrit en C avec les sockets POSIX.
 * Point d'entrée : lit les options, prépare le dossier servi et lance le serveur.
 */

#include "serveur.h"

#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#define PORT_PAR_DEFAUT    8080
#define DOSSIER_PAR_DEFAUT "site"

/**
 * Rôle : affiche le mode d'emploi du programme.
 * Paramètres :
 *   - programme : nom du programme, tel que lancé
 */
static void afficher_aide(const char *programme)
{
    printf("Utilisation : %s [-p port] [-d dossier]\n"
           "  -p  port d'écoute (défaut : %d)\n"
           "  -d  dossier à servir (défaut : %s)\n",
           programme, PORT_PAR_DEFAUT, DOSSIER_PAR_DEFAUT);
}

/**
 * Rôle : convertit un texte en entier compris entre min et max.
 * Paramètres :
 *   - texte : le texte à convertir
 *   - min, max : bornes autorisées
 * Retour : l'entier, ou -1 si le texte n'est pas un nombre valide.
 */
static int lire_entier(const char *texte, int min, int max)
{
    char *fin;
    long valeur = strtol(texte, &fin, 10);

    if (fin == texte || *fin != '\0' || valeur < min || valeur > max)
        return -1;
    return (int)valeur;
}

int main(int argc, char *argv[])
{
    int port = PORT_PAR_DEFAUT;
    const char *dossier = DOSSIER_PAR_DEFAUT;
    int option;

    while ((option = getopt(argc, argv, "p:d:h")) != -1) {
        switch (option) {
        case 'p':
            port = lire_entier(optarg, 1, 65535);
            if (port < 0) {
                fprintf(stderr, "Port invalide : %s\n", optarg);
                return EXIT_FAILURE;
            }
            break;
        case 'd':
            dossier = optarg;
            break;
        case 'h':
            afficher_aide(argv[0]);
            return EXIT_SUCCESS;
        default:
            afficher_aide(argv[0]);
            return EXIT_FAILURE;
        }
    }

    /* Le dossier servi, en chemin absolu */
    char racine[PATH_MAX];
    struct stat infos;
    if (realpath(dossier, racine) == NULL || stat(racine, &infos) < 0 || !S_ISDIR(infos.st_mode)) {
        fprintf(stderr, "Dossier introuvable : %s\n", dossier);
        return EXIT_FAILURE;
    }

    /* Écrire vers un client déjà parti ne doit pas arrêter le programme */
    signal(SIGPIPE, SIG_IGN);

    int ecoute = creer_socket_ecoute(port);
    if (ecoute < 0)
        return EXIT_FAILURE;

    printf("Serveur prêt sur http://localhost:%d (dossier %s)\n", port, racine);
    fflush(stdout);

    lancer_serveur(ecoute, racine);
    return EXIT_SUCCESS;
}
