#include <math.h> //utiliser un tableau pour acos ??
#include <stdio.h> //TODO : à virer

#include "../common_code/debug.h"

#if USE_SDL
#   include "debug/affichage.h"
#endif

#include "trajectoire.h"
#include "odometrie.h"
#include "reglages.h"
#include "asser.h"
#include "communication.h"
#include "match.h"

//et encore de vilaines variables globales !
static s_trajectoire trajectoire;
static s_consigne consigne;

void start()
{
	while(match_get_etat() != MATCH_FIN)
	{
		//on met à jour la consigne pour l'asser
		update_consigne();

		//asservissement
		asser(consigne);

		//on recalcule la position actuelle du robot (via les roues codeuses)
		actualise_position();

		//on envoie notre position au PC (débug)
		//NB: vu que le traitement un peu long, je ne l'active que si le debug
		//est actif (de toute façon il ne se passe rien si DEBUG n'est pas
		//actif
#if DEBUG
		static int prev_x	  = 0;
		static int prev_y	  = 0;
		static int prev_theta = 0;

		int current_x	  = get_x_actuel();
		int current_y	  = get_y_actuel();
		int current_theta = get_theta_actuel();

		if (prev_x != current_x ||
			prev_y != current_y ||
			prev_theta != current_theta)
		{
			debug("position actuelle : x=%d y=%d, theta=%d\n",
				  get_x_actuel(), get_y_actuel(),get_theta_actuel());
		}

		prev_x	   = current_x;
		prev_y	   = current_y;
		prev_theta = current_theta;
#endif

#if USE_SDL
		set_position(get_x_actuel(), get_y_actuel(),get_theta_actuel());
#endif
	}
}

void update_consigne()
{
	switch (trajectoire.type)
	{
		case alpha_delta :
			make_trajectoire_alpha_delta(trajectoire.alpha,trajectoire.delta);
			break;
		case theta :
			make_trajectoire_theta(trajectoire.theta);
			break;
		case xy_relatif :
			make_trajectoire_xy_relatif(trajectoire.x_relatif,trajectoire.y_relatif);
			break;
		case xy_absolu :
			make_trajectoire_xy_absolu(trajectoire.x_absolu,trajectoire.y_absolu);
			break;
		case chemin :
			make_trajectoire_chemin(trajectoire.chemin);
			break;
		case stop :
			make_trajectoire_alpha_delta(0,0);
			break;
		/*case emergency_stop :
			motors_stop(); //dans hardware.c
			break;*/
		case null :
			break;
	}
}

void make_trajectoire_alpha_delta(int new_alpha, int new_delta)
{
	set_consigne_alpha_delta(new_alpha,new_delta);
	trajectoire.type=null;
}

void make_trajectoire_theta(int theta_voulu)
{
	int new_alpha=theta_voulu-get_theta_actuel();
	set_consigne_alpha_delta(new_alpha,0);
	trajectoire.type=null;
}

void make_trajectoire_xy_relatif(int x_voulu, int y_voulu)
{
	trajectoire.x_absolu=x_voulu+get_x_actuel();
	trajectoire.y_absolu=y_voulu+get_y_actuel();
	trajectoire.type=xy_absolu;
	make_trajectoire_xy_absolu(trajectoire.x_absolu,trajectoire.y_absolu);
}

void make_trajectoire_xy_absolu(int x_voulu, int y_voulu)
{
	int new_alpha;
	int new_delta;
	x_voulu-=get_x_actuel();
	y_voulu-=get_y_actuel();
	calcul_alpha_delta_restant(x_voulu, y_voulu, &new_alpha, &new_delta);

	//évite que le robot ne tourne pour rien quand il a atteint xy avec la précision voulue
	if (new_delta<-PRECISION_DELTA || PRECISION_DELTA<new_delta)
	{
		set_consigne_alpha_delta(new_alpha,new_delta);
	}
	else
	{
		trajectoire.type=stop;
	}
}

void make_trajectoire_chemin(s_liste liste_positions)
{
	//si il ne reste plus qu'un point la trajectoire n'est plus du même type
	if (trajectoire.chemin.taille==1)
	{
		trajectoire.type=xy_absolu;
		trajectoire.x_absolu=liste_positions.point[0].x;
		trajectoire.y_absolu=liste_positions.point[0].y;
		return;

	}

	//calcul de la consigne
	int new_alpha;
	int new_delta;
	int x_voulu=liste_positions.point[liste_positions.taille-1].x-get_x_actuel();
	int y_voulu=liste_positions.point[liste_positions.taille-1].y-get_y_actuel();
	calcul_alpha_delta_restant(x_voulu, y_voulu, &new_alpha, &new_delta);
	int sgn_delta=(new_delta > 0) - (new_delta < 0);

	//gestion du changement de point "cible"
	if (new_delta<-PRECISION_DELTA || PRECISION_DELTA<new_delta)
	{
		set_consigne_alpha_delta(new_alpha,sgn_delta*CONSTANTE_DELTA);
	}
	else
	{
		trajectoire.chemin.taille-=1;
	}
}

void calcul_alpha_delta_restant(int x_voulu, int y_voulu, int * new_alpha, int * new_delta)
{
	//x et y sont relatifs mais l'orientation des axes reste absolue

	//un peu moche...
	long int x_voulu_l=x_voulu;
	long int y_voulu_l=y_voulu;

	//voir si pas meilleur moyen pour le calcul de sqrt
	double distance = sqrt((double)(x_voulu_l*x_voulu_l+y_voulu_l*y_voulu_l));
	*new_delta=(int) distance;
	int sgn_x=(x_voulu > 0) - (x_voulu < 0);
	 //voir si pas meilleur moyen pour le calcul de acos (tableau ?)
	int theta_voulu=(int)(1000.0*acos((double)(y_voulu)/distance)*(-1.0*sgn_x));
	*new_alpha=theta_voulu-get_theta_actuel();

	//on borne *new_alpha (à tester) entre pi et -pi
	*new_alpha=*new_alpha%((int)(DEUX_PI*1000.0));
	if (*new_alpha>(int)((DEUX_PI*1000.0)/2.0))
	{
		*new_alpha-=(int)(DEUX_PI*1000.0);
	}
	else if (*new_alpha<-(int)((DEUX_PI*1000.0)/2.0))
	{
		*new_alpha+=(int)(DEUX_PI*1000.0);
	}

	//TODO : gestion point non atteignable
	//(si l'on demande un point trop prés du robot et à la perpendiculaire de la direction du robot il se met à tourner autour du point)
	
	//gestion de la marche arrière
	if (*new_alpha>1571) //1571~=(pi/2)*1000
	{
		*new_alpha-=3142; //3142~=pi*1000
		*new_delta=-*new_delta;
	}
	else if (*new_alpha<-1571) //1571~=(pi/2)*1000
	{
		*new_alpha+=3142; //3142~=pi*1000
		*new_delta=-*new_delta;
	}
}

void set_trajectoire_alpha_delta(int alpha, int delta)
{
	trajectoire.type=alpha_delta;
	trajectoire.alpha=alpha;
	trajectoire.delta=delta;
}

void set_trajectoire_xy_relatif(int x, int y)
{
	trajectoire.type=xy_relatif;
	trajectoire.x_relatif=x;
	trajectoire.y_relatif=y;
}

void set_trajectoire_xy_absolu(int x, int y)
{
	trajectoire.type=xy_absolu;
	trajectoire.x_absolu=x;
	trajectoire.y_absolu=y;
}

void set_trajectoire_theta(int new_theta)
{
	trajectoire.type=theta;
	trajectoire.theta=new_theta;
}

void set_trajectoire_chemin(s_liste liste_positions)
{
	trajectoire.type=chemin;
	trajectoire.chemin=liste_positions;
}

void set_consigne_alpha_delta(int new_alpha, int new_delta)
{
	consigne.alpha=new_alpha+get_alpha_actuel();
	consigne.delta=new_delta+get_delta_actuel();
}

void init_trajectoire()
{
	trajectoire.type=null;
	trajectoire.delta=0;
	trajectoire.alpha=0;
}

//TODO : à virer (utile uniquement pour du debug)
int get_delta_voulu()
{
	return consigne.delta;
}

int get_alpha_voulu()
{
	return consigne.alpha;
}
