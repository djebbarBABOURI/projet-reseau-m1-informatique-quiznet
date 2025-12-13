#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Client QuizNet - Interface Python pour le jeu de quiz multijoueur
"""

import socket
import json
import threading
import time
import sys
import os
import select
from typing import Optional, Dict, List, Any
from datetime import datetime
from queue import Queue
from tkinter import messagebox

# Configuration
UDP_PORT = 5555
DISCOVERY_TIMEOUT = 2
BUFFER_SIZE = 8192


class QuizNetClient:
    """Client QuizNet pour se connecter au serveur et jouer"""
    
    def __init__(self):
        self.tcp_socket: Optional[socket.socket] = None
        self.server_addr: Optional[tuple] = None
        self.server_name: Optional[str] = None
        self.pseudo: Optional[str] = None
        self.connected = False
        self.listener_thread: Optional[threading.Thread] = None
        self.stop_listener = False
        self.current_session_id: Optional[int] = None
        self.is_creator = False
        self.lives = 4
        self.jokers = {"fifty": 1, "skip": 1}
        self.current_question: Optional[Dict] = None
        self.question_start_time: Optional[float] = None
        self.themes: List[Dict] = []
        self.sessions: List[Dict] = []
        self.response_queue = Queue()
        self.push_messages = Queue()
        self.socket_lock = threading.Lock()
        self.waiting_for_response = False
        self.pending_response = None
        self.in_game = False
        self.time_limit = 30
        self.push_callback = None  # Callback pour les messages push (utilisé par GUI)
        self.last_error: Optional[Dict] = None  # Dernière erreur rencontrée
        
    def discover_servers(self) -> List[Dict[str, Any]]:
        """Découvre les serveurs QuizNet disponibles via UDP broadcast"""
        print("\n🔍 Recherche de serveurs QuizNet...")
        
        servers = []
        udp_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        udp_sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        udp_sock.bind(("", 0))
        udp_sock.settimeout(DISCOVERY_TIMEOUT)
        
        try:
            # Essayer d'abord le broadcast local
            try:
                udp_sock.sendto(b"looking for quiznet servers", ('255.255.255.255', UDP_PORT))
            except (OSError, socket.error) as e:
                # Si le broadcast échoue, essayer localhost
                try:
                    udp_sock.sendto(b"looking for quiznet servers", ('127.0.0.1', UDP_PORT))
                    print("  ℹ️ Broadcast UDP non disponible, recherche sur localhost uniquement")
                except:
                    print(f"  ⚠️ Impossible d'envoyer le broadcast UDP: {e}")
                    print("  💡 Assurez-vous que le serveur est lancé sur cette machine")
            
            start_time = time.time()
            while time.time() - start_time < DISCOVERY_TIMEOUT:
                try:
                    data, addr = udp_sock.recvfrom(1024)
                    message = data.decode().strip()
                    if message.startswith("hello i'm a quiznet server:"):
                        parts = message.split(":")
                        if len(parts) >= 3:
                            server_name = parts[1]
                            tcp_port = int(parts[2])
                            servers.append({
                                "name": server_name,
                                "address": addr[0],
                                "port": tcp_port
                            })
                            print(f"  ✓ Serveur trouvé: {server_name} ({addr[0]}:{tcp_port})")
                except socket.timeout:
                    break
                except Exception as e:
                    continue
        except Exception as e:
            # Ne pas afficher l'erreur si c'est juste qu'il n'y a pas de serveur
            if "No route to host" not in str(e) and "Network is unreachable" not in str(e):
                print(f"  ⚠️ Erreur lors de la découverte: {e}")
        finally:
            udp_sock.close()
        
        if not servers:
            print("  ⚠️ Aucun serveur trouvé")
            print("  💡 Vérifiez que:")
            print("     - Le serveur QuizNet est lancé")
            print("     - Le serveur écoute sur le port UDP 5555")
            print("     - Vous êtes sur le même réseau (ou utilisez localhost)")
        else:
            print(f"  ✓ {len(servers)} serveur(s) trouvé(s)\n")
        
        return servers
    
    def connect_to_server(self, address: str, port: int) -> bool:
        """Se connecte au serveur TCP"""
        try:
            self.tcp_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.tcp_socket.settimeout(5)
            self.tcp_socket.connect((address, port))
            self.server_addr = (address, port)
            self.connected = True
            
            # Démarrer le thread d'écoute
            self.stop_listener = False
            self.listener_thread = threading.Thread(target=self._listen_server, daemon=True)
            self.listener_thread.start()
            
            print(f"✓ Connecté au serveur {address}:{port}")
            return True
        except Exception as e:
            print(f"❌ Erreur de connexion: {e}")
            self.connected = False
            return False
    
    def disconnect(self):
        """Déconnecte du serveur"""
        self.stop_listener = True
        self.connected = False
        if self.tcp_socket:
            try:
                self.tcp_socket.close()
            except:
                pass
            self.tcp_socket = None
    
    def _send_request(self, method: str, path: str, data: Optional[Dict] = None) -> Optional[Dict]:
        """Envoie une requête au serveur et retourne la réponse"""
        if not self.connected or not self.tcp_socket:
            print("❌ Non connecté au serveur")
            return None
        
        with self.socket_lock:
            try:
                # Construction de la requête
                request = f"{method} {path}\n"
                if data:
                    request += json.dumps(data)
                request += "\n"
                
                # Envoi
                self.tcp_socket.sendall(request.encode())
                
                # Attendre la réponse (avec timeout)
                try:
                    response = self.response_queue.get(timeout=10)
                    return response
                except:
                    return None
                    
            except Exception as e:
                print(f"❌ Erreur lors de l'envoi de la requête: {e}")
                return None
    
    def _listen_server(self):
        """Thread d'écoute des messages du serveur"""
        buffer = ""
        while self.connected and not self.stop_listener:
            try:
                if not self.tcp_socket:
                    break
                
                # Configurer le timeout pour permettre l'interruption
                self.tcp_socket.settimeout(0.5)
                
                try:
                    chunk = self.tcp_socket.recv(BUFFER_SIZE).decode('utf-8', errors='ignore')
                    if not chunk:
                        break
                    buffer += chunk
                except socket.timeout:
                    continue
                except OSError:
                    break
                
                # Traiter tous les messages complets dans le buffer
                while buffer:
                    message_processed = False
                    
                    # Chercher un JSON complet
                    json_start = buffer.find("{")
                    if json_start != -1:
                        # Trouver la fin du JSON en comptant les accolades
                        brace_count = 0
                        json_end = -1
                        in_string = False
                        escape = False
                        
                        for i in range(json_start, len(buffer)):
                            char = buffer[i]
                            
                            if escape:
                                escape = False
                                continue
                            
                            if char == '\\':
                                escape = True
                                continue
                            
                            if char == '"' and not escape:
                                in_string = not in_string
                                continue
                            
                            if not in_string:
                                if char == '{':
                                    brace_count += 1
                                elif char == '}':
                                    brace_count -= 1
                                    if brace_count == 0:
                                        json_end = i + 1
                                        break
                        
                        if json_end != -1:
                            # Extraire le message (peut inclure "POST path\n" avant le JSON)
                            message = buffer[:json_end].strip()
                            # Avancer le buffer après le JSON et le \n suivant s'il existe
                            if json_end < len(buffer) and buffer[json_end] == '\n':
                                buffer = buffer[json_end + 1:].lstrip()
                            else:
                                buffer = buffer[json_end:].lstrip()
                            
                            # Debug: afficher tous les messages reçus
                            if message.startswith("POST ") or message.startswith("GET "):
                                print(f"DEBUG _listen_server: Message push reçu: {message[:150]}")
                            
                            self._process_message(message)
                            message_processed = True
                    
                    if not message_processed:
                        # Pas de message complet, attendre plus de données
                        break
                    
            except Exception as e:
                if self.connected and not self.stop_listener:
                    pass
                break
        
        self.connected = False
    
    def _process_message(self, message: str):
        """Traite un message reçu du serveur"""
        if not message:
            return
        
        try:
            # Debug: afficher le message reçu
            if message.startswith("POST question") or message.startswith("POST session"):
                print(f"DEBUG _process_message: Message reçu: {message[:200]}")
            
            # Extraire le JSON du message
            json_start = message.find("{")
            if json_start == -1:
                print(f"DEBUG _process_message: Pas de JSON trouvé dans: {message[:100]}")
                return
            
            json_text = message[json_start:]
            data = json.loads(json_text)
            print(f"DEBUG _process_message: JSON parsé, action={data.get('action', 'N/A')}")
            
            # Liste des actions qui sont des messages push (événements du serveur)
            push_actions = [
                "session/player/joined", "session/started", "question/new",
                "question/results", "session/finished", "session/player/left",
                "session/player/eliminated", "joker/use"
            ]
            
            action = data.get("action", "")
            statut = data.get("statut", "")
            
            # Si le message commence par "POST " ou "GET ", c'est un message push du serveur
            if message.startswith("POST ") or message.startswith("GET "):
                self._handle_push_message("", data)
            # Sinon, vérifier si c'est un événement push ou une réponse à une requête
            elif action in push_actions:
                # C'est un message push (événement)
                self._handle_push_message("", data)
            elif statut or action:
                # C'est une réponse à une requête (a un statut ou une action correspondant à une requête)
                # Les réponses aux requêtes ont des actions comme "player/register", "themes/list", etc.
                # qui ne sont pas dans la liste des push_actions
                self.response_queue.put(data)
            else:
                # Message inconnu, traiter comme réponse par défaut
                self.response_queue.put(data)
                    
        except json.JSONDecodeError as e:
            # JSON invalide, ignorer
            pass
        except Exception:
            pass
    
    def _handle_push_message(self, header: str, data: Dict):
        """Gère les messages push du serveur"""
        try:
            action = data.get("action", "")
            print(f"DEBUG _handle_push_message: action={action}, has_callback={self.push_callback is not None}")
            
            # Si un callback est défini (pour GUI), l'appeler
            if self.push_callback:
                print(f"DEBUG _handle_push_message: Appel du callback GUI pour action={action}")
                self.push_callback(header, data)
                return
            
            # Sinon, utiliser le comportement par défaut (terminal)
            action = data.get("action", "")
            
            if action == "session/player/joined":
                pseudo = data.get("pseudo", "")
                nb_players = data.get("nbPlayers", 0)
                if not self.in_game:
                    print(f"\n🎮 {pseudo} a rejoint la session ({nb_players} joueur(s))")
            
            elif action == "session/started":
                print(f"DEBUG _handle_push_message: session/started reçu, countdown={data.get('countdown', 3)}")
                self.in_game = True
                countdown = data.get("countdown", 3)
                print(f"\n🚀 La session démarre dans {countdown} secondes...")
                for i in range(countdown, 0, -1):
                    print(f"   {i}...")
                    time.sleep(1)
                print("\n🎯 La session a commencé !\n")
            
            elif action == "question/new":
                self.current_question = data
                self.question_start_time = time.time()
                self.time_limit = data.get("timeLimit", 30)  # Récupérer depuis la session si disponible
                self._display_question(data)
            
            elif action == "question/results":
                self._display_results(data)
                self.current_question = None
            
            elif action == "session/finished":
                self._display_final_results(data)
                self.current_session_id = None
                self.in_game = False
            
            elif action == "session/player/left":
                pseudo = data.get("pseudo", "")
                reason = data.get("reason", "disconnected")
                if self.in_game:
                    print(f"\n⚠️ {pseudo} a quitté la session ({reason})")
            
            elif action == "session/player/eliminated":
                pseudo = data.get("pseudo", "")
                if self.in_game:
                    print(f"\n💀 {pseudo} a été éliminé !")
            
            elif action == "joker/use":
                self.jokers = data.get("jokers", self.jokers)
                if "remainingAnswers" in data:
                    # Mettre à jour la question avec les réponses restantes
                    if self.current_question:
                        self.current_question["remainingAnswers"] = data["remainingAnswers"]
                    # Réafficher la question avec les réponses restantes
                    if self.current_question:
                        self._display_question(self.current_question)
                else:
                    print(f"\n🎲 Question passée !")
                    self.current_question = None
        except Exception as e:
            pass  # Ignorer les erreurs de parsing dans le listener
    
    # === Authentification ===
    
    def register(self, pseudo: str, password: str) -> bool:
        """Inscrit un nouveau joueur"""
        response = self._send_request("POST", "player/register", {
            "pseudo": pseudo,
            "password": password
        })
        
        if response and response.get("statut") == "201":
            print(f"✓ Compte créé avec succès pour {pseudo}")
            return True
        else:
            message = response.get("message", "Erreur inconnue") if response else "Pas de réponse"
            print(f"❌ Erreur d'inscription: {message}")
            return False
    
    def login(self, pseudo: str, password: str) -> bool:
        """Connecte un joueur"""
        response = self._send_request("POST", "player/login", {
            "pseudo": pseudo,
            "password": password
        })
        
        if response and response.get("statut") == "200":
            self.pseudo = pseudo
            print(f"✓ Connecté en tant que {pseudo}")
            return True
        else:
            message = response.get("message", "Erreur inconnue") if response else "Pas de réponse"
            print(f"❌ Erreur de connexion: {message}")
            return False
    
    # === Thèmes et Sessions ===
    
    def get_themes(self) -> List[Dict]:
        """Récupère la liste des thèmes"""
        response = self._send_request("GET", "themes/list")
        
        if response and response.get("statut") == "200":
            self.themes = response.get("themes", [])
            return self.themes
        return []
    
    def get_sessions(self) -> List[Dict]:
        """Récupère la liste des sessions"""
        response = self._send_request("GET", "sessions/list")
        
        if response and response.get("statut") == "200":
            self.sessions = response.get("sessions", [])
            return self.sessions
        return []
    
    def create_session(self, name: str, theme_ids: List[int], difficulty: str, 
                      nb_questions: int, time_limit: int, mode: str, max_players: int) -> Optional[int]:
        """Crée une nouvelle session"""
        if not self.pseudo:
            print("❌ Erreur: Vous devez être connecté pour créer une session")
            return None
        
        response = self._send_request("POST", "session/create", {
            "name": name,
            "themeIds": theme_ids,
            "difficulty": difficulty,
            "nbQuestions": nb_questions,
            "timeLimit": time_limit,
            "mode": mode,
            "maxPlayers": max_players,
            "pseudo": self.pseudo  # Ajouter le pseudo pour identifier le créateur
        })
        
        if response and response.get("statut") == "201":
            self.current_session_id = response.get("sessionId")
            self.is_creator = response.get("isCreator", False)
            self.time_limit = time_limit  # Stocker le temps limite
            if "lives" in response:
                self.lives = response["lives"]
            if "jokers" in response:
                self.jokers = response["jokers"]
            print(f"✓ Session créée (ID: {self.current_session_id})")
            return self.current_session_id
        else:
            # Afficher plus de détails sur l'erreur
            if response:
                statut = response.get("statut", "inconnu")
                message = response.get("message", "Erreur inconnue")
                action = response.get("action", "session/create")
                print(f"❌ Erreur de création de session:")
                print(f"   Statut: {statut}")
                print(f"   Action: {action}")
                print(f"   Message: {message}")
                # Stocker le message d'erreur pour l'interface graphique
                self.last_error = {
                    "statut": statut,
                    "message": message,
                    "action": action
                }
            else:
                print(f"❌ Erreur de création: Pas de réponse du serveur")
                print(f"   Vérifiez que le serveur est bien lancé et accessible")
                self.last_error = {
                    "statut": "timeout",
                    "message": "Pas de réponse du serveur",
                    "action": "session/create"
                }
            return None
    # def join_session(self, session_id: int) -> bool:
    # """Envoie une requête pour rejoindre une session existante"""
    # response = self._send_request("POST", "session/join", {
    #     "sessionId": session_id
    # })

    # if response and response.get("statut") == "200":
    #     print(f"✓ Rejoint la session {session_id} avec succès")
    #     return True
    # else:
    #     message = response.get("message", "Erreur inconnue") if response else "Pas de réponse"
    #     print(f"❌ Erreur en rejoignant la session {session_id}: {message}")
    #     return False

    def join_session(self, session_id: int) -> bool:
        """Rejoint une session existante"""
        response = self._send_request("POST", "session/join", {
            "sessionId": session_id
        })
        
        if response and response.get("statut") == "201":
            self.current_session_id = session_id
            self.is_creator = response.get("isCreator", False)
            # Récupérer le time_limit depuis les sessions disponibles
            for session in self.sessions:
                if session.get("id") == session_id:
                    self.time_limit = session.get("timeLimit", 30)
                    break
            if "lives" in response:
                self.lives = response["lives"]
            if "jokers" in response:
                self.jokers = response["jokers"]
            print(f"✓ Rejoint la session {session_id}")
            return True
        else:
            if response and response.get("statut") == "403":
                message = response.get("message", "Accès interdit à cette session")
                messagebox.showerror("Erreur", message)
            else:
                message = response.get("message", "Erreur inconnue") if response else "Pas de réponse"
                messagebox.showerror("Erreur de connexion", message)

            return False

    
    def start_session(self) -> bool:
        """Démarre la session (créateur uniquement)"""
        response = self._send_request("POST", "session/start")
        return response is not None
    
    # === Jeu ===
    
    def use_joker(self, joker_type: str) -> bool:
        """Utilise un joker"""
        response = self._send_request("POST", "joker/use", {
            "type": joker_type
        })
        
        if response and response.get("statut") == "200":
            if "jokers" in response:
                self.jokers = response["jokers"]
            return True
        return False
    
    def answer_question(self, answer: Any) -> bool:
        """Envoie une réponse à la question actuelle"""
        if not self.current_question or not self.question_start_time:
            return False
        
        response_time = time.time() - self.question_start_time
        
        response = self._send_request("POST", "question/answer", {
            "answer": answer,
            "responseTime": round(response_time, 2)
        })
        
        if response and response.get("statut") == "200":
            print("✓ Réponse envoyée")
            return True
        return False
    
    def _display_question(self, question_data: Dict):
        """Affiche une question"""
        os.system('clear' if os.name != 'nt' else 'cls')
        print("\n" + "="*60)
        print(f"Question {question_data.get('questionNum', 0)}/{question_data.get('totalQuestions', 0)}")
        print("="*60)
        print(f"\n📝 {question_data.get('question', '')}")
        print(f"📊 Difficulté: {question_data.get('difficulty', '')}")
        print(f"⏱️  Temps limite: {self._get_time_limit()} secondes")
        
        q_type = question_data.get("type", "")
        if q_type == "qcm":
            answers = question_data.get("answers", [])
            # Vérifier si certaines réponses ont été éliminées (joker 50/50)
            remaining = question_data.get("remainingAnswers", None)
            if remaining:
                # Afficher uniquement les réponses restantes
                print("\nRéponses possibles (après joker 50/50):")
                for i, ans in enumerate(answers):
                    if str(i) in remaining or ans in remaining:
                        print(f"  {i}. {ans}")
            else:
                print("\nRéponses possibles:")
                for i, ans in enumerate(answers):
                    print(f"  {i}. {ans}")
        
        elif q_type == "boolean":
            print("\nRéponses possibles:")
            print("  0. Faux")
            print("  1. Vrai")
        
        elif q_type == "text":
            print("\nRéponse attendue: Texte libre")
    
    def _display_results(self, results_data: Dict):
        """Affiche les résultats d'une question"""
        print("\n" + "="*60)
        print("📊 RÉSULTATS")
        print("="*60)
        print(f"\n✅ Bonne réponse: {results_data.get('correctAnswer', '')}")
        if "explanation" in results_data:
            print(f"💡 Explication: {results_data.get('explanation', '')}")
        
        results = results_data.get("results", [])
        print("\n📈 Classement temporaire:")
        print("-"*60)
        
        # Trier par score décroissant
        sorted_results = sorted(results, key=lambda x: x.get("totalScore", 0), reverse=True)
        
        for i, result in enumerate(sorted_results, 1):
            pseudo = result.get("pseudo", "")
            score = result.get("totalScore", 0)
            points = result.get("points", 0)
            correct = "✓" if result.get("correct", False) else "✗"
            
            if "lives" in result:
                lives = result.get("lives", 0)
                print(f"{i}. {pseudo} - Score: {score} (+{points}) - Vies: {lives} {correct}")
            else:
                print(f"{i}. {pseudo} - Score: {score} (+{points}) {correct}")
        
        if "lastPlayer" in results_data:
            print(f"\n⚠️ Dernier joueur: {results_data.get('lastPlayer')} (perd une vie)")
        
        print("\n" + "-"*60)
        print("⏳ Pause de 5 secondes avant la prochaine question...\n")
        time.sleep(5)
    
    def _display_final_results(self, results_data: Dict):
        """Affiche les résultats finaux"""
        os.system('clear' if os.name != 'nt' else 'cls')
        print("\n" + "="*60)
        print("🎉 FIN DE LA SESSION")
        print("="*60)
        
        mode = results_data.get("mode", "")
        ranking = results_data.get("ranking", [])
        
        if mode == "battle":
            winner = results_data.get("winner", "")
            print(f"\n🏆 VAINQUEUR: {winner}\n")
        
        print("📊 Classement final:")
        print("-"*60)
        
        for result in ranking:
            rank = result.get("rank", 0)
            pseudo = result.get("pseudo", "")
            score = result.get("score", 0)
            correct = result.get("correctAnswers", 0)
            
            if mode == "battle":
                lives = result.get("lives", 0)
                eliminated = result.get("eliminatedAt", None)
                if eliminated:
                    print(f"{rank}. {pseudo} - Vies: {lives} - Score: {score} - Réponses correctes: {correct} (Éliminé Q{eliminated})")
                else:
                    print(f"{rank}. {pseudo} - Vies: {lives} - Score: {score} - Réponses correctes: {correct}")
            else:
                print(f"{rank}. {pseudo} - Score: {score} - Réponses correctes: {correct}")
        
        print("\n" + "="*60 + "\n")
        input("Appuyez sur Entrée pour retourner au menu principal...")
    
    def _get_time_limit(self) -> int:
        """Retourne le temps limite pour la session actuelle"""
        return self.time_limit


def main():
    """Fonction principale"""
    client = QuizNetClient()
    
    print("="*60)
    print("🎮 QUIZNET - Client Python")
    print("="*60)
    
    # Découverte des serveurs
    servers = client.discover_servers()
    
    # Si aucun serveur trouvé, proposer de se connecter à localhost
    if not servers:
        print("\n💡 Aucun serveur trouvé via broadcast UDP")
        print("   Vous pouvez vous connecter directement à un serveur sur localhost")
        use_localhost = input("   Se connecter à localhost:8080 ? (o/n): ").strip().lower()
        
        if use_localhost == 'o' or use_localhost == 'oui' or use_localhost == 'y' or use_localhost == 'yes':
            servers = [{
                "name": "QuizNetServer (localhost)",
                "address": "127.0.0.1",
                "port": 8080
            }]
            print("✓ Connexion à localhost:8080\n")
        else:
            print("❌ Aucun serveur disponible. Arrêt.")
            print("\n💡 Pour lancer le serveur :")
            print("   cd server")
            print("   ./main 8080")
            return
    
    # Sélection du serveur
    print("Serveurs disponibles:")
    for i, server in enumerate(servers, 1):
        print(f"  {i}. {server['name']} ({server['address']}:{server['port']})")
    
    # Si un seul serveur, le sélectionner automatiquement
    if len(servers) == 1:
        selected_server = servers[0]
        print(f"\n✓ Connexion automatique au serveur {selected_server['name']}")
    else:
        while True:
            try:
                choice = input("\nChoisissez un serveur (1-{}): ".format(len(servers)))
                server_idx = int(choice) - 1
                if 0 <= server_idx < len(servers):
                    selected_server = servers[server_idx]
                    break
                else:
                    print("❌ Choix invalide")
            except ValueError:
                print("❌ Veuillez entrer un nombre")
            except KeyboardInterrupt:
                print("\n👋 Au revoir !")
                return
    
    # Connexion au serveur
    if not client.connect_to_server(selected_server["address"], selected_server["port"]):
        print("❌ Impossible de se connecter au serveur")
        return
    
    # Menu principal
    while True:
        try:
            print("\n" + "="*60)
            print("MENU PRINCIPAL")
            print("="*60)
            print("1. S'inscrire")
            print("2. Se connecter")
            print("3. Quitter")
            
            choice = input("\nVotre choix: ").strip()
            
            if choice == "1":
                pseudo = input("Pseudo: ").strip()
                password = input("Mot de passe: ").strip()
                if client.register(pseudo, password):
                    client.login(pseudo, password)
                    break
            
            elif choice == "2":
                pseudo = input("Pseudo: ").strip()
                password = input("Mot de passe: ").strip()
                if client.login(pseudo, password):
                    break
            
            elif choice == "3":
                client.disconnect()
                print("👋 Au revoir !")
                return
            
        except KeyboardInterrupt:
            client.disconnect()
            print("\n👋 Au revoir !")
            return
    
    # Menu de jeu
    while client.connected:
        try:
            print("\n" + "="*60)
            print(f"Bienvenue {client.pseudo} !")
            print("="*60)
            print("1. Voir les thèmes")
            print("2. Voir les sessions")
            print("3. Créer une session")
            print("4. Rejoindre une session")
            print("5. Déconnexion")
            
            choice = input("\nVotre choix: ").strip()
            
            if choice == "1":
                themes = client.get_themes()
                if themes:
                    print("\n📚 Thèmes disponibles:")
                    for theme in themes:
                        print(f"  - {theme.get('name')} (ID: {theme.get('id')})")
                else:
                    print("❌ Aucun thème disponible")
            
            elif choice == "2":
                sessions = client.get_sessions()
                if sessions:
                    print("\n🎮 Sessions disponibles:")
                    for session in sessions:
                        status_emoji = "🟢" if session.get("status") == "waiting" else "🟡" if session.get("status") == "playing" else "🔴"
                        print(f"  {status_emoji} [{session.get('id')}] {session.get('name')} - {session.get('nbPlayers')}/{session.get('maxPlayers')} joueurs - {session.get('mode')}")
                else:
                    print("ℹ️ Aucune session disponible")
            
            elif choice == "3":
                # Créer une session
                themes = client.get_themes()
                if not themes:
                    print("❌ Aucun thème disponible")
                    continue
                
                print("\n📚 Thèmes disponibles:")
                for theme in themes:
                    print(f"  {theme.get('id')}. {theme.get('name')}")
                
                theme_ids_str = input("ID des thèmes (séparés par des virgules): ").strip()
                theme_ids = [int(tid.strip()) for tid in theme_ids_str.split(",")]
                
                print("\nDifficulté:")
                print("  1. Facile")
                print("  2. Moyen")
                print("  3. Difficile")
                diff_choice = input("Choix (1-3): ").strip()
                difficulty_map = {"1": "facile", "2": "moyen", "3": "difficile"}
                difficulty = difficulty_map.get(diff_choice, "moyen")
                
                nb_questions = int(input("Nombre de questions (10-50): ").strip())
                time_limit = int(input("Temps limite par question (10-60 sec): ").strip())
                
                print("\nMode:")
                print("  1. Solo")
                print("  2. Battle")
                mode_choice = input("Choix (1-2): ").strip()
                mode = "solo" if mode_choice == "1" else "battle"
                
                max_players = int(input("Nombre maximum de joueurs: ").strip())
                name = input("Nom de la session: ").strip()
                
                session_id = client.create_session(name, theme_ids, difficulty, nb_questions, time_limit, mode, max_players)
                
                if session_id is not None:
                    # Salle d'attente
                    _waiting_room(client)
            
            elif choice == "4":
                sessions = client.get_sessions()
                waiting_sessions = [s for s in sessions if s.get("status") == "waiting"]
                
                if not waiting_sessions:
                    print("❌ Aucune session en attente")
                    continue
                
                print("\n🎮 Sessions en attente:")
                for session in waiting_sessions:
                    print(f"  [{session.get('id')}] {session.get('name')} - {session.get('nbPlayers')}/{session.get('maxPlayers')} joueurs")
                
                session_id = int(input("\nID de la session à rejoindre: ").strip())
                if client.join_session(session_id):
                    _waiting_room(client)
            
            elif choice == "5":
                client.disconnect()
                print("👋 Déconnexion...")
                break
            
        except KeyboardInterrupt:
            print("\n\n⚠️ Interruption détectée")
            if client.current_session_id:
                print("Vous êtes dans une session. Déconnexion...")
            client.disconnect()
            break
        except Exception as e:
            print(f"❌ Erreur: {e}")
            import traceback
            traceback.print_exc()


def _waiting_room(client: QuizNetClient):
    """Gère la salle d'attente"""
    print("\n" + "="*60)
    print("🚪 SALLE D'ATTENTE")
    print("="*60)
    
    if client.is_creator:
        print("👑 Vous êtes le créateur de la session")
        print("Appuyez sur Entrée pour démarrer la session (minimum 2 joueurs requis)...")
        input()
        client.start_session()
    else:
        print("⏳ En attente que le créateur démarre la session...")
        # Attendre que la session commence (géré par le listener)
        while client.current_session_id and client.connected:
            time.sleep(1)
    
    # Le jeu se déroule via les messages push du serveur
    _play_game(client)


def _play_game(client: QuizNetClient):
    """Gère le déroulement du jeu"""
    answered = False
    
    while client.current_session_id and client.connected and client.in_game:
        if client.current_question and not answered:
            q_type = client.current_question.get("type", "")
            
            # Afficher les options de jokers avant la réponse
            print("\n" + "-"*60)
            if client.jokers.get("fifty", 0) > 0 and q_type == "qcm":
                print("💡 Jokers disponibles: [5] 50/50", end="")
                if client.jokers.get("skip", 0) > 0:
                    print(" [P] Passer")
                else:
                    print()
            elif client.jokers.get("skip", 0) > 0:
                print("💡 Jokers disponibles: [P] Passer")
            else:
                print("💡 Aucun joker disponible")
            
            try:
                # Lire la réponse de l'utilisateur
                user_input_raw = input("\nVotre réponse (ou joker): ").strip()
                user_input = user_input_raw.upper()
                
                # Vérifier les jokers
                if user_input == "5" and client.jokers.get("fifty", 0) > 0 and q_type == "qcm":
                    if client.use_joker("fifty"):
                        # Le serveur enverra les réponses restantes
                        continue
                elif user_input == "P" and client.jokers.get("skip", 0) > 0:
                    if client.use_joker("skip"):
                        answered = True
                        client.current_question = None
                        continue
                else:
                    # Traiter la réponse normale
                    if q_type == "qcm":
                        try:
                            answer_idx = int(user_input_raw)
                            # Vérifier que l'indice est valide
                            answers = client.current_question.get("answers", [])
                            remaining = client.current_question.get("remainingAnswers", None)
                            
                            # Si joker 50/50 utilisé, vérifier que l'indice est dans les réponses restantes
                            if remaining:
                                # remainingAnswers peut être une liste d'indices (strings) ou de réponses
                                valid_indices = []
                                for i in range(len(answers)):
                                    if str(i) in remaining or answers[i] in remaining:
                                        valid_indices.append(i)
                                if answer_idx in valid_indices:
                                    if client.answer_question(answer_idx):
                                        answered = True
                                else:
                                    print(f"❌ Réponse invalide. Réponses disponibles: {valid_indices}")
                            else:
                                if 0 <= answer_idx < len(answers):
                                    if client.answer_question(answer_idx):
                                        answered = True
                                else:
                                    print(f"❌ Réponse invalide (0-{len(answers)-1})")
                        except ValueError:
                            print("❌ Veuillez entrer un nombre")
                    
                    elif q_type == "boolean":
                        try:
                            answer_int = int(user_input_raw)
                            answer_bool = answer_int == 1
                            if client.answer_question(answer_bool):
                                answered = True
                        except ValueError:
                            if user_input in ["V", "VRAI", "TRUE", "T", "O", "OUI", "YES", "Y"]:
                                if client.answer_question(True):
                                    answered = True
                            elif user_input in ["F", "FAUX", "FALSE", "N", "NON", "NO"]:
                                if client.answer_question(False):
                                    answered = True
                            else:
                                print("❌ Réponse invalide (0/1, V/F, Vrai/Faux, Oui/Non)")
                    
                    elif q_type == "text":
                        if user_input_raw:
                            # Garder la réponse en minuscules pour la comparaison (le serveur gère l'insensibilité à la casse)
                            if client.answer_question(user_input_raw):
                                answered = True
                        else:
                            print("❌ Veuillez entrer une réponse")
                
            except KeyboardInterrupt:
                print("\n⚠️ Interruption - Vous quittez la session")
                client.disconnect()
                break
            except EOFError:
                break
        
        # Réinitialiser answered quand une nouvelle question arrive
        if not client.current_question:
            answered = False
        
        time.sleep(0.5)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\n👋 Au revoir !")
        sys.exit(0)

