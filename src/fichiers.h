#ifndef FICHIERS_H
#define FICHIERS_H

/* ============================================================================
 *  Module fichiers : trouver le fichier demandé dans le dossier servi et l'envoyer.
 * ========================================================================= */

#include <stddef.h>
#include "http.h"

/**
 * Rôle : devine le type d'un fichier à partir de son extension.
 * But : le navigateur en a besoin pour afficher une page, appliquer un style ou montrer une image.
 * Paramètres :
 *   - chemin : chemin ou nom du fichier
 * Retour : le type MIME, ou « application/octet-stream » si l'extension est inconnue.
 */
const char *type_mime(const char *chemin);

/**
 * Rôle : décode une adresse : « %20 » devient une espace, et ce qui suit « ? » est ignoré.
 * But : retrouver le vrai nom du fichier demandé.
 * Paramètres :
 *   - source : le chemin tel qu'envoyé par le navigateur
 *   - destination : où écrire le chemin décodé
 *   - taille : taille de destination
 * Retour : 0 si le décodage réussit, -1 si l'adresse est invalide ou trop longue.
 */
int decoder_url(const char *source, char *destination, size_t taille);

/**
 * Rôle : répond à une requête GET ou HEAD avec le fichier demandé, ou avec une erreur.
 * But : c'est le cœur du serveur de fichiers.
 * Paramètres :
 *   - client : descripteur de la connexion
 *   - racine : chemin absolu du dossier servi
 *   - requete : la requête analysée
 *   - octets : reçoit le nombre d'octets de contenu envoyés
 * Retour : le code de statut envoyé (200, 301, 403, 404...).
 */
int servir_fichier(int client, const char *racine, const TRequete *requete, long long *octets);

#endif
