#ifndef TRAITEMENT_H
#define TRAITEMENT_H

/* ============================================================================
 *  Module traitement : tout ce que le serveur fait pour un visiteur.
 * ========================================================================= */

/**
 * Rôle : traite une connexion de bout en bout : lecture, analyse, réponse.
 * But : regrouper en un seul appel le travail fait pour chaque visiteur.
 * Paramètres :
 *   - client : descripteur de la connexion
 *   - racine : chemin absolu du dossier servi
 *   - ip : adresse du visiteur, pour le journal
 * Retour : le code de statut envoyé, ou 0 si le client est parti sans rien demander.
 */
int traiter_client(int client, const char *racine, const char *ip);

#endif
