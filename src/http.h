#ifndef HTTP_H
#define HTTP_H

/* ============================================================================
 *  Module http : lire une requête, la découper et envoyer les réponses.
 * ========================================================================= */

#include <stddef.h>

#define TAILLE_REQUETE_MAX 8192   /* au-delà, les en-têtes sont refusés (431) */
#define TAILLE_METHODE     16
#define TAILLE_CHEMIN      2048
#define TAILLE_VERSION     16

/* Résultats possibles de lire_requete, en plus d'une longueur positive */
#define LECTURE_FERMEE      (-1)  /* le client est parti, ou erreur réseau */
#define LECTURE_DELAI       (-2)  /* le client n'a rien envoyé à temps */
#define LECTURE_TROP_LONGUE (-3)  /* les en-têtes dépassent TAILLE_REQUETE_MAX */

/* La première ligne d'une requête, par exemple « GET /index.html HTTP/1.1 » */
typedef struct {
    char methode[TAILLE_METHODE];
    char chemin[TAILLE_CHEMIN];
    char version[TAILLE_VERSION];
} TRequete;

/**
 * Rôle : lit ce que le client envoie, jusqu'à la ligne vide qui termine les en-têtes.
 * But : obtenir toute la requête, même si elle arrive en plusieurs morceaux.
 * Paramètres :
 *   - client : descripteur de la connexion
 *   - tampon : zone où ranger la requête, terminée par '\0'
 *   - taille : taille du tampon en octets
 * Retour : le nombre d'octets lus, ou LECTURE_FERMEE, LECTURE_DELAI, LECTURE_TROP_LONGUE.
 */
int lire_requete(int client, char *tampon, size_t taille);

/**
 * Rôle : découpe la première ligne de la requête en méthode, chemin et version.
 * But : savoir ce que le client demande.
 * Paramètres :
 *   - tampon : la requête lue par lire_requete
 *   - requete : la structure à remplir
 * Retour : 0 si la ligne est valide, -1 sinon.
 */
int analyser_requete(const char *tampon, TRequete *requete);

/**
 * Rôle : envoie tous les octets demandés, en plusieurs fois si besoin.
 * But : send peut n'envoyer qu'une partie des données ; cette fonction insiste jusqu'au bout.
 * Paramètres :
 *   - client : descripteur de la connexion
 *   - donnees : les octets à envoyer
 *   - taille : leur nombre
 * Retour : 0 si tout est parti, -1 si la connexion est perdue.
 */
int envoyer_tout(int client, const void *donnees, size_t taille);

/**
 * Rôle : donne la phrase officielle associée à un code HTTP (« Not Found » pour 404).
 * Paramètres :
 *   - code : le code de statut
 * Retour : la phrase, ou « Internal Server Error » pour un code inconnu.
 */
const char *texte_statut(int code);

/**
 * Rôle : envoie la ligne de statut et les en-têtes d'une réponse.
 * But : annoncer au navigateur le résultat, le type et la taille du contenu qui suit.
 * Paramètres :
 *   - client : descripteur de la connexion
 *   - code : code de statut (200, 404...)
 *   - type : type du contenu, par exemple « text/html; charset=utf-8 »
 *   - taille : taille du contenu en octets
 *   - autres : en-têtes supplémentaires terminés par \r\n, ou NULL
 * Retour : 0 si tout est envoyé, -1 sinon.
 */
int envoyer_entetes(int client, int code, const char *type, long long taille, const char *autres);

/**
 * Rôle : envoie une réponse d'erreur avec une petite page HTML explicative.
 * Paramètres :
 *   - client : descripteur de la connexion
 *   - code : code de statut (301, 400, 403, 404...)
 *   - avec_corps : 0 pour une requête HEAD, qui ne reçoit que les en-têtes
 *   - autres : en-têtes supplémentaires (Location, Allow...), ou NULL
 * Retour : 0 si tout est envoyé, -1 sinon.
 */
int envoyer_erreur(int client, int code, int avec_corps, const char *autres);

#endif
