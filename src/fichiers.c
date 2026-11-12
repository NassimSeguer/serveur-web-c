#include "fichiers.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#define TAILLE_BLOC 65536   /* les fichiers sont envoyés par morceaux de 64 Ko */

/* Une extension et le type MIME correspondant */
typedef struct {
    const char *extension;
    const char *type;
} TTypeMime;

static const TTypeMime TYPES[] = {
    { "html",  "text/html; charset=utf-8" },
    { "htm",   "text/html; charset=utf-8" },
    { "css",   "text/css; charset=utf-8" },
    { "js",    "text/javascript; charset=utf-8" },
    { "json",  "application/json" },
    { "txt",   "text/plain; charset=utf-8" },
    { "png",   "image/png" },
    { "jpg",   "image/jpeg" },
    { "jpeg",  "image/jpeg" },
    { "gif",   "image/gif" },
    { "svg",   "image/svg+xml" },
    { "webp",  "image/webp" },
    { "ico",   "image/x-icon" },
    { "pdf",   "application/pdf" },
    { "woff2", "font/woff2" },
};

const char *type_mime(const char *chemin)
{
    const char *nom = strrchr(chemin, '/');
    nom = (nom != NULL) ? nom + 1 : chemin;

    const char *point = strrchr(nom, '.');
    if (point != NULL) {
        for (size_t i = 0; i < sizeof TYPES / sizeof TYPES[0]; i++) {
            if (strcasecmp(point + 1, TYPES[i].extension) == 0)
                return TYPES[i].type;
        }
    }
    return "application/octet-stream";
}

/**
 * Rôle : convertit un chiffre hexadécimal en valeur.
 * Paramètres :
 *   - c : le caractère, de '0' à '9' ou de 'a' à 'f' (majuscules acceptées)
 * Retour : sa valeur de 0 à 15, ou -1 si ce n'est pas un chiffre hexadécimal.
 */
static int valeur_hexa(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    c = (char)tolower((unsigned char)c);
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return -1;
}

int decoder_url(const char *source, char *destination, size_t taille)
{
    size_t j = 0;

    for (size_t i = 0; source[i] != '\0' && source[i] != '?' && source[i] != '#'; i++) {
        char c = source[i];
        if (c == '%') {
            int fort = valeur_hexa(source[i + 1]);
            int faible = (fort < 0) ? -1 : valeur_hexa(source[i + 2]);
            if (fort < 0 || faible < 0)
                return -1;
            c = (char)(fort * 16 + faible);
            if (c == '\0')
                return -1;          /* un octet nul couperait le nom du fichier */
            i += 2;
        }
        if (j + 1 >= taille)
            return -1;
        destination[j++] = c;
    }
    destination[j] = '\0';
    return 0;
}

/**
 * Rôle : cherche un élément du chemin qui commence par un point : « .. », « .git », « .env »...
 * But : empêcher de remonter au-dessus du dossier servi et de lire les fichiers cachés.
 * Paramètres :
 *   - chemin : le chemin décodé
 * Retour : 1 si un tel élément existe, 0 sinon.
 */
static int contient_element_cache(const char *chemin)
{
    const char *debut = chemin;

    while (*debut != '\0') {
        while (*debut == '/')
            debut++;
        if (*debut == '.')
            return 1;
        while (*debut != '\0' && *debut != '/')
            debut++;
    }
    return 0;
}

/**
 * Rôle : vérifie qu'un chemin absolu se trouve dans le dossier servi.
 * Paramètres :
 *   - chemin : chemin absolu, sans lien symbolique
 *   - racine : chemin absolu du dossier servi
 * Retour : 1 si chemin est la racine ou se trouve dedans, 0 sinon.
 */
static int est_dans_racine(const char *chemin, const char *racine)
{
    size_t longueur = strlen(racine);

    if (strcmp(racine, "/") == 0)
        return 1;
    return strncmp(chemin, racine, longueur) == 0
        && (chemin[longueur] == '/' || chemin[longueur] == '\0');
}

/**
 * Rôle : trouve le fichier réel qui correspond au chemin demandé.
 * But : realpath suit les liens symboliques ; on vérifie ensuite que la cible
 *       reste dans le dossier servi, sinon un lien pourrait mener n'importe où.
 * Paramètres :
 *   - racine : chemin absolu du dossier servi
 *   - chemin : chemin décodé, commençant par '/'
 *   - reel : reçoit le chemin réel (tableau de PATH_MAX octets)
 * Retour : 0 si le fichier existe dans la racine, 403 s'il en sort, 404 s'il n'existe pas.
 */
static int resoudre(const char *racine, const char *chemin, char *reel)
{
    char demande[PATH_MAX];
    int longueur = snprintf(demande, sizeof demande, "%s%s", racine, chemin);

    if (longueur < 0 || (size_t)longueur >= sizeof demande)
        return 404;
    if (realpath(demande, reel) == NULL)
        return errno == EACCES ? 403 : 404;
    return est_dans_racine(reel, racine) ? 0 : 403;
}

/**
 * Rôle : envoie une réponse d'erreur et rend son code.
 * But : écrire « return repondre(...) » au lieu de deux lignes à chaque erreur.
 * Retour : le code passé en paramètre.
 */
static int repondre(int client, int code, int avec_corps, const char *autres)
{
    envoyer_erreur(client, code, avec_corps, autres);
    return code;
}

/**
 * Rôle : redirige « /dossier » vers « /dossier/ ».
 * But : sans la barre finale, les liens relatifs de la page du dossier pointeraient au mauvais endroit.
 * Retour : 301.
 */
static int rediriger_dossier(int client, const TRequete *requete, int avec_corps)
{
    char entete[TAILLE_CHEMIN + 32];
    int longueur = (int)strcspn(requete->chemin, "?#");

    snprintf(entete, sizeof entete, "Location: %.*s/\r\n", longueur, requete->chemin);
    return repondre(client, 301, avec_corps, entete);
}

/**
 * Rôle : envoie le contenu d'un fichier ouvert, bloc par bloc.
 * But : ne jamais charger un gros fichier en entier en mémoire.
 * Paramètres :
 *   - client : descripteur de la connexion
 *   - fichier : descripteur du fichier ouvert
 *   - octets : augmenté du nombre d'octets envoyés
 * Retour : 0 si tout est envoyé, -1 sinon.
 */
static int envoyer_contenu(int client, int fichier, long long *octets)
{
    char bloc[TAILLE_BLOC];
    ssize_t lus;

    while ((lus = read(fichier, bloc, sizeof bloc)) > 0) {
        if (envoyer_tout(client, bloc, (size_t)lus) < 0)
            return -1;
        *octets += lus;
    }
    return lus < 0 ? -1 : 0;
}

/**
 * Rôle : ouvre un fichier ordinaire et l'envoie avec ses en-têtes.
 * Paramètres :
 *   - client : descripteur de la connexion
 *   - chemin : chemin du fichier sur le disque
 *   - avec_corps : 0 pour HEAD, qui ne reçoit que les en-têtes
 *   - octets : reçoit le nombre d'octets de contenu envoyés
 * Retour : le code de statut envoyé.
 */
static int envoyer_fichier(int client, const char *chemin, int avec_corps, long long *octets)
{
    int fichier = open(chemin, O_RDONLY | O_CLOEXEC);
    if (fichier < 0)
        return repondre(client, errno == EACCES ? 403 : 404, avec_corps, NULL);

    struct stat infos;
    if (fstat(fichier, &infos) < 0 || !S_ISREG(infos.st_mode)) {
        close(fichier);
        return repondre(client, 404, avec_corps, NULL);
    }

    if (envoyer_entetes(client, 200, type_mime(chemin), (long long)infos.st_size, NULL) == 0 && avec_corps)
        envoyer_contenu(client, fichier, octets);
    close(fichier);
    return 200;
}

int servir_fichier(int client, const char *racine, const TRequete *requete, long long *octets)
{
    int avec_corps = strcmp(requete->methode, "HEAD") != 0;
    char chemin[TAILLE_CHEMIN];
    char reel[PATH_MAX];
    struct stat infos;

    *octets = 0;

    /* 1. Décoder l'adresse : « /fichier%20avec%20espace.txt » devient « /fichier avec espace.txt » */
    if (decoder_url(requete->chemin, chemin, sizeof chemin) < 0)
        return repondre(client, 400, avec_corps, NULL);

    /* 2. Refuser les remontées « .. » et les fichiers cachés */
    if (contient_element_cache(chemin))
        return repondre(client, 403, avec_corps, NULL);

    /* 3. Trouver le fichier réel et vérifier qu'il reste dans le dossier servi */
    int code = resoudre(racine, chemin, reel);
    if (code != 0)
        return repondre(client, code, avec_corps, NULL);

    /* 4. Un dossier : rediriger vers « dossier/ », puis servir son index.html */
    if (stat(reel, &infos) == 0 && S_ISDIR(infos.st_mode)) {
        size_t longueur = strlen(chemin);
        if (chemin[longueur - 1] != '/')
            return rediriger_dossier(client, requete, avec_corps);
        if (longueur + strlen("index.html") >= sizeof chemin)
            return repondre(client, 404, avec_corps, NULL);
        strcat(chemin, "index.html");
        code = resoudre(racine, chemin, reel);
        if (code != 0)
            return repondre(client, code, avec_corps, NULL);
    }

    /* 5. Ouvrir le fichier et l'envoyer */
    return envoyer_fichier(client, reel, avec_corps, octets);
}
