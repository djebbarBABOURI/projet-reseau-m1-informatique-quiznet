#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Script de lancement intelligent pour l'interface graphique QuizNet
Détecte les problèmes de compatibilité Tkinter et propose des solutions
"""

import sys
import subprocess
import os

def check_tkinter_safe():
    """Vérifie si Tkinter peut être importé de manière sécurisée"""
    try:
        # Essayer d'importer tkinter dans un sous-processus pour éviter le crash
        result = subprocess.run(
            [sys.executable, "-c", "import tkinter; print('OK')"],
            capture_output=True,
            timeout=5,
            stderr=subprocess.PIPE
        )
        
        if result.returncode == 0:
            return True, None
        else:
            error_msg = result.stderr.decode('utf-8', errors='ignore')
            if "macOS" in error_msg or "abort" in error_msg.lower():
                return False, "incompatible_macos"
            return False, error_msg
    except subprocess.TimeoutExpired:
        return False, "timeout"
    except Exception as e:
        return False, str(e)

def print_solutions():
    """Affiche les solutions au problème Tkinter"""
    print("="*60)
    print("❌ Tkinter n'est pas compatible avec votre version de macOS")
    print("="*60)
    print("\n🔧 Solutions possibles :")
    print("\n1. ✅ Utiliser la version terminal (recommandé, fonctionne immédiatement) :")
    print("   python3 quiznet_client.py")
    print("\n2. Installer Python avec Tkinter via Homebrew :")
    print("   brew install python-tk")
    print("   /opt/homebrew/bin/python3 quiznet_client_gui.py")
    print("\n3. Utiliser le Python système :")
    print("   /usr/bin/python3 quiznet_client_gui.py")
    print("\n4. Installer via pyenv :")
    print("   pyenv install 3.11.0")
    print("   pyenv local 3.11.0")
    print("   python3 quiznet_client_gui.py")
    print("="*60)

def main():
    """Fonction principale"""
    print("🔍 Vérification de la compatibilité Tkinter...")
    
    works, error = check_tkinter_safe()
    
    if not works:
        print_solutions()
        print("\n💡 Pour l'instant, utilisez la version terminal :")
        print("   python3 quiznet_client.py")
        sys.exit(1)
    
    # Si Tkinter fonctionne, lancer l'interface graphique
    print("✅ Tkinter est disponible, lancement de l'interface graphique...\n")
    
    # Importer et lancer l'interface graphique
    try:
        from quiznet_client_gui import main as gui_main
        gui_main()
    except Exception as e:
        print(f"❌ Erreur lors du lancement de l'interface : {e}")
        print("\n💡 Essayez la version terminal :")
        print("   python3 quiznet_client.py")
        sys.exit(1)

if __name__ == "__main__":
    main()

