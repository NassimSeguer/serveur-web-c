#ifndef JOURNAL_H
#define JOURNAL_H

/* ============================================================================
 *  Module journal : une ligne par requête dans le terminal.
 * ========================================================================= */

/**
 * Rôle : écrit une ligne de journal : date, adresse, requête, code et taille envoyée.
 * But : suivre l'activité du serveur et repérer les erreurs.
 *       Un verrou empêche deux threads de mélanger leurs lignes.
 * Paramètres :
 *   - ip : adresse du visiteur
 *   - methode : méthode de la requête, ou « - »
 *   - chemin : chemin demandé, ou « - »
 *   - code : code de statut envoyé
 *   - octets : nombre d'octets de contenu envoyés
 */
void journaliser(const char *ip, const char *methode, const char *chemin, int code, long long octets);

#endif
