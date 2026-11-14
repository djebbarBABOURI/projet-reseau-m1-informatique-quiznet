# QuizNet

QuizNet est une application de quiz multijoueur développée dans le cadre du projet de **réseaux informatiques de Master 1 Informatique**. Elle repose sur une architecture client-serveur : un serveur écrit en C gère les connexions, les sessions et les questions, tandis que les clients Python permettent aux joueurs d'utiliser l'application depuis un terminal ou une interface graphique.

## Sommaire

- [Fonctionnalités](#fonctionnalités)
- [Architecture](#architecture)
- [Technologies utilisées](#technologies-utilisées)
- [Prérequis](#prérequis)
- [Installation](#installation)
- [Lancement](#lancement)
- [Déroulement d'une partie](#déroulement-dune-partie)
- [Types de questions et modes de jeu](#types-de-questions-et-modes-de-jeu)
- [Protocole de communication](#protocole-de-communication)
- [Base de données](#base-de-données)
- [Tests](#tests)
- [Dépannage](#dépannage)
- [Pistes d'amélioration](#pistes-damélioration)

## Fonctionnalités

### Comptes joueurs
- Création d'un compte avec un pseudo et un mot de passe.
- Connexion et déconnexion.
- Vérification des identifiants côté serveur.

### Découverte et gestion des sessions
- Découverte automatique des serveurs QuizNet disponibles sur le réseau local grâce à UDP.
- Création d'une session avec un nom, un ou plusieurs thèmes, une difficulté, un nombre de questions, une durée par question, un mode de jeu et un nombre maximal de joueurs.
- Consultation des sessions disponibles.
- Rejoindre une session existante.
- Salle d'attente et démarrage de la partie par le créateur.

### Jeu
- Questions à choix multiples (QCM), questions vrai/faux et questions à réponse textuelle.
- Chronomètre pour chaque question.
- Réception des questions et des événements de jeu envoyés par le serveur.
- Affichage des résultats et du classement à la fin d'une session.
- Enregistrement d'un historique local à la session côté client et possibilité d'afficher le replay depuis l'interface graphique.

### Jokers
Chaque joueur dispose d'un exemplaire de chaque joker par session :
- **50/50** : sur une question QCM, retire deux mauvaises réponses.
- **Passer** : permet de passer la question sans pénalité de vie en mode Battle.

Les jokers doivent être utilisés avant l'envoi d'une réponse. Le joker 50/50 est réservé aux QCM.

## Architecture

Le projet est organisé en deux composants principaux :

```text
projet-reseau-m1-informatique-quiznet/
├── client/
│   ├── quiznet_client.py          # Client en ligne de commande
│   ├── quiznet_client_gui.py      # Client avec interface graphique Tkinter
│   ├── launch_gui.py              # Vérification de Tkinter puis lancement de l'interface
│   ├── check_tkinter.py           # Vérification de la disponibilité de Tkinter
│   └── README.md                  # Instructions spécifiques au client
│
├── server/
│   ├── main.c                     # Point d'entrée du serveur
│   ├── server.c / server.h        # Initialisation du serveur TCP et communication
│   ├── udp.c / udp.h              # Découverte des serveurs par UDP
│   ├── handlers.c / handlers.h    # Traitement des requêtes et événements
│   ├── db_utils.c / db_utils.h    # Accès et opérations sur la base SQLite
│   ├── questions.c / questions.h  # Gestion des questions
│   ├── clients.c / clients.h      # Gestion des clients connectés/authentifiés
│   ├── utils.c / utils.h          # Fonctions utilitaires
│   ├── sha256.c / sha256.h        # Hachage des mots de passe
│   ├── build.sh                   # Script de compilation
│   └── sqlite/
│       ├── quiznet.db             # Base SQLite utilisée par le serveur
│       ├── sqlite3.h
│       ├── sql.c / sql.h
│       └── libsqlite3.a           # Bibliothèque SQLite statique fournie
│
├── TEST_JOKERS.md                 # Scénarios détaillés de test des jokers
├── test_jokers_quick.md           # Test rapide des jokers
├── test_sessions.py               # Test manuel de GET sessions/list
└── ELEMENTS_MANQUANTS.md          # Liste de tâches et pistes restantes
```

### Communication réseau

Le serveur utilise deux protocoles :

- **TCP** pour les échanges entre le client et le serveur : authentification, création/rejoindre une session, réponses aux questions et événements de jeu.
- **UDP** pour la découverte automatique des serveurs sur le réseau local.

Le serveur écoute sur le port TCP fourni au démarrage. Le service de découverte UDP écoute sur le port **5555**. Le client envoie le message `looking for quiznet servers` et le serveur répond avec son nom et son port TCP.

Le serveur accepte plusieurs connexions clientes et traite les communications dans des threads.

## Technologies utilisées

| Composant | Technologies |
|---|---|
| Serveur | C |
| Client | Python |
| Interface graphique | Tkinter |
| Communication | Sockets TCP et UDP |
| Stockage | SQLite |
| Concurrence côté serveur | Pthreads |
| Compilation | GCC |

## Prérequis

- Un système compatible avec les sockets POSIX et GCC (Linux ou macOS recommandé pour le serveur).
- **GCC** et les outils de compilation.
- **Python 3** pour les clients.
- **Tkinter** pour utiliser le client graphique.
- La bibliothèque SQLite statique et les fichiers d'en-tête fournis dans `server/sqlite/`.

Aucune dépendance Python externe n'est indiquée par le code du client : les modules utilisés sont principalement ceux de la bibliothèque standard de Python. Tkinter doit toutefois être disponible pour l'interface graphique.

## Installation

### 1. Récupérer le projet

```bash
git clone <URL_DU_DEPOT>
cd projet-reseau-m1-informatique-quiznet
```

Si le projet a été fourni sous forme d'archive, décompressez-la puis ouvrez un terminal dans le dossier du projet.

### 2. Compiler le serveur

```bash
cd server
chmod +x build.sh
./build.sh
```

Le script compile les sources C et produit l'exécutable `server/main`. Il lie la bibliothèque SQLite statique fournie dans `server/sqlite/`.

En cas de problème de compilation, vérifiez que `gcc`, les outils de compilation et les fichiers `sqlite/libsqlite3.a` et `sqlite/sqlite3.h` sont présents.

## Lancement

Les commandes suivantes sont à exécuter dans des terminaux distincts.

### 1. Démarrer le serveur

Depuis le dossier `server/` :

```bash
./main 8080
```

`8080` est un exemple de port TCP. Vous pouvez choisir un autre port disponible en le passant comme argument.

Le serveur utilise également le port UDP **5555** pour la découverte automatique. Ce port est défini dans le code du service UDP.

**Important :** lancez le serveur depuis le dossier `server/`, car le chemin de la base de données est relatif (`sqlite/quiznet.db`).

### 2. Démarrer le client graphique

Depuis un autre terminal, à la racine du projet :

```bash
cd client
python3 launch_gui.py
```

Vous pouvez également lancer directement l'interface :

```bash
python3 quiznet_client_gui.py
```

Le script `launch_gui.py` vérifie d'abord la disponibilité de Tkinter. Si l'interface graphique ne peut pas être lancée, utilisez le client terminal.

### 3. Démarrer le client en ligne de commande

```bash
cd client
python3 quiznet_client.py
```

Pour tester une partie multijoueur, lancez plusieurs instances du client, par exemple dans plusieurs terminaux ou sur plusieurs machines du même réseau local.

### 4. Connexion au serveur

1. Démarrez le serveur.
2. Lancez le client.
3. Sélectionnez le serveur détecté automatiquement.
4. Créez un compte ou connectez-vous avec un compte existant.
5. Consultez les thèmes et les sessions disponibles.
6. Créez une session ou rejoignez une session en attente.

Si la découverte automatique ne fonctionne pas, vérifiez que les machines sont sur le même réseau local et que les ports TCP choisis et UDP 5555 ne sont pas bloqués par le pare-feu.

## Déroulement d'une partie

1. Un joueur se connecte et crée une session.
2. Il choisit les paramètres de la session : thèmes, difficulté, nombre de questions, durée, mode et nombre maximal de joueurs.
3. Les autres joueurs se connectent et rejoignent la session.
4. Les joueurs attendent dans la salle d'attente.
5. Le créateur démarre la partie.
6. Le serveur envoie les questions aux joueurs et gère le déroulement de la session.
7. Les joueurs répondent avant la fin du chronomètre ou utilisent éventuellement un joker.
8. Les résultats sont communiqués, puis la question suivante est lancée.
9. À la fin de la session, les résultats finaux sont affichés.

## Types de questions et modes de jeu

### Types de questions

Les types présents dans le modèle de données sont :

- `qcm` : question à choix multiples ;
- `boolean` : question vrai/faux ;
- `text` : question à réponse textuelle.

Les difficultés disponibles dans l'interface sont `facile`, `moyen` et `difficile`.

### Modes de jeu

- **Solo** : mode de session sans mécanisme de vies de Battle.
- **Battle** : mode avec un système de vies. Le client initialise l'affichage à quatre vies ; les vies et l'élimination sont gérées au cours de la session.

Les paramètres proposés par l'interface de création comprennent :
- nombre de questions : de 10 à 50 ;
- durée par question : de 10 à 60 secondes ;
- nombre maximal de joueurs : au moins 2 (la valeur proposée par défaut est 6).

## Protocole de communication

Les requêtes applicatives utilisent une ligne contenant la méthode et le chemin, suivie éventuellement d'un corps JSON. Par exemple :

```text
POST player/login
{"pseudo":"joueur1","password":"mot_de_passe"}
```

Les chemins actuellement traités par le serveur comprennent notamment :

| Méthode | Chemin | Rôle |
|---|---|---|
| `POST` | `player/register` | Créer un compte |
| `POST` | `player/login` | Ouvrir une session joueur |
| `POST` | `player/logout` | Se déconnecter |
| `GET` | `themes/list` | Récupérer les thèmes |
| `GET` | `sessions/list` | Lister les sessions |
| `POST` | `session/create` | Créer une session |
| `POST` | `session/join` | Rejoindre une session |
| `POST` | `session/start` | Démarrer une session |
| `POST` | `question/answer` | Envoyer une réponse |
| `POST` | `joker/use` | Utiliser un joker |

Le serveur envoie également des notifications aux clients, notamment lors du démarrage d'une session, de l'arrivée ou du départ d'un joueur, de l'envoi d'une question et de la fin d'une session.

Cette liste décrit les routes visibles dans le traitement des requêtes du serveur ; elle ne constitue pas une spécification exhaustive de tous les messages échangés.

## Base de données

Le serveur utilise SQLite. Le fichier `server/sqlite/quiznet.db` contient la base utilisée à l'exécution. Le code initialise notamment les tables suivantes :

- `players` : comptes joueurs ;
- `themes` : thèmes des questions ;
- `questions` : énoncés, types, difficultés, réponses et explications ;
- `sessions` : paramètres et état des sessions ;
- `themes_session` : thèmes associés aux sessions ;
- `players_session` : participation, score et vies des joueurs ;
- `jokers` : jokers disponibles par joueur et par session ;
- `session_questions` : questions sélectionnées pour une session ;
- `answers` : réponses et résultats ;
- `ranking` : classement d'une session.

Le fichier de base fourni dans le dépôt contient déjà des thèmes et des questions. Le contenu de la base évolue pendant l'utilisation de l'application.

**Sauvegarde :** avant de réinitialiser ou de remplacer `quiznet.db`, faites-en une copie si vous souhaitez conserver les comptes et les données des sessions. Évitez de publier une base contenant des comptes ou des données personnelles réelles.

## Tests

### Test rapide du serveur

Démarrez le serveur dans `server/` :

```bash
./main 8080
```

Puis, depuis la racine du projet, exécutez :

```bash
python3 test_sessions.py
```

Ce script envoie une requête `GET sessions/list` à `127.0.0.1:8080`. Il suppose donc que le serveur écoute sur ce port et sur la même machine.

### Tests des jokers

Deux documents décrivent les vérifications manuelles :

- `TEST_JOKERS.md` : scénarios détaillés ;
- `test_jokers_quick.md` : parcours de test rapide.

Ces tests couvrent notamment l'initialisation des jokers, le fonctionnement du 50/50, le passage d'une question, la désactivation des jokers après une réponse et le mode Battle.

## Dépannage

### Le client ne trouve aucun serveur

- Vérifiez que le serveur est démarré.
- Vérifiez que le port UDP 5555 est disponible et autorisé par le pare-feu.
- Vérifiez que les machines sont sur le même réseau local.
- Pour un test sur une seule machine, vérifiez également que le service UDP est bien démarré.

### La connexion TCP échoue

- Vérifiez le port passé au serveur (`./main 8080` dans les exemples).
- Vérifiez que le port TCP n'est pas déjà utilisé.
- Vérifiez les règles du pare-feu.
- Assurez-vous que le client utilise l'adresse IP et le port du serveur.

### La base SQLite ne s'ouvre pas

Lancez le serveur depuis le dossier `server/` :

```bash
cd server
./main 8080
```

Le chemin `sqlite/quiznet.db` est relatif au répertoire courant.

### Tkinter est introuvable ou incompatible

Essayez d'abord le client terminal :

```bash
python3 quiznet_client.py
```

Sur macOS, l'installation de Python et de Tkinter peut dépendre de la distribution utilisée. Le fichier `client/README.md` fournit des pistes spécifiques, notamment l'utilisation de `launch_gui.py` ou d'une installation Python disposant de Tkinter.

### Le serveur ne compile pas

Vérifiez la présence de GCC et des fichiers SQLite fournis. Relancez la compilation depuis `server/` :

```bash
./build.sh
```

## Pistes d'amélioration

Le fichier `ELEMENTS_MANQUANTS.md` recense des fonctionnalités envisagées ou restant à compléter. Parmi les idées qui y figurent :

- permettre aux joueurs de proposer leurs propres questions ;
- ajouter un classement global des meilleurs joueurs ;
- améliorer les possibilités de replay des sessions terminées ;
- ajouter un mode survie avec des vies bonus ;
- proposer une difficulté mixte dans une même session ;
- compléter les rapports et la documentation du projet.

Ces éléments sont des pistes recensées dans le dépôt et ne sont pas présentés ici comme des fonctionnalités déjà disponibles.

---

## Contexte du projet

Projet réalisé dans le cadre du **Master 1 Informatique – Réseaux**.

