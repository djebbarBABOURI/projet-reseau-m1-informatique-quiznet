# 🚀 Test Rapide des Jokers

## Test en 5 minutes

### 1. Préparation (1 minute)
```bash
# Terminal 1 - Serveur
cd server
./main 8080

# Terminal 2 - Client 1
cd client
python3 quiznet_client_gui.py

# Terminal 3 - Client 2 (optionnel, pour tester avec 2 joueurs)
cd client
python3 quiznet_client_gui.py
```

### 2. Test rapide (3 minutes)

#### Étape A : Vérification initiale
1. Connectez-vous au serveur
2. Créez une session avec au moins 1 question QCM
3. **Vérifiez** : Les boutons affichent "🎯 50/50 (1)" et "⏭️ Passer (1)"

#### Étape B : Test 50/50
1. Démarrez la session
2. Quand une question QCM apparaît :
   - **AVANT de répondre**, cliquez sur "🎯 50/50 (1)"
   - **Vérifiez** : Seulement 2 réponses restent visibles
   - **Vérifiez** : Le bouton affiche maintenant "🎯 50/50 (0)"
   - Répondez avec une des 2 réponses restantes

#### Étape C : Test Passer
1. À la question suivante :
   - **AVANT de répondre**, cliquez sur "⏭️ Passer (1)"
   - **Vérifiez** : Message "Question passée !"
   - **Vérifiez** : Le bouton affiche "⏭️ Passer (0)"
   - **Vérifiez** : La question suivante apparaît

#### Étape D : Vérification restriction
1. À la question suivante :
   - Répondez d'abord (cliquez "Envoyer la réponse")
   - **Essayez** de cliquer sur un joker
   - **Vérifiez** : Les boutons sont grisés (désactivés)

### 3. Résultat attendu ✅

Si tout fonctionne :
- ✅ Jokers initialisés à 1 chacun
- ✅ 50/50 élimine 2 réponses sur une QCM
- ✅ Passer saute la question
- ✅ Jokers mis à jour en temps réel
- ✅ Jokers désactivés après réponse

---

## Test avec logs de débogage

Pour voir les messages de débogage, le code affiche déjà des messages dans la console :
- `DEBUG handle_push_message_gui: action=joker/use`
- Messages sur les jokers restants

---

## Points critiques à vérifier

1. **Le joker 50/50 ne fonctionne QUE sur les questions QCM**
   - Sur Vrai/Faux ou Texte libre, le bouton doit être grisé

2. **Les jokers ne peuvent être utilisés qu'AVANT de répondre**
   - Après avoir cliqué "Envoyer la réponse", les jokers doivent être désactivés

3. **Les jokers sont mis à jour immédiatement**
   - Dès que vous utilisez un joker, le nombre doit changer (1 → 0)

4. **Le 50/50 affiche seulement les réponses restantes**
   - Sur une QCM avec 4 réponses, après 50/50, seulement 2 doivent être visibles

---

**Si tous ces tests passent, les jokers fonctionnent correctement ! 🎉**

