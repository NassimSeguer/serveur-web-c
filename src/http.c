#include "http.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>

int lire_requete(int client, char *tampon, size_t taille)
{
    size_t total = 0;

    while (total < taille - 1) {
        ssize_t lus = recv(client, tampon + total, taille - 1 - total, 0);
        if (lus < 0) {
            if (errno == EINTR)
                continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return LECTURE_DELAI;
            return LECTURE_FERMEE;
        }
        if (lus == 0)
            return LECTURE_FERMEE;

        total += (size_t)lus;
        tampon[total] = '\0';

        /* Une ligne vide marque la fin des en-têtes */
        if (strstr(tampon, "\r\n\r\n") != NULL || strstr(tampon, "\n\n") != NULL)
            return (int)total;
    }
    return LECTURE_TROP_LONGUE;
}

int analyser_requete(const char *tampon, TRequete *requete)
{
    /* Les largeurs (%15s...) empêchent d'écrire au-delà des tableaux */
    if (sscanf(tampon, "%15s %2047s %15s", requete->methode, requete->chemin, requete->version) != 3)
        return -1;
    if (strncmp(requete->version, "HTTP/1.", 7) != 0)
        return -1;
    if (requete->chemin[0] != '/')
        return -1;
    return 0;
}

int envoyer_tout(int client, const void *donnees, size_t taille)
{
    const char *octets = donnees;

    while (taille > 0) {
        /* MSG_NOSIGNAL : si le client est parti, send renvoie une erreur au lieu d'arrêter le programme */
        ssize_t envoyes = send(client, octets, taille, MSG_NOSIGNAL);
        if (envoyes < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        octets += envoyes;
        taille -= (size_t)envoyes;
    }
    return 0;
}

const char *texte_statut(int code)
{
    switch (code) {
    case 200: return "OK";
    case 301: return "Moved Permanently";
    case 400: return "Bad Request";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 408: return "Request Timeout";
    case 431: return "Request Header Fields Too Large";
    default:  return "Internal Server Error";
    }
}

/**
 * Rôle : donne une explication en français, affichée dans les pages d'erreur.
 * Paramètres :
 *   - code : le code de statut
 * Retour : la phrase d'explication.
 */
static const char *explication(int code)
{
    switch (code) {
    case 301: return "Cette page a déménagé.";
    case 400: return "La requête est mal formée.";
    case 403: return "L'accès à cette ressource est interdit.";
    case 404: return "La page demandée n'existe pas.";
    case 405: return "Seules les méthodes GET et HEAD sont acceptées.";
    case 408: return "La requête n'est pas arrivée à temps.";
    case 431: return "Les en-têtes de la requête sont trop longs.";
    default:  return "Le serveur a rencontré une erreur.";
    }
}

int envoyer_entetes(int client, int code, const char *type, long long taille, const char *autres)
{
    /* La date au format HTTP, toujours en temps universel */
    char date[64];
    time_t maintenant = time(NULL);
    struct tm utc;
    gmtime_r(&maintenant, &utc);
    strftime(date, sizeof date, "%a, %d %b %Y %H:%M:%S GMT", &utc);

    char entetes[1024];
    int longueur = snprintf(entetes, sizeof entetes,
                            "HTTP/1.1 %d %s\r\n"
                            "Date: %s\r\n"
                            "Server: serveur-web-c\r\n"
                            "Content-Type: %s\r\n"
                            "Content-Length: %lld\r\n"
                            "Connection: close\r\n"
                            "%s"
                            "\r\n",
                            code, texte_statut(code), date, type, taille,
                            autres != NULL ? autres : "");
    if (longueur < 0 || (size_t)longueur >= sizeof entetes)
        return -1;
    return envoyer_tout(client, entetes, (size_t)longueur);
}

int envoyer_erreur(int client, int code, int avec_corps, const char *autres)
{
    char page[512];
    int longueur = snprintf(page, sizeof page,
                            "<!DOCTYPE html>\n"
                            "<html lang=\"fr\"><head><meta charset=\"utf-8\">"
                            "<title>%d %s</title></head>\n"
                            "<body><h1>%d %s</h1><p>%s</p></body></html>\n",
                            code, texte_statut(code), code, texte_statut(code), explication(code));
    if (longueur < 0 || (size_t)longueur >= sizeof page)
        return -1;
    if (envoyer_entetes(client, code, "text/html; charset=utf-8", longueur, autres) < 0)
        return -1;
    return avec_corps ? envoyer_tout(client, page, (size_t)longueur) : 0;
}
