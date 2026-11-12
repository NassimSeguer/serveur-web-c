/*
 * serveur-web-c : un serveur HTTP écrit en C avec les sockets POSIX.
 * Point d'entrée : lit les options, prépare le dossier servi et lance le serveur.
 */

#include "serveur.h"

#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define PORT_PAR_DEFAUT    8080
#define DOSSIER_PAR_DEFAUT "site"
#define DELAI_PAR_DEFAUT   5

/**
 * Rôle : affiche le mode d'emploi du programme.
 * Paramètres :
 *   - programme : nom du programme, tel que lancé
 */
static void afficher_aide(const char *programme)
{
    printf("Utilisation : %s [-p port] [-d dossier] [-m threads|fork|simple] [-t secondes]\n"
           "  -p  port d'écoute (défaut : %d)\n"
           "  -d  dossier à servir (défaut : %s)\n"
           "  -m  un thread par visiteur, un processus, ou un visiteur à la fois (défaut : threads)\n"
           "  -t  attente maximale d'un client, en secondes (défaut : %d)\n",
           programme, PORT_PAR_DEFAUT, DOSSIER_PAR_DEFAUT, DELAI_PAR_DEFAUT);
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

/**
 * Rôle : traduit le nom d'un mode en valeur TMode.
 * Paramètres :
 *   - texte : « threads », « fork » ou « simple »
 *   - mode : reçoit le mode reconnu
 * Retour : 0 si le nom est connu, -1 sinon.
 */
static int lire_mode(const char *texte, TMode *mode)
{
    if (strcmp(texte, "threads") == 0)
        *mode = MODE_THREADS;
    else if (strcmp(texte, "fork") == 0)
        *mode = MODE_FORK;
    else if (strcmp(texte, "simple") == 0)
        *mode = MODE_SIMPLE;
    else
        return -1;
    return 0;
}

/**
 * Rôle : donne le nom d'un mode, pour l'afficher au démarrage.
 * Paramètres :
 *   - mode : le mode
 * Retour : « threads », « fork » ou « simple ».
 */
static const char *nom_mode(TMode mode)
{
    switch (mode) {
    case MODE_FORK:   return "fork";
    case MODE_SIMPLE: return "simple";
    default:          return "threads";
    }
}

/**
 * Rôle : associe une fonction à un signal, par exemple Ctrl+C (SIGINT).
 * But : sans l'option SA_RESTART, un accept en attente est interrompu par le
 *       signal, ce qui laisse la boucle du serveur voir la demande d'arrêt.
 * Paramètres :
 *   - numero : le signal
 *   - action : la fonction à appeler
 */
static void installer_signal(int numero, void (*action)(int))
{
    struct sigaction reglage;

    memset(&reglage, 0, sizeof reglage);
    reglage.sa_handler = action;
    sigemptyset(&reglage.sa_mask);
    reglage.sa_flags = 0;
    sigaction(numero, &reglage, NULL);
}

int main(int argc, char *argv[])
{
    int port = PORT_PAR_DEFAUT;
    int delai = DELAI_PAR_DEFAUT;
    const char *dossier = DOSSIER_PAR_DEFAUT;
    TMode mode = MODE_THREADS;
    int option;

    while ((option = getopt(argc, argv, "p:d:m:t:h")) != -1) {
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
        case 'm':
            if (lire_mode(optarg, &mode) < 0) {
                fprintf(stderr, "Mode inconnu : %s\n", optarg);
                return EXIT_FAILURE;
            }
            break;
        case 't':
            delai = lire_entier(optarg, 1, 3600);
            if (delai < 0) {
                fprintf(stderr, "Délai invalide : %s\n", optarg);
                return EXIT_FAILURE;
            }
            break;
        case 'h':
            afficher_aide(argv[0]);
            return EXIT_SUCCESS;
        default:
            afficher_aide(argv[0]);
            return EXIT_FAILURE;
        }
    }

    /* Le dossier servi, en chemin absolu et sans lien symbolique :
       c'est la référence des contrôles de sécurité */
    char racine[PATH_MAX];
    struct stat infos;
    if (realpath(dossier, racine) == NULL || stat(racine, &infos) < 0 || !S_ISDIR(infos.st_mode)) {
        fprintf(stderr, "Dossier introuvable : %s\n", dossier);
        return EXIT_FAILURE;
    }

    /* Ctrl+C ou kill : arrêt propre */
    installer_signal(SIGINT, arreter_serveur);
    installer_signal(SIGTERM, arreter_serveur);

    /* Écrire vers un client déjà parti ne doit pas arrêter le programme */
    signal(SIGPIPE, SIG_IGN);

    /* En mode fork, le système récupère seul les enfants terminés : pas de zombies */
    if (mode == MODE_FORK)
        signal(SIGCHLD, SIG_IGN);

    int ecoute = creer_socket_ecoute(port);
    if (ecoute < 0)
        return EXIT_FAILURE;

    printf("Serveur prêt sur http://localhost:%d (dossier %s, mode %s)\n", port, racine, nom_mode(mode));
    printf("Ctrl+C pour l'arrêter.\n");
    fflush(stdout);

    lancer_serveur(ecoute, racine, mode, delai);

    close(ecoute);
    printf("\nServeur arrêté.\n");
    return EXIT_SUCCESS;
}
