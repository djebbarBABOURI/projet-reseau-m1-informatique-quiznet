# 📋 Éléments Manquants dans le Projet QuizNet

## 🔴 Critiques (À implémenter absolument)

### 1. Handler pour les Jokers (`POST joker/use`)
**Statut** : ❌ Non implémenté

**Problème** : Le client peut envoyer des requêtes `POST joker/use`, mais le serveur n'a pas de handler pour les traiter.

**À faire** :
- Créer `handle_joker_use()` dans `handlers.c`
- Vérifier que le joker est disponible (table `jokers`)
- Pour le joker 50/50 : éliminer 2 mauvaises réponses aléatoirement et renvoyer les réponses restantes
- Pour le joker Passer : marquer la question comme passée (pas de perte de vie en mode Battle)
- Mettre à jour les jokers restants dans la base de données
- Envoyer la réponse avec les jokers restants

**Référence protocole** : Lignes 11-12 du sujet

---

### 2. Gestion des Déconnexions
**Statut** : ⚠️ Partiellement implémenté

**Problème** : Le serveur détecte les déconnexions (dans `communicationThread`), mais n'envoie pas le message `POST session/player/left` aux autres joueurs.

**À faire** :
- Dans `communicationThread`, quand `bytes <= 0`, détecter la déconnexion
- Récupérer le `player_id` et la `session_id` du joueur déconnecté
- Envoyer `POST session/player/left` à tous les autres joueurs de la session
- Gérer le cas où le joueur déconnecté était dans une session active

**Référence protocole** : Section "Déconnexion d'un joueur"

---

### 3. Gestion de la Déconnexion du Créateur
**Statut** : ❌ Non implémenté

**Problème** : Si le créateur d'une session se déconnecte, aucun autre joueur ne peut démarrer la session.

**À faire** :
- Détecter quand le créateur se déconnecte
- Transférer le rôle de créateur (`is_creator = 1`) au joueur suivant dans la session
- Notifier les joueurs du changement de créateur (optionnel mais recommandé)

**Référence protocole** : Section "Robustesse" - "La déconnexion du créateur de session"

---

### 4. Vérification des Réponses en Double
**Statut** : ❌ Non implémenté

**Problème** : Un joueur peut répondre plusieurs fois à la même question.

**À faire** :
- Dans `handle_question_answer()`, vérifier si le joueur a déjà répondu à cette question
- Utiliser la table `player_answers` pour vérifier l'existence d'une réponse
- Rejeter les réponses en double avec un message d'erreur approprié

**Référence protocole** : Section "Gestion de la triche" - "Vérifier que le joueur ne répond qu'une seule fois par question"

---

### 5. Gestion Intelligente des Timers
**Statut** : ⚠️ Implémenté mais incomplet

**Problème** : Le serveur attend toujours le timeout complet (`sleep(time_limit)`) même si tous les joueurs ont répondu.

**À faire** :
- Créer un mécanisme pour vérifier si tous les joueurs ont répondu
- Dès que tous ont répondu, envoyer immédiatement les résultats (au lieu d'attendre le timeout)
- Sinon, attendre le timeout maximum
- Le serveur fait autorité sur le temps (comme spécifié dans le protocole)

**Référence protocole** : Section "Synchronisation" et "Résultats d'une question"

---

### 6. Pause de 5 Secondes entre les Questions
**Statut** : ❌ Non implémenté

**Problème** : Le protocole spécifie une pause de 5 secondes après l'affichage des résultats, mais le code passe directement à la question suivante.

**À faire** :
- Dans `question_loop_thread()`, après `send_question_results()`, ajouter `sleep(5)`
- Avant d'envoyer la première question, s'assurer que le countdown est terminé

**Référence protocole** : Section "Déroulement d'une session" - étape 3

---

### 7. Vérification du Temps de Réponse Côté Serveur
**Statut** : ⚠️ Partiellement implémenté

**Problème** : Le serveur reçoit le `responseTime` du client mais ne vérifie pas si la réponse est arrivée dans le délai imparti.

**À faire** :
- Stocker l'heure d'envoi de chaque question (timestamp)
- Quand une réponse arrive, calculer le temps écoulé depuis l'envoi de la question
- Comparer avec le `time_limit` de la session
- Rejeter les réponses qui arrivent après le timeout (même si le client prétend avoir répondu à temps)

**Référence protocole** : Section "Gestion de la triche" - "Vérifier que les réponses arrivent dans le délai imparti"

---

## 🟡 Importants (À vérifier/améliorer)

### 8. Gestion Complète des Vies en Mode Battle
**Statut** : ⚠️ À vérifier

**À vérifier** :
- Les vies sont-elles correctement décrémentées quand un joueur répond incorrectement ?
- Les vies sont-elles correctement décrémentées pour le `lastPlayer` ?
- Les joueurs avec 0 vie sont-ils correctement éliminés ?
- Le message `POST session/player/eliminated` est-il envoyé ?
- La partie se termine-t-elle correctement quand il ne reste qu'un joueur avec des vies ?

**Référence protocole** : Section "Gestion des vies en mode Battle"

---

### 9. Questions dans la Base de Données
**Statut** : ⚠️ À vérifier

**À vérifier** :
- Y a-t-il au moins 30 questions dans la base de données ?
- Les questions sont-elles réparties sur au moins 2 thèmes différents ?
- Y a-t-il un équilibre entre les niveaux de difficulté (facile, moyen, difficile) ?
- Les questions couvrent-elles les 3 types (QCM, Vrai/Faux, Texte libre) ?

**Référence protocole** : Section "Les questions"

---

### 10. Correction des Erreurs de Compilation
**Statut** : ⚠️ En cours

**Problèmes identifiés** :
- ✅ Format string pour `get_correct_answer()` : corrigé (`%d` → `%s`)
- ✅ Format string pour `results[i].answer` : corrigé (`%d` → `%s`)
- ✅ Warnings de pointeur pour `append_json_string()` : corrigés (cast `const unsigned char *`)
- ❌ Erreur de linker avec `libsqlite3.a` : l'archive est corrompue ou pour une autre architecture

**Solution pour libsqlite3.a** :
- Utiliser `-lsqlite3` au lieu de `./sqlite/libsqlite3.a` (le système a sqlite3 installé)
- Ou recompiler sqlite3 pour l'architecture arm64 (macOS Apple Silicon)

---

## 🟢 Mineurs (Améliorations optionnelles)

### 11. Gestion des Sessions Terminées
**Statut** : ✅ Implémenté mais à vérifier

**À vérifier** :
- Les sessions terminées sont-elles correctement marquées comme "finished" ?
- Les sessions terminées n'apparaissent-elles plus dans la liste des sessions disponibles ?

---

### 12. Gestion des Erreurs
**Statut** : ⚠️ À améliorer

**À améliorer** :
- Gestion des erreurs de base de données
- Gestion des erreurs réseau (timeout, connexion perdue)
- Messages d'erreur plus explicites pour le client

---

## 📝 Résumé des Actions Prioritaires

1. **Implémenter `handle_joker_use()`** - Fonctionnalité critique manquante
2. **Gérer les déconnexions** - Envoyer les notifications aux autres joueurs
3. **Vérifier les réponses en double** - Sécurité anti-triche
4. **Améliorer la gestion des timers** - Envoyer les résultats dès que tous ont répondu
5. **Ajouter la pause de 5 secondes** - Conformité au protocole
6. **Vérifier le temps de réponse côté serveur** - Sécurité anti-triche
7. **Corriger l'erreur de compilation** - Utiliser `-lsqlite3` au lieu de l'archive statique

---

## 🔍 Points à Tester

Une fois les corrections apportées, tester :

1. ✅ Utilisation des jokers (50/50 et Passer)
2. ✅ Déconnexion d'un joueur pendant une session
3. ✅ Déconnexion du créateur
4. ✅ Tentative de répondre deux fois à la même question
5. ✅ Réponse après le timeout
6. ✅ Envoi des résultats dès que tous ont répondu (avant le timeout)
7. ✅ Mode Battle : gestion des vies et éliminations
8. ✅ Pause de 5 secondes entre les questions
