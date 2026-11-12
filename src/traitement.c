#include "traitement.h"
#include "fichiers.h"
#include "http.h"

#include <stdio.h>
#include <string.h>

int traiter_client(int client, const char *racine, const char *ip)
{
    char tampon[TAILLE_REQUETE_MAX];
    TRequete requete = { "-", "-", "-" };
    long long octets = 0;
    int code;

    int lus = lire_requete(client, tampon, sizeof tampon);
    if (lus == LECTURE_FERMEE || lus == LECTURE_DELAI)
        return 0;

    if (lus == LECTURE_TROP_LONGUE) {
        code = 431;
        envoyer_erreur(client, code, 1, NULL);
    } else if (analyser_requete(tampon, &requete) < 0) {
        code = 400;
        envoyer_erreur(client, code, 1, NULL);
    } else if (strcmp(requete.methode, "GET") != 0 && strcmp(requete.methode, "HEAD") != 0) {
        code = 405;
        envoyer_erreur(client, code, 1, "Allow: GET, HEAD\r\n");
    } else {
        code = servir_fichier(client, racine, &requete, &octets);
    }

    printf("%s  %s %s  %d  %lld o\n", ip, requete.methode, requete.chemin, code, octets);
    fflush(stdout);
    return code;
}
