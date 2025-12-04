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
extern Question geo_questions[];
extern Question histoire_questions[];
extern Question sciences_questions[];
extern Question math_questions[];
extern Question info_questions[];
extern Question lit_questions[];
extern Question cinema_questions[];
extern Question musique_questions[];
extern Question sport_questions[];
extern Question art_questions[];
extern Question cuisine_questions[];
extern Question animaux_questions[];
extern Question astro_questions[];
extern Question techno_questions[];
extern Question philo_questions[];
extern Question eco_questions[];
extern Question politique_questions[];
extern Question langues_questions[];
extern Question religion_questions[];
extern Question culture_questions[];

// Tableau global de pointeurs vers tous les tableaux
extern Question *all_questions[20];

#endif // QUESTIONS_H
