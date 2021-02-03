#include <sys/types.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include<time.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <fcntl.h>
#include <errno.h>


#define RAND_MAX 32768


/************************************************************************************************************************************/
/*                                                                                                                                  */
/*     LE NOMBRE DE THREAD DOIT ETRE UN MULTIPLE DE 20 000 000 POUR FAIRE 20 000 000 SINON IL Y'AURA UNE PERTE DU A LA DIVISION     */
/*                                                                                                                                  */
/************************************************************************************************************************************/

/*****************************************/
/*                                       */
/*    gcc -pthread -o projet projet.c    */
/*                                       */
/*****************************************/

double *tableauDeRandom;


/** c'est le nombre de random max que peux faire un processus avant de devoir laisser la tache a un autre
 *  Et revenir dessus que apres l'autre processus ait fini.
 */
double *nombreDeRandomLocalMax;
double *nombreDeRandomGlobal;
double *nombreDeRandomMax;


key_t cleTab;
key_t cleNb1;
key_t cleNb2;
key_t cleNb3;

int   idPartageTab;
int   idPartageNb1;
int   idPartageNb2;
int   idPartageNb3;


void remettreLeTableauAZero()
{
    for(int i = 0; i < RAND_MAX; i++)
        tableauDeRandom[i] = 0;
}

void faireDesRandom()
{
    int random;
    for(int i = 0; i < *nombreDeRandomLocalMax && *nombreDeRandomGlobal <= *nombreDeRandomMax; i++)
    {
        random = rand() % (RAND_MAX);
        tableauDeRandom[random]++;
        (*nombreDeRandomGlobal)++;
    }
}


void affichage()
{
    for(int i=0; i<RAND_MAX; i++)
    {
        printf("Case n°%d = %lf\n", i, tableauDeRandom[i]);
    }
}


void equilibre()
{
    double min, max, total = 0;
    for(int i=0; i<RAND_MAX; i++)
    {
        if(i==0)
            min = max = tableauDeRandom[i];

        if(tableauDeRandom[i] > max)
            max = tableauDeRandom[i];

        if(tableauDeRandom[i] < min)
            min = tableauDeRandom[i];
        total += tableauDeRandom[i];
    }

    printf("on considere rand comme équilibré si l'ecart entre min et max n'est pas superieur a 5%%\n");

    if(min >= max*0.95)
        printf("rand est une fonction équilibrée car min = %lf, max = %lf et 95%% de max = %lf, \nici min est different de max de %lf%%, total : %lf, moyenne : %lf\n",
            min, max, max*0.95, 100*(min/max)-100, total, (double)(total/RAND_MAX));
    else
        printf("rand n'est pas une fonction équilibrée car min = %lf, max = %lf et 95%% de max = %lf, \nici min est different de max de %lf%%, total : %lf, moyenne : %lf\n",
            min, max, max*0.95, 100*(min/max)-100, total, (double)(total/RAND_MAX));
}


int main(int argc, char* argv[])
{
    if(argv[1] == NULL)
    {
        printf("argument incorrect\n");
        return -1;
    }
        


    idPartageTab       = shmget(cleTab, sizeof(double)*RAND_MAX, 0666 | IPC_CREAT);
    tableauDeRandom    = (double *)shmat(idPartageTab, 0, 0);

    if(tableauDeRandom == -1){
        printf("Erreur lors du partage de la mémoire !");
        switch (errno) {
            case EINVAL:
                printf("Identifiant invalide !\n");
                break;
            case EACCES:
                printf("Accès refusé !\n");
                break;
            case ENOMEM:
                printf("Mémoire saturée !\n");
                break;
            default:
                printf("Erreur inconnue\n");
                break;
        }
        return -1;
    }

    remettreLeTableauAZero();


    //MEMO : Il s'agit de la deuxieme facon de faire, elle marche tout aussi bien
    //nombreDeRandomLocalMax   = mmap(NULL, sizeof *nombreDeRandomLocalMax, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    //nombreDeRandomGlobal     = mmap(NULL, sizeof *nombreDeRandomGlobal  , PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    //nombreDeRandomMax        = mmap(NULL, sizeof *nombreDeRandomMax     , PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    if((cleNb1=ftok(".", 2)) == -1 || (cleNb2=ftok(".", 3)) == -1 || (cleNb3=ftok(".", 4)) == -1)
    {
        perror("ftok\n");
        return -1;
    }


    idPartageNb1             = shmget(cleNb1, sizeof(double), 0666 | IPC_CREAT);
    nombreDeRandomLocalMax   = (double*)shmat(idPartageNb1, 0, 0);

    idPartageNb2             = shmget(cleNb2, sizeof(double), 0666 | IPC_CREAT);
    nombreDeRandomGlobal     = (double*)shmat(idPartageNb2, 0, 0);

    idPartageNb3             = shmget(cleNb3, sizeof(double), 0666 | IPC_CREAT);
    nombreDeRandomMax        = (double*)shmat(idPartageNb3, 0, 0);


    /********************************/
    /* CHANGEMENT DU NOMBRE DE RAND */
    /********************************/
    
    *nombreDeRandomMax      = 2000000000;
    *nombreDeRandomLocalMax = *nombreDeRandomMax/(atoi(argv[1]));
    *nombreDeRandomGlobal   = 0;



    sem_t *sem = sem_open("/semaphore1", O_CREAT,  0644, 1);


    printf("pid du pere: %d\n", getpid());
    int e = getpid();
    pid_t process;
    for (int i = 0; i < atoi(argv[1]); i++)
    {
        if(!fork())
        {
            printf("Creation d'un fils du pere : ppid=%d, AU TRAVAIL!\n", getppid());

            sem_wait(sem);//Je ferme le semaphore
            srand(time(NULL)); //genere une nouvelle seed pour avoir de nouveau random a chaque lancement (tant que le lancement n'est pas trop proche du precedent)
            printf("un fils rentre : %lf\n", *nombreDeRandomGlobal);

            faireDesRandom();
            sleep(1); //sleep pour la facilité de lecture

            printf("un fils sort   : %lf\n", *nombreDeRandomGlobal);
            sem_post(sem);//J'ouvre le semaphore
            
            exit(1);
        }
    }

    for(int i=0; i<atoi(argv[1]); i++)
        wait(NULL);

    printf("\nTABLEAU REMPLIS\n");
    //affichage();
    equilibre();


    sem_close(sem);
    sem_unlink("/semaphore1");


    return 0;
}

//https://stackoverflow.com/questions/6847973/do-forked-child-processes-use-the-same-semaphore
//https://stackoverflow.com/questions/40644239/posix-semaphores-between-child-and-parent-processes