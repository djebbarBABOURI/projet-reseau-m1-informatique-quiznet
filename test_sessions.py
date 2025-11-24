#!/usr/bin/env python3
# Script de test pour vérifier la réponse du serveur pour sessions/list

import socket
import json

def test_sessions_list():
    """Teste la requête GET sessions/list"""
    try:
        # Connexion au serveur
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(5)
        sock.connect(('127.0.0.1', 8080))
        
        # Envoi de la requête
        request = "GET sessions/list\n\n"
        print(f"Envoi: {repr(request)}")
        sock.sendall(request.encode())
        
        # Réception de la réponse
        response = b""
        while True:
            chunk = sock.recv(4096)
            if not chunk:
                break
            response += chunk
            # Vérifier si on a reçu un JSON complet
            try:
                json_str = response.decode('utf-8')
                if json_str.strip().endswith('}'):
                    break
            except:
                pass
        
        sock.close()
        
        # Afficher la réponse
        print(f"\nRéponse brute reçue ({len(response)} bytes):")
        print(response.decode('utf-8', errors='ignore'))
        
        # Parser le JSON
        try:
            json_str = response.decode('utf-8')
            # Trouver le JSON dans la réponse
            json_start = json_str.find('{')
            if json_start != -1:
                json_text = json_str[json_start:]
                data = json.loads(json_text)
                print(f"\nJSON parsé:")
                print(json.dumps(data, indent=2, ensure_ascii=False))
                
                if data.get("statut") == "200":
                    sessions = data.get("sessions", [])
                    print(f"\n✓ Succès! {len(sessions)} session(s) trouvée(s):")
                    for session in sessions:
                        print(f"  - ID: {session.get('id')}, Nom: {session.get('name')}, Statut: {session.get('status')}")
                else:
                    print(f"\n✗ Erreur: statut={data.get('statut')}, message={data.get('message')}")
            else:
                print("\n✗ Aucun JSON trouvé dans la réponse")
        except json.JSONDecodeError as e:
            print(f"\n✗ Erreur de parsing JSON: {e}")
            print(f"Texte reçu: {json_str[:500]}")
            
    except Exception as e:
        print(f"✗ Erreur: {e}")
        import traceback
        traceback.print_exc()

if __name__ == "__main__":
    print("Test de la requête GET sessions/list\n")
    test_sessions_list()

