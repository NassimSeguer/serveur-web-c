# 🌐 Serveur web en C

> Un serveur HTTP écrit de zéro en C, avec les sockets POSIX de Linux.

🚧 Projet en cours.

## Étapes

- [x] 1. Répondre « Bonjour » à un navigateur
- [x] 2. Envoyer les fichiers d'un dossier, avec le bon type et une page 404
- [x] 3. Servir plusieurs visiteurs à la fois (`fork`, puis threads)
- [x] 4. Bloquer les chemins dangereux, journaliser les requêtes, ajouter des tests

## Compiler et lancer

Sous Linux, ou dans Ubuntu avec WSL :

```bash
make
./serveur
```

Puis ouvrir http://localhost:8080 dans un navigateur.

## 👤 Auteur

**Nassim Seguer**, étudiant en L3 Informatique à l'UFR MIM de Metz · [LinkedIn](https://linkedin.com/in/nassim-seguer)
