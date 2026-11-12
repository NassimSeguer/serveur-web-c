#include "serveur.h"
#include "http.h"
#include "traitement.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

/* Ce qu'il faut pour servir une connexion, transmis au thread ou au processus */
typedef struct {
    int client;
    char ip[INET_ADDRSTRLEN];
    const char *racine;
} TConnexion;

/* Passe à 1 quand Ctrl+C est pressé. « volatile sig_atomic_t » est le seul
   type qu'un gestionnaire de signal peut modifier sans risque. */
static volatile sig_atomic_t arret_demande = 0;

void arreter_serveur(int numero)
{
    (void)numero;
    arret_demande = 1;
}

int creer_socket_ecoute(int port)
{
    int ecoute = socket(AF_INET, SOCK_STREAM, 0);
    if (ecoute < 0) {
        perror("socket");
        return -1;
    }

    /* Pouvoir relancer le serveur aussitôt après l'avoir arrêté */
    int oui = 1;
    if (setsockopt(ecoute, SOL_SOCKET, SO_REUSEADDR, &oui, sizeof oui) < 0) {
        perror("setsockopt");
        close(ecoute);
        return -1;
    }

    struct sockaddr_in adresse = {0};
    adresse.sin_family = AF_INET;
    adresse.sin_port = htons((unsigned short)port);
    adresse.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(ecoute, (struct sockaddr *)&adresse, sizeof adresse) < 0) {
        perror("bind");
        close(ecoute);
        return -1;
    }
    if (listen(ecoute, 64) < 0) {
        perror("listen");
        close(ecoute);
        return -1;
    }
    return ecoute;
}

/**
 * Rôle : fixe une attente maximale pour les lectures et les écritures sur une connexion.
 * But : un client muet ou très lent ne doit pas occuper le serveur indéfiniment.
 * Paramètres :
 *   - client : descripteur de la connexion
 *   - secondes : attente maximale
 */
static void limiter_attente(int client, int secondes)
{
    struct timeval delai = { .tv_sec = secondes, .tv_usec = 0 };

    setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &delai, sizeof delai);
    setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, &delai, sizeof delai);
}

/**
 * Rôle : sert une connexion, puis la ferme proprement.
 * Paramètres :
 *   - connexion : la connexion à servir
 */
static void servir_connexion(const TConnexion *connexion)
{
    traiter_client(connexion->client, connexion->racine, connexion->ip);
    fermer_connexion(connexion->client);
}

/**
 * Rôle : point de départ d'un thread : sert sa connexion, puis libère sa copie.
 * Paramètres :
 *   - argument : un TConnexion alloué par lancer_thread
 * Retour : NULL, le thread n'a rien à transmettre.
 */
static void *thread_client(void *argument)
{
    TConnexion *connexion = argument;

    servir_connexion(connexion);
    free(connexion);
    return NULL;
}

/**
 * Rôle : confie une connexion à un nouveau thread.
 * But : le fil principal retourne aussitôt à accept pendant que le thread répond.
 * Paramètres :
 *   - modele : la connexion à confier
 */
static void lancer_thread(const TConnexion *modele)
{
    /* Le thread reçoit sa propre copie : la variable du fil principal va être réutilisée */
    TConnexion *connexion = malloc(sizeof *connexion);
    if (connexion == NULL) {
        close(modele->client);
        return;
    }
    *connexion = *modele;

    /* Ctrl+C doit arriver au fil principal, qui attend dans accept. Le nouveau
       thread hérite du masque de signaux : on bloque SIGINT et SIGTERM le temps
       de le créer, puis on rétablit le masque du fil principal. */
    sigset_t arret, ancien;
    sigemptyset(&arret);
    sigaddset(&arret, SIGINT);
    sigaddset(&arret, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &arret, &ancien);

    pthread_t thread;
    int erreur = pthread_create(&thread, NULL, thread_client, connexion);
    pthread_sigmask(SIG_SETMASK, &ancien, NULL);

    if (erreur != 0) {
        fprintf(stderr, "pthread_create : %s\n", strerror(erreur));
        close(connexion->client);
        free(connexion);
        return;
    }

    /* Détaché : ses ressources sont libérées dès qu'il se termine, sans pthread_join */
    pthread_detach(thread);
}

/**
 * Rôle : confie une connexion à un nouveau processus.
 * But : chaque visiteur a sa propre mémoire ; si un enfant plante, le serveur continue.
 * Paramètres :
 *   - ecoute : la prise d'écoute, que l'enfant referme
 *   - connexion : la connexion à confier
 */
static void lancer_processus(int ecoute, const TConnexion *connexion)
{
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        close(connexion->client);
        return;
    }

    if (pid == 0) {
        /* Processus enfant : il n'a pas besoin de la prise d'écoute */
        close(ecoute);
        servir_connexion(connexion);
        _exit(EXIT_SUCCESS);
    }

    /* Processus parent : l'enfant a sa propre copie du descripteur, on ferme la nôtre */
    close(connexion->client);
}

void lancer_serveur(int ecoute, const char *racine, TMode mode, int delai)
{
    while (!arret_demande) {
        struct sockaddr_in adresse;
        socklen_t longueur = sizeof adresse;
        int client = accept(ecoute, (struct sockaddr *)&adresse, &longueur);
        if (client < 0) {
            /* EINTR : accept a été interrompu par un signal, sans doute Ctrl+C */
            if (errno != EINTR)
                perror("accept");
            continue;
        }

        limiter_attente(client, delai);

        TConnexion connexion = { .client = client, .racine = racine };
        inet_ntop(AF_INET, &adresse.sin_addr, connexion.ip, sizeof connexion.ip);

        switch (mode) {
        case MODE_SIMPLE:
            servir_connexion(&connexion);
            break;
        case MODE_FORK:
            lancer_processus(ecoute, &connexion);
            break;
        case MODE_THREADS:
            lancer_thread(&connexion);
            break;
        }
    }
}
