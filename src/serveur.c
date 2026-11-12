#include "serveur.h"
#include "traitement.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

/* Ce qu'il faut pour servir une connexion, transmis au thread ou au processus */
typedef struct {
    int client;
    char ip[INET_ADDRSTRLEN];
    const char *racine;
} TConnexion;

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
 * Rôle : sert une connexion, puis la ferme.
 * Paramètres :
 *   - connexion : la connexion à servir
 */
static void servir_connexion(const TConnexion *connexion)
{
    traiter_client(connexion->client, connexion->racine, connexion->ip);
    close(connexion->client);
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

    pthread_t thread;
    int erreur = pthread_create(&thread, NULL, thread_client, connexion);
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

void lancer_serveur(int ecoute, const char *racine, TMode mode)
{
    for (;;) {
        struct sockaddr_in adresse;
        socklen_t longueur = sizeof adresse;
        int client = accept(ecoute, (struct sockaddr *)&adresse, &longueur);
        if (client < 0) {
            perror("accept");
            continue;
        }

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
