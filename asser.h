#ifndef ASSER_H
#define ASSER_H

#include "trajectoire.h"

typedef struct
{
	int preced;
	int actuelle;
	long int sum;
} s_erreur;

void asser(s_consigne consigne);
void init_asser();
void update_erreurs(s_consigne consigne);
void mise_echelle(long int * commande_D, long int * commande_G);
void ecretage(long int * reponse,long int reponse_preced);
void ecretage_acceleration(long int * reponse,long int reponse_preced);
void ecretage_deceleration(long int * reponse,long int reponse_preced);
void ecretage_vitesse(long int * reponse);
int asser_done(int erreur_delta, int erreur_alpha);
int arret_ok(long int commande_moteur_D,long int commande_moteur_G);
int convert2PWM(long int commande);

#endif
