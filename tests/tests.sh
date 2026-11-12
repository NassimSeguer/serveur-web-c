#!/usr/bin/env bash
# Tests automatiques de serveur-web-c.
# Le script crée un petit site temporaire, lance le serveur dans chaque mode,
# envoie des requêtes avec curl et vérifie les réponses.
# Code de retour : 0 si toutes les vérifications passent.

set -u
cd "$(dirname "$0")/.." || exit 1

SERVEUR="${SERVEUR:-./serveur}"
PORT="${PORT:-8181}"
URL="http://127.0.0.1:$PORT"
reussis=0
echecs=0
PID=""

# verifier description attendu obtenu
verifier() {
    if [ "$2" = "$3" ]; then
        reussis=$((reussis + 1))
        printf '  ✔ %s\n' "$1"
    else
        echecs=$((echecs + 1))
        printf '  ✘ %s (attendu : %s, obtenu : %s)\n' "$1" "$2" "$3"
    fi
}

# info format [options curl...] : renvoie les informations demandées sur la réponse
info() {
    local format=$1
    shift
    curl -s -o /dev/null -w "$format" --max-time 5 "$@"
}

# ligne_brute texte : envoie le texte tel quel et renvoie la première ligne de la réponse
ligne_brute() {
    local reponse=""
    exec 3<>"/dev/tcp/127.0.0.1/$PORT" || return
    printf '%b' "$1" >&3
    IFS= read -r -t 5 reponse <&3
    exec 3>&-
    printf '%s' "${reponse%$'\r'}"
}

arreter() {
    if [ -n "$PID" ]; then
        kill -INT "$PID" 2>/dev/null
        wait "$PID" 2>/dev/null
        PID=""
    fi
}

[ -x "$SERVEUR" ] || { echo "Programme introuvable : $SERVEUR (lancer make d'abord)"; exit 1; }

SITE=$(mktemp -d)
JOURNAL=$(mktemp)
trap 'arreter; rm -rf "$SITE" "$JOURNAL"' EXIT

# Le site de test
printf '<h1>Accueil</h1>\n' > "$SITE/index.html"
printf 'body { color: red; }\n' > "$SITE/style.css"
printf 'PNG' > "$SITE/image.png"
printf 'espace\n' > "$SITE/fichier avec espace.txt"
printf 'secret\n' > "$SITE/.cache"
mkdir "$SITE/sous-dossier" "$SITE/vide"
printf 'sous\n' > "$SITE/sous-dossier/index.html"
ln -s /etc/hostname "$SITE/lien-externe"
head -c 5000000 /dev/urandom > "$SITE/gros.bin"

for mode in threads fork; do
    echo "== Mode $mode"
    "$SERVEUR" -p "$PORT" -d "$SITE" -m "$mode" -t 1 > "$JOURNAL" 2>&1 &
    PID=$!
    for _ in $(seq 50); do
        info '%{http_code}' "$URL/" > /dev/null && break
        sleep 0.1
    done

    # Fichiers et types
    verifier "page d'accueil" "200" "$(info '%{http_code}' "$URL/")"
    verifier "type HTML" "text/html; charset=utf-8" "$(info '%{content_type}' "$URL/")"
    verifier "type CSS" "text/css; charset=utf-8" "$(info '%{content_type}' "$URL/style.css")"
    verifier "type PNG" "image/png" "$(info '%{content_type}' "$URL/image.png")"
    verifier "page absente" "404" "$(info '%{http_code}' "$URL/absente.html")"
    verifier "nom avec espaces" "200" "$(info '%{http_code}' "$URL/fichier%20avec%20espace.txt")"

    # Dossiers
    verifier "dossier sans barre finale" "301 $URL/sous-dossier/" "$(info '%{http_code} %{redirect_url}' "$URL/sous-dossier")"
    verifier "index d'un dossier" "200" "$(info '%{http_code}' "$URL/sous-dossier/")"
    verifier "dossier sans index" "404" "$(info '%{http_code}' "$URL/vide/")"

    # Méthodes
    verifier "HEAD : en-têtes sans contenu" "200 0" "$(info '%{http_code} %{size_download}' -I "$URL/")"
    verifier "HEAD : taille annoncée" "content-length: 17" \
        "$(curl -s -I "$URL/" | tr -d '\r' | grep -i '^content-length' | tr '[:upper:]' '[:lower:]')"
    verifier "méthode refusée" "405" "$(info '%{http_code}' -X POST "$URL/")"
    verifier "méthodes permises" "allow: get, head" \
        "$(curl -s -o /dev/null -D - -X POST "$URL/" | tr -d '\r' | grep -i '^allow' | tr '[:upper:]' '[:lower:]')"

    # Sécurité
    verifier "remontée ../" "403" "$(info '%{http_code}' --path-as-is "$URL/../../etc/passwd")"
    verifier "remontée encodée %2e%2e" "403" "$(info '%{http_code}' --path-as-is "$URL/%2e%2e/%2e%2e/etc/passwd")"
    verifier "remontée encodée ..%2f" "403" "$(info '%{http_code}' --path-as-is "$URL/..%2f..%2fetc%2fpasswd")"
    verifier "fichier caché" "403" "$(info '%{http_code}' "$URL/.cache")"
    verifier "lien vers l'extérieur" "403" "$(info '%{http_code}' "$URL/lien-externe")"
    verifier "requête mal formée" "HTTP/1.1 400 Bad Request" "$(ligne_brute 'BONJOUR\r\n\r\n')"
    verifier "en-têtes trop longs" "431" \
        "$(info '%{http_code}' -H "X-Long: $(head -c 10000 /dev/zero | tr '\0' a)" "$URL/")"

    # Gros fichier et charge
    verifier "gros fichier intact" "$(sha256sum < "$SITE/gros.bin" | cut -d' ' -f1)" \
        "$(curl -s --max-time 10 "$URL/gros.bin" | sha256sum | cut -d' ' -f1)"
    verifier "40 requêtes en parallèle" "40" \
        "$(seq 40 | xargs -P 20 -I{} curl -s -o /dev/null -w '%{http_code}\n' --max-time 5 "$URL/" | grep -c '^200$')"

    # Un client muet ne bloque pas les autres, et finit par recevoir 408
    exec 4<>"/dev/tcp/127.0.0.1/$PORT"
    verifier "un client muet ne bloque pas les autres" "200" "$(info '%{http_code}' --max-time 2 "$URL/")"
    reponse=""
    IFS= read -r -t 4 reponse <&4
    exec 4>&-
    verifier "client muet : délai dépassé" "HTTP/1.1 408 Request Timeout" "${reponse%$'\r'}"

    # Le journal : une ligne par requête, sans caractère de contrôle
    ligne_brute 'GET /a\033[31mrouge HTTP/1.1\r\n\r\n' > /dev/null
    sleep 0.3
    verifier "journal des requêtes" "1" "$(grep -c 'GET /style.css  200' "$JOURNAL")"
    verifier "journal sans caractère de contrôle" "0" "$(grep -c $'\033' "$JOURNAL")"

    # Arrêt : on laisse les dernières connexions se fermer, puis Ctrl+C
    sleep 1.5
    kill -INT "$PID"
    wait "$PID"
    code=$?
    PID=""
    verifier "arrêt propre avec Ctrl+C" "0" "$code"
done

echo
echo "$reussis vérifications réussies, $echecs en échec"
[ "$echecs" -eq 0 ]
