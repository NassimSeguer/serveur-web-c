#include "serveur.h"
#include "traitement.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>

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

void lancer_serveur(int ecoute, const char *racine)
{
    for (;;) {
        struct sockaddr_in adresse;
        socklen_t longueur = sizeof adresse;
        int client = accept(ecoute, (struct sockaddr *)&adresse, &longueur);
        if (client < 0) {
            perror("accept");
            continue;
        }

        char ip[INET_ADDRSTRLEN] = "?";
        inet_ntop(AF_INET, &adresse.sin_addr, ip, sizeof ip);

        traiter_client(client, racine, ip);
        close(client);
    }
}
