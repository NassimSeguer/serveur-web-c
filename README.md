# 🌐 Serveur web en C

> Un serveur HTTP écrit de zéro en C avec les sockets POSIX de Linux : fichiers statiques, plusieurs visiteurs à la fois, protections de base et tests automatiques.

![C](https://img.shields.io/badge/C-A8B9CC?style=flat&logo=c&logoColor=black)
![Linux](https://img.shields.io/badge/Linux-FCC624?style=flat&logo=linux&logoColor=black)
![Tests](https://img.shields.io/badge/tests-54%20v%C3%A9rifications-2ea44f?style=flat)

![La page de démonstration envoyée par le serveur](images/apercu.png)

## ✨ Ce que fait le serveur

- **Il envoie les fichiers d'un dossier** avec le bon type : HTML, CSS, JavaScript, images, PDF…
- **Il gère les dossiers** : `/blog` redirige vers `/blog/`, qui affiche `blog/index.html`.
- **Il sert plusieurs visiteurs à la fois**, avec un thread ou un processus par visiteur.
- **Il se protège** des chemins du type `../`, des fichiers cachés et des liens qui sortent du dossier.
- **Il ne se laisse pas bloquer** : un client trop lent reçoit une erreur 408 au bout du délai.
- **Il tient un journal** de chaque requête, sans laisser un visiteur y glisser de faux caractères.
- **Il s'arrête proprement** avec Ctrl+C.

## 🚀 Compiler et lancer

Sous Linux, ou sous Windows dans Ubuntu avec WSL :

```bash
git clone https://github.com/NassimSeguer/serveur-web-c.git
cd serveur-web-c
make
./serveur
```

Ouvrez ensuite http://localhost:8080. Pour servir un autre dossier sur un autre port :

```bash
./serveur -d ../mon-site -p 3000
```

| Option | Rôle | Valeur par défaut |
|---|---|---|
| `-p` | Port d'écoute | `8080` |
| `-d` | Dossier à servir | `site` |
| `-m` | `threads`, `fork` ou `simple` (un visiteur à la fois) | `threads` |
| `-t` | Attente maximale d'un client, en secondes | `5` |

## 📜 Exemple de journal

```
Serveur prêt sur http://localhost:8080 (dossier /chemin/vers/serveur-web-c/site, mode threads)
Ctrl+C pour l'arrêter.
2026-11-12 14:29:10  127.0.0.1        GET /  200  779 o
2026-11-12 14:29:10  127.0.0.1        GET /style.css  200  400 o
2026-11-12 14:29:10  127.0.0.1        GET /favicon.ico  404  0 o
2026-11-12 14:29:13  127.0.0.1        GET /../../etc/passwd  403  0 o
2026-11-12 14:29:13  127.0.0.1        POST /  405  0 o

Serveur arrêté.
```

## 🧪 Tests

```bash
make test       # 54 vérifications, dans les modes threads et fork
make verifier   # les mêmes, avec AddressSanitizer et UBSan
```

Le script `tests/tests.sh` crée un site temporaire, lance le serveur et l'interroge avec `curl`. Il vérifie les types MIME, les erreurs 404, les redirections, `HEAD`, le refus de `POST` et les attaques par `../`, même encodées. Il contrôle aussi les fichiers cachés, les liens symboliques, les requêtes mal formées ou trop longues, l'intégrité d'un fichier de 5 Mo, 40 requêtes simultanées, un client muet, le journal et l'arrêt.

## 🗂️ Organisation du code

| Module | Rôle |
|---|---|
| `main.c` | Options, signaux et démarrage |
| `serveur.c` | Prise d'écoute, accueil des visiteurs, threads ou processus |
| `traitement.c` | Tout le travail fait pour un visiteur |
| `http.c` | Lecture de la requête et envoi des réponses |
| `fichiers.c` | Recherche du fichier, sécurité des chemins, types MIME |
| `journal.c` | Une ligne de journal par requête |

Chaque fonction est documentée dans les fichiers `.h`, ou juste au-dessus d'elle pour les fonctions internes : rôle, but, paramètres et valeur de retour.

## 🧠 Points techniques

### Le trajet d'une requête

1. `accept` donne une nouvelle prise, réservée à ce visiteur, pendant que la prise d'écoute continue d'attendre les suivants.
2. `lire_requete` lit jusqu'à la ligne vide qui termine les en-têtes, même si la requête arrive en plusieurs morceaux.
3. `analyser_requete` découpe la première ligne : `GET /index.html HTTP/1.1`.
4. `servir_fichier` décode l'adresse, vérifie le chemin, puis envoie les en-têtes et le fichier par blocs de 64 Ko.
5. `journaliser` écrit une ligne, puis `fermer_connexion` raccroche.

### Le format d'une réponse

```
HTTP/1.1 200 OK
Content-Type: text/html; charset=utf-8
Content-Length: 779
Connection: close

<!DOCTYPE html>...
```

Chaque ligne d'en-tête se termine par `\r\n`. Une ligne vide sépare les en-têtes du contenu, et `Content-Length` indique au navigateur où la réponse s'arrête.

### Threads ou processus

| | Un thread par visiteur (`-m threads`) | Un processus par visiteur (`-m fork`) |
|---|---|---|
| Création | Rapide | Plus lente : le processus est copié |
| Mémoire | Partagée : un verrou protège le journal | Séparée : une erreur reste dans l'enfant |
| Fin | Thread détaché, libéré tout seul | `SIGCHLD` ignoré : pas de processus zombie |

En mode `simple`, un seul client muet bloque tout le serveur : c'est ce que les deux autres modes corrigent.

### Les signaux

- **Ctrl+C** envoie `SIGINT`. Installé sans `SA_RESTART`, il interrompt `accept`, et la boucle voit qu'il faut s'arrêter.
- **En mode threads**, `SIGINT` est bloqué dans les threads des visiteurs, pour qu'il arrive au fil principal.
- **`SIGPIPE`** est ignoré, et `send` utilise `MSG_NOSIGNAL` : écrire vers un client parti renvoie une erreur au lieu d'arrêter le programme.

### La sécurité des chemins

1. Le décodage refuse les `%` mal formés et l'octet nul `%00`.
2. Tout élément du chemin qui commence par un point est refusé : `..`, `.git`, `.env`…
3. `realpath` résout les liens symboliques, et le chemin obtenu doit rester dans le dossier servi.

### Les délais

`SO_RCVTIMEO` et `SO_SNDTIMEO` limitent l'attente sur chaque connexion : un client muet reçoit une erreur 408. À la fin, `shutdown` annonce la fin de la réponse, puis le serveur lit ce qui reste avant de fermer : une fermeture brutale pourrait effacer la réponse avant que le client la lise.

## ⚠️ Limites et pistes

- Une seule requête par connexion : pas de `keep-alive`.
- IPv4 seulement.
- Un thread par visiteur, sans limite : un groupe de threads fixe ou `epoll` tiendraient mieux une forte charge.
- Pas de HTTPS : il faudrait une bibliothèque comme OpenSSL.

## 🪜 Les étapes du projet

Chaque étape a son commit dans l'historique, et les tests ont le leur :

1. Répondre « Bonjour » à un navigateur.
2. Envoyer les fichiers d'un dossier, avec le bon type et une page 404.
3. Servir plusieurs visiteurs à la fois, avec `fork` ou des threads.
4. Bloquer les chemins dangereux, limiter les attentes et tenir un journal.
5. Ajouter les tests automatiques et la vérification mémoire.

## 👤 Auteur

**Nassim Seguer**, étudiant en L3 Informatique à l'UFR MIM de Metz · [LinkedIn](https://linkedin.com/in/nassim-seguer)
