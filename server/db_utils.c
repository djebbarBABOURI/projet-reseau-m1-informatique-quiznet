#include "db_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "sqlite/sqlite3.h"

void create_tables(sqlite3 *db) {
    const char *sql_statements[] = {
        "CREATE TABLE IF NOT EXISTS players ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "pseudo TEXT UNIQUE NOT NULL,"
        "password_hash TEXT NOT NULL,"
        "created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");",

        "CREATE TABLE IF NOT EXISTS themes ("
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    name TEXT"
        ");",

        "CREATE TABLE IF NOT EXISTS questions ("
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    theme_id INTEGER REFERENCES themes(id),"
        "    difficulty TEXT CHECK(difficulty IN ('facile','moyen','difficile')),"
        "    type TEXT CHECK(type IN ('qcm','boolean','text')),"
        "    enonce TEXT,"
        "    answers TEXT,"
        "    correct_answer TEXT"
        ");",

        "CREATE TABLE IF NOT EXISTS sessions ("
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    name TEXT,"
        "    difficulty TEXT,"
        "    nb_questions INTEGER,"
        "    time_limit INTEGER,"
        "    mode TEXT CHECK(mode IN ('solo','battle')),"
        "    max_players INTEGER,"
        "    status TEXT CHECK(status IN ('waiting','playing','finished')),"
        "    created_by INTEGER REFERENCES players(id)"
        ");",

        "CREATE TABLE IF NOT EXISTS themes_session ("
        "    session_id INTEGER REFERENCES sessions(id),"
        "    theme_id INTEGER REFERENCES themes(id),"
        "    PRIMARY KEY(session_id, theme_id)"
        ");",

        "CREATE TABLE IF NOT EXISTS players_session ("
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    player_id INTEGER REFERENCES players(id),"
        "    session_id INTEGER REFERENCES sessions(id),"
        "    lives INTEGER,"
        "    current_score INTEGER,"
        "    is_creator INTEGER,"
        "    eliminated_at INTEGER"
        ");",

        "CREATE TABLE IF NOT EXISTS jokers ("
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    players_session_id INTEGER REFERENCES players_session(id),"
        "    fifty INTEGER,"
        "    skip INTEGER"
        ");",

        "CREATE TABLE IF NOT EXISTS session_questions ("
        "    session_id INTEGER REFERENCES sessions(id),"
        "    question_id INTEGER REFERENCES questions(id),"
        "    num_question INTEGER,"
        "    PRIMARY KEY(session_id, num_question)"
        ");",

        "CREATE TABLE IF NOT EXISTS answers ("
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    players_session_id INTEGER REFERENCES players_session(id),"
        "    question_id INTEGER REFERENCES questions(id),"
        "    answer TEXT,"
        "    response_time REAL,"
        "    correct INTEGER,"
        "    points INTEGER"
        ");",

        "CREATE TABLE IF NOT EXISTS ranking ("
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    session_id INTEGER REFERENCES sessions(id),"
        "    player_id INTEGER REFERENCES players(id),"
        "    rank INTEGER,"
        "    score INTEGER,"
        "    correct_answers INTEGER,"
        "    eliminated_at INTEGER"
        ");"
    };

    size_t count = sizeof(sql_statements) / sizeof(sql_statements[0]);
    for (size_t i = 0; i < count; i++) {
        char *errmsg = 0;
        if (sqlite3_exec(db, sql_statements[i], 0, 0, &errmsg) != SQLITE_OK) {
            fprintf(stderr, "SQL error: %s\n", errmsg);
            sqlite3_free(errmsg);
        }
    }
}


// 1. Géographie
Question geo_questions[10] = {
    {"Quelle est la capitale de la France ?", "text", "[]", "[\"Paris\"]", "facile"},
    {"La Terre possède 7 continents.", "boolean", "[]", "[\"true\"]", "facile"},
    {"Quel est le plus grand désert du monde ?", "qcm", "[\"Sahara\",\"Gobi\",\"Antarctique\",\"Kalahari\"]", "[\"Antarctique\"]", "moyen"},
    {"Quels pays font partie de l''Amérique du Nord ?", "qcm", "[\"États-Unis\",\"Mexique\",\"Brésil\",\"Canada\"]", "[\"États-Unis\",\"Mexique\",\"Canada\"]", "moyen"},
    {"Dans quel pays se trouve Kyoto ?", "text", "[]", "[\"Japon\"]", "facile"},
    {"L''Australie est plus grande que l''Europe.", "boolean", "[]", "[\"true\"]", "facile"},
    {"Quel fleuve traverse Londres ?", "qcm", "[\"Thames\",\"Seine\",\"Rhin\",\"Volga\"]", "[\"Thames\"]", "moyen"},
    {"Quels continents sont entièrement dans l''hémisphère sud ?", "qcm", "[\"Australie\",\"Antarctique\",\"Afrique\",\"Amérique du Sud\"]", "[\"Australie\",\"Antarctique\"]", "difficile"},
    {"Quel est le plus haut sommet du monde ?", "text", "[]", "[\"Everest\"]", "moyen"},
    {"Quel est l''océan à l''Est des États-Unis ?", "qcm", "[\"Atlantique\",\"Indien\",\"Pacifique\",\"Arctique\"]", "[\"Atlantique\"]", "facile"}
};

// 2. Histoire
Question histoire_questions[10] = {
    {"En quelle année a eu lieu la Révolution française ?", "text", "[]", "[\"1789\"]", "facile"},
    {"La Première Guerre mondiale a débuté en 1914.", "boolean", "[]", "[\"true\"]", "facile"},
    {"Qui était le premier empereur romain ?", "qcm", "[\"Auguste\",\"Néron\",\"Jules César\",\"Trajan\"]", "[\"Auguste\"]", "moyen"},
    {"Quel mur est tombé en 1989 ?", "qcm", "[\"Mur de Berlin\",\"Grande Muraille de Chine\",\"Mur d''Hadrien\",\"Mur d''Adriano\"]", "[\"Mur de Berlin\"]", "facile"},
    {"Qui a découvert l''Amérique en 1492 ?", "text", "[]", "[\"Christophe Colomb\"]", "facile"},
    {"Quel roi est surnommé le \\\"Roi Soleil\\\" ?", "text", "[]", "[\"Louis XIV\"]", "moyen"},
    {"Quand a eu lieu la Seconde Guerre mondiale ?", "text", "[]", "[\"1939-1945\"]", "facile"},
    {"Qui a signé la Déclaration d''Indépendance américaine ?", "qcm", "[\"Thomas Jefferson\",\"George Washington\",\"Benjamin Franklin\",\"John Adams\"]", "[\"Thomas Jefferson\"]", "moyen"},
    {"Quel traité a mis fin à la Première Guerre mondiale ?", "text", "[]", "[\"Traité de Versailles\"]", "moyen"},
    {"Napoléon Bonaparte a été empereur des Français.", "boolean", "[]", "[\"true\"]", "facile"}
};

// 3. Sciences
Question sciences_questions[10] = {
    {"Quelle planète est connue comme la planète rouge ?", "qcm", "[\"Mars\",\"Vénus\",\"Jupiter\",\"Mercure\"]", "[\"Mars\"]", "facile"},
    {"L''oxygène est le gaz que nous respirons principalement.", "boolean", "[]", "[\"true\"]", "facile"},
    {"Quelle est la formule chimique de l''eau ?", "text", "[]", "[\"H2O\"]", "facile"},
    {"Qui a développé la théorie de la relativité ?", "text", "[]", "[\"Albert Einstein\"]", "moyen"},
    {"Qu''est-ce que la photosynthèse ?", "text", "[]", "[\"Processus par lequel les plantes transforment la lumière en énergie\"]", "moyen"},
    {"Quel est l''organe principal du système circulatoire ?", "qcm", "[\"Cœur\",\"Foie\",\"Poumon\",\"Rein\"]", "[\"Cœur\"]", "facile"},
    {"Quel est le symbole chimique du fer ?", "qcm", "[\"Fe\",\"Ir\",\"F\",\"Fr\"]", "[\"Fe\"]", "facile"},
    {"Combien de chromosomes humains y a-t-il ?", "text", "[]", "[\"46\"]", "facile"},
    {"La vitesse de la lumière est d''environ 300 000 km/s.", "boolean", "[]", "[\"true\"]", "facile"},
    {"Qui a inventé le microscope ?", "text", "[]", "[\"Anton van Leeuwenhoek\"]", "moyen"}
};

// 4. Mathématiques
Question math_questions[10] = {
    {"Combien font 7 + 8 ?", "qcm", "[\"14\",\"15\",\"16\",\"13\"]", "[\"15\"]", "facile"},
    {"La racine carrée de 64 est 8.", "boolean", "[]", "[\"true\"]", "facile"},
    {"Combien y a-t-il de degrés dans un cercle ?", "text", "[]", "[\"360\"]", "facile"},
    {"Quel est le résultat de 9 x 6 ?", "qcm", "[\"54\",\"45\",\"63\",\"48\"]", "[\"54\"]", "facile"},
    {"Combien de côtés a un hexagone ?", "text", "[]", "[\"6\"]", "facile"},
    {"La valeur de Pi est approximativement 3.1416.", "boolean", "[]", "[\"true\"]", "facile"},
    {"Quel est le carré de 12 ?", "qcm", "[\"144\",\"124\",\"132\",\"112\"]", "[\"144\"]", "facile"},
    {"Quel est le résultat de 15 ÷ 3 ?", "text", "[]", "[\"5\"]", "facile"},
    {"La somme des angles d''un triangle est 180 degrés.", "boolean", "[]", "[\"true\"]", "facile"},
    {"Combien font 11 x 11 ?", "qcm", "[\"121\",\"111\",\"132\",\"99\"]", "[\"121\"]", "facile"}
};

// 5. Informatique
Question info_questions[10] = {
    {"Que signifie HTML ?", "text", "[]", "[\"HyperText Markup Language\"]", "facile"},
    {"Quel langage est utilisé pour les styles CSS ?", "text", "[]", "[\"CSS (Cascading Style Sheets)\"]", "facile"},
    {"Que signifie CPU ?", "text", "[]", "[\"Central Processing Unit\"]", "facile"},
    {"Quel est le système d''exploitation de Google pour mobiles ?", "qcm", "[\"Android\",\"iOS\",\"Windows Phone\",\"BlackBerry OS\"]", "[\"Android\"]", "facile"},
    {"Qu''est-ce qu''un algorithme ?", "text", "[]", "[\"Suite d''instructions pour résoudre un problème\"]", "moyen"},
    {"Quel langage utilise la fonction \\\"print()\\\" ?", "qcm", "[\"Python\",\"C\",\"Java\",\"Ruby\"]", "[\"Python\"]", "facile"},
    {"Que signifie SQL ?", "text", "[]", "[\"Structured Query Language\"]", "facile"},
    {"Qu''est-ce qu''une variable en programmation ?", "text", "[]", "[\"Emplacement mémoire contenant une valeur\"]", "facile"},
    {"Quel est le langage principal d''Android ?", "qcm", "[\"Java\",\"Swift\",\"Kotlin\",\"C#\"]", "[\"Kotlin\"]", "facile"},
    {"Que signifie RAM ?", "text", "[]", "[\"Random Access Memory\"]", "facile"}
};

// 6. Littérature
Question lit_questions[10] = {
    {"Qui a écrit \\\"Les Misérables\\\" ?", "text", "[]", "[\"Victor Hugo\"]", "facile"},
    {"Qui est l''auteur de \\\"1984\\\" ?", "text", "[]", "[\"George Orwell\"]", "facile"},
    {"Quel est le célèbre détective créé par Arthur Conan Doyle ?", "qcm", "[\"Sherlock Holmes\",\"Hercule Poirot\",\"Miss Marple\",\"Philip Marlowe\"]", "[\"Sherlock Holmes\"]", "facile"},
    {"Qui a écrit \\\"Le Petit Prince\\\" ?", "text", "[]", "[\"Antoine de Saint-Exupéry\"]", "facile"},
    {"Qui a écrit \\\"Hamlet\\\" ?", "text", "[]", "[\"William Shakespeare\"]", "facile"},
    {"Quel roman commence par \\\"Call me Ishmael\\\" ?", "text", "[]", "[\"Moby-Dick\"]", "moyen"},
    {"Qui a écrit \\\"Germinal\\\" ?", "text", "[]", "[\"Émile Zola\"]", "moyen"},
    {"Qui est l''auteur de \\\"La Peste\\\" ?", "text", "[]", "[\"Albert Camus\"]", "moyen"},
    {"Quel poète français a écrit \\\"Les Fleurs du mal\\\" ?", "text", "[]", "[\"Charles Baudelaire\"]", "moyen"},
    {"Qui a écrit \\\"Le Rouge et le Noir\\\" ?", "text", "[]", "[\"Stendhal\"]", "moyen"}
};

// 7. Cinéma
Question cinema_questions[10] = {
    {"Qui a réalisé \\\"Titanic\\\" ?", "text", "[]", "[\"James Cameron\"]", "facile"},
    {"Quel film a remporté l''Oscar du meilleur film en 1994 ?", "qcm", "[\"Forrest Gump\",\"Pulp Fiction\",\"The Shawshank Redemption\",\"Four Weddings and a Funeral\"]", "[\"Forrest Gump\"]", "facile"},
    {"Qui joue Jack Sparrow dans \\\"Pirates des Caraïbes\\\" ?", "text", "[]", "[\"Johnny Depp\"]", "facile"},
    {"Quel est le réalisateur de \\\"Inception\\\" ?", "text", "[]", "[\"Christopher Nolan\"]", "moyen"},
    {"Quel film a pour héros \\\"Forrest Gump\\\" ?", "text", "[]", "[\"Forrest Gump\"]", "facile"},
    {"Quel film d''animation a été produit par Pixar en 2003 ?", "qcm", "[\"Finding Nemo\",\"Toy Story 2\",\"Monsters, Inc.\",\"The Incredibles\"]", "[\"Finding Nemo\"]", "facile"},
    {"Qui a réalisé \\\"Avatar\\\" ?", "text", "[]", "[\"James Cameron\"]", "facile"},
    {"Quel film met en scène \\\"Darth Vader\\\" ?", "qcm", "[\"Star Wars\",\"Star Trek\",\"Guardians of the Galaxy\",\"Battlestar Galactica\"]", "[\"Star Wars\"]", "facile"},
    {"Quel film se déroule à Gotham City ?", "text", "[]", "[\"Batman\"]", "facile"},
    {"Quel film raconte l''histoire de Simba ?", "text", "[]", "[\"Le Roi Lion\"]", "facile"}
};

// 8. Musique
Question musique_questions[10] = {
    {"Qui a chanté \\\"Thriller\\\" ?", "text", "[]", "[\"Michael Jackson\"]", "facile"},
    {"Quel groupe a composé \\\"Bohemian Rhapsody\\\" ?", "text", "[]", "[\"Queen\"]", "facile"},
    {"Qui est surnommé le King of Pop ?", "text", "[]", "[\"Michael Jackson\"]", "facile"},
    {"Quel est l''instrument principal d''un pianiste ?", "text", "[]", "[\"Piano\"]", "facile"},
    {"Quel groupe est connu pour \\\"Hotel California\\\" ?", "text", "[]", "[\"Eagles\"]", "facile"},
    {"Qui a chanté \\\"Like a Virgin\\\" ?", "text", "[]", "[\"Madonna\"]", "facile"},
    {"Quel est le style musical du reggae ?", "text", "[]", "[\"Reggae\"]", "facile"},
    {"Qui a composé la 9ème symphonie ?", "text", "[]", "[\"Ludwig van Beethoven\"]", "moyen"},
    {"Quel est l''instrument principal d''un violoniste ?", "text", "[]", "[\"Violon\"]", "facile"},
    {"Qui a chanté \\\"Imagine\\\" ?", "text", "[]", "[\"John Lennon\"]", "facile"}
};

// 9. Sport
Question sport_questions[10] = {
    {"Combien de joueurs dans une équipe de football ?", "qcm", "[\"9\",\"10\",\"11\",\"12\"]", "[\"11\"]", "facile"},
    {"Quelle ville a accueilli les JO 2016 ?", "text", "[]", "[\"Rio de Janeiro\"]", "facile"},
    {"Qui a remporté la Coupe du Monde 2018 ?", "qcm", "[\"France\",\"Croatie\",\"Brésil\",\"Allemagne\"]", "[\"France\"]", "facile"},
    {"Quel sport utilise une raquette et un volant ?", "qcm", "[\"Tennis\",\"Badminton\",\"Squash\",\"Ping-pong\"]", "[\"Badminton\"]", "facile"},
    {"Qui est surnommé \\\"Le Roi du Tennis\\\" ?", "qcm", "[\"Roger Federer\",\"Rafael Nadal\",\"Novak Djokovic\",\"Pete Sampras\"]", "[\"Roger Federer\"]", "facile"},
    {"Combien de minutes dure un match de rugby ?", "text", "[]", "[\"80\"]", "facile"},
    {"Quel pays a remporté l''Euro 2020 (tourné en 2021) ?", "qcm", "[\"Italie\",\"Angleterre\",\"Portugal\",\"France\"]", "[\"Italie\"]", "facile"},
    {"Quel sport pratiquait Michael Jordan ?", "text", "[]", "[\"Basket-ball\"]", "facile"},
    {"Quel pays a remporté la Coupe du Monde 2010 ?", "text", "[]", "[\"Espagne\"]", "facile"},
    {"Quelle distance fait un marathon ?", "text", "[]", "[\"42.195 km\"]", "facile"}
};

// 10. Art
Question art_questions[10] = {
    {"Qui a peint la Joconde ?", "text", "[]", "[\"Léonard de Vinci\"]", "facile"},
    {"Quel peintre est célèbre pour les tournesols ?", "text", "[]", "[\"Vincent van Gogh\"]", "facile"},
    {"Qui a sculpté le David ?", "text", "[]", "[\"Michel-Ange\"]", "facile"},
    {"Quel mouvement artistique est Picasso associé ?", "qcm", "[\"Cubisme\",\"Surréalisme\",\"Impressionnisme\",\"Baroque\"]", "[\"Cubisme\"]", "moyen"},
    {"Quel artiste a peint \\\"La Nuit étoilée\\\" ?", "text", "[]", "[\"Vincent van Gogh\"]", "facile"},
    {"Qui a peint \\\"Guernica\\\" ?", "text", "[]", "[\"Pablo Picasso\"]", "facile"},
    {"Quel artiste est connu pour ses boîtes de soupe Campbell ?", "text", "[]", "[\"Andy Warhol\"]", "facile"},
    {"Qui a peint \\\"Le Cri\\\" ?", "text", "[]", "[\"Edvard Munch\"]", "facile"},
    {"Quel mouvement artistique est Salvador Dali associé ?", "text", "[]", "[\"Surréalisme\"]", "moyen"},
    {"Qui a peint \\\"La Cène\\\" ?", "text", "[]", "[\"Léonard de Vinci\"]", "facile"}
};

// 11. Cuisine
Question cuisine_questions[10] = {
    {"Quel pays est célèbre pour les sushis ?", "text", "[]", "[\"Japon\"]", "facile"},
    {"Quel fromage vient de Suisse ?", "qcm", "[\"Gruyère\",\"Camembert\",\"Roquefort\",\"Cheddar\"]", "[\"Gruyère\"]", "facile"},
    {"Quel ingrédient est essentiel dans la pâte à crêpes ?", "qcm", "[\"Farine\",\"Tomate\",\"Riz\",\"Beurre\"]", "[\"Farine\"]", "facile"},
    {"Quel fruit est utilisé pour faire du guacamole ?", "qcm", "[\"Avocat\",\"Citron\",\"Tomate\",\"Mangue\"]", "[\"Avocat\"]", "facile"},
    {"Quelle herbe est typique du pesto ?", "qcm", "[\"Basilic\",\"Persil\",\"Coriandre\",\"Thym\"]", "[\"Basilic\"]", "facile"},
    {"Quel pays est célèbre pour la pizza ?", "text", "[]", "[\"Italie\"]", "facile"},
    {"Quel légume est utilisé pour la ratatouille ?", "qcm", "[\"Aubergine\",\"Pomme de terre\",\"Chou\",\"Navet\"]", "[\"Aubergine\"]", "facile"},
    {"Quel aliment est fermenté pour faire du kimchi ?", "qcm", "[\"Chou\",\"Riz\",\"Poisson\",\"Soja\"]", "[\"Chou\"]", "facile"},
    {"Quel dessert italien est à base de mascarpone ?", "text", "[]", "[\"Tiramisu\"]", "facile"},
    {"Quel aliment est la base du couscous ?", "text", "[]", "[\"Semoule\"]", "facile"}
};

// 12. Animaux
Question animaux_questions[10] = {
    {"Quel est le plus grand mammifère terrestre ?", "qcm", "[\"Éléphant d''Afrique\",\"Girafe\",\"Rhinocéros\",\"Hippopotame\"]", "[\"Éléphant d''Afrique\"]", "facile"},
    {"Quel animal pond des œufs ?", "qcm", "[\"Autruche\",\"Chien\",\"Chat\",\"Chèvre\"]", "[\"Autruche\"]", "facile"},
    {"Quel est l''oiseau le plus rapide ?", "qcm", "[\"Faucon pèlerin\",\"Aigle royal\",\"Albatros\",\"Colibri\"]", "[\"Faucon pèlerin\"]", "facile"},
    {"Le dauphin est un mammifère.", "boolean", "[]", "[\"true\"]", "facile"},
    {"Quel animal est surnommé le roi de la jungle ?", "qcm", "[\"Lion\",\"Tigre\",\"Éléphant\",\"Panthère\"]", "[\"Lion\"]", "facile"},
    {"Combien de pattes a une araignée ?", "qcm", "[\"6\",\"8\",\"10\",\"4\"]", "[\"8\"]", "facile"},
    {"Quel mammifère peut voler ?", "qcm", "[\"Chauve-souris\",\"Souris\",\"Écureuil\",\"Colibri\"]", "[\"Chauve-souris\"]", "facile"},
    {"Le caméléon peut changer de couleur.", "boolean", "[]", "[\"true\"]", "facile"},
    {"Quel animal a une trompe ?", "qcm", "[\"Éléphant\",\"Rhinocéros\",\"Girafe\",\"Hippopotame\"]", "[\"Éléphant\"]", "facile"},
    {"Quel animal est connu pour sa lenteur ?", "qcm", "[\"Paresseux\",\"Tortue\",\"Escargot\",\"Koala\"]", "[\"Paresseux\"]", "facile"}
};

// 13. Économie
Question eco_questions[10] = {
    {"Quel pays a la plus grande économie du monde ?", "text", "[]", "[\"États-Unis\"]", "facile"},
    {"Le PIB signifie Produit Intérieur Brut.", "boolean", "[]", "[\"true\"]", "facile"},
    {"Quel est le principal indicateur de l''inflation ?", "qcm", "[\"Indice des prix à la consommation\",\"Taux de chômage\",\"PIB\",\"Taux directeur\"]", "[\"Indice des prix à la consommation\"]", "moyen"},
    {"L''offre et la demande déterminent le prix d''un bien sur le marché.", "boolean", "[]", "[\"true\"]", "facile"},
    {"Quel est l''objectif principal de la politique monétaire ?", "qcm", "[\"Stabilité des prix\",\"Éducation\",\"Santé\",\"Environnement\"]", "[\"Stabilité des prix\"]", "facile"},
    {"Quelle institution contrôle la masse monétaire dans un pays ?", "text", "[]", "[\"Banque centrale\"]", "facile"},
    {"Le chômage structurel résulte d''un décalage entre les compétences et les emplois.", "boolean", "[]", "[\"true\"]", "moyen"},
    {"Quel terme désigne une période de forte récession économique ?", "qcm", "[\"Crise\",\"Boom\",\"Inflation\",\"Déflation\"]", "[\"Crise\"]", "facile"},
    {"La mondialisation favorise les échanges commerciaux internationaux.", "boolean", "[]", "[\"true\"]", "facile"},
    {"Quel est le rôle des marchés financiers ?", "text", "[]", "[\"Faciliter l''échange de capitaux entre investisseurs et entreprises\"]", "moyen"}
};

Question astro_questions[10] = {
    {"Quelle planète est la plus proche du Soleil ?", "qcm", "[\"Mercure\",\"Vénus\",\"Terre\",\"Mars\"]", "[\"Mercure\"]", "facile"},
    {"La Lune est un satellite naturel de la Terre.", "boolean", "[]", "[\"true\"]", "facile"},
    {"Combien de planètes dans le système solaire ?", "text", "[]", "[\"8\"]", "facile"},
    {"Quelle est la planète géante gazeuse la plus massive ?", "qcm", "[\"Jupiter\",\"Saturne\",\"Uranus\",\"Neptune\"]", "[\"Jupiter\"]", "facile"},
    {"Quel est le plus grand satellite de Jupiter ?", "text", "[]", "[\"Ganymède\"]", "moyen"},
    {"Quelle est la planète surnommée la planète bleue ?", "qcm", "[\"Terre\",\"Neptune\",\"Uranus\",\"Vénus\"]", "[\"Terre\"]", "facile"},
    {"Quelle planète est la plus chaude ?", "qcm", "[\"Vénus\",\"Mercure\",\"Mars\",\"Jupiter\"]", "[\"Vénus\"]", "moyen"},
    {"Combien de satellites naturels a Saturne ?", "text", "[]", "[\"82\"]", "difficile"},
    {"Le Soleil est une étoile.", "boolean", "[]", "[\"true\"]", "facile"},
    {"Quelle planète a des anneaux visibles ?", "qcm", "[\"Saturne\",\"Jupiter\",\"Mars\",\"Vénus\"]", "[\"Saturne\"]", "facile"}
};


Question techno_questions[10] = {
    {"Quel est le fondateur de Microsoft ?", "qcm", "[\"Bill Gates\",\"Steve Jobs\",\"Mark Zuckerberg\",\"Elon Musk\"]", "[\"Bill Gates\"]", "facile"},
    {"Quel réseau social appartient à Meta ?", "qcm", "[\"Facebook\",\"Twitter\",\"Snapchat\",\"TikTok\"]", "[\"Facebook\"]", "facile"},
    {"Quel langage est utilisé pour Android ?", "qcm", "[\"Kotlin\",\"Swift\",\"Python\",\"C\"]", "[\"Kotlin\"]", "facile"},
    {"Que signifie IoT ?", "text", "[]", "[\"Internet of Things\"]", "moyen"},
    {"Quel moteur de recherche est le plus utilisé ?", "qcm", "[\"Google\",\"Bing\",\"Yahoo\",\"DuckDuckGo\"]", "[\"Google\"]", "facile"},
    {"Quel est le système d''exploitation open source le plus connu ?", "qcm", "[\"Linux\",\"Windows\",\"macOS\",\"Android\"]", "[\"Linux\"]", "facile"},
    {"Quelle technologie permet les paiements sans contact ?", "qcm", "[\"NFC\",\"RFID\",\"Bluetooth\",\"WiFi\"]", "[\"NFC\"]", "facile"},
    {"Que signifie AI ?", "text", "[]", "[\"Artificial Intelligence\"]", "facile"},
    {"Qui a créé le langage Python ?", "text", "[]", "[\"Guido van Rossum\"]", "moyen"},
    {"Le Bitcoin est une cryptomonnaie.", "boolean", "[]", "[\"true\"]", "facile"}
};

Question philo_questions[10] = {
    {"Qui a écrit \\\"La République\\\" ?", "text", "[]", "[\"Platon\"]", "facile"},
    {"Socrate a été condamné à mort.", "boolean", "[]", "[\"true\"]", "facile"},
    {"Qui a écrit \\\"Critique de la raison pure\\\" ?", "text", "[]", "[\"Immanuel Kant\"]", "moyen"},
    {"Qu''est-ce que l''existentialisme ?", "text", "[]", "[\"Courant philosophique centré sur l''existence humaine\"]", "moyen"},
    {"Aristote était l''élève de ?", "qcm", "[\"Platon\",\"Socrate\",\"Pythagore\",\"Épicure\"]", "[\"Platon\"]", "facile"},
    {"Descartes a dit \\\"Je pense, donc je suis\\\".", "boolean", "[]", "[\"true\"]", "facile"},
    {"Qui a écrit \\\"Ainsi parlait Zarathoustra\\\" ?", "text", "[]", "[\"Friedrich Nietzsche\"]", "moyen"},
    {"Le stoïcisme enseigne à contrôler ses émotions.", "boolean", "[]", "[\"true\"]", "facile"},
    {"Épicure est connu pour ?", "qcm", "[\"Recherche du plaisir raisonnable\",\"La guerre\",\"La politique\",\"La rhétorique\"]", "[\"Recherche du plaisir raisonnable\"]", "moyen"},
    {"Qui est le père de la philosophie moderne ?", "text", "[]", "[\"René Descartes\"]", "moyen"}
};


// 17. Politique
Question politique_questions[10] = {
    {"Qui est le président actuel des États-Unis ?", "text", "[]", "[\"Joe Biden\"]", "facile"},
    {"Le parlement français est bicaméral.", "boolean", "[]", "[\"true\"]", "facile"},
    {"Quel parti politique Angela Merkel a-t-elle dirigé ?", "qcm", "[\"CDU\",\"SPD\",\"FDP\",\"Die Linke\"]", "[\"CDU\"]", "facile"},
    {"Que signifie ONU ?", "text", "[]", "[\"Organisation des Nations Unies\"]", "facile"},
    {"Quel pays a le système politique de la monarchie constitutionnelle ?", "qcm", "[\"Royaume-Uni\",\"États-Unis\",\"France\",\"Russie\"]", "[\"Royaume-Uni\"]", "facile"},
    {"Le suffrage universel est le droit de vote pour tous.", "boolean", "[]", "[\"true\"]", "facile"},
    {"Qui est le Premier ministre britannique en 2024 ?", "text", "[]", "[\"Rishi Sunak\"]", "moyen"},
    {"Quel pays pratique le système présidentiel ?", "qcm", "[\"États-Unis\",\"Royaume-Uni\",\"Allemagne\",\"Espagne\"]", "[\"États-Unis\"]", "facile"},
    {"La démocratie est un système politique où le pouvoir appartient au peuple.", "boolean", "[]", "[\"true\"]", "facile"},
    {"Que signifie UE ?", "text", "[]", "[\"Union Européenne\"]", "facile"}
};

// 18. Langues
Question langues_questions[10] = {
    {"Quelle est la langue officielle du Brésil ?", "text", "[]", "[\"Portugais\"]", "facile"},
    {"L''anglais est une langue germanique.", "boolean", "[]", "[\"true\"]", "facile"},
    {"Quel est le pays où l''espagnol est parlé ?", "qcm", "[\"Mexique\",\"France\",\"Italie\",\"Allemagne\"]", "[\"Mexique\"]", "facile"},
    {"Que signifie \\\"Bonjour\\\" en anglais ?", "text", "[]", "[\"Hello\"]", "facile"},
    {"Quelle langue utilise les kanjis ?", "qcm", "[\"Japonais\",\"Chinois\",\"Coréen\",\"Vietnamien\"]", "[\"Japonais\"]", "moyen"},
    {"Quelle langue est la plus parlée dans le monde ?", "qcm", "[\"Mandarin\",\"Anglais\",\"Espagnol\",\"Hindi\"]", "[\"Mandarin\"]", "facile"},
    {"Quelle est la langue officielle de la Suisse ?", "qcm", "[\"Allemand\",\"Français\",\"Italien\",\"Toutes les réponses\"]", "[\"Toutes les réponses\"]", "facile"},
    {"Quelle langue est utilisée en Algérie ?", "qcm", "[\"Arabe\",\"Français\",\"Tamazight\",\"Toutes les réponses\"]", "[\"Toutes les réponses\"]", "facile"},
    {"Le français est une langue romane.", "boolean", "[]", "[\"true\"]", "facile"},
    {"Que signifie \\\"Gracias\\\" en français ?", "text", "[]", "[\"Merci\"]", "facile"}
};

// 19. Religion
Question religion_questions[10] = {
    {"Combien de livres dans la Bible ?", "text", "[]", "[\"66\"]", "moyen"},
    {"Le Coran est le livre sacré de l''islam.", "boolean", "[]", "[\"true\"]", "facile"},
    {"Qui a fondé le christianisme ?", "text", "[]", "[\"Jésus-Christ\"]", "facile"},
    {"Quel est le lieu saint des musulmans ?", "text", "[]", "[\"La Mecque\"]", "facile"},
    {"Le bouddhisme a été fondé en Inde.", "boolean", "[]", "[\"true\"]", "facile"},
    {"Quel est le dieu principal dans l''hindouisme ?", "qcm", "[\"Brahma\",\"Vishnu\",\"Shiva\",\"Tous\"]", "[\"Tous\"]", "moyen"},
    {"Quel est le jour saint des chrétiens ?", "qcm", "[\"Dimanche\",\"Samedi\",\"Vendredi\",\"Lundi\"]", "[\"Dimanche\"]", "facile"},
    {"Combien de piliers dans l''islam ?", "text", "[]", "[\"5\"]", "facile"},
    {"La Torah est le livre sacré du judaïsme.", "boolean", "[]", "[\"true\"]", "facile"},
    {"Quelle religion pratique la méditation pour atteindre le Nirvana ?", "text", "[]", "[\"Bouddhisme\"]", "facile"}
};

// 20. Culture générale
Question culture_questions[10] = {
    {"Quelle est la capitale de l''Australie ?", "text", "[]", "[\"Canberra\"]", "facile"},
    {"La Terre est-elle plate ?", "boolean", "[]", "[\"false\"]", "facile"},
    {"Quel est le plus grand océan ?", "qcm", "[\"Pacifique\",\"Atlantique\",\"Indien\",\"Arctique\"]", "[\"Pacifique\"]", "facile"},
    {"Quel est le plus long fleuve du monde ?", "qcm", "[\"Nil\",\"Amazone\",\"Yangtsé\",\"Mississippi\"]", "[\"Nil\"]", "moyen"},
    {"Combien de couleurs dans l''arc-en-ciel ?", "text", "[]", "[\"7\"]", "facile"},
    {"Le mont Everest est la plus haute montagne.", "boolean", "[]", "[\"true\"]", "facile"},
    {"Qui a peint \\\"La Joconde\\\" ?", "text", "[]", "[\"Léonard de Vinci\"]", "facile"},
    {"Quel pays a inventé le papier ?", "qcm", "[\"Chine\",\"Égypte\",\"Grèce\",\"Italie\"]", "[\"Chine\"]", "facile"},
    {"Quel élément chimique a pour symbole \\\"O\\\" ?", "qcm", "[\"Oxygène\",\"Or\",\"Osmium\",\"Oganesson\"]", "[\"Oxygène\"]", "facile"},
    {"Combien de joueurs y a-t-il dans une équipe de rugby ?", "qcm", "[\"13\",\"14\",\"15\",\"16\"]", "[\"15\"]", "facile"}
};

// Tableaux de questions
Question *all_questions[20] = {
    geo_questions,       // Géographie
    histoire_questions,  // Histoire
    sciences_questions,  // Sciences
    math_questions,      // Mathématiques
    info_questions,      // Informatique
    lit_questions,       // Littérature
    cinema_questions,    // Cinéma
    musique_questions,   // Musique
    sport_questions,     // Sport
    art_questions,       // Art
    cuisine_questions,   // Cuisine
    animaux_questions,   // Animaux
    astro_questions,     // Astronomie
    techno_questions,    // Technologie
    philo_questions,     // Philosophie
    eco_questions,       // Économie
    politique_questions, // Politique
    langues_questions,   // Langues
    religion_questions,  // Religion
    culture_questions    // Culture générale
};

// Noms des thèmes dans le même ordre
const char *themes_names[20] = {
    "Géographie",
    "Histoire",
    "Sciences",
    "Mathématiques",
    "Informatique",
    "Littérature",
    "Cinéma",
    "Musique",
    "Sport",
    "Art",
    "Cuisine",
    "Animaux",
    "Astronomie",
    "Technologie",
    "Philosophie",
    "Économie",
    "Politique",
    "Langues",
    "Religion",
    "Culture générale"
};


void populate_db(sqlite3 *db) {
    for (int t = 0; t < 20; t++) {
        if (insert_theme(db, themes_names[t]) != SQLITE_OK) continue;
        int theme_id = (int)sqlite3_last_insert_rowid(db);

        for (int q = 0; q < 10; q++) {
            Question *qn = &all_questions[t][q];

            if (insert_question(db,
                                theme_id,
                                qn->difficulty,
                                qn->type,
                                qn->question,
                                qn->options,
                                qn->answer) != SQLITE_OK) {
                fprintf(stderr, "Erreur insertion question %d thème %d (type='%s')\n", q, t, qn->type);
            }
        }
    }
}

// Fonction pour ajouter un thème
int insert_theme(sqlite3 *db, const char *name) {
    char sql[256];
    snprintf(sql, sizeof(sql),
             "INSERT INTO themes(name) VALUES('%s');", name);

    char *err_msg = NULL;
    int rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error (insert_theme): %s\n", err_msg);
        sqlite3_free(err_msg);
        return rc;
    }
    return SQLITE_OK;
}

// Fonction pour ajouter une question
int insert_question(sqlite3 *db, int theme_id, const char *difficulty,
                    const char *type, const char *enonce,
                    const char *answers_json, const char *correct_answer_json) {
    char sql[4096];
    snprintf(sql, sizeof(sql),
             "INSERT INTO questions(theme_id, difficulty, type, enonce, answers, correct_answer) "
             "VALUES(%d, '%s', '%s', '%s', '%s', '%s');",
             theme_id,
             difficulty,
             type,
             enonce,
             answers_json,
             correct_answer_json);

    char *err_msg = NULL;
    int rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error (insert_question): %s\n", err_msg);
        sqlite3_free(err_msg);
        return rc;
    }
    return SQLITE_OK;
}