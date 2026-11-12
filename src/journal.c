#include "journal.h"

#include <pthread.h>
#include <stdio.h>
#include <time.h>

/* Un seul thread à la fois écrit dans le journal */
static pthread_mutex_t verrou = PTHREAD_MUTEX_INITIALIZER;

/**
 * Rôle : copie un texte en remplaçant les caractères de contrôle par « ? ».
 * But : un visiteur ne doit pas pouvoir glisser de fausses lignes ou des
 *       codes de couleur dans le journal.
 * Paramètres :
 *   - source : le texte d'origine
 *   - destination : où écrire la copie
 *   - taille : taille de destination
 */
static void nettoyer(const char *source, char *destination, size_t taille)
{
    size_t j = 0;

    for (size_t i = 0; source[i] != '\0' && j + 1 < taille; i++) {
        unsigned char c = (unsigned char)source[i];
        destination[j++] = (c < 0x20 || c == 0x7f) ? '?' : (char)c;
    }
    destination[j] = '\0';
}

void journaliser(const char *ip, const char *methode, const char *chemin, int code, long long octets)
{
    char date[32];
    time_t maintenant = time(NULL);
    struct tm locale;
    localtime_r(&maintenant, &locale);
    strftime(date, sizeof date, "%Y-%m-%d %H:%M:%S", &locale);

    char methode_propre[32];
    char chemin_propre[256];
    nettoyer(methode, methode_propre, sizeof methode_propre);
    nettoyer(chemin, chemin_propre, sizeof chemin_propre);

    pthread_mutex_lock(&verrou);
    printf("%s  %-15s  %s %s  %d  %lld o\n", date, ip, methode_propre, chemin_propre, code, octets);
    fflush(stdout);
    pthread_mutex_unlock(&verrou);
}
