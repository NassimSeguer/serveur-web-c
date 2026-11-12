#ifndef SERVEUR_H
#define SERVEUR_H

/* ============================================================================
 *  Module serveur : la prise d'écoute et l'accueil des visiteurs.
 * ========================================================================= */

/**
 * Rôle : crée la prise d'écoute TCP sur le port demandé.
 * But : socket, bind et listen réunis en un appel.
 * Paramètres :
 *   - port : numéro de port, entre 1 et 65535
 * Retour : le descripteur de la prise, ou -1 en cas d'échec.
 */
int creer_socket_ecoute(int port);

/**
 * Rôle : accepte les visiteurs un par un et répond à chacun.
 * Paramètres :
 *   - ecoute : la prise créée par creer_socket_ecoute
 *   - racine : chemin absolu du dossier servi
 */
void lancer_serveur(int ecoute, const char *racine);

#endif
