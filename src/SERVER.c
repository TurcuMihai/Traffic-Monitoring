#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <sqlite3.h>
#include <time.h>
#include <signal.h>

#define PORT 2002
#define NUMBER_THREADS 30

    /* Definirea functiilor */
static void *treat(void *); 
void worker (int cl, int idThread); // functia atribuita thread-urilor
int check_username(char *username); // verifica daca numele de utilizator este valid
int check_password(char *username, char *msg); // verifica daca parola introdusa este valida
int get_combustibil(char *username); // interogheaza baza de date daca contul utilizatorului este configurat pentru a primi info despre combustibil
int get_vreme(char *username);  // interogheaza baza de date daca contul utilizatorului este configurat pentru a primi info despre vreme
int get_sport(char *username);  // interogheaza baza de date daca contul utilizatorului este configurat pentru a primi info despre sport
void *extract_info(void * arg); // functia thread-ului ce actualizeaza info despre vreme si sport
void trimite_strazi(int cl, char *intersectie1, int idThread); // functie ce trimite clientului strazi pe care poate circula in continuare
int get_distanta(char * strada); // interogheaza baza de date pt a obtine distanta unei strazi
int get_limita_viteza(char * strada); // interogheazabaza de date pt a obtine limita de viteza de pe o anumita strada
void get_intersectie2(char *intersectie, char * strada, char * intersectie_viitoare); // interogheaza baza de date pentru a obtine intersectia spre care se intreapta clientul
void trimite_combustibil(int cl, char *strada, int idThread); // trimite clientului info despre preturile la combustibil de pe strada pe care circula
void trimite_eveniment(char *strada, char *evenient); //trimite
void check_eveniment(int cl, char *strada, int idThread); // interogheaza baza de date despre evenimente de pe o anumita strada
void inserare_eveniment(char *strada, char * eveniment); // insereaza in baza de date informatii despre un eveniment petrecut pe o strada


    /* Structura in care retinem informatiile thread-urilor*/
typedef struct {
    pthread_t idThread; // id-ul thread-ului
    int thCount; // nr de conexiuni servite
}Thread;

Thread *threadsPool; // un array de structuri Thread

pthread_mutex_t lacate[NUMBER_THREADS] = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mlock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t lock_db = PTHREAD_MUTEX_INITIALIZER;
pthread_t informatii;

sqlite3 *db;
int sd;
int rc;
char *err_msg;
int file_descriptors[NUMBER_THREADS];
char vremea[300];
char sport[300];

int main(int argc, char* arvg[])
{

	rc = sqlite3_open("mydb.db",&db);
	if(rc != SQLITE_OK)
	{
		printf("[server]Eroare la deschiderea bazei de date!\n");
		exit(EXIT_FAILURE);
	}
    char *query = "Drop table if exists evenimente;";
    int dc = sqlite3_exec(db,query,0,0,&err_msg);
    if(dc != SQLITE_OK)
    {
        printf("Eroare la stargerea din baza de date!\n");
        exit(EXIT_FAILURE);
    }
    query = "CREATE TABLE evenimente (strada TEXT NOT NULL,eveniment TEXT NOT NULL)";
    dc = sqlite3_exec(db,query,0,0,&err_msg);
    if(dc != SQLITE_OK)
    {
        printf("Eroare la crearea tabelului evenimente!\n");
        exit(EXIT_FAILURE);
    }

    for(int i = 0; i< NUMBER_THREADS; i++)
    {
        file_descriptors[i] = -1;
    }

    struct sockaddr_in server;

    // AM CREAT THREAD-UL CARE ACTUALIZEAZA VREMEA SI SPORTUL
    pthread_create(&informatii,NULL,&extract_info,NULL);

    void threadCreate(int);
    threadsPool = calloc(sizeof(Thread),NUMBER_THREADS);

    if ((sd = socket (AF_INET, SOCK_STREAM, 0)) == -1)
    {
      perror ("[server]Eroare la socket().\n");
      return errno;
    }

    int on = 1;
    setsockopt(sd,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on));
    bzero (&server, sizeof (server));

   /* umplem structura folosita pentru realizarea conexiunii cu serverul */
   server.sin_family = AF_INET;
   server.sin_addr.s_addr = htonl(INADDR_ANY);
   server.sin_port = htons (PORT);
   
    if (bind (sd, (struct sockaddr *) &server, sizeof (struct sockaddr)) == -1)
    {
    	perror ("[server]Eroare la bind().\n");
    	return errno;
    }

    if (listen (sd, 2) == -1)
    {
    	perror ("[server]Eroare la listen().\n");
    	return errno;
    }

    printf("Nr threaduri %d \n",NUMBER_THREADS);
    
    for(int i=0; i<NUMBER_THREADS; i++)
        threadCreate(i);
    while(1)
    {
        printf("[server]Asteptam la portul %d ... \n", PORT);
        pause();
    }    
}

void threadCreate(int i)
{
	void *treat(void *);
	
	pthread_create(&threadsPool[i].idThread,NULL,&treat,(void*)i);
	return; /* threadul principal returneaza */
}

void *treat(void * arg)
{		
		int client;
		struct sockaddr_in from; 
 	    bzero (&from, sizeof (from));
 		printf ("[thread]- %d - pornit...\n", (int) arg);
        fflush(stdout);

		for( ; ; )
		{
			int length = sizeof (from);
			pthread_mutex_lock(&mlock);


			if ( (client = accept (sd, (struct sockaddr *) &from, &length)) < 0)
				{
	 			 perror ("[thread]Eroare la accept().\n");	  			
				}
			pthread_mutex_unlock(&mlock);
			threadsPool[(int)arg].thCount++;
			worker(client,(int)arg); 
			close (client);			
		}	
}

void worker(int cl, int idThread)
{
    sqlite3_stmt *res;
    struct users{
        char username[100];
        char password[100];
        char intersectie1[100];
        char intersectie2[100];
        char strada[100];
        int viteza;
        int limita_viteza;
        int distanta_ramasa;
        int vreme;
        int sport;
        int combustibil;
    }user;

    int len;
    int dc;
    char vr[300];
    char mesaj[300];
    int username_ok = 0;
    int password_ok = 0;
    char var1[10];
    char var2[10];
    char eveniment[100];
    char convert[10];

    printf("Conectat la Thread-ul %d\n",idThread);

    if(read(cl,&len,sizeof(int)) == -1)
    {
        printf("Eroare la read(136).\n");
        exit(EXIT_FAILURE);
    }

    if(read(cl,mesaj,len) == -1)
    {
        printf("Eroare la read(142).\n");
        exit(EXIT_FAILURE);
    }
    mesaj[len] = '\0';
    if(strcmp(mesaj,"Exit") == 0)
    {
        printf("Utilizator %d deconectat!\n",idThread);

        return;
    }
    if(strcmp(mesaj,"Autentificare") == 0)
    {
        int user_valid = 0;
        int parola_valida = 0;
        while(user_valid == 0)
        {
            if(read(cl,&len,sizeof(int)) == -1)
            {
                printf("Eroare le read (159).\n");
                exit(EXIT_FAILURE);
            }
            if(read(cl,mesaj,len) == -1)
            {
                printf("Eroare la read(164).\n");
                exit(EXIT_FAILURE);
            }
            mesaj[len]='\0';

            if(strcmp(mesaj,"Exit") == 0)
            {
                printf("Utilizator %d deconectat!\n",idThread);
                file_descriptors[idThread] = -1;
                return;
            }
            if(check_username(mesaj) == 1)
            {
                strcpy(user.username,mesaj);
                user_valid = 1;
                len = strlen("User valid.");
                pthread_mutex_lock(&lacate[idThread]);
                if(write(cl,&len,sizeof(int)) == -1)
                {
                    printf("Eroare la write(246).\n");
                    exit(EXIT_FAILURE);
                }
                if(write(cl,"User valid.",len) == -1)
                {
                    printf("Eroare la write(251).\n");
                    exit(EXIT_FAILURE);
                }
                pthread_mutex_unlock(&lacate[idThread]);
            }
            else
            {
                len = strlen("User invalid");
                pthread_mutex_lock(&lacate[idThread]);
                if(write(cl,&len,sizeof(int)) == -1)
                {
                    printf("Eroare la write(262).\n");
                    exit(EXIT_FAILURE);
                }
                if(write(cl,"User invalid",len) == -1)
                {
                    printf("Eroare la write(267).\n");
                    exit(EXIT_FAILURE);
                }
                pthread_mutex_unlock(&lacate[idThread]);
            }
        }
        while(parola_valida == 0)
        {
            if(read(cl,&len,sizeof(int)) == -1)
            {
                printf("Eroare le read (277).\n");
                exit(EXIT_FAILURE);
            }
            if(read(cl,mesaj,len) == -1)
            {
                printf("Eroare la read(282).\n");
                exit(EXIT_FAILURE);
            }
            mesaj[len]='\0';

            if(strcmp(mesaj,"Exit") == 0)
            {
                printf("Utilizator %d deconectat!\n",idThread);
                file_descriptors[idThread] = -1;
                return;
            }

            if(check_password(user.username,mesaj) == 1)
            {
                strcpy(user.password,mesaj);
                parola_valida = 1;
                len = strlen("Parola valida.");
                pthread_mutex_lock(&lacate[idThread]);
                if(write(cl,&len,sizeof(int)) == -1)
                {
                    printf("Eroare la write(302).\n");
                    exit(EXIT_FAILURE);
                }
                if(write(cl,"Parola valida.",len) == -1)
                {
                    printf("Eroare la write(307).\n");
                    exit(EXIT_FAILURE);
                }
                pthread_mutex_unlock(&lacate[idThread]);
            }
            else
            {
                len = strlen("Parola invalida.");
                pthread_mutex_lock(&lacate[idThread]);
                if(write(cl,&len,sizeof(int)) == -1)
                {
                    printf("Eroare la write(318).\n");
                    exit(EXIT_FAILURE);
                }
                if(write(cl,"Parola invalida.",len) == -1)
                {
                    printf("Eroare la write(323).\n");
                    exit(EXIT_FAILURE);
                }
                pthread_mutex_unlock(&lacate[idThread]);
            }
        }
    }
    if(strcmp(mesaj,"Inregistrare") == 0)
    {
        int user_valid = 0;
        int parola_valida = 0;
        int vreme_valid = 0;
        int sport_valid = 0;
        int combustibil_valid = 0;
        int reusit = 0;
        int conectat = 0;
        int loop = 0;
        while(reusit == 0 & conectat == 0)
        {
            user_valid = 0;
            parola_valida = 0;
            vreme_valid = 0;
            sport_valid = 0;
            combustibil_valid = 0;
            
            while(user_valid == 0)
            {
                if(read(cl,&len,sizeof(int)) == -1)
                {
                    printf("Eroare la read(352).\n");
                    exit(EXIT_FAILURE);
                }
                if(read(cl,mesaj,len) == -1)
                {
                    printf("Eroare la read(357).\n");
                    exit(EXIT_FAILURE);
                }
                mesaj[len] = '\0';
                if(strcmp(mesaj,"Exit") == 0)
                {
                    printf("Utilizator %d deconectat!\n",idThread);
                    file_descriptors[idThread] = -1;

                    return;
                }
                if(check_username(mesaj) == 0)
                {
                    strcpy(user.username,mesaj);
                    user_valid = 1;
                    len = strlen("User valid.");
                    pthread_mutex_lock(&lacate[idThread]);
                    if(write(cl,&len,sizeof(int)) == -1)
                    {
                        printf("Eroare la write(376).\n");
                        exit(EXIT_FAILURE);
                    }
                    if(write(cl,"User valid.",len) == -1)
                    {
                        printf("Eroare la write(381).\n");
                        exit(EXIT_FAILURE);
                    }
                    pthread_mutex_unlock(&lacate[idThread]);
                }
                else
                {
                    len = strlen("User invalid.");
                    pthread_mutex_lock(&lacate[idThread]);
                    if(write(cl,&len,sizeof(int)) == -1)
                    {
                        printf("Eroare la write(392).\n");
                        exit(EXIT_FAILURE);
                    }
                    if(write(cl,"User invalid.",len) == -1)
                    {
                        printf("Eroare la write(397).\n");
                        exit(EXIT_FAILURE);
                    }
                    pthread_mutex_unlock(&lacate[idThread]);
                }
            }
            while(parola_valida == 0)
            {
                if(read(cl,&len,sizeof(int)) == -1)
                {
                    printf("Eroare la read(407).\n");
                    exit(EXIT_FAILURE);
                }
                if(read(cl,mesaj,len) == -1)
                {
                    printf("Eroare la read(412).\n");
                    exit(EXIT_FAILURE);
                }
                mesaj[len] = '\0';

                if(strcmp(mesaj,"Exit") == 0)
                {
                    printf("Utilizator %d deconectat!\n.", idThread);
                    file_descriptors[idThread] = -1;
                    return;
                }
                strcpy(user.password,mesaj);

                if(read(cl,&len,sizeof(int)) == -1)
                {
                    printf("Eroare la read(427).\n");
                    exit(EXIT_FAILURE);
                }
                if(read(cl,mesaj,len) == -1)
                {
                    printf("Eroare la read(432).\n");
                    exit(EXIT_FAILURE);
                }

                mesaj[len]='\0';
                if(strcmp(mesaj,"Exit") == 0)
                {
                    printf("Utilizator %d deconectat!\n.", idThread);
                    file_descriptors[idThread] = -1;
                    return;
                }
                if(strcmp(user.password,mesaj) == 0)
                {
                    parola_valida = 1;
                    len = strlen("Parola valida.");
                    pthread_mutex_lock(&lacate[idThread]);
                    if(write(cl,&len,sizeof(int)) == -1)
                    {
                        printf("Eroare la write(450).\n");
                        exit(EXIT_FAILURE);
                    }
                    if(write(cl,"Parola valida.", len) == -1)
                    {
                        printf("Eroare la write(455).\n");
                        exit(EXIT_FAILURE);
                    }
                    pthread_mutex_unlock(&lacate[idThread]);
                }
                else
                {
                    len = strlen("Parola invalida.");
                    pthread_mutex_lock(&lacate[idThread]);
                    if(write(cl,&len,sizeof(int)) == -1)
                    {
                        printf("Eroare la write(466).\n");
                        exit(EXIT_FAILURE);
                    }
                    if(write(cl,"Parola invalida.", len) == -1)
                    {
                        printf("Eroare la write(471).\n");
                        exit(EXIT_FAILURE);
                    }
                    pthread_mutex_unlock(&lacate[idThread]);
                }
            }
            while(vreme_valid == 0)
            {
                if(read(cl,&len,sizeof(int)) == -1)
                {
                    printf("Eroare la read(481).\n");
                    exit(EXIT_FAILURE);
                }
                if(read(cl,mesaj,len) == -1)
                {
                    printf("Eroare la read(486).\n");
                    exit(EXIT_FAILURE);
                }
                mesaj[len]='\0';
                if(strcmp(mesaj,"Da") == 0)
                {
                    user.vreme = 1;
                    vreme_valid = 1;
                    len = strlen("Raspuns valid.");
                    pthread_mutex_lock(&lacate[idThread]);
                    if(write(cl,&len,sizeof(int)) == -1)
                    {
                        printf("Eroare la write(498).\n");
                        exit(EXIT_FAILURE);
                    }
                    if(write(cl,"Raspuns valid.",len) == -1)
                    {
                        printf("Eroare la write(503).\n");
                        exit(EXIT_FAILURE);
                    }
                    pthread_mutex_unlock(&lacate[idThread]);
                } else
                if(strcmp(mesaj,"Nu") == 0)
                {
                    user.vreme = 0;
                    vreme_valid = 1;
                    len = strlen("Raspuns valid.");
                    pthread_mutex_lock(&lacate[idThread]);
                    if(write(cl,&len,sizeof(int)) == -1)
                    {
                        printf("Eroare la write(516).\n");
                        exit(EXIT_FAILURE);
                    }
                    if(write(cl,"Raspuns valid.",len) == -1)
                    {
                        printf("Eroare la write(521).\n");
                        exit(EXIT_FAILURE);
                    }
                    pthread_mutex_unlock(&lacate[idThread]);
                } else
				if(strcmp(mesaj,"Exit") == 0)
				{
					printf("Utilizator %d deconectat!\n",idThread);
                    file_descriptors[idThread] = -1;
					return;
				} else
                {
                    len = strlen("Raspuns invalid.");
                    pthread_mutex_lock(&lacate[idThread]);
                    if(write(cl,&len,sizeof(int)) == -1)
                    {
                        printf("Eroare la write(537).\n");
                        exit (EXIT_FAILURE);
                    }
                    if(write(cl,"Raspuns invalid.",len) == -1)
                    {
                        printf("Eroare la write(542).\n");
                        exit(EXIT_FAILURE);
                    }
                    pthread_mutex_unlock(&lacate[idThread]);
                }
            }
            while(sport_valid == 0)
            {
                if(read(cl,&len,sizeof(int)) == -1)
                {
                    printf("Eroare la read(552).\n");
                    exit(EXIT_FAILURE);
                }
                if(read(cl,mesaj,len) == -1)
                {
                    printf("Eroare la read(557).\n");
                    exit(EXIT_FAILURE);
                }
                mesaj[len]='\0';
                if(strcmp(mesaj,"Da") == 0)
                {
                    user.sport = 1;
                    sport_valid = 1;
                    len = strlen("Raspuns valid.");
                    pthread_mutex_lock(&lacate[idThread]);
                    if(write(cl,&len,sizeof(int)) == -1)
                    {
                        printf("Eroare la write(569).\n");
                        exit(EXIT_FAILURE);
                    }
                    if(write(cl,"Raspuns valid.",len) == -1)
                    {
                        printf("Eroare la write(574).\n");
                        exit(EXIT_FAILURE);
                    }
                    pthread_mutex_unlock(&lacate[idThread]);
                } else
                if(strcmp(mesaj,"Nu") == 0)
                {
                    user.sport = 0;
                    sport_valid = 1;
                    len = strlen("Raspuns valid.");
                    pthread_mutex_lock(&lacate[idThread]);
                    if(write(cl,&len,sizeof(int)) == -1)
                    {
                        printf("Eroare la write(587).\n");
                        exit(EXIT_FAILURE);
                    }
                    if(write(cl,"Raspuns valid.",len) == -1)
                    {
                        printf("Eroare la write(592).\n");
                        exit(EXIT_FAILURE);
                    }
                    pthread_mutex_unlock(&lacate[idThread]);
                } else
				if(strcmp(mesaj,"Exit") == 0)
				{
					printf("Utilizator %d deconectat!\n",idThread);
                    file_descriptors[idThread] = -1;
					return;
				} else
                {
                    len = strlen("Raspuns invalid.");
                    pthread_mutex_lock(&lacate[idThread]);
                    if(write(cl,&len,sizeof(int)) == -1)
                    {
                        printf("Eroare la write(608).\n");
                        exit(EXIT_FAILURE);
                    }
                    if(write(cl,"Raspuns invalid.",len) == -1)
                    {
                        printf("Eroare la write(613).\n");
                        exit(EXIT_FAILURE);
                    }
                    pthread_mutex_unlock(&lacate[idThread]);
                }
            }
            while(combustibil_valid == 0)
            {
                if(read(cl,&len,sizeof(int)) == -1)
                {
                    printf("Eroare la read(623).\n");
                    exit(EXIT_FAILURE);
                }
                if(read(cl,mesaj,len) == -1)
                {
                    printf("Eroare la read(628).\n");
                    exit(EXIT_FAILURE);
                }
                mesaj[len]='\0';
                if(strcmp(mesaj,"Da") == 0)
                {
                    user.combustibil = 1;
                    combustibil_valid = 1;
                    len = strlen("Raspuns valid.");
                    pthread_mutex_lock(&lacate[idThread]);
                    if(write(cl,&len,sizeof(int)) == -1)
                    {
                        printf("Eroare la write(640).\n");
                        exit(EXIT_FAILURE);
                    }
                    if(write(cl,"Raspuns valid.",len) == -1)
                    {
                        printf("Eroare la write(645).\n");
                        exit(EXIT_FAILURE);
                    }
                    pthread_mutex_unlock(&lacate[idThread]);
                } else
                if(strcmp(mesaj,"Nu") == 0)
                {
                    user.combustibil = 0;
                    combustibil_valid = 1;
                    len = strlen("Raspuns valid.");
                    pthread_mutex_lock(&lacate[idThread]);
                    if(write(cl,&len,sizeof(int)) == -1)
                    {
                        printf("Eroare la write(658).\n");
                        exit(EXIT_FAILURE);
                    }
                    if(write(cl,"Raspuns valid.",len) == -1)
                    {
                        printf("Eroare la write(663).\n");
                        exit(EXIT_FAILURE);
                    }
                    pthread_mutex_unlock(&lacate[idThread]);
                } else
				if(strcmp(mesaj,"Exit") == 0)
				{
					printf("Utilizator %d deconectat!\n",idThread);
                    file_descriptors[idThread] = -1;
					return;
				} else
                {
                    len = strlen("Raspuns invalid.");
                    pthread_mutex_lock(&lacate[idThread]);
                    if(write(cl,&len,sizeof(int)) == -1)
                    {
                        printf("Eroare la write(679).\n");
                        exit(EXIT_FAILURE);
                    }
                    if(write(cl,"Raspuns invalid.",len) == -1)
                    {
                        printf("Eroare la write(684).\n");
                        exit(EXIT_FAILURE);
                    }
                    pthread_mutex_unlock(&lacate[idThread]);
                }
            }
            pthread_mutex_lock(&lock_db);
            if(check_username(user.username) == 0)
            {
                char *query = sqlite3_mprintf("INSERT INTO users (username,password,vreme,sport,combustibil) VALUES ('%s','%s','%d','%d','%d');",user.username,user.password,user.vreme,user.sport,user.combustibil);
                dc = sqlite3_exec(db,query,0,0,&err_msg);
                if(dc != SQLITE_OK)
                {
                    printf("Eroare la inserarea in baza de date!\n");
                    exit(EXIT_FAILURE);
                }
                len = strlen("Cont creat.");
                pthread_mutex_lock(&lacate[idThread]);
                if(write(cl,&len,sizeof(int)) == -1)
                {
                    printf("Eroare la write(704).\n");
                    exit(EXIT_FAILURE);
                }
                if(write(cl,"Cont creat.",len) == -1)
                {
                    printf("Eroare la write(709).\n");
                    exit(EXIT_FAILURE);
                }
                pthread_mutex_unlock(&lacate[idThread]);
                reusit = 1;
            }
            else
            {
                loop++;
                len = strlen("Eroare la crearea contului.");
                pthread_mutex_lock(&lacate[idThread]);
                if(write(cl,&len,sizeof(int)) == -1)
                {
                    printf("Eroare la write(722).\n");
                    exit(EXIT_FAILURE);
                }
                if(write(cl,"Eroare la crearea contului.",len) == -1)
                {
                    printf("Eroare la write(727).\n");
                    exit(EXIT_FAILURE);
                }
                pthread_mutex_unlock(&lacate[idThread]);
            }
            pthread_mutex_unlock(&lock_db);

        }
    }
 
    user.vreme = get_vreme(user.username);
    user.sport = get_sport(user.username);
    user.combustibil = get_combustibil(user.username);
    pthread_mutex_lock(&lacate[idThread]);
    if(write(cl,&user.vreme,sizeof(int)) == -1)
    {
        printf("Eroare la write(743).\n");
        exit(EXIT_FAILURE);
    }
    if(write(cl,&user.sport,sizeof(int)) == -1)
    {
        printf("Eroare la write(748).\n");
        exit(EXIT_FAILURE);
    }
    if(write(cl,&user.combustibil,sizeof(int)) == -1)
    {
        printf("Eroare la write(753).\n");
        exit(EXIT_FAILURE);
    }
    pthread_mutex_unlock(&lacate[idThread]);
    if(user.vreme == 1)
    {
        strcpy(vr,vremea); 
        len = strlen(vr);
        pthread_mutex_lock(&lacate[idThread]);
        if(write(cl,&len,sizeof(int)) == -1)
        {
            printf("Eroare la write(764).\n");
            exit(EXIT_FAILURE);
        }
        if(write(cl,vr,len) == -1)
        {
            printf("Eroare la write(769).\n");
            exit(EXIT_FAILURE);
        }
        pthread_mutex_unlock(&lacate[idThread]);
    }
    if(user.sport == 1)
    {
        strcpy(vr,sport); 
        len = strlen(vr);
        pthread_mutex_lock(&lacate[idThread]);
        if(write(cl,&len,sizeof(int)) == -1)
        {
            printf("Eroare la write(781).\n");
            exit(EXIT_FAILURE);
        }
        if(write(cl,vr,len) == -1)
        {
            printf("Eroare la write(786).\n");
            exit(EXIT_FAILURE);
        }
        pthread_mutex_unlock(&lacate[idThread]);
    }

    if(read(cl,&len,sizeof(int)) == -1)
    {
        printf("Eroare la read(794).\n");
        exit(EXIT_FAILURE);
    }
    if(read(cl,mesaj,len) == -1)
    {
        printf("Eroare la read(799).\n");
        exit(EXIT_FAILURE);
    }
    mesaj[len] = '\0';
    if(strcmp(mesaj,"Exit") == 0)
    {
        printf("Utilizator %d deconectat!\n",idThread);
        file_descriptors[idThread] = -1;
		return;
    }

    strcpy(user.intersectie1,mesaj);
  
    trimite_strazi(cl,user.intersectie1,idThread);
 

    if(read(cl,&len,sizeof(int)) == -1)
    {
        printf("Eroare la read(817).\n");
        exit(EXIT_FAILURE);
    }
    if(read(cl,mesaj,len) == -1)
    {
        printf("Eroare la read(822).\n");
        exit(EXIT_FAILURE);
    }
  
    mesaj[len] = '\0';
    strcpy(user.strada,mesaj);
    user.distanta_ramasa = get_distanta(user.strada) * 100;
    user.limita_viteza = get_limita_viteza(user.strada);
    get_intersectie2(user.intersectie1, user.strada,user.intersectie2);
    file_descriptors[idThread] = cl;
    
   
                    strcpy(mesaj,"Ai intrat pe ");
                    strcat(mesaj,user.strada);
                    strcat(mesaj,".");
                    char limit[10];
                    pthread_mutex_lock(&lacate[idThread]);
                    len = strlen(mesaj);
                    if(write(cl,&len,sizeof(int)) == -1)
                    {
                        printf("Eroare la write(842).\n");
                        exit(EXIT_FAILURE);
                    }
                    if(write(cl,mesaj,len) == -1)
                    {
                        printf("Eroare la write(847).\n");
                        exit(EXIT_FAILURE);
                    }

                    check_eveniment(cl,user.strada,idThread);
                    pthread_mutex_unlock(&lacate[idThread]);
                    if(user.combustibil == 1)
                    {
                        trimite_combustibil(cl,user.strada,idThread);
                    }



    while(1)
    {
        if(read(cl,&len,sizeof(int)) == -1)
        {
            printf("Eroare la read(864).\n");
            exit(EXIT_FAILURE);
        }
        if(read(cl,mesaj,len) == -1)
        {
            printf("Eroare la read(869).\n");
            exit(EXIT_FAILURE);
        }
        mesaj[len] = '\0';
  
        strncpy(var1,mesaj,8);
        strncpy(var2,mesaj,7);
        var1[strlen(var1)] = '\0';
        var2[strlen(var2)] = '\0';
        if(strcmp(var1,"comanda:") == 0)
        {

            strcpy(mesaj,mesaj+8);
            if(strcmp(mesaj,"Accident") == 0)
            {
                strcpy(eveniment,"accident");
                trimite_eveniment(user.strada,eveniment);

            } else
            if(strcmp(mesaj,"Politie") == 0)
            {
                strcpy(eveniment,"politie");
                trimite_eveniment(user.strada,eveniment);

            } else
            if(strcmp(mesaj,"Trafic") == 0)
            {     
                strcpy(eveniment,"trafic");
                trimite_eveniment(user.strada,eveniment);
            } else
            if(strcmp(mesaj,"Vreme") == 0)
            {
                if(user.vreme == 1)
                {
                    strcpy(vr,vremea);
                    len = strlen(vr);
                    pthread_mutex_lock(&lacate[idThread]);
                    if(write(cl,&len,sizeof(int)) == -1)
                    {
                        printf("Eroare la write(908).\n");
                        exit(EXIT_FAILURE);
                    }
                    if(write(cl,vr,len) == -1)
                    {
                        printf("Eroare la write(912).\n");
                        exit(EXIT_FAILURE);
                    }
                    pthread_mutex_unlock(&lacate[idThread]);

                }   
                else
                {
                    len = strlen("\nNu ai selectat sa primesti informatii despre vreme.\n");
                    pthread_mutex_lock(&lacate[idThread]);
                    if(write(cl,&len,sizeof(int)) == -1)
                    {
                        printf("Eroare la write(925).\n");
                        exit(EXIT_FAILURE);
                    }
                    if(write(cl,"\nNu ai selectat sa primesti informatii despre vreme.\n",len) == -1)
                    {
                        printf("Eroare la write(930).\n");
                        exit(EXIT_FAILURE);
                    }
                    pthread_mutex_unlock(&lacate[idThread]);
                }
            } else
            if(strcmp(mesaj,"Sport") == 0)
            {
                if(user.sport == 1)
                {
                    strcpy(vr,sport);
                    len = strlen(vr);
                    pthread_mutex_lock(&lacate[idThread]);
                    if(write(cl,&len,sizeof(int)) == -1)
                    {
                        printf("Eroare la write(945).\n");
                        exit(EXIT_FAILURE);
                    }
                    if(write(cl,vr,len) == -1)
                    {
                        printf("Eroare la write(950).\n");
                        exit(EXIT_FAILURE);
                    }
                    pthread_mutex_unlock(&lacate[idThread]);

                }   
                else
                {
                    len = strlen("\nNu ai selectat sa primesti informatii despre sport.\n");
                    pthread_mutex_lock(&lacate[idThread]);
                    if(write(cl,&len,sizeof(int)) == -1)
                    {
                        printf("Eroare la write(962).\n");
                        exit(EXIT_FAILURE);
                    }
                    if(write(cl,"\nNu ai selectat sa primesti informatii despre sport.\n",len) == -1)
                    {
                        printf("Eroare la write(967).\n");
                        exit(EXIT_FAILURE);
                    }
                    pthread_mutex_unlock(&lacate[idThread]);
                }
            } else
            if(strcmp(mesaj,"Combustibil") == 0)
            {
                if(user.combustibil == 1)
                {
                    trimite_combustibil(cl,user.strada,idThread);
                }
                else
                {
                    len = strlen("\nNu ai selectat sa primesti informatii despre preturile la combustibil.\n");
                    pthread_mutex_lock(&lacate[idThread]);
                    if(write(cl,&len,sizeof(int)) == -1)
                    {
                        printf("Eroare la write(985).\n");
                        exit(EXIT_FAILURE);
                    }
                    if(write(cl,"\nNu ai selectat sa primesti informatii despre preturile la combustibil.\n",len) == -1)
                    {
                        printf("Eroare la write(990).\n");
                        exit(EXIT_FAILURE);
                    }
                    pthread_mutex_unlock(&lacate[idThread]);
                }
                
            } else
            if(strcmp(mesaj,"Exit") == 0)
            {
                printf("Utilizator %d deconectat!\n",idThread);
                file_descriptors[idThread] = -1;

				return;
            } 
            else
            {
                    len = strlen("\nComanda invalida!\n");
                    pthread_mutex_lock(&lacate[idThread]);
                    if(write(cl,&len,sizeof(int)) == -1)
                    {
                        printf("Eroare la write(1010).\n");
                        exit(EXIT_FAILURE);
                    }
                    if(write(cl,"\nComanda invalida!\n",len) == -1)
                    {
                        printf("Eroare la write(1015).\n");
                        exit(EXIT_FAILURE);
                    } 
                    pthread_mutex_unlock(&lacate[idThread]);
            }
            
        }
        else
        if(strcmp(var2,"viteza:") == 0)
        {
            strcpy(mesaj,mesaj+7);
            user.viteza = atoi(mesaj);
            user.distanta_ramasa = user.distanta_ramasa - user.viteza;
            if(user.distanta_ramasa < 0)
            {
                strcpy(user.intersectie1,user.intersectie2);
                trimite_strazi(cl,user.intersectie1,idThread);
            }
            else
            {
                if(user.viteza > user.limita_viteza)
                {
                    char atentionare[100];
                    char vit[4];
                    pthread_mutex_lock (&lacate[idThread]);
                    strcpy(atentionare,"Limita de viteza pe aceasta strada este de ");
                    sprintf(vit,"%d",user.limita_viteza);
                    strcat(atentionare, vit);
                    strcat(atentionare," KM/h iar tu circuli cu ");
                    sprintf(vit,"%d",user.viteza);
                    strcat(atentionare,vit);
                    strcat(atentionare," KM/h.\nIti recomandam sa reduci viteza.\n");
                    len = strlen(atentionare);
                    if(write(cl,&len,sizeof(int)) == -1)
                    {
                        printf("Eroare la write(1050).\n");
                        exit(EXIT_FAILURE);
                    }
                    if(write(cl,atentionare,len) == -1)
                    {
                        printf("Eroare la write(1055).\n");
                        exit(EXIT_FAILURE);
                    }
                    pthread_mutex_unlock(&lacate[idThread]);
                }
                else
                {
                    char atentionare[100];
                    char vit[4];
                    pthread_mutex_lock (&lacate[idThread]);
                    strcpy(atentionare,"Limita de viteza pe aceasta strada este de ");
                    sprintf(vit,"%d",user.limita_viteza);
                    strcat(atentionare, vit);
                    strcat(atentionare," KM/h iar tu circuli cu ");
                    sprintf(vit,"%d",user.viteza);
                    strcat(atentionare,vit);
                    strcat(atentionare," KM/h.\n");
                    len = strlen(atentionare);
                    if(write(cl,&len,sizeof(int)) == -1)
                    {
                        printf("Eroare la write(1075).\n");
                        exit(EXIT_FAILURE);
                    }
                    if(write(cl,atentionare,len) == -1)
                    {
                        printf("Eroare la write(1080).\n");
                        exit(EXIT_FAILURE);
                    }
                    pthread_mutex_unlock(&lacate[idThread]);
                }
            }
            printf("%d\n",user.distanta_ramasa);
        }
        else
        {
            strcpy(user.strada,mesaj);
            user.distanta_ramasa = get_distanta(user.strada) * 100;
            user.limita_viteza = get_limita_viteza(user.strada);
            get_intersectie2(user.intersectie1, user.strada,user.intersectie2);
            if(user.viteza > user.limita_viteza)
            {
                    char atentionare[100];
                    char vit[4];
                    char send[30];
                    strcpy(send,"Ai intrat pe ");
                    strcat(send,user.strada);
                    pthread_mutex_lock(&lacate[idThread]);
                    len = strlen(send);
                    if(write(cl,&len,sizeof(int)) == -1)
                    {
                        printf("Eroare la write(1105).\n");
                        exit(EXIT_FAILURE);
                    }
                    if(write(cl,send,len) == -1)
                    {
                        printf("Eroare la write(1110).\n");
                        exit(EXIT_FAILURE);
                    }
                    strcpy(atentionare,"Limita de viteza pe aceasta strada este de ");
                    sprintf(vit,"%d",user.limita_viteza);
                    strcat(atentionare, vit);
                    strcat(atentionare," KM/h iar tu circuli cu ");
                    sprintf(vit,"%d",user.viteza);
                    strcat(atentionare,vit);
                    strcat(atentionare," KM/h.\nIti recomandam sa reduci viteza.\n");
                    len = strlen(atentionare);
                    if(write(cl,&len,sizeof(int)) == -1)
                    {
                        printf("Eroare la write(1123).\n");
                        exit(EXIT_FAILURE);
                    }
                    if(write(cl,atentionare,len) == -1)
                    {
                        printf("Eroare la write(1128).\n");
                        exit(EXIT_FAILURE);
                    }
                    check_eveniment(cl,user.strada,idThread);
                    pthread_mutex_unlock(&lacate[idThread]);
                    if(user.combustibil == 1)
                    {
                        trimite_combustibil(cl,user.strada,idThread);
                    }
       
            }
            else
            {
                    pthread_mutex_lock (&lacate[idThread]);
                    char atentionare[100];
                    char vit[4];
                    char send[30];
                    strcpy(send,"Ai intrat pe ");
                    strcat(send,user.strada);
                    len = strlen(send);
                    if(write(cl,&len,sizeof(int)) == -1)
                    {
                        printf("Eroare la write(1150).\n");
                        exit(EXIT_FAILURE);
                    }
                    if(write(cl,send,len) == -1)
                    {
                        printf("Eroare la write(1155).\n");
                        exit(EXIT_FAILURE);
                    }
                    strcpy(atentionare,"Limita de viteza pe aceasta strada este de ");
                    sprintf(vit,"%d",user.limita_viteza);
                    strcat(atentionare, vit);
                    strcat(atentionare," KM/h iar tu circuli cu ");
                    sprintf(vit,"%d",user.viteza);
                    strcat(atentionare,vit);
                    strcat(atentionare," KM/h.\n\n");
                    len = strlen(atentionare);
                    if(write(cl,&len,sizeof(int)) == -1)
                    {
                        printf("Eroare la write(1068).\n");
                        exit(EXIT_FAILURE);
                    }
                    if(write(cl,atentionare,len) == -1)
                    {
                        printf("Eroare la write(1173).\n");
                        exit(EXIT_FAILURE);
                    }
                    check_eveniment(cl,user.strada,idThread);
                    pthread_mutex_unlock(&lacate[idThread]);
                    if(user.combustibil == 1)
                    {
                        trimite_combustibil(cl,user.strada,idThread);
                    }

            }
        }
    }
}

void trimite_eveniment(char *strada, char *eveniment)
{
    char mesaj[100];
    int len;
    if(strcmp(eveniment,"trafic") == 0)
    {
        inserare_eveniment(strada," TRAFIC");
        for(int i = 0; i < NUMBER_THREADS ; i++)
        {
            if(file_descriptors[i] != -1)
            {
                pthread_mutex_lock(&lacate[i]);
                strcpy(mesaj,"\nPe ");
                strcat(mesaj,strada);
                strcat(mesaj," a fost raportat TRAFIC AGLOMERAT.\n\n");
                len = strlen(mesaj);
                
                if(write(file_descriptors[i],&len,sizeof(int)) == -1)
                {
                    printf("Eroare la write(1207).\n");
                    exit(EXIT_FAILURE);
                }
                if(write(file_descriptors[i],mesaj,len) == -1)
                {
                    printf("Eroare la write(1212).\n");
                    exit(EXIT_FAILURE);
                }
                pthread_mutex_unlock(&lacate[i]);
            }
        }
    }
    if(strcmp(eveniment,"politie") == 0)
    {
        inserare_eveniment(strada," POLITIE");
        for(int i = 0; i < NUMBER_THREADS ; i++)
        {
            if(file_descriptors[i] != -1)
            {
                pthread_mutex_lock(&lacate[i]);
                strcpy(mesaj,"\nPe ");
                strcat(mesaj,strada);
                strcat(mesaj," a fost raportata POLITIE.\n\n");
                len = strlen(mesaj);
                if(write(file_descriptors[i],&len,sizeof(int)) == -1)
                {
                    printf("Eroare la write(1233).\n");
                    exit(EXIT_FAILURE);
                }
                if(write(file_descriptors[i],mesaj,len) == -1)
                {
                    printf("Eroare la write(1238).\n");
                    exit(EXIT_FAILURE);
                }
                pthread_mutex_unlock(&lacate[i]);
            }
        }
    }
    if(strcmp(eveniment,"accident") == 0)
    {
        inserare_eveniment(strada," ACCIDENT");
        for(int i = 0; i < NUMBER_THREADS ; i++)
        {
            if(file_descriptors[i] != -1)
            {
                pthread_mutex_lock(&lacate[i]);
                strcpy(mesaj,"\nPe ");
                strcat(mesaj,strada);
                strcat(mesaj," a fost raportat un ACCIDENT.\n\n");
                len = strlen(mesaj);
                if(write(file_descriptors[i],&len,sizeof(int)) == -1)
                {
                    printf("Eroare la write(1259).\n");
                    exit(EXIT_FAILURE);
                }
                if(write(file_descriptors[i],mesaj,len) == -1)
                {
                    printf("Eroare la write(1264).\n");
                    exit(EXIT_FAILURE);
                }
                pthread_mutex_unlock(&lacate[i]);
            }
        }
    }
}

int get_combustibil( char * username)
{
    sqlite3_stmt *res;
	int dc;
	char query[100]="";
	strcat(query, "SELECT combustibil from users where username = '");
	strcat(query, username);
	strcat(query,"'");
	dc = sqlite3_prepare_v2(db,query,-1,&res,0);
	if(dc != SQLITE_OK)
	{
			printf("Eroare la interogare.\n");
			sqlite3_close(db);
			exit(EXIT_FAILURE);
	}
	dc = sqlite3_step(res);
	while(dc != SQLITE_DONE)
	{
		int r = sqlite3_column_int(res,0);
		if(r == 1)
		{
			return 1;
		}
		else
		{
			return 0;
		}
		dc = sqlite3_step(res);
	}
}

int get_vreme( char* username)
{
    sqlite3_stmt *res;
	int dc;
	char query[100]="";
	strcat(query, "SELECT vreme from users where username = '");
	strcat(query, username);
	strcat(query,"'");
	dc = sqlite3_prepare_v2(db,query,-1,&res,0);
	if(dc != SQLITE_OK)
	{
			printf("Eroare la interogare.\n");
			sqlite3_close(db);
			exit(EXIT_FAILURE);
	}
	dc = sqlite3_step(res);
	while(dc != SQLITE_DONE)
	{
		int r = sqlite3_column_int(res,0);
		if(r == 1)
		{
			return 1;
		}
		else
		{
			return 0;
		}
		dc = sqlite3_step(res);
    }
}

int get_sport ( char* username)
{
    sqlite3_stmt *res;
	int dc;
	char query[100]="";
	strcat(query, "SELECT sport from users where username = '");
	strcat(query, username);
	strcat(query,"'");
	dc = sqlite3_prepare_v2(db,query,-1,&res,0);
	if(dc != SQLITE_OK)
	{
			printf("Eroare la interogare.\n");
			sqlite3_close(db);
			exit(EXIT_FAILURE);
	}
	dc = sqlite3_step(res);
	while(dc != SQLITE_DONE)
	{
		int r = sqlite3_column_int(res,0);
		if(r == 1)
		{
			return 1;
		}
		else
		{
			return 0;
		}
		dc = sqlite3_step(res);
	}
}

int check_password(char *username, char *msg)
{
	sqlite3_stmt *res;
	int dc;
	char query[100]="";
	strcat(query, "SELECT password from users where username = '");
	strcat(query, username);
	strcat(query,"'");
	dc = sqlite3_prepare_v2(db,query,-1,&res,0);
	if(dc != SQLITE_OK)
	{
			printf("Eroare la interogare.\n");
			sqlite3_close(db);
			exit(EXIT_FAILURE);
	}
	dc = sqlite3_step(res);
	while(dc != SQLITE_DONE)
	{
		if(strcmp(msg,sqlite3_column_text(res,0)) == 0)
		{
			return 1;
		}
		dc = sqlite3_step(res);
	}
	return 0;
}
int check_username(char *username)
{
	sqlite3_stmt *res;
	int dc;
	char query[100]="";
	strcat(query, "SELECT username from users where username = '");
	strcat(query, username);
	strcat(query,"'");
	dc = sqlite3_prepare_v2(db,query,-1,&res,0);
	if(dc != SQLITE_OK)
	{
			printf("Eroare la interogare.\n");
			sqlite3_close(db);
			exit(EXIT_FAILURE);
	}
	dc = sqlite3_step(res);
	while(dc != SQLITE_DONE)
	{
		if(strcmp(username,sqlite3_column_text(res,0)) == 0)
		{
			return 1;
		}
		dc = sqlite3_step(res);
	}
	return 0; 
}

void *extract_info(void * arg)
{
  int number, min, max;
  char mesaj[100], tip[20], nr[10];
  sqlite3_stmt *res;
  int dc;
  char query[100] = "SELECT * from vreme where id ='";
  sprintf(nr,"%d",number);
  strcat(query,nr);
  strcat(query,"'");
  dc = sqlite3_prepare_v2(db,query,-1,&res,0);
  if(dc != SQLITE_OK)
  {
    printf("Eroare la interogare.\n");
    sqlite3_close(db);
  }
  dc = sqlite3_step(res);
	strcpy(vremea,"\nVreme : ");
	strcat(vremea,sqlite3_column_text(res,1));
	strcat(vremea,"\nTemperatura de afara este de maxim ");
	strcat(vremea,sqlite3_column_text(res,2));
	strcat(vremea," grade si minim ");
	strcat(vremea,sqlite3_column_text(res,3));
	strcat(vremea," grade.\n");
	strcat(vremea,sqlite3_column_text(res,4));
	strcat(vremea,"\n\n");
        printf("%s",vremea);

    number = rand() % 2;
    strcpy(query,"Select * from sport where id = '"); 
	sprintf(nr,"%d",number);
	strcat(query,nr);
	strcat(query,"'");
	dc = sqlite3_prepare_v2(db,query,-1,&res,0);
	if(dc != SQLITE_OK)
	{
		printf("Eroare la interogare.\n");
		sqlite3_close(db);
		exit(EXIT_FAILURE);
	}
	dc = sqlite3_step(res);
	strcpy(sport,"\n");
	strcat(sport,sqlite3_column_text(res,1));
	strcat(sport," : ");
	strcat(sport,"Maine de la ora ");
	strcat(sport,sqlite3_column_text(res,4));
	strcat(sport," ");
	strcat(sport,sqlite3_column_text(res,2));
	strcat(sport," o va infrunta pe ");
	strcat(sport,sqlite3_column_text(res,3));
	strcat(sport,".\n\n");
	printf("%s",sport);

    time_t timer = time(NULL);
	struct tm *tm = localtime(&timer);
    int minute = tm->tm_min;
    while(1)
    {
			time_t timer = time(NULL);
			struct tm *tm = localtime(&timer);
			if(tm->tm_min % 2 == 0 && tm->tm_min != minute)
			{
				minute = tm->tm_min;
				number = rand() % 3;
				sqlite3_stmt * res;
				int dc;
				char query[100] = "Select * from vreme where id = '"; 
				sprintf(nr,"%d",number);
				strcat(query,nr);
				strcat(query,"'");
				dc = sqlite3_prepare_v2(db,query,-1,&res,0);
				if(dc != SQLITE_OK)
				{
					printf("Eroare la interogare.\n");
					sqlite3_close(db);
					exit(EXIT_FAILURE);
				}
				dc = sqlite3_step(res);
				strcpy(vremea,"\nVreme : ");
				strcat(vremea,sqlite3_column_text(res,1));
				strcat(vremea,"\nTemperatura de afara este de maxim ");
				strcat(vremea,sqlite3_column_text(res,2));
				strcat(vremea," grade si minim ");
				strcat(vremea,sqlite3_column_text(res,3));
				strcat(vremea," grade.\n");
				strcat(vremea,sqlite3_column_text(res,4));
				strcat(vremea,"\n\n");

				printf("%s",vremea);

				number = rand() % 2;
				strcpy(query,"Select * from sport where id = '"); 
				sprintf(nr,"%d",number);
				strcat(query,nr);
				strcat(query,"'");
				dc = sqlite3_prepare_v2(db,query,-1,&res,0);
				if(dc != SQLITE_OK)
				{
					printf("Eroare la interogare.\n");
					sqlite3_close(db);
					exit(EXIT_FAILURE);
				}
				dc = sqlite3_step(res);
				strcpy(sport,"\n");
				strcat(sport,sqlite3_column_text(res,1));
				strcat(sport," : ");
				strcat(sport,"Maine de la ora ");
				strcat(sport,sqlite3_column_text(res,4));
				strcat(sport," ");
				strcat(sport,sqlite3_column_text(res,2));
				strcat(sport," o va infrunta pe ");
				strcat(sport,sqlite3_column_text(res,3));
				strcat(sport,".\n\n");
				printf("%s",sport); 
            }
    }
}

void trimite_strazi(int cl,char *intersectie1, int idThread)
{
    char strazi[300];
    int len;
    sqlite3_stmt *res;
    int dc;
    char query[100] = "Select strada from strazi where intersectie1 = '";
    strcat(query,intersectie1);
    strcat(query,"' or intersectie2 = '");
    strcat(query,intersectie1);
    strcat(query,"'");
    dc = sqlite3_prepare_v2(db,query,-1,&res,0);
    if(dc != SQLITE_OK)
    {
        printf("Eroare la interogare.\n");
        sqlite3_close(db);
        exit(EXIT_FAILURE);
    }
    dc = sqlite3_step(res);
    strcpy(strazi,"Alege:");
    while(dc != SQLITE_DONE)
    {
        strcat(strazi,sqlite3_column_text(res,0));
        strcat(strazi,"!");
        dc = sqlite3_step(res);
    }

    len = strlen(strazi);
    pthread_mutex_lock(&lacate[idThread]);
    if(write(cl,&len,sizeof(int)) == -1)
    {
        printf("Eroare la write(1568).\n");
        exit(EXIT_FAILURE);
    }
    if(write(cl,strazi,len) == -1)
    {
        printf("Eroare la write(1573).\n");
        exit(EXIT_FAILURE);
    }
    pthread_mutex_unlock(&lacate[idThread]);
}

void trimite_combustibil(int cl, char *strada, int idThread)
{
    sqlite3_stmt *res;
    int dc;
    int len;
    char mesaj[100]="Pe aceasa strada se afla o benzinarie care are pretul la benzina ";
    char query[100] = "Select benzina, motorina from combustibil where strada ='";
    strcat(query,strada);
    strcat(query,"'");
    dc = sqlite3_prepare_v2(db,query,-1,&res,0);
    if(dc != SQLITE_OK)
    {
        printf("Eroare la interogare.\n");
        sqlite3_close(db);
        exit(EXIT_FAILURE);
    }

    dc = sqlite3_step(res);
    while(dc != SQLITE_DONE)
    {
        strcat(mesaj,sqlite3_column_text(res,0));
        strcat(mesaj," si la motorina ");
        strcat(mesaj,sqlite3_column_text(res,1));
        strcat(mesaj,".\n");
            dc = sqlite3_step(res);
    }
    
    if(strcmp(mesaj,"Pe aceasa strada se afla o benzinarie care are pretul la benzina ") == 0)
    {
        strcpy(mesaj,"Pe aceasta strada nu se afla benzinarii.\n");
    }
    len = strlen(mesaj);
    pthread_mutex_lock(&lacate[idThread]);
    if(write(cl,&len,sizeof(int)) == -1)
    {
        printf("Eroare la write(1614).\n");
        exit(EXIT_FAILURE);
    }
    if(write(cl,mesaj,len) == -1)
    {
        printf("Eroare la write(1619).\n");
        exit(EXIT_FAILURE);
    }
    pthread_mutex_unlock(&lacate[idThread]);
}

int get_distanta(char * strada)
{
    sqlite3_stmt *res;
    int dc;
    char query[100] = "Select distanta from strazi where strada = '";
    strcat(query,strada);
    strcat(query,"'");
    dc = sqlite3_prepare_v2(db,query,-1,&res,0);
    if(dc != SQLITE_OK)
    {
        printf("Eroare la interogare.(EXIT_FAILURE)\n");
        sqlite3_close(db);
        exit(EXIT_FAILURE);
    }
    dc = sqlite3_step(res);
    return sqlite3_column_int(res,0);
}

int get_limita_viteza(char * strada)
{
    sqlite3_stmt *res;
    int dc;
    char query[100] = "Select viteza from strazi where strada = '";
    strcat(query,strada);
    strcat(query,"'");
    dc = sqlite3_prepare_v2(db,query,-1,&res,0);
    if(dc != SQLITE_OK)
    {
        printf("Eroare la interogare.(2)\n");
        sqlite3_close(db);
        exit(EXIT_FAILURE);
    }
    dc = sqlite3_step(res);
    return sqlite3_column_int(res,0);
}
void get_intersectie2(char* intersectie, char * strada,char *intersectie_viitoare)
{
    sqlite3_stmt *res;
    int dc;
    char query[100] = "Select intersectie1, intersectie2 from strazi where strada ='";
    strcat(query,strada);
    strcat(query,"'");
    dc = sqlite3_prepare_v2(db,query,-1,&res,0);
    if(dc != SQLITE_OK)
    {
        printf("Eroare la interogare.(3)\n");
        sqlite3_close(db);
        exit(EXIT_FAILURE);
    }
    dc = sqlite3_step(res);

    if(strcmp(intersectie,sqlite3_column_text(res,0)) == 0)
    {
        strcpy(intersectie_viitoare,sqlite3_column_text(res,1));
    }
    else
    {
        strcpy(intersectie_viitoare,sqlite3_column_text(res,0));
    }

}

void check_eveniment(int cl, char *strada,int idThread)
{
    sqlite3_stmt *res;
    int dc;
    int len;
    char eveniment[100] = "Pe acest drum s-a raportat: ";
    char query[100] = "Select eveniment from evenimente where strada = '";
    strcat(query,strada);
    strcat(query,"'");
    dc = sqlite3_prepare_v2(db,query,-1,&res,0);
    if(dc != SQLITE_OK)
    {
        printf("Eroare la interogare.\n");
        exit(EXIT_FAILURE);
    }
    dc = sqlite3_step(res);
    while ( dc != SQLITE_DONE)
    {
        strcat(eveniment,sqlite3_column_text(res,0));
        dc = sqlite3_step(res);
    }

    if(strcmp(eveniment,"Pe acest drum s-a raportat: ") != 0)
    {

        len = strlen(eveniment);
        if(write(cl,&len,sizeof(int)) == -1)
        {
            printf("Eroare la write(1715).\n");
            exit(EXIT_FAILURE);
        }
        if(write(cl,eveniment,len) == -1)
        {
            printf("Eroare la write(1720).\n");
            exit(EXIT_FAILURE);
        }

    }

}
void inserare_eveniment(char *strada, char * eveniment)
{
    sqlite3_stmt *res;
    int dc;
    char result[100] = "";
    char query[100] = "SELECT strada from evenimente where eveniment = '";
    strcat(query,eveniment);
    strcat(query,"' AND strada = '");
    strcat(query,strada);
    strcat(query,"'");
    dc = sqlite3_prepare_v2(db,query,-1,&res,0);
    if(dc != SQLITE_OK)
    {
        printf("Eroare la interogare.\n");
        exit(EXIT_FAILURE);
    }
    dc = sqlite3_step(res);

    if(sqlite3_column_text(res,0) != NULL)
    {
        strcat(result,sqlite3_column_text(res,0));
    }

    if(strcmp(result,"") == 0)
    {
        strcpy(query,"INSERT INTO evenimente(strada,eveniment)VALUES('");
        strcat(query,strada);
        strcat(query,"','");
        strcat(query,eveniment);
        strcat(query,"')");
        dc = sqlite3_exec(db,query,0,0,&err_msg);
        if(dc != SQLITE_OK)
        {
            printf("Eroare la inserarea in baza de date!\n");
            exit(EXIT_FAILURE);
        }
    }
}
