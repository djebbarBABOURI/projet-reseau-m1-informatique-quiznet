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
except Exception as e:
    if "macOS" in str(e) or "abort" in str(e).lower():
        print("="*60)
        print("❌ Erreur de compatibilité Tkinter avec macOS")
        print("="*60)
        print(f"\nErreur: {e}")
        print("\n🔧 Solutions possibles :")
        print("\n1. Utiliser la version terminal (recommandé) :")
        print("   python3 quiznet_client.py")
        print("\n2. Installer Python via Homebrew avec Tkinter :")
        print("   brew install python-tk")
        print("   # Puis utiliser: /opt/homebrew/bin/python3 quiznet_client_gui.py")
        print("\n3. Utiliser pyenv pour installer une version compatible :")
        print("   pyenv install 3.11.0")
        print("   pyenv local 3.11.0")
        print("\n4. Utiliser Python de la version système :")
        print("   /usr/bin/python3 quiznet_client_gui.py")
        print("="*60)
    else:
        print(f"❌ Erreur inattendue: {e}")
        print("\n💡 Essayez la version terminal :")
        print("   python3 quiznet_client.py")
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
        self.root.title("🎮 QuizNet - Client Graphique")
        self.root.geometry("900x700")
        self.root.resizable(True, True)
        
        # Configuration des couleurs modernes
        self.colors = {
            'bg': '#f5f5f5',
            'fg': '#2c3e50',
            'primary': '#3498db',
            'secondary': '#2ecc71',
            'danger': '#e74c3c',
            'warning': '#f39c12',
            'success': '#27ae60',
            'light': '#ecf0f1',
            'dark': '#34495e'
        }
        
        # Appliquer le style
        self.setup_styles()
        
        self.client = QuizNetClient()
        self.current_page = "discovery"
        self.selected_server = None
        self.themes = []
        self.selected_theme_ids: List[int] = []
        self.selected_themes_var = tk.StringVar(value="Aucun thème sélectionné")
        self.sessions = []
        
        # Variables pour le jeu
        self.question_timer = None
        self.time_remaining = 0
        self.answered = False
        self.timer_running = False
        
        self.setup_ui()
        self.discover_servers()
    
    def setup_styles(self):
        """Configure les styles modernes pour l'interface"""
        style = ttk.Style()
        style.theme_use('clam')
        
        # Configuration des couleurs de base
        style.configure('TFrame', background=self.colors['bg'])
        style.configure('TLabel', background=self.colors['bg'], foreground=self.colors['fg'])
        style.configure('TButton', padding=10)
        style.configure('Title.TLabel', font=('Arial', 18, 'bold'), foreground=self.colors['primary'])
        style.configure('Subtitle.TLabel', font=('Arial', 12), foreground=self.colors['dark'])
        style.configure('Primary.TButton', background=self.colors['primary'], foreground='white')
        style.configure('Success.TButton', background=self.colors['success'], foreground='white')
        style.configure('Danger.TButton', background=self.colors['danger'], foreground='white')
    
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
        # Titre avec style moderne
        title_frame = ttk.Frame(self.discovery_frame)
        title_frame.grid(row=0, column=0, columnspan=3, pady=(0, 20), sticky=(tk.W, tk.E))
        
        title_label = ttk.Label(title_frame, text="🔍 Recherche de serveurs QuizNet", 
                               style='Title.TLabel')
        title_label.pack()
        
        subtitle_label = ttk.Label(title_frame, text="Sélectionnez un serveur pour commencer", 
                                   style='Subtitle.TLabel')
        subtitle_label.pack(pady=(5, 0))
        
        # Liste des serveurs avec style amélioré
        list_frame = ttk.LabelFrame(self.discovery_frame, text="Serveurs disponibles", padding="10")
        list_frame.grid(row=1, column=0, columnspan=3, pady=10, sticky=(tk.W, tk.E, tk.N, tk.S))
        
        self.servers_listbox = tk.Listbox(list_frame, height=12, width=60, 
                                          font=('Arial', 10),
                                          selectbackground=self.colors['primary'],
                                          selectforeground='white')
        self.servers_listbox.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        
        scrollbar = ttk.Scrollbar(list_frame, orient=tk.VERTICAL, command=self.servers_listbox.yview)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        self.servers_listbox.config(yscrollcommand=scrollbar.set)
        
        # Boutons avec style moderne
        button_frame = ttk.Frame(self.discovery_frame)
        button_frame.grid(row=2, column=0, columnspan=3, pady=20)
        
        refresh_btn = ttk.Button(button_frame, text="🔄 Actualiser", 
                                command=self.discover_servers, style='Primary.TButton')
        refresh_btn.pack(side=tk.LEFT, padx=10)
        
        connect_btn = ttk.Button(button_frame, text="➡️ Se connecter", 
                               command=self.connect_to_selected_server, style='Success.TButton')
        connect_btn.pack(side=tk.LEFT, padx=10)
        
        self.discovery_frame.columnconfigure(0, weight=1)
        self.discovery_frame.rowconfigure(1, weight=1)
    
    def setup_auth_page(self):
        """Configure la page d'authentification"""
        # Titre
        title_frame = ttk.Frame(self.auth_frame)
        title_frame.grid(row=0, column=0, columnspan=2, pady=(0, 20), sticky=(tk.W, tk.E))
        
        title_label = ttk.Label(title_frame, text="🔐 Authentification", 
                               style='Title.TLabel')
        title_label.pack()
        
        subtitle_label = ttk.Label(title_frame, text="Créez un compte ou connectez-vous", 
                                   style='Subtitle.TLabel')
        subtitle_label.pack(pady=(5, 0))
        
        # Container pour les deux formulaires
        forms_container = ttk.Frame(self.auth_frame)
        forms_container.grid(row=1, column=0, columnspan=2, pady=10, sticky=(tk.W, tk.E))
        
        # Formulaire d'inscription
        register_frame = ttk.LabelFrame(forms_container, text="✨ Nouveau compte", padding="15")
        register_frame.grid(row=0, column=0, padx=10, pady=10, sticky=(tk.W, tk.E, tk.N))
        
        ttk.Label(register_frame, text="Pseudo:", font=('Arial', 10, 'bold')).grid(row=0, column=0, sticky=tk.W, pady=8)
        self.register_pseudo = ttk.Entry(register_frame, width=35, font=('Arial', 10))
        self.register_pseudo.grid(row=0, column=1, pady=8, padx=10)
        
        ttk.Label(register_frame, text="Mot de passe:", font=('Arial', 10, 'bold')).grid(row=1, column=0, sticky=tk.W, pady=8)
        self.register_password = ttk.Entry(register_frame, width=35, show="*", font=('Arial', 10))
        self.register_password.grid(row=1, column=1, pady=8, padx=10)
        
        register_btn = ttk.Button(register_frame, text="📝 S'inscrire", 
                                 command=self.register, style='Primary.TButton')
        register_btn.grid(row=2, column=0, columnspan=2, pady=15)
        
        # Formulaire de connexion
        login_frame = ttk.LabelFrame(forms_container, text="🔑 Connexion", padding="15")
        login_frame.grid(row=0, column=1, padx=10, pady=10, sticky=(tk.W, tk.E, tk.N))
        
        ttk.Label(login_frame, text="Pseudo:", font=('Arial', 10, 'bold')).grid(row=0, column=0, sticky=tk.W, pady=8)
        self.login_pseudo = ttk.Entry(login_frame, width=35, font=('Arial', 10))
        self.login_pseudo.grid(row=0, column=1, pady=8, padx=10)
        
        ttk.Label(login_frame, text="Mot de passe:", font=('Arial', 10, 'bold')).grid(row=1, column=0, sticky=tk.W, pady=8)
        self.login_password = ttk.Entry(login_frame, width=35, show="*", font=('Arial', 10))
        self.login_password.grid(row=1, column=1, pady=8, padx=10)
        
        login_btn = ttk.Button(login_frame, text="🚀 Se connecter", 
                              command=self.login, style='Success.TButton')
        login_btn.grid(row=2, column=0, columnspan=2, pady=15)
        
        forms_container.columnconfigure(0, weight=1)
        forms_container.columnconfigure(1, weight=1)
        self.auth_frame.columnconfigure(0, weight=1)
    
    def setup_main_page(self):
        """Configure la page principale"""
        # Titre avec style moderne
        title_frame = ttk.Frame(self.main_page_frame)
        title_frame.grid(row=0, column=0, columnspan=2, pady=(0, 15), sticky=(tk.W, tk.E))
        
        self.main_title = ttk.Label(title_frame, text="🎮 QuizNet", 
                                    style='Title.TLabel')
        self.main_title.pack()
        
        # Onglets pour thèmes et sessions
        main_notebook = ttk.Notebook(self.main_page_frame)
        main_notebook.grid(row=1, column=0, columnspan=2, pady=10, sticky=(tk.W, tk.E, tk.N, tk.S))
        
        # Page thèmes
        themes_frame = ttk.Frame(main_notebook, padding="15")
        main_notebook.add(themes_frame, text="📚 Thèmes")
        
        # Bouton d'actualisation
        refresh_btn = ttk.Button(themes_frame, text="🔄 Actualiser les thèmes", 
                                command=self.refresh_themes, style='Primary.TButton')
        refresh_btn.pack(pady=(0, 10))
        
        # Treeview avec style amélioré
        tree_frame = ttk.LabelFrame(themes_frame, text="Liste des thèmes", padding="10")
        tree_frame.pack(fill=tk.BOTH, expand=True, pady=5)
        
        columns = ("id", "name")
        self.themes_tree = ttk.Treeview(tree_frame, columns=columns, show="headings", 
                                        selectmode="extended", height=12)
        self.themes_tree.heading("id", text="ID")
        self.themes_tree.heading("name", text="Nom du thème")
        self.themes_tree.column("id", width=80, anchor=tk.CENTER)
        self.themes_tree.column("name", anchor=tk.W, width=400)
        self.themes_tree.pack(fill=tk.BOTH, expand=True)
        self.themes_tree.bind("<<TreeviewSelect>>", self.on_theme_select)
        
        # Label de sélection
        selection_label = ttk.LabelFrame(themes_frame, text="Sélection", padding="10")
        selection_label.pack(fill=tk.X, pady=(10, 0))
        ttk.Label(selection_label, textvariable=self.selected_themes_var, 
                 anchor="w", font=('Arial', 10)).pack(fill=tk.X)
        
        # Page sessions
        sessions_frame = ttk.Frame(main_notebook, padding="15")
        main_notebook.add(sessions_frame, text="🎮 Sessions")
        
        # Boutons d'action
        button_frame = ttk.Frame(sessions_frame)
        button_frame.pack(pady=(0, 10))
        
        refresh_btn = ttk.Button(button_frame, text="🔄 Actualiser", 
                                command=self.refresh_sessions, style='Primary.TButton')
        refresh_btn.pack(side=tk.LEFT, padx=5)
        
        create_btn = ttk.Button(button_frame, text="➕ Créer une session", 
                               command=self.show_create_session_dialog, style='Success.TButton')
        create_btn.pack(side=tk.LEFT, padx=5)
        
        # Liste des sessions avec style amélioré
        list_frame = ttk.LabelFrame(sessions_frame, text="Sessions disponibles", padding="10")
        list_frame.pack(fill=tk.BOTH, expand=True, pady=5)
        
        self.sessions_listbox = tk.Listbox(list_frame, height=12, 
                                          font=('Arial', 10),
                                          selectbackground=self.colors['primary'],
                                          selectforeground='white')
        self.sessions_listbox.pack(fill=tk.BOTH, expand=True)
        
        # Bouton de rejoindre
        join_btn = ttk.Button(sessions_frame, text="➡️ Rejoindre la session", 
                            command=self.join_selected_session, style='Success.TButton')
        join_btn.pack(pady=10)
        
        # Page salle d'attente
        self.waiting_frame = ttk.Frame(main_notebook, padding="15")
        main_notebook.add(self.waiting_frame, text="🚪 Salle d'attente")
        
        # Zone de texte avec style
        text_frame = ttk.LabelFrame(self.waiting_frame, text="Messages", padding="10")
        text_frame.pack(fill=tk.BOTH, expand=True, pady=(0, 10))
        
        self.waiting_text = scrolledtext.ScrolledText(text_frame, height=18, width=60,
                                                      font=('Arial', 10),
                                                      wrap=tk.WORD)
        self.waiting_text.pack(fill=tk.BOTH, expand=True)
        
        # Bouton de démarrage
        self.start_button = ttk.Button(self.waiting_frame, text="🚀 Démarrer la session", 
                                       command=self.start_session, state=tk.DISABLED,
                                       style='Success.TButton')
        self.start_button.pack(pady=10)
        
        self.main_page_frame.columnconfigure(0, weight=1)
        self.main_page_frame.rowconfigure(1, weight=1)
    
    def setup_game_page(self):
        """Configure la page de jeu"""
        # Informations de la question avec style amélioré
        self.question_frame = ttk.LabelFrame(self.game_frame, text="📝 Question", padding="15")
        self.question_frame.grid(row=0, column=0, columnspan=2, pady=10, sticky=(tk.W, tk.E))
        
        self.question_label = ttk.Label(self.question_frame, text="En attente d'une question...", 
                                       font=("Arial", 13, "bold"), wraplength=700,
                                       foreground=self.colors['dark'])
        self.question_label.grid(row=0, column=0, columnspan=2, pady=8)
        
        self.question_info = ttk.Label(self.question_frame, text="", 
                                      font=("Arial", 11),
                                      foreground=self.colors['dark'])
        self.question_info.grid(row=1, column=0, columnspan=2, pady=5)
        
        self.timer_label = ttk.Label(self.question_frame, text="", 
                                    font=("Arial", 16, "bold"), 
                                    foreground=self.colors['danger'])
        self.timer_label.grid(row=2, column=0, columnspan=2, pady=10)
        
        # Réponses avec style amélioré
        self.answers_frame = ttk.LabelFrame(self.game_frame, text="💭 Réponses", padding="15")
        self.answers_frame.grid(row=1, column=0, columnspan=2, pady=10, sticky=(tk.W, tk.E))
        
        self.answer_var = tk.StringVar()
        self.answer_buttons = []
        
        # Zone de réponse texte (créée une seule fois)
        self.text_answer_entry = ttk.Entry(self.answers_frame, width=50, font=('Arial', 11))
        
        # Jokers avec style amélioré
        self.jokers_frame = ttk.LabelFrame(self.game_frame, text="🎲 Jokers", padding="15")
        self.jokers_frame.grid(row=2, column=0, columnspan=2, pady=10, sticky=(tk.W, tk.E))
        
        jokers_inner = ttk.Frame(self.jokers_frame)
        jokers_inner.pack()
        
        self.fifty_button = ttk.Button(jokers_inner, text="🎯 50/50 (1)", 
                                       command=lambda: self.use_joker("fifty"), 
                                       state=tk.DISABLED, style='Warning.TButton')
        self.fifty_button.pack(side=tk.LEFT, padx=10)
        
        self.skip_button = ttk.Button(jokers_inner, text="⏭️ Passer (1)", 
                                      command=lambda: self.use_joker("skip"), 
                                      state=tk.DISABLED, style='Danger.TButton')
        self.skip_button.pack(side=tk.LEFT, padx=10)
        
        # Bouton de réponse avec style
        self.submit_button = ttk.Button(self.game_frame, text="✅ Envoyer la réponse", 
                                       command=self.submit_answer, state=tk.DISABLED,
                                       style='Success.TButton')
        self.submit_button.grid(row=3, column=0, columnspan=2, pady=15)
        
        # Résultats avec style amélioré
        self.results_frame = ttk.LabelFrame(self.game_frame, text="📊 Résultats", padding="15")
        self.results_frame.grid(row=4, column=0, columnspan=2, pady=10, sticky=(tk.W, tk.E, tk.N, tk.S))
        
        self.results_text = scrolledtext.ScrolledText(self.results_frame, height=12, width=60,
                                                      font=('Arial', 10),
                                                      wrap=tk.WORD)
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
        for item in self.themes_tree.get_children():
            self.themes_tree.delete(item)
        
        if themes:
            for theme in themes:
                theme_id = theme.get("id")
                name = theme.get("name")
                if theme_id is None or name is None:
                    continue
                self.themes_tree.insert("", tk.END, iid=str(theme_id), values=(theme_id, name))
        else:
            self.selected_theme_ids = []
        
        self.update_selected_themes_label()

    def on_theme_select(self, event=None):
        """Gère la sélection des thèmes dans la liste"""
        selection = self.themes_tree.selection()
        ids = []
        for item in selection:
            try:
                ids.append(int(item))
            except ValueError:
                continue
        self.selected_theme_ids = ids
        self.update_selected_themes_label()

    def update_selected_themes_label(self):
        """Met à jour le texte affichant la sélection de thèmes"""
        if not self.selected_theme_ids:
            self.selected_themes_var.set("Aucun thème sélectionné")
            return
        names = []
        for theme_id in self.selected_theme_ids:
            for theme in self.themes:
                if theme.get("id") == theme_id:
                    names.append(theme.get("name"))
                    break
        joined = ", ".join(f"{name} (ID {tid})" for name, tid in zip(names, self.selected_theme_ids))
        self.selected_themes_var.set(f"Thèmes sélectionnés : {joined}")
    
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
        dialog.title("➕ Créer une session")
        dialog.geometry("500x650")
        dialog.transient(self.root)
        dialog.grab_set()
        dialog.configure(bg=self.colors['bg'])
        
        # Titre
        title_frame = ttk.Frame(dialog)
        title_frame.grid(row=0, column=0, columnspan=3, pady=(10, 20), sticky=(tk.W, tk.E))
        ttk.Label(title_frame, text="➕ Créer une nouvelle session", 
                 style='Title.TLabel').pack()
        
        # Container principal avec padding
        main_container = ttk.Frame(dialog, padding="15")
        main_container.grid(row=1, column=0, columnspan=3, sticky=(tk.W, tk.E))
        
        # Thèmes
        themes_frame = ttk.LabelFrame(main_container, text="📚 Thèmes (sélection multiple)", padding="10")
        themes_frame.grid(row=0, column=0, columnspan=2, pady=8, sticky=(tk.W, tk.E))
        
        themes_listbox = tk.Listbox(themes_frame, selectmode=tk.MULTIPLE, height=8, 
                                   exportselection=False, font=('Arial', 10),
                                   selectbackground=self.colors['primary'],
                                   selectforeground='white')
        themes_listbox.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        scrollbar = ttk.Scrollbar(themes_frame, orient=tk.VERTICAL, command=themes_listbox.yview)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        themes_listbox.config(yscrollcommand=scrollbar.set)
        for index, theme in enumerate(self.themes):
            themes_listbox.insert(tk.END, f"[{theme.get('id')}] {theme.get('name')}")
            if theme.get("id") in self.selected_theme_ids:
                themes_listbox.selection_set(index)
        
        # Difficulté
        difficulty_frame = ttk.LabelFrame(main_container, text="⚙️ Paramètres", padding="10")
        difficulty_frame.grid(row=1, column=0, columnspan=2, pady=8, sticky=(tk.W, tk.E))
        
        ttk.Label(difficulty_frame, text="Difficulté:", font=('Arial', 10, 'bold')).grid(row=0, column=0, sticky=tk.W, pady=5, padx=5)
        difficulty_var = tk.StringVar(value="moyen")
        difficulty_combo = ttk.Combobox(difficulty_frame, textvariable=difficulty_var, 
                                       values=["facile", "moyen", "difficile"], 
                                       state="readonly", width=25, font=('Arial', 10))
        difficulty_combo.grid(row=0, column=1, pady=5, padx=5, sticky=(tk.W, tk.E))
        
        # Nombre de questions
        ttk.Label(difficulty_frame, text="Nombre de questions (10-50):", font=('Arial', 10, 'bold')).grid(row=1, column=0, sticky=tk.W, pady=5, padx=5)
        nb_questions_entry = ttk.Entry(difficulty_frame, width=25, font=('Arial', 10))
        nb_questions_entry.insert(0, "20")
        nb_questions_entry.grid(row=1, column=1, pady=5, padx=5, sticky=(tk.W, tk.E))
        
        # Temps limite
        ttk.Label(difficulty_frame, text="Temps limite (10-60 sec):", font=('Arial', 10, 'bold')).grid(row=2, column=0, sticky=tk.W, pady=5, padx=5)
        time_limit_entry = ttk.Entry(difficulty_frame, width=25, font=('Arial', 10))
        time_limit_entry.insert(0, "30")
        time_limit_entry.grid(row=2, column=1, pady=5, padx=5, sticky=(tk.W, tk.E))
        
        # Mode
        ttk.Label(difficulty_frame, text="Mode:", font=('Arial', 10, 'bold')).grid(row=3, column=0, sticky=tk.W, pady=5, padx=5)
        mode_var = tk.StringVar(value="solo")
        mode_combo = ttk.Combobox(difficulty_frame, textvariable=mode_var, 
                                 values=["solo", "battle"], state="readonly",
                                 width=25, font=('Arial', 10))
        mode_combo.grid(row=3, column=1, pady=5, padx=5, sticky=(tk.W, tk.E))
        
        # Nombre max de joueurs
        ttk.Label(difficulty_frame, text="Nombre max de joueurs:", font=('Arial', 10, 'bold')).grid(row=4, column=0, sticky=tk.W, pady=5, padx=5)
        max_players_entry = ttk.Entry(difficulty_frame, width=25, font=('Arial', 10))
        max_players_entry.insert(0, "6")
        max_players_entry.grid(row=4, column=1, pady=5, padx=5, sticky=(tk.W, tk.E))
        
        # Nom de la session
        name_frame = ttk.LabelFrame(main_container, text="📝 Informations", padding="10")
        name_frame.grid(row=2, column=0, columnspan=2, pady=8, sticky=(tk.W, tk.E))
        
        ttk.Label(name_frame, text="Nom de la session:", font=('Arial', 10, 'bold')).grid(row=0, column=0, sticky=tk.W, pady=5, padx=5)
        name_entry = ttk.Entry(name_frame, width=35, font=('Arial', 10))
        name_entry.grid(row=0, column=1, pady=5, padx=5, sticky=(tk.W, tk.E))
        
        difficulty_frame.columnconfigure(1, weight=1)
        name_frame.columnconfigure(1, weight=1)
        main_container.columnconfigure(0, weight=1)
        
        # Label de statut pour le feedback
        status_frame = ttk.Frame(dialog)
        status_frame.grid(row=2, column=0, columnspan=3, pady=10)
        status_label = ttk.Label(status_frame, text="", foreground=self.colors['primary'], 
                                 font=('Arial', 9), wraplength=450)
        status_label.pack()
        
        def create():
            """Crée une nouvelle session avec validation"""
            # Réinitialiser le message de statut
            status_label.config(text="", foreground=self.colors['primary'])
            
            try:
                # Validation des thèmes
                selection = themes_listbox.curselection()
                if not selection:
                    status_label.config(text="❌ Veuillez sélectionner au moins un thème", 
                                      foreground=self.colors['danger'])
                    messagebox.showwarning("Thèmes manquants", "Veuillez sélectionner au moins un thème")
                    return
                
                theme_ids = []
                for idx in selection:
                    try:
                        theme_ids.append(int(self.themes[idx].get("id")))
                    except (ValueError, IndexError, TypeError):
                        continue
                
                if not theme_ids:
                    status_label.config(text="❌ Aucun thème valide sélectionné", 
                                      foreground=self.colors['danger'])
                    messagebox.showwarning("Thèmes invalides", "Aucun thème valide sélectionné")
                    return
                
                # Validation du nom
                name = name_entry.get().strip()
                if not name:
                    status_label.config(text="❌ Veuillez entrer un nom de session", 
                                      foreground=self.colors['danger'])
                    messagebox.showwarning("Erreur", "Veuillez entrer un nom de session")
                    return
                
                # Validation des valeurs numériques
                try:
                    nb_questions = int(nb_questions_entry.get())
                    if nb_questions < 10 or nb_questions > 50:
                        status_label.config(text="❌ Le nombre de questions doit être entre 10 et 50", 
                                          foreground=self.colors['danger'])
                        messagebox.showwarning("Valeur invalide", "Le nombre de questions doit être entre 10 et 50")
                        return
                except ValueError:
                    status_label.config(text="❌ Nombre de questions invalide", 
                                      foreground=self.colors['danger'])
                    messagebox.showerror("Erreur", "Le nombre de questions doit être un nombre valide")
                    return
                
                try:
                    time_limit = int(time_limit_entry.get())
                    if time_limit < 10 or time_limit > 60:
                        status_label.config(text="❌ Le temps limite doit être entre 10 et 60 secondes", 
                                          foreground=self.colors['danger'])
                        messagebox.showwarning("Valeur invalide", "Le temps limite doit être entre 10 et 60 secondes")
                        return
                except ValueError:
                    status_label.config(text="❌ Temps limite invalide", 
                                      foreground=self.colors['danger'])
                    messagebox.showerror("Erreur", "Le temps limite doit être un nombre valide")
                    return
                
                try:
                    max_players = int(max_players_entry.get())
                    if max_players < 2:
                        status_label.config(text="❌ Le nombre maximum de joueurs doit être au moins 2", 
                                          foreground=self.colors['danger'])
                        messagebox.showwarning("Valeur invalide", "Le nombre maximum de joueurs doit être au moins 2")
                        return
                except ValueError:
                    status_label.config(text="❌ Nombre de joueurs invalide", 
                                      foreground=self.colors['danger'])
                    messagebox.showerror("Erreur", "Le nombre maximum de joueurs doit être un nombre valide")
                    return
                
                # Désactiver le bouton pendant la création
                create_btn.config(state=tk.DISABLED)
                status_label.config(text="⏳ Création de la session en cours...", 
                                  foreground=self.colors['primary'])
                
                def do_create():
                    """Fonction exécutée dans un thread séparé pour créer la session"""
                    try:
                        session_id = self.client.create_session(
                            name, theme_ids, difficulty_var.get(), nb_questions, 
                            time_limit, mode_var.get(), max_players
                        )
                        
                        if session_id:
                            # Succès - fermer la dialog et afficher la salle d'attente
                            self.root.after(0, lambda: dialog.destroy())
                            self.root.after(0, lambda: status_label.config(text="✅ Session créée avec succès !", 
                                                                          foreground=self.colors['success']if status_label.winfo_exists() else None))
                            self.root.after(0, self.show_waiting_room)
                            # Rafraîchir la liste des sessions
                            self.root.after(0, self.refresh_sessions)
                        else:
                            # Échec - réactiver le bouton et afficher l'erreur détaillée
                            # Récupérer le message d'erreur du serveur
                            error_info = self.client.last_error
                            if error_info:
                                error_statut = error_info.get("statut", "inconnu")
                                error_message = error_info.get("message", "Erreur inconnue")
                                error_action = error_info.get("action", "session/create")
                                
                                error_msg = f"Erreur {error_statut}: {error_message}"
                                
                                # Message détaillé pour la boîte de dialogue
                                if error_statut == "404":
                                    detailed_msg = (
                                        f"❌ Route non implémentée sur le serveur\n\n"
                                        f"Le serveur a retourné : {error_statut} - {error_message}\n\n"
                                        f"La route '{error_action}' n'est pas encore implémentée sur le serveur.\n"
                                        f"Il faut ajouter le traitement de cette route dans le code serveur (main.c)."
                                    )
                                else:
                                    detailed_msg = (
                                        f"❌ Erreur lors de la création de session\n\n"
                                        f"Statut: {error_statut}\n"
                                        f"Message: {error_message}\n\n"
                                        f"Vérifiez que :\n"
                                        f"- Le serveur est bien lancé\n"
                                        f"- Vous êtes connecté et authentifié\n"
                                        f"- Les paramètres sont corrects"
                                    )
                            else:
                                error_msg = "Échec de la création de session"
                                detailed_msg = (
                                    "Échec de la création de session.\n\n"
                                    "Vérifiez que :\n"
                                    "- Le serveur est bien lancé\n"
                                    "- Vous êtes connecté et authentifié\n"
                                    "- La route 'session/create' est implémentée sur le serveur"
                                )
                            
                            self.root.after(0, lambda: create_btn.config(state=tk.NORMAL))
                            self.root.after(0, lambda: status_label.config(
                                text=f"❌ {error_msg}", 
                                foreground=self.colors['danger'])if status_label.winfo_exists() else None)
                            self.root.after(0, lambda msg=detailed_msg: messagebox.showerror("Erreur de création", msg))
                    except Exception as e:
                        # Erreur inattendue
                        error_msg = f"Erreur lors de la création: {str(e)}"
                        self.root.after(0, lambda: create_btn.config(state=tk.NORMAL))
                        self.root.after(0, lambda: status_label.config(
                            text=f"❌ {error_msg}", 
                            foreground=self.colors['danger']))
                        self.root.after(0, lambda: messagebox.showerror("Erreur", error_msg))
                
                # Lancer la création dans un thread séparé
                threading.Thread(target=do_create, daemon=True).start()
                
            except Exception as e:
                # Erreur générale
                create_btn.config(state=tk.NORMAL)
                status_label.config(text=f"❌ Erreur: {str(e)}", 
                                  foreground=self.colors['danger'])
                messagebox.showerror("Erreur", f"Une erreur est survenue: {str(e)}")
        
        # Boutons avec style
        button_frame = ttk.Frame(dialog)
        button_frame.grid(row=4, column=0, columnspan=3, pady=(10, 20))
        
        create_btn = ttk.Button(button_frame, text="✅ Créer la session", command=create, 
                               style='Success.TButton')
        create_btn.pack(side=tk.LEFT, padx=10)
        
        cancel_btn = ttk.Button(button_frame, text="❌ Annuler", command=dialog.destroy,
                               style='Danger.TButton')
        cancel_btn.pack(side=tk.LEFT, padx=10)
        
        dialog.columnconfigure(0, weight=1)
    
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
            # Changer la couleur selon le temps restant
            if self.time_remaining <= 5:
                color = self.colors['danger']
            elif self.time_remaining <= 10:
                color = self.colors['warning']
            else:
                color = self.colors['primary']
            
            self.timer_label.config(
                text=f"⏱️ Temps restant: {self.time_remaining}s",
                foreground=color
            )
            self.time_remaining -= 1
            self.root.after(1000, self.update_timer)
        elif self.time_remaining <= 0:
            self.timer_label.config(
                text="⏱️ Temps écoulé",
                foreground=self.colors['danger']
            )
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

