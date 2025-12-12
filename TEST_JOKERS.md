# 🎲 Guide de Test des Jokers

Ce guide vous permet de vérifier que les jokers fonctionnent correctement dans le client QuizNet.

## Prérequis

1. **Serveur lancé** : Assurez-vous que le serveur QuizNet est lancé
   ```bash
   cd server
   ./main 8080
   ```

2. **Client lancé** : Lancez le client GUI
   ```bash
   cd client
   python3 quiznet_client_gui.py
   ```

## Tests à effectuer

### ✅ Test 1 : Vérification de l'initialisation des jokers

**Étapes :**
1. Connectez-vous au serveur
2. Créez une nouvelle session ou rejoignez une session existante
3. Allez dans la salle d'attente

**Résultat attendu :**
- Dans la salle d'attente, vous devriez voir : "🎲 Jokers disponibles: 50/50 (1), Passer (1)"
- Les boutons de jokers dans la page de jeu doivent afficher :
  - "🎯 50/50 (1)"
  - "⏭️ Passer (1)"

---

### ✅ Test 2 : Test du joker 50/50

**Étapes :**
1. Démarrez une session avec au moins 2 joueurs
2. Attendez qu'une question QCM apparaisse
3. **AVANT de répondre**, cliquez sur le bouton "🎯 50/50 (1)"

**Résultats attendus :**
- Le bouton 50/50 se désactive immédiatement
- Le texte du bouton change pour "🎯 50/50 (0)"
- Seulement 2 réponses sur 4 restent visibles (les 2 mauvaises réponses sont éliminées)
- Vous pouvez toujours répondre avec l'une des 2 réponses restantes
- Après avoir répondu, le joker 50/50 reste à 0

**Vérifications supplémentaires :**
- Essayez d'utiliser le joker 50/50 sur une question Vrai/Faux ou Texte libre
  - Le bouton doit être désactivé (grisé)
- Essayez d'utiliser le joker 50/50 après avoir répondu
  - Le bouton doit être désactivé

---

### ✅ Test 3 : Test du joker Passer

**Étapes :**
1. Démarrez une session avec au moins 2 joueurs
2. Attendez qu'une question apparaisse (peu importe le type)
3. **AVANT de répondre**, cliquez sur le bouton "⏭️ Passer (1)"

**Résultats attendus :**
- Une boîte de dialogue apparaît : "Question passée !"
- Le bouton Passer se désactive
- Le texte du bouton change pour "⏭️ Passer (0)"
- Le bouton "Envoyer la réponse" se désactive
- La question disparaît et vous attendez la suivante
- En mode Battle, vous ne perdez pas de vie

**Vérifications supplémentaires :**
- Essayez d'utiliser le joker Passer après avoir répondu
  - Le bouton doit être désactivé
  - Un message d'avertissement peut apparaître

---

### ✅ Test 4 : Vérification que les jokers ne peuvent pas être utilisés après avoir répondu

**Étapes :**
1. Démarrez une session
2. Attendez qu'une question apparaisse
3. Répondez à la question (cliquez sur "Envoyer la réponse")
4. Essayez d'utiliser un joker

**Résultat attendu :**
- Les deux boutons de jokers sont désactivés (grisés)
- Si vous essayez de cliquer, rien ne se passe
- Un message d'avertissement peut apparaître : "Vous ne pouvez utiliser un joker qu'avant de répondre"

---

### ✅ Test 5 : Vérification de la mise à jour des jokers restants

**Étapes :**
1. Utilisez le joker 50/50 sur une question QCM
2. Observez l'affichage des jokers

**Résultat attendu :**
- Le bouton 50/50 affiche maintenant "🎯 50/50 (0)"
- Le bouton Passer affiche toujours "⏭️ Passer (1)"
- Les jokers sont mis à jour en temps réel

**Étapes supplémentaires :**
1. Utilisez le joker Passer sur une question
2. Observez l'affichage des jokers

**Résultat attendu :**
- Le bouton Passer affiche maintenant "⏭️ Passer (0)"
- Le bouton 50/50 affiche toujours "🎯 50/50 (0)" (si déjà utilisé)

---

### ✅ Test 6 : Test avec plusieurs questions

**Étapes :**
1. Démarrez une session avec plusieurs questions
2. Utilisez le joker 50/50 sur la première question QCM
3. Répondez à la question
4. Attendez la question suivante
5. Vérifiez les jokers disponibles

**Résultat attendu :**
- Le joker 50/50 reste à 0 (déjà utilisé)
- Le joker Passer est toujours disponible (1) si pas encore utilisé
- Vous pouvez utiliser le joker Passer sur la nouvelle question

---

### ✅ Test 7 : Test en mode Battle

**Étapes :**
1. Créez une session en mode Battle
2. Utilisez le joker Passer sur une question
3. Vérifiez que vous n'avez pas perdu de vie

**Résultat attendu :**
- Vous ne perdez pas de vie en utilisant le joker Passer
- Le joker Passer est bien décompté

---

## Checklist de vérification

- [ ] Les jokers s'initialisent correctement (1 de chaque au début)
- [ ] Le joker 50/50 fonctionne uniquement sur les questions QCM
- [ ] Le joker 50/50 élimine bien 2 mauvaises réponses
- [ ] Seules les 2 réponses restantes sont affichées après utilisation du 50/50
- [ ] Le joker Passer fonctionne sur tous les types de questions
- [ ] Le joker Passer saute la question sans pénalité
- [ ] Les jokers ne peuvent pas être utilisés après avoir répondu
- [ ] Les jokers restants sont mis à jour en temps réel
- [ ] L'affichage des boutons montre le nombre de jokers restants
- [ ] Les jokers ne peuvent être utilisés qu'une seule fois par session

## Problèmes courants et solutions

### ❌ Les jokers ne s'affichent pas
- Vérifiez que vous avez bien rejoint ou créé une session
- Vérifiez que le serveur renvoie bien les jokers dans la réponse `session/create` ou `session/join`

### ❌ Le joker 50/50 n'élimine pas les bonnes réponses
- Vérifiez que le serveur envoie bien `remainingAnswers` dans la réponse `joker/use`
- Vérifiez que le format de `remainingAnswers` est correct (liste d'indices ou de réponses)

### ❌ Les jokers peuvent être utilisés après avoir répondu
- Vérifiez que la variable `self.answered` est bien mise à `True` après l'envoi de la réponse
- Vérifiez que les boutons sont bien désactivés dans `set_answered()`

### ❌ Les jokers ne se mettent pas à jour
- Vérifiez que le message `joker/use` du serveur contient bien le champ `jokers` avec les valeurs mises à jour
- Vérifiez que `update_jokers_display()` est appelé après réception du message

## Test avec le client terminal

Si vous voulez aussi tester avec le client terminal :

```bash
cd client
python3 quiznet_client.py
```

Dans le client terminal :
- Tapez `5` pour utiliser le joker 50/50
- Tapez `P` pour utiliser le joker Passer
- Les jokers disponibles sont affichés avant chaque question

---

**Bon test ! 🎮**

