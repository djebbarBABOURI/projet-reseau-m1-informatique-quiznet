#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Client QuizNet - Interface Graphique avec Tkinter
"""

try:
    import tkinter as tk
    from tkinter import ttk, scrolledtext, messagebox
except ImportError:
    print("="*60)
    print("❌ Tkinter n'est pas installé sur votre système")
    print("="*60)
    print("\nPour installer Tkinter sur macOS avec Homebrew :")
    print("  brew install python-tk@3.13")
    print("\nOu utilisez la version terminal qui fonctionne sans Tkinter :")
    print("  python3 quiznet_client.py")
    print("\nPour plus d'informations, consultez INSTALL_TKINTER.md")
    print("="*60)
    import sys
    sys.exit(1)

import threading
import time
from quiznet_client import QuizNetClient
from typing import Optional, Dict, List


class QuizNetGUI:
    """Interface graphique pour le client QuizNet"""
    
    def __init__(self, root):
        self.root = root
        self.root.title("QuizNet - Client Graphique")
        self.root.geometry("800x600")
        self.root.resizable(True, True)
        
        self.client = QuizNetClient()
        self.current_page = "discovery"
        self.selected_server = None
        self.themes = []
        self.sessions = []
        
        # Variables pour le jeu
        self.question_timer = None
        self.time_remaining = 0
        self.answered = False
        self.timer_running = False
        
        self.setup_ui()
        self.discover_servers()
    
    def setup_ui(self):
        """Configure l'interface utilisateur"""
        # Frame principal
        self.main_frame = ttk.Frame(self.root, padding="10")
        self.main_frame.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
        self.root.columnconfigure(0, weight=1)
        self.root.rowconfigure(0, weight=1)
        self.main_frame.columnconfigure(0, weight=1)
        self.main_frame.rowconfigure(0, weight=1)
        
        # Notebook pour les différentes pages
        self.notebook = ttk.Notebook(self.main_frame)
        self.notebook.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
        
        # Page de découverte
        self.discovery_frame = ttk.Frame(self.notebook, padding="10")
        self.notebook.add(self.discovery_frame, text="Serveurs")
        self.setup_discovery_page()
        
        # Page d'authentification
        self.auth_frame = ttk.Frame(self.notebook, padding="10")
        self.notebook.add(self.auth_frame, text="Connexion")
        self.setup_auth_page()
        
        # Page principale
        self.main_page_frame = ttk.Frame(self.notebook, padding="10")
        self.notebook.add(self.main_page_frame, text="Menu")
        self.setup_main_page()
        
        # Page de jeu
        self.game_frame = ttk.Frame(self.notebook, padding="10")
        self.notebook.add(self.game_frame, text="Jeu")
        self.setup_game_page()
        
        # Masquer les pages non pertinentes
        self.notebook.hide(1)  # Auth
        self.notebook.hide(2)  # Main
        self.notebook.hide(3)  # Game
    
    def setup_discovery_page(self):
        """Configure la page de découverte des serveurs"""
        # Titre
        title_label = ttk.Label(self.discovery_frame, text="🔍 Recherche de serveurs QuizNet", 
                               font=("Arial", 16, "bold"))
        title_label.grid(row=0, column=0, columnspan=2, pady=10)
        
        # Liste des serveurs
        ttk.Label(self.discovery_frame, text="Serveurs disponibles:").grid(row=1, column=0, sticky=tk.W, pady=5)
        
        self.servers_listbox = tk.Listbox(self.discovery_frame, height=10, width=50)
        self.servers_listbox.grid(row=2, column=0, columnspan=2, pady=5, sticky=(tk.W, tk.E))
        
        scrollbar = ttk.Scrollbar(self.discovery_frame, orient=tk.VERTICAL, command=self.servers_listbox.yview)
        scrollbar.grid(row=2, column=2, sticky=(tk.N, tk.S))
        self.servers_listbox.config(yscrollcommand=scrollbar.set)
        
        # Boutons
        button_frame = ttk.Frame(self.discovery_frame)
        button_frame.grid(row=3, column=0, columnspan=2, pady=10)
        
        ttk.Button(button_frame, text="🔄 Actualiser", command=self.discover_servers).pack(side=tk.LEFT, padx=5)
        ttk.Button(button_frame, text="➡️ Se connecter", command=self.connect_to_selected_server).pack(side=tk.LEFT, padx=5)
        
        self.discovery_frame.columnconfigure(0, weight=1)
    
    def setup_auth_page(self):
        """Configure la page d'authentification"""
        # Titre
        title_label = ttk.Label(self.auth_frame, text="🔐 Authentification", 
                               font=("Arial", 16, "bold"))
        title_label.grid(row=0, column=0, columnspan=2, pady=10)
        
        # Formulaire d'inscription
        register_frame = ttk.LabelFrame(self.auth_frame, text="Nouveau compte", padding="10")
        register_frame.grid(row=1, column=0, columnspan=2, pady=10, sticky=(tk.W, tk.E))
        
        ttk.Label(register_frame, text="Pseudo:").grid(row=0, column=0, sticky=tk.W, pady=5)
        self.register_pseudo = ttk.Entry(register_frame, width=30)
        self.register_pseudo.grid(row=0, column=1, pady=5, padx=5)
        
        ttk.Label(register_frame, text="Mot de passe:").grid(row=1, column=0, sticky=tk.W, pady=5)
        self.register_password = ttk.Entry(register_frame, width=30, show="*")
        self.register_password.grid(row=1, column=1, pady=5, padx=5)
        
        ttk.Button(register_frame, text="S'inscrire", command=self.register).grid(row=2, column=0, columnspan=2, pady=10)
        
        # Formulaire de connexion
        login_frame = ttk.LabelFrame(self.auth_frame, text="Connexion", padding="10")
        login_frame.grid(row=2, column=0, columnspan=2, pady=10, sticky=(tk.W, tk.E))
        
        ttk.Label(login_frame, text="Pseudo:").grid(row=0, column=0, sticky=tk.W, pady=5)
        self.login_pseudo = ttk.Entry(login_frame, width=30)
        self.login_pseudo.grid(row=0, column=1, pady=5, padx=5)
        
        ttk.Label(login_frame, text="Mot de passe:").grid(row=1, column=0, sticky=tk.W, pady=5)
        self.login_password = ttk.Entry(login_frame, width=30, show="*")
        self.login_password.grid(row=1, column=1, pady=5, padx=5)
        
        ttk.Button(login_frame, text="Se connecter", command=self.login).grid(row=2, column=0, columnspan=2, pady=10)
        
        self.auth_frame.columnconfigure(0, weight=1)
    
    def setup_main_page(self):
        """Configure la page principale"""
        # Titre
        self.main_title = ttk.Label(self.main_page_frame, text="🎮 QuizNet", 
                                    font=("Arial", 16, "bold"))
        self.main_title.grid(row=0, column=0, columnspan=2, pady=10)
        
        # Onglets pour thèmes et sessions
        main_notebook = ttk.Notebook(self.main_page_frame)
        main_notebook.grid(row=1, column=0, columnspan=2, pady=10, sticky=(tk.W, tk.E, tk.N, tk.S))
        
        # Page thèmes
        themes_frame = ttk.Frame(main_notebook, padding="10")
        main_notebook.add(themes_frame, text="Thèmes")
        
        ttk.Button(themes_frame, text="🔄 Actualiser les thèmes", 
                  command=self.refresh_themes).pack(pady=5)
        
        self.themes_text = scrolledtext.ScrolledText(themes_frame, height=10, width=50)
        self.themes_text.pack(fill=tk.BOTH, expand=True, pady=5)
        
        # Page sessions
        sessions_frame = ttk.Frame(main_notebook, padding="10")
        main_notebook.add(sessions_frame, text="Sessions")
        
        button_frame = ttk.Frame(sessions_frame)
        button_frame.pack(pady=5)
        
        ttk.Button(button_frame, text="🔄 Actualiser", 
                  command=self.refresh_sessions).pack(side=tk.LEFT, padx=5)
        ttk.Button(button_frame, text="➕ Créer une session", 
                  command=self.show_create_session_dialog).pack(side=tk.LEFT, padx=5)
        
        self.sessions_listbox = tk.Listbox(sessions_frame, height=10)
        self.sessions_listbox.pack(fill=tk.BOTH, expand=True, pady=5)
        
        ttk.Button(sessions_frame, text="➡️ Rejoindre la session", 
                  command=self.join_selected_session).pack(pady=5)
        
        # Page salle d'attente
        self.waiting_frame = ttk.Frame(main_notebook, padding="10")
        main_notebook.add(self.waiting_frame, text="Salle d'attente")
        
        self.waiting_text = scrolledtext.ScrolledText(self.waiting_frame, height=15, width=50)
        self.waiting_text.pack(fill=tk.BOTH, expand=True, pady=5)
        
        self.start_button = ttk.Button(self.waiting_frame, text="🚀 Démarrer la session", 
                                       command=self.start_session, state=tk.DISABLED)
        self.start_button.pack(pady=5)
        
        self.main_page_frame.columnconfigure(0, weight=1)
        self.main_page_frame.rowconfigure(1, weight=1)
    
    def setup_game_page(self):
        """Configure la page de jeu"""
        # Informations de la question
        self.question_frame = ttk.LabelFrame(self.game_frame, text="Question", padding="10")
        self.question_frame.grid(row=0, column=0, columnspan=2, pady=10, sticky=(tk.W, tk.E))
        
        self.question_label = ttk.Label(self.question_frame, text="En attente d'une question...", 
                                       font=("Arial", 12), wraplength=600)
        self.question_label.grid(row=0, column=0, columnspan=2, pady=5)
        
        self.question_info = ttk.Label(self.question_frame, text="", font=("Arial", 10))
        self.question_info.grid(row=1, column=0, columnspan=2, pady=5)
        
        self.timer_label = ttk.Label(self.question_frame, text="", font=("Arial", 14, "bold"), 
                                     foreground="red")
        self.timer_label.grid(row=2, column=0, columnspan=2, pady=5)
        
        # Réponses
        self.answers_frame = ttk.LabelFrame(self.game_frame, text="Réponses", padding="10")
        self.answers_frame.grid(row=1, column=0, columnspan=2, pady=10, sticky=(tk.W, tk.E))
        
        self.answer_var = tk.StringVar()
        self.answer_buttons = []
        
        # Zone de réponse texte (créée une seule fois)
        self.text_answer_entry = ttk.Entry(self.answers_frame, width=40)
        
        # Jokers
        self.jokers_frame = ttk.LabelFrame(self.game_frame, text="Jokers", padding="10")
        self.jokers_frame.grid(row=2, column=0, columnspan=2, pady=10, sticky=(tk.W, tk.E))
        
        self.fifty_button = ttk.Button(self.jokers_frame, text="50/50 (1)", 
                                       command=lambda: self.use_joker("fifty"), state=tk.DISABLED)
        self.fifty_button.grid(row=0, column=0, padx=5)
        
        self.skip_button = ttk.Button(self.jokers_frame, text="Passer (1)", 
                                      command=lambda: self.use_joker("skip"), state=tk.DISABLED)
        self.skip_button.grid(row=0, column=1, padx=5)
        
        # Bouton de réponse
        self.submit_button = ttk.Button(self.game_frame, text="✅ Envoyer la réponse", 
                                       command=self.submit_answer, state=tk.DISABLED)
        self.submit_button.grid(row=3, column=0, columnspan=2, pady=10)
        
        # Résultats
        self.results_frame = ttk.LabelFrame(self.game_frame, text="Résultats", padding="10")
        self.results_frame.grid(row=4, column=0, columnspan=2, pady=10, sticky=(tk.W, tk.E, tk.N, tk.S))
        
        self.results_text = scrolledtext.ScrolledText(self.results_frame, height=10, width=50)
        self.results_text.pack(fill=tk.BOTH, expand=True)
        
        self.game_frame.columnconfigure(0, weight=1)
        self.game_frame.rowconfigure(4, weight=1)
    
    def discover_servers(self):
        """Découvre les serveurs disponibles"""
        self.servers_listbox.delete(0, tk.END)
        self.servers_listbox.insert(0, "Recherche en cours...")
        
        def discover():
            servers = self.client.discover_servers()
            self.root.after(0, self.update_servers_list, servers)
        
        threading.Thread(target=discover, daemon=True).start()
    
    def update_servers_list(self, servers):
        """Met à jour la liste des serveurs"""
        self.servers_listbox.delete(0, tk.END)
        self.servers = servers
        
        for server in servers:
            server_text = f"{server['name']} - {server['address']}:{server['port']}"
            self.servers_listbox.insert(tk.END, server_text)
    
    def connect_to_selected_server(self):
        """Se connecte au serveur sélectionné"""
        selection = self.servers_listbox.curselection()
        if not selection:
            messagebox.showwarning("Aucun serveur sélectionné", "Veuillez sélectionner un serveur")
            return
        
        server = self.servers[selection[0]]
        
        def connect():
            if self.client.connect_to_server(server['address'], server['port']):
                self.root.after(0, lambda: self.notebook.select(1))  # Page auth
                self.root.after(0, lambda: self.notebook.hide(0))  # Cacher discovery
            else:
                self.root.after(0, lambda: messagebox.showerror("Erreur", "Impossible de se connecter au serveur"))
        
        threading.Thread(target=connect, daemon=True).start()
    
    def register(self):
        """Inscrit un nouveau joueur"""
        pseudo = self.register_pseudo.get()
        password = self.register_password.get()
        
        if not pseudo or not password:
            messagebox.showwarning("Champs manquants", "Veuillez remplir tous les champs")
            return
        
        def do_register():
            if self.client.register(pseudo, password):
                self.root.after(0, lambda: self.client.login(pseudo, password))
                self.root.after(0, self.on_login_success)
            else:
                self.root.after(0, lambda: messagebox.showerror("Erreur", "Échec de l'inscription"))
        
        threading.Thread(target=do_register, daemon=True).start()
    
    def login(self):
        """Connecte un joueur"""
        pseudo = self.login_pseudo.get()
        password = self.login_password.get()
        
        if not pseudo or not password:
            messagebox.showwarning("Champs manquants", "Veuillez remplir tous les champs")
            return
        
        def do_login():
            if self.client.login(pseudo, password):
                self.root.after(0, self.on_login_success)
            else:
                self.root.after(0, lambda: messagebox.showerror("Erreur", "Identifiants incorrects"))
        
        threading.Thread(target=do_login, daemon=True).start()
    
    def on_login_success(self):
        """Appelé après une connexion réussie"""
        self.notebook.select(2)  # Page principale
        self.notebook.hide(1)  # Cacher auth
        self.main_title.config(text=f"🎮 QuizNet - Bienvenue {self.client.pseudo} !")
        self.refresh_themes()
        self.refresh_sessions()
        
        # Définir le callback pour les messages push
        self.client.push_callback = self.handle_push_message_gui
    
    def refresh_themes(self):
        """Actualise la liste des thèmes"""
        def do_refresh():
            themes = self.client.get_themes()
            self.root.after(0, self.update_themes, themes)
        
        threading.Thread(target=do_refresh, daemon=True).start()
    
    def update_themes(self, themes):
        """Met à jour l'affichage des thèmes"""
        self.themes = themes
        self.themes_text.delete(1.0, tk.END)
        
        if themes:
            for theme in themes:
                self.themes_text.insert(tk.END, f"• {theme.get('name')} (ID: {theme.get('id')})\n")
        else:
            self.themes_text.insert(tk.END, "Aucun thème disponible")
    
    def refresh_sessions(self):
        """Actualise la liste des sessions"""
        def do_refresh():
            sessions = self.client.get_sessions()
            self.root.after(0, self.update_sessions, sessions)
        
        threading.Thread(target=do_refresh, daemon=True).start()
    
    def update_sessions(self, sessions):
        """Met à jour l'affichage des sessions"""
        self.sessions = sessions
        self.sessions_listbox.delete(0, tk.END)
        
        for session in sessions:
            status_emoji = "🟢" if session.get("status") == "waiting" else "🟡" if session.get("status") == "playing" else "🔴"
            session_text = f"{status_emoji} [{session.get('id')}] {session.get('name')} - {session.get('nbPlayers')}/{session.get('maxPlayers')} - {session.get('mode')}"
            self.sessions_listbox.insert(tk.END, session_text)
    
    def show_create_session_dialog(self):
        """Affiche la boîte de dialogue de création de session"""
        dialog = tk.Toplevel(self.root)
        dialog.title("Créer une session")
        dialog.geometry("400x500")
        dialog.transient(self.root)
        dialog.grab_set()
        
        # Thèmes
        ttk.Label(dialog, text="Thèmes (IDs séparés par des virgules):").grid(row=0, column=0, sticky=tk.W, pady=5, padx=10)
        themes_entry = ttk.Entry(dialog, width=30)
        themes_entry.grid(row=0, column=1, pady=5, padx=10)
        
        # Difficulté
        ttk.Label(dialog, text="Difficulté:").grid(row=1, column=0, sticky=tk.W, pady=5, padx=10)
        difficulty_var = tk.StringVar(value="moyen")
        difficulty_combo = ttk.Combobox(dialog, textvariable=difficulty_var, 
                                       values=["facile", "moyen", "difficile"], state="readonly")
        difficulty_combo.grid(row=1, column=1, pady=5, padx=10)
        
        # Nombre de questions
        ttk.Label(dialog, text="Nombre de questions (10-50):").grid(row=2, column=0, sticky=tk.W, pady=5, padx=10)
        nb_questions_entry = ttk.Entry(dialog, width=30)
        nb_questions_entry.insert(0, "20")
        nb_questions_entry.grid(row=2, column=1, pady=5, padx=10)
        
        # Temps limite
        ttk.Label(dialog, text="Temps limite (10-60 sec):").grid(row=3, column=0, sticky=tk.W, pady=5, padx=10)
        time_limit_entry = ttk.Entry(dialog, width=30)
        time_limit_entry.insert(0, "30")
        time_limit_entry.grid(row=3, column=1, pady=5, padx=10)
        
        # Mode
        ttk.Label(dialog, text="Mode:").grid(row=4, column=0, sticky=tk.W, pady=5, padx=10)
        mode_var = tk.StringVar(value="solo")
        mode_combo = ttk.Combobox(dialog, textvariable=mode_var, 
                                 values=["solo", "battle"], state="readonly")
        mode_combo.grid(row=4, column=1, pady=5, padx=10)
        
        # Nombre max de joueurs
        ttk.Label(dialog, text="Nombre max de joueurs:").grid(row=5, column=0, sticky=tk.W, pady=5, padx=10)
        max_players_entry = ttk.Entry(dialog, width=30)
        max_players_entry.insert(0, "6")
        max_players_entry.grid(row=5, column=1, pady=5, padx=10)
        
        # Nom de la session
        ttk.Label(dialog, text="Nom de la session:").grid(row=6, column=0, sticky=tk.W, pady=5, padx=10)
        name_entry = ttk.Entry(dialog, width=30)
        name_entry.grid(row=6, column=1, pady=5, padx=10)
        
        def create():
            try:
                theme_ids = [int(tid.strip()) for tid in themes_entry.get().split(",")]
                nb_questions = int(nb_questions_entry.get())
                time_limit = int(time_limit_entry.get())
                max_players = int(max_players_entry.get())
                name = name_entry.get()
                
                if not name:
                    messagebox.showwarning("Erreur", "Veuillez entrer un nom de session")
                    return
                
                def do_create():
                    session_id = self.client.create_session(
                        name, theme_ids, difficulty_var.get(), nb_questions, 
                        time_limit, mode_var.get(), max_players
                    )
                    if session_id:
                        self.root.after(0, lambda: dialog.destroy())
                        self.root.after(0, self.show_waiting_room)
                    else:
                        self.root.after(0, lambda: messagebox.showerror("Erreur", "Échec de la création de session"))
                
                threading.Thread(target=do_create, daemon=True).start()
            except ValueError:
                messagebox.showerror("Erreur", "Valeurs invalides")
        
        ttk.Button(dialog, text="Créer", command=create).grid(row=7, column=0, columnspan=2, pady=20)
        ttk.Button(dialog, text="Annuler", command=dialog.destroy).grid(row=8, column=0, columnspan=2)
    
    def join_selected_session(self):
        """Rejoint la session sélectionnée"""
        selection = self.sessions_listbox.curselection()
        if not selection:
            messagebox.showwarning("Aucune session sélectionnée", "Veuillez sélectionner une session")
            return
        
        session = self.sessions[selection[0]]
        
        def do_join():
            if self.client.join_session(session.get('id')):
                self.root.after(0, self.show_waiting_room)
            else:
                self.root.after(0, lambda: messagebox.showerror("Erreur", "Impossible de rejoindre la session"))
        
        threading.Thread(target=do_join, daemon=True).start()
    
    def show_waiting_room(self):
        """Affiche la salle d'attente"""
        self.notebook.select(2)  # Page principale
        # Aller à l'onglet "Salle d'attente"
        main_notebook = self.main_page_frame.winfo_children()[1]  # Le notebook dans main_page
        if isinstance(main_notebook, ttk.Notebook):
            main_notebook.select(2)  # Onglet salle d'attente
        
        self.waiting_text.delete(1.0, tk.END)
        self.waiting_text.insert(tk.END, "🚪 SALLE D'ATTENTE\n\n")
        
        if self.client.is_creator:
            self.waiting_text.insert(tk.END, "👑 Vous êtes le créateur de la session\n")
            self.waiting_text.insert(tk.END, "Appuyez sur 'Démarrer' quand au moins 2 joueurs sont présents\n\n")
            self.start_button.config(state=tk.NORMAL)
        else:
            self.waiting_text.insert(tk.END, "⏳ En attente que le créateur démarre la session...\n\n")
            self.start_button.config(state=tk.DISABLED)
    
    def start_session(self):
        """Démarre la session"""
        def do_start():
            self.client.start_session()
        
        threading.Thread(target=do_start, daemon=True).start()
        self.start_button.config(state=tk.DISABLED)
    
    def handle_push_message_gui(self, header: str, data: Dict):
        """Gère les messages push du serveur (version GUI)"""
        action = data.get("action", "")
        
        if action == "session/player/joined":
            pseudo = data.get("pseudo", "")
            nb_players = data.get("nbPlayers", 0)
            self.root.after(0, lambda: self.waiting_text.insert(
                tk.END, f"🎮 {pseudo} a rejoint la session ({nb_players} joueur(s))\n"))
            self.root.after(0, lambda: self.waiting_text.see(tk.END))
        
        elif action == "session/started":
            self.client.in_game = True
            self.root.after(0, self.start_game)
        
        elif action == "question/new":
            self.client.current_question = data
            self.client.question_start_time = time.time()
            self.client.time_limit = data.get("timeLimit", self.client.time_limit)
            self.root.after(0, lambda: self.display_question(data))
        
        elif action == "question/results":
            self.timer_running = False
            self.root.after(0, lambda: self.display_results(data))
            self.client.current_question = None
            self.answered = False
        
        elif action == "session/finished":
            self.client.current_session_id = None
            self.client.in_game = False
            self.root.after(0, lambda: self.display_final_results(data))
        
        elif action == "session/player/left":
            pseudo = data.get("pseudo", "")
            self.root.after(0, lambda: self.results_text.insert(
                tk.END, f"⚠️ {pseudo} a quitté la session\n"))
            self.root.after(0, lambda: self.results_text.see(tk.END))
        
        elif action == "session/player/eliminated":
            pseudo = data.get("pseudo", "")
            self.root.after(0, lambda: self.results_text.insert(
                tk.END, f"💀 {pseudo} a été éliminé !\n"))
            self.root.after(0, lambda: self.results_text.see(tk.END))
        
        elif action == "joker/use":
            self.client.jokers = data.get("jokers", self.client.jokers)
            if "remainingAnswers" in data:
                if self.client.current_question:
                    self.client.current_question["remainingAnswers"] = data["remainingAnswers"]
                self.root.after(0, lambda: self.display_question(self.client.current_question))
                # Mettre à jour le bouton 50/50
                self.root.after(0, lambda: self.fifty_button.config(state=tk.DISABLED))
            else:
                self.root.after(0, lambda: messagebox.showinfo("Joker", "Question passée !"))
                self.answered = True
                self.client.current_question = None
                self.root.after(0, lambda: self.submit_button.config(state=tk.DISABLED))
    
    def start_game(self):
        """Démarre le jeu"""
        self.notebook.select(3)  # Page de jeu
        self.results_text.delete(1.0, tk.END)
        self.results_text.insert(tk.END, "🎯 La session a commencé !\n\n")
    
    def display_question(self, question_data: Dict):
        """Affiche une question"""
        self.client.current_question = question_data
        self.client.question_start_time = time.time()
        self.answered = False
        
        question_num = question_data.get('questionNum', 0)
        total_questions = question_data.get('totalQuestions', 0)
        question = question_data.get('question', '')
        difficulty = question_data.get('difficulty', '')
        q_type = question_data.get("type", "")
        
        self.question_label.config(text=question)
        self.question_info.config(
            text=f"Question {question_num}/{total_questions} - Difficulté: {difficulty}")
        
        # Nettoyer les réponses précédentes (sauf text_answer_entry)
        for widget in self.answers_frame.winfo_children():
            if widget != self.text_answer_entry:
                widget.destroy()
        
        # Cacher le champ texte par défaut
        self.text_answer_entry.grid_remove()
        
        self.answer_buttons = []
        self.answer_var = tk.StringVar()
        
        if q_type == "qcm":
            answers = question_data.get("answers", [])
            remaining = question_data.get("remainingAnswers", None)
            
            for i, ans in enumerate(answers):
                if not remaining or str(i) in remaining or ans in remaining:
                    rb = ttk.Radiobutton(self.answers_frame, text=ans, variable=self.answer_var, 
                                        value=str(i))
                    rb.grid(row=i, column=0, sticky=tk.W, pady=2)
                    self.answer_buttons.append(rb)
        
        elif q_type == "boolean":
            rb_true = ttk.Radiobutton(self.answers_frame, text="Vrai", variable=self.answer_var, value="true")
            rb_true.grid(row=0, column=0, sticky=tk.W, pady=2)
            rb_false = ttk.Radiobutton(self.answers_frame, text="Faux", variable=self.answer_var, value="false")
            rb_false.grid(row=1, column=0, sticky=tk.W, pady=2)
            self.answer_buttons = [rb_true, rb_false]
        
        elif q_type == "text":
            self.text_answer_entry.grid(row=0, column=0, pady=5)
            self.text_answer_entry.delete(0, tk.END)
            self.text_answer_entry.config(state=tk.NORMAL)
        
        # Activer les boutons
        self.submit_button.config(state=tk.NORMAL)
        
        # Gérer les jokers
        if q_type == "qcm" and self.client.jokers.get("fifty", 0) > 0:
            self.fifty_button.config(state=tk.NORMAL)
        else:
            self.fifty_button.config(state=tk.DISABLED)
        
        if self.client.jokers.get("skip", 0) > 0:
            self.skip_button.config(state=tk.NORMAL)
        else:
            self.skip_button.config(state=tk.DISABLED)
        
        # Démarrer le timer
        self.timer_running = True
        self.time_remaining = self.client.time_limit
        self.update_timer()
    
    def update_timer(self):
        """Met à jour le timer"""
        if self.timer_running and self.time_remaining > 0 and not self.answered:
            self.timer_label.config(text=f"⏱️ Temps restant: {self.time_remaining}s")
            self.time_remaining -= 1
            self.root.after(1000, self.update_timer)
        elif self.time_remaining <= 0:
            self.timer_label.config(text="⏱️ Temps écoulé")
            self.timer_running = False
    
    def use_joker(self, joker_type: str):
        """Utilise un joker"""
        def do_use():
            self.client.use_joker(joker_type)
        
        threading.Thread(target=do_use, daemon=True).start()
        
        if joker_type == "fifty":
            self.fifty_button.config(state=tk.DISABLED)
        elif joker_type == "skip":
            self.skip_button.config(state=tk.DISABLED)
            self.answered = True
    
    def submit_answer(self):
        """Envoie la réponse"""
        if self.answered:
            return
        
        q_type = self.client.current_question.get("type", "") if self.client.current_question else ""
        answer = None
        
        if q_type == "qcm":
            answer_val = self.answer_var.get()
            if answer_val:
                answer = int(answer_val)
            else:
                messagebox.showwarning("Réponse manquante", "Veuillez sélectionner une réponse")
                return
        
        elif q_type == "boolean":
            answer_val = self.answer_var.get()
            if answer_val:
                answer = answer_val == "true"
            else:
                messagebox.showwarning("Réponse manquante", "Veuillez sélectionner une réponse")
                return
        
        elif q_type == "text":
            answer = self.text_answer_entry.get()
            if not answer:
                messagebox.showwarning("Réponse manquante", "Veuillez entrer une réponse")
                return
        
        if answer is not None:
            def do_answer():
                self.client.answer_question(answer)
                self.root.after(0, lambda: self.set_answered())
            
            threading.Thread(target=do_answer, daemon=True).start()
    
    def set_answered(self):
        """Marque la question comme répondue"""
        self.answered = True
        self.submit_button.config(state=tk.DISABLED)
        self.results_text.insert(tk.END, "✓ Réponse envoyée, en attente des résultats...\n")
    
    def display_results(self, results_data: Dict):
        """Affiche les résultats"""
        self.results_text.delete(1.0, tk.END)
        self.results_text.insert(tk.END, "📊 RÉSULTATS\n")
        self.results_text.insert(tk.END, "="*50 + "\n\n")
        
        correct_answer = results_data.get('correctAnswer', '')
        explanation = results_data.get('explanation', '')
        
        self.results_text.insert(tk.END, f"✅ Bonne réponse: {correct_answer}\n")
        if explanation:
            self.results_text.insert(tk.END, f"💡 {explanation}\n\n")
        
        results = results_data.get("results", [])
        sorted_results = sorted(results, key=lambda x: x.get("totalScore", 0), reverse=True)
        
        self.results_text.insert(tk.END, "📈 Classement temporaire:\n")
        self.results_text.insert(tk.END, "-"*50 + "\n")
        
        for i, result in enumerate(sorted_results, 1):
            pseudo = result.get("pseudo", "")
            score = result.get("totalScore", 0)
            points = result.get("points", 0)
            correct = "✓" if result.get("correct", False) else "✗"
            
            if "lives" in result:
                lives = result.get("lives", 0)
                self.results_text.insert(tk.END, 
                    f"{i}. {pseudo} - Score: {score} (+{points}) - Vies: {lives} {correct}\n")
            else:
                self.results_text.insert(tk.END, 
                    f"{i}. {pseudo} - Score: {score} (+{points}) {correct}\n")
        
        if "lastPlayer" in results_data:
            self.results_text.insert(tk.END, 
                f"\n⚠️ Dernier joueur: {results_data.get('lastPlayer')} (perd une vie)\n")
        
        self.answered = False
    
    def display_final_results(self, results_data: Dict):
        """Affiche les résultats finaux"""
        self.results_text.delete(1.0, tk.END)
        self.results_text.insert(tk.END, "🎉 FIN DE LA SESSION\n")
        self.results_text.insert(tk.END, "="*50 + "\n\n")
        
        mode = results_data.get("mode", "")
        ranking = results_data.get("ranking", [])
        
        if mode == "battle":
            winner = results_data.get("winner", "")
            self.results_text.insert(tk.END, f"🏆 VAINQUEUR: {winner}\n\n")
        
        self.results_text.insert(tk.END, "📊 Classement final:\n")
        self.results_text.insert(tk.END, "-"*50 + "\n")
        
        for result in ranking:
            rank = result.get("rank", 0)
            pseudo = result.get("pseudo", "")
            score = result.get("score", 0)
            correct = result.get("correctAnswers", 0)
            
            if mode == "battle":
                lives = result.get("lives", 0)
                eliminated = result.get("eliminatedAt", None)
                if eliminated:
                    self.results_text.insert(tk.END, 
                        f"{rank}. {pseudo} - Vies: {lives} - Score: {score} - Réponses: {correct} (Éliminé Q{eliminated})\n")
                else:
                    self.results_text.insert(tk.END, 
                        f"{rank}. {pseudo} - Vies: {lives} - Score: {score} - Réponses: {correct}\n")
            else:
                self.results_text.insert(tk.END, 
                    f"{rank}. {pseudo} - Score: {score} - Réponses correctes: {correct}\n")
        
        messagebox.showinfo("Fin de session", "La session est terminée !")
        self.notebook.select(2)  # Retour au menu principal


def main():
    """Fonction principale"""
    root = tk.Tk()
    app = QuizNetGUI(root)
    root.mainloop()


if __name__ == "__main__":
    main()

