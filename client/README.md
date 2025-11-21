# QuizNet Client - Guide de lancement

## 🚀 Lancement rapide

### Version Terminal (Recommandée - Fonctionne toujours)

```bash
python3 quiznet_client.py
```

Cette version fonctionne sans interface graphique et offre toutes les fonctionnalités.

### Version Interface Graphique

Si vous avez un problème de compatibilité Tkinter sur macOS, essayez :

```bash
# Option 1 : Utiliser le script de lancement intelligent
python3 launch_gui.py

# Option 2 : Installer Python avec Tkinter via Homebrew
brew install python-tk
/opt/homebrew/bin/python3 quiznet_client_gui.py

# Option 3 : Utiliser le Python système
/usr/bin/python3 quiznet_client_gui.py
```

## ❌ Problème : "macOS 14 (1408) or later required"

Cette erreur indique que votre version de Python/Tkinter n'est pas compatible avec votre macOS.

### Solutions :

1. **Utiliser la version terminal** (solution immédiate) :
   ```bash
   python3 quiznet_client.py
   ```

2. **Installer Python via Homebrew** :
   ```bash
   brew install python-tk
   /opt/homebrew/bin/python3 quiznet_client_gui.py
   ```

3. **Utiliser pyenv** :
   ```bash
   pyenv install 3.11.0
   pyenv local 3.11.0
   python3 quiznet_client_gui.py
   ```

## 📋 Prérequis

- Python 3.7 ou supérieur
- Pour l'interface graphique : Tkinter (généralement inclus avec Python)
- Un serveur QuizNet en cours d'exécution

## 🔗 Utilisation

1. Assurez-vous que le serveur QuizNet est lancé
2. Lancez le client (terminal ou graphique)
3. Le client recherchera automatiquement les serveurs disponibles
4. Connectez-vous ou créez un compte
5. Créez ou rejoignez une session de quiz

