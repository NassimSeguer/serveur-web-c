/*
 * serveur-web-c, étape 1 : répondre « Bonjour » à un navigateur.
 *
 * Le programme ouvre une prise réseau TCP sur le port 8080, attend les
 * connexions une par une et renvoie toujours la même réponse.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT 8080

int main(void)
{
    /* 1. Créer une prise TCP (SOCK_STREAM) en IPv4 (AF_INET) */
    int ecoute = socket(AF_INET, SOCK_STREAM, 0);
    if (ecoute < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }

    /* 2. Pouvoir relancer le serveur aussitôt après l'avoir arrêté */
    int oui = 1;
    if (setsockopt(ecoute, SOL_SOCKET, SO_REUSEADDR, &oui, sizeof oui) < 0) {
        perror("setsockopt");
        return EXIT_FAILURE;
    }

    /* 3. Attacher la prise au port 8080, sur toutes les adresses de la machine */
    struct sockaddr_in adresse = {0};
    adresse.sin_family = AF_INET;
    adresse.sin_port = htons(PORT);              /* octet de poids fort en premier */
    adresse.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(ecoute, (struct sockaddr *)&adresse, sizeof adresse) < 0) {
        perror("bind");
        return EXIT_FAILURE;
    }

    /* 4. Se mettre en écoute, avec une file d'attente de 10 connexions */
    if (listen(ecoute, 10) < 0) {
        perror("listen");
        return EXIT_FAILURE;
    }
    printf("Serveur prêt sur http://localhost:%d\n", PORT);

    /* La réponse HTTP : ligne de statut, en-têtes, ligne vide, puis le contenu */
    const char *reponse =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain; charset=utf-8\r\n"
        "Content-Length: 8\r\n"
        "Connection: close\r\n"
        "\r\n"
        "Bonjour\n";

    /* 5. Servir les visiteurs les uns après les autres, sans fin */
    for (;;) {
        int client = accept(ecoute, NULL, NULL);
        if (client < 0) {
            perror("accept");
            continue;
        }

        char tampon[4096];
        ssize_t lus = read(client, tampon, sizeof tampon - 1);
        if (lus > 0) {
            tampon[lus] = '\0';
            printf("--- Requête reçue ---\n%s", tampon);
        }

        if (write(client, reponse, strlen(reponse)) < 0)
            perror("write");
        close(client);
    }
}
