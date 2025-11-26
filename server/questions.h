#ifndef QUESTIONS_H
#define QUESTIONS_H

// Définition de la structure Question
typedef struct {
    const char* question;
    const char* type;
    const char* options;
    const char* answer;
    const char* difficulty;
} Question;


// Déclaration des tableaux externes
extern Question geo_questions[10];
extern Question histoire_questions[10];
extern Question sciences_questions[10];
extern Question math_questions[10];
extern Question info_questions[10];
extern Question lit_questions[10];
extern Question cinema_questions[10];
extern Question musique_questions[10];
extern Question sport_questions[10];
extern Question art_questions[10];
extern Question cuisine_questions[10];
extern Question animaux_questions[10];
extern Question astro_questions[10];
extern Question techno_questions[10];
extern Question philo_questions[10];
extern Question eco_questions[10];
extern Question politique_questions[10];
extern Question langues_questions[10];
extern Question religion_questions[10];
extern Question culture_questions[10];

// Tableau global de pointeurs vers tous les tableaux
extern Question *all_questions[20];

#endif // QUESTIONS_H
