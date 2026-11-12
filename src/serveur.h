#ifndef SERVEUR_H
#define SERVEUR_H

/* ============================================================================
 *  Module serveur : la prise d'écoute et l'accueil des visiteurs.
 * ========================================================================= */

/* Façon de servir plusieurs visiteurs */
typedef enum {
    MODE_SIMPLE,    /* un visiteur à la fois */
    MODE_FORK,      /* un processus par visiteur */
    MODE_THREADS    /* un thread par visiteur */
} TMode;

/**
 * Rôle : crée la prise d'écoute TCP sur le port demandé.
 * But : socket, bind et listen réunis en un appel.
 * Paramètres :
 *   - port : numéro de port, entre 1 et 65535
 * Retour : le descripteur de la prise, ou -1 en cas d'échec.
 */
int creer_socket_ecoute(int port);

/**
 * Rôle : demande l'arrêt du serveur ; prévue pour être appelée par un signal (Ctrl+C).
 * But : la boucle d'accueil s'arrête proprement au lieu d'être tuée en plein travail.
 * Paramètres :
 *   - numero : numéro du signal reçu (inutilisé)
 */
void arreter_serveur(int numero);

/**
 * Rôle : accepte les visiteurs jusqu'à l'arrêt, et confie chacun à un thread,
 *        à un processus, ou le sert directement en mode simple.
 * Paramètres :
 *   - ecoute : la prise créée par creer_socket_ecoute
 *   - racine : chemin absolu du dossier servi
 *   - mode : MODE_SIMPLE, MODE_FORK ou MODE_THREADS
 *   - delai : attente maximale d'un client, en secondes
 */
void lancer_serveur(int ecoute, const char *racine, TMode mode, int delai);

#endif
