#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Script de vérification de Tkinter avant lancement de l'interface graphique
"""

import sys
import subprocess

def check_tkinter():
    """Vérifie si Tkinter peut être importé"""
    try:
        # Essayer d'importer tkinter dans un sous-processus pour éviter le crash
        result = subprocess.run(
            [sys.executable, "-c", "import tkinter; print('OK')"],
            capture_output=True,
            timeout=5
        )
        if result.returncode == 0:
            return True, None
        else:
            error_msg = result.stderr.decode('utf-8', errors='ignore')
            return False, error_msg
    except subprocess.TimeoutExpired:
        return False, "Timeout lors de la vérification"
    except Exception as e:
        return False, str(e)

if __name__ == "__main__":
    print("🔍 Vérification de Tkinter...")
    works, error = check_tkinter()
    
    if works:
        print("✅ Tkinter est disponible")
        sys.exit(0)
    else:
        print("❌ Tkinter n'est pas disponible ou incompatible")
        if error:
            print(f"Erreur: {error}")
        print("\n" + "="*60)
        print("💡 Solutions :")
        print("\n1. Utiliser la version terminal (recommandé) :")
        print("   python3 quiznet_client.py")
        print("\n2. Installer Python avec Tkinter via Homebrew :")
        print("   brew install python-tk")
        print("   /opt/homebrew/bin/python3 quiznet_client_gui.py")
        print("\n3. Utiliser le Python système :")
        print("   /usr/bin/python3 quiznet_client_gui.py")
        print("="*60)
        sys.exit(1)

