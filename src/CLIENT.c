#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <sqlite3.h>
#include <time.h>
#include <stdio.h>
#include <pthread.h>
#include <signal.h>


#define IP "127.0.0.1"
#define PORT 2002

// thread-uri ce comunica cu serverul
void *cititor(void * i);
void *speed(void *i);
void alege_strada(int sd, char *strada, char *alegere);

pthread_mutex_t lacat = PTHREAD_MUTEX_INITIALIZER;

int len;
char mesaj[300];
char input[100];
char comanda[100];
int locatie_corecta = 0;

struct account{
  char username[100];
  char password[100];
  char strada[30];
  int vreme;
  int sport;
  int combustibil;
}acc;

pthread_t operator;
pthread_t viteza;

int main(int argc, char*argv[])
{
    int input_corect = 0;
    int sd;
    struct sockaddr_in server;

    if((sd = socket(AF_INET, SOCK_STREAM,0)) == -1)
    {
        printf("Eroare la socket().\n");
        exit(1);
    }

    // umplem structura folosita pentru realizarea conexiunii cu serverul 
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(IP);
    server.sin_port = htons(PORT);
    printf("Conectare . . . \n");
    if(connect (sd, (struct sockaddr*) &server, sizeof(struct sockaddr)) == -1)
    {
        perror("[client] Eroare la connect().\n");
        return errno;
    }
    printf("BINE AI VENIT!\n");
    while(input_corect == 0)
    {
        printf("[client] Alege o actiune (Autentificare / Inregistrare / Exit): ");
        fgets(input,100,stdin);
        input[strlen(input) - 1] = '\0';
        if(strcmp(input,"Inregistrare")==0 || strcmp(input,"Autentificare")==0 || strcmp(input,"Exit")==0)
        {
          input_corect = 1;
        }
        else
        {
            printf("[client] Introdu o comanda valida!\n");
        }
        if(strcmp(input,"Exit") == 0)
        {
            len = strlen("Exit");
            if(write(sd,&len,sizeof(int)) == -1)
            {
                printf("Eroare la write(86).\n");
                exit(1);
            }

            printf("LA REVEDERE!\n");

            if(write(sd,"Exit",len) == -1)
            {
                printf("Eroare la write(94).\n");
                exit(1);
            }
            exit(1);
        }
    }
    if(strcmp(input,"Autentificare") == 0)
    {
        int user_valid = 0;
        int parola_valida = 0;
        
        len = strlen("Autentificare");
        if(write(sd,&len,sizeof(int)) == -1)
        {
            printf("Eroare la write(108).\n");
            exit(1);
        }
        if(write(sd,"Autentificare",len) == -1)
        {
            printf("Eroare la write(113).\n");
            exit(1);
        } 
        
        while(user_valid == 0)
        {
            printf("Introdu numele de utilizator: ");
            fgets(acc.username,100,stdin);
            acc.username[strlen(acc.username) - 1] = '\0';

            if(strcmp(acc.username,"Exit") == 0)
            {
                len = strlen("Exit");
                if(write(sd,&len,sizeof(int)) == -1)
                {
                    printf("Eroare la write(128).\n");
                    exit(1);
                }

                printf("LA REVEDERE!\n");

                if(write(sd,"Exit",len) == -1)
                {
                    printf("Eroare la write(136).\n");
                    exit(1);
                } 
                exit(1);
            }

            len = strlen(acc.username);
            if(write(sd,&len,sizeof(int)) == -1)
            {
                printf("Eroare la write(145).\n");
                exit(1);
            }
            if(write(sd,acc.username,len) == -1)
            {
                printf("Eroare la write(150).\n");
                exit(1);
            }

            if(read(sd,&len,sizeof(int)) == -1)
            {
                printf("Eroare la read(156).\n");
                exit(1);
            }
            if(read(sd,mesaj,len) == -1)
            {
                printf("Eroare la read(161)\n");
                exit(1);
            }
            mesaj[len] = '\0';
            if(strcmp(mesaj,"User valid.") == 0)
            {
                user_valid = 1;
                printf("Nume de utilizator valid.\n");
            }
            else
            {
                printf("Nume de utilizator invalid.\n");
            }
        }
        while(parola_valida == 0)
        {
            printf("Introduceti parola: ");
            fgets(acc.password,100,stdin);
            acc.password[strlen(acc.password) - 1] = '\0';
            if(strcmp(acc.password,"Exit") == 0)
            {
                len = strlen("Exit");
                if(write(sd,&len,sizeof(int)) == -1)
                {
                    printf("Eroare la write(185).\n");
                    exit(1);
                }

                printf("LA REVEDERE!\n");

                if(write(sd,"Exit",len) == -1)
                {
                    printf("Eroare la write(193).\n");
                    exit(1);
                } 
                exit(1);
            }

            len = strlen(acc.password);
            if(write(sd,&len,sizeof(int)) == -1)
            {
                printf("Eroare la write(202).\n");
                exit(1);
            }
            if(write(sd,acc.password,len) == -1)
            {
                printf("Eroare la write(207).\n");
                exit(1);
            }


            if(read(sd,&len,sizeof(int)) == -1)
            {
                printf("Eroare la read(214).\n");
                exit(1);
            }
            if(read(sd,mesaj,len) == -1)
            {
                printf("Eroare la read(219)\n");
                exit(1);
            }
            mesaj[len] = '\0';
            if(strcmp(mesaj,"Parola valida.") == 0)
            {
                parola_valida = 1;
                printf("Parola valida.\n");
            }
            else
            {
                printf("Parola invalida.\n");
            }
        }
    }
    if(strcmp(input,"Inregistrare") == 0)
    {
        char confirmare[100];
        int reusit = 0;
        int loop = 0;
        int user_valid = 0;
        int parola_valida = 0;
        int vreme_valid = 0;
        int sport_valid = 0;
        int combustibil_valid = 0;

        len = strlen("Inregistrare");
        if(write(sd,&len,sizeof(int)) == -1)
        {
            printf("Eroare la write(248).\n");
            exit(1);
        }
        if(write(sd,"Inregistrare",len) == -1)
        {
            printf("Eroare la write(253).\n");
            exit(1);
        }
        while(reusit == 0)
        {
            user_valid = 0;
            parola_valida = 0;
            vreme_valid = 0;
            sport_valid = 0;
            combustibil_valid = 0;
            
            while(user_valid == 0)
            {
                printf("Introdu numele de utilizator: ");
                fgets(acc.username,100,stdin);
                acc.username[strlen(acc.username) - 1] = '\0';
                if(strcmp(acc.username, "Exit") == 0)
                {
                    len = strlen("Exit");
                    if(write(sd,&len,sizeof(int)) == -1)
                    {
                        printf("Eroare la write(274).\n");
                        exit(1);
                    }
                    if(write(sd,"Exit", len) == -1)
                    {
                        printf("Eroare la write(279).\n");
                        exit(1);
                    }
                    printf("LA REVEDERE!\n");
                    exit(1);
                }
                
                len = strlen(acc.username);
                if(write(sd,&len,sizeof(int)) == -1)
                {
                    printf("Eroare la write(289).\n");
                    exit(1);
                }
                if(write(sd,acc.username,len) == -1)
                {
                    printf("Eroare la write(294).\n");
                    exit(1);
                }
                if(read(sd,&len,sizeof(int)) == -1)
                {
                    printf("Eroare al read(298).\n");
                    exit(1);
                }
                if(read(sd,mesaj,len) == -1)
                {
                    printf("Eroare la read(304).\n");
                    exit(1);
                }
                mesaj[len] = '\0';

                if(strcmp(mesaj,"User valid.") == 0)
                {
                    user_valid = 1;
                    printf("Nume de utilizator valid.\n");
                }
                else
                {
                    printf("Nume de utilizator deja existent. Te rugam introdu alt nume.\n");
                }
            }
            while(parola_valida == 0)
            {
                printf("Introdu parola: ");
                fgets(acc.password,100,stdin);
                acc.password[strlen(acc.password) - 1] = '\0';
                if(strcmp(acc.password, "Exit") == 0)
                {
                    len = strlen("Exit");
                    if(write(sd,&len,sizeof(int)) == -1)
                    {
                        printf("Eroare la write(329).\n");
                        exit(1);
                    }
                    if(write(sd,"Exit", len) == -1)
                    {
                        printf("Eroare la write(334).\n");
                        exit(1);
                    }
                    printf("LA REVEDERE!\n");
                    exit(1);
                }
                len = strlen(acc.password);
                if(write(sd,&len,sizeof(int)) == -1)
                {
                    printf("Eroare la write(343).\n");
                    exit(1);
                }
                if(write(sd,acc.password,len) == -1)
                {
                    printf("Eroare la write(348).\n");
                    exit(1);
                }
                printf("Confirma parola: ");
                fgets(confirmare,100,stdin);
                confirmare[strlen(confirmare) - 1] = '\0';

                len = strlen(confirmare);
                if(strcmp(confirmare, "Exit") == 0)
                {
                    len = strlen("Exit");
                    if(write(sd,&len,sizeof(int)) == -1)
                    {
                        printf("Eroare la write(361).\n");
                        exit(1);
                    }
                    if(write(sd,"Exit", len) == -1)
                    {
                        printf("Eroare la write(366).\n");
                        exit(1);
                    }
                    printf("LA REVEDERE!\n");
                    exit(1);
                }
                if(write(sd,&len,sizeof(int)) == -1)
                {
                    printf("Eroare la write(374).\n");
                    exit(1);
                }
                if(write(sd,confirmare,len) == -1)
                {
                    printf("Eroare la write(379).\n");
                    exit(1);
                }
                if(read(sd,&len,sizeof(int)) == -1)
                {
                    printf("Eroare la read(353).\n");
                    exit(1);
                }
                if(read(sd,mesaj,len) == -1)
                {
                    printf("Eroare la read(358).\n");
                    exit(1);
                }
                mesaj[len] = '\0';
                if(strcmp(mesaj,"Parola valida.") == 0)
                {
                    parola_valida = 1;
                    printf("Parola valida!\n");
                }
                else
                {
                    printf("%s\n",mesaj);
                    printf("Parola gresita!\n");
                }
            }
            while(vreme_valid == 0)
            {
                printf("Doresti sa primesti informatii despre vreme? [Da/Nu]");
                fgets(mesaj,300,stdin);
                mesaj[strlen(mesaj) - 1] = '\0';

                len = strlen(mesaj);
                if(strcmp(mesaj, "Exit") == 0)
                {
                    len = strlen("Exit");
                    if(write(sd,&len,sizeof(int)) == -1)
                    {
                        printf("Eroare la write(416).\n");
                        exit(1);
                    }
                    if(write(sd,"Exit", len) == -1)
                    {
                        printf("Eroare la write(421).\n");
                        exit(1);
                    }
                    printf("LA REVEDERE!\n");
                    exit(1);
                }
                if(write(sd,&len,sizeof(int)) == -1)
                {
                    printf("Eroare la write(429).\n");
                    exit(1);
                }
                if(write(sd,mesaj,len) == -1)
                {
                    printf("Eroare la write(434).\n");
                    exit(1);
                }
                if(read(sd,&len,sizeof(int)) == -1)
                {
                    printf("Eroare la read(439).\n");
                    exit(1);
                }
                if(read(sd,mesaj,len) == -1)
                {
                    printf("Eroare la read(444).\n");
                    exit(1);
                }
                mesaj[len]='\0';
                if(strcmp(mesaj,"Raspuns valid.") == 0)
                {
                    vreme_valid = 1;
                }
                else
                {
                    printf("%s\n", mesaj);
                    printf("Comanda invalida!\n");
                }
            }
            while(sport_valid == 0)
            {
                printf("Doresti sa primesti informatii despre sport? [Da/Nu]");
                fgets(mesaj,300,stdin);
                mesaj[strlen(mesaj) - 1] = '\0';
                
                if(strcmp(mesaj, "Exit") == 0)
                {
                    len = strlen("Exit");
                    if(write(sd,&len,sizeof(int)) == -1)
                    {
                        printf("Eroare la write(469).\n");
                        exit(1);
                    }
                    if(write(sd,"Exit", len) == -1)
                    {
                        printf("Eroare la write(474).\n");
                        exit(1);
                    }
                    printf("LA REVEDERE!\n");
                    exit(1);
                }
                len = strlen(mesaj);
                if(write(sd,&len,sizeof(int)) == -1)
                {
                    printf("Eroare la write(483).\n");
                    exit(1);
                }
                if(write(sd,mesaj,len) == -1)
                {
                    printf("Eroare la write(488).\n");
                    exit(1);
                }
                if(read(sd,&len,sizeof(int)) == -1)
                {
                    printf("Eroare la read(493).\n");
                    exit(1);
                }
                if(read(sd,mesaj,len) == -1)
                {
                    printf("Eroare la read(498).\n");
                    exit(1);
                }
                mesaj[len]='\0';
                if(strcmp(mesaj,"Raspuns valid.") == 0)
                {
                    sport_valid = 1;
                }
                else
                {
                    printf("Comanda invalida!\n");
                }
            }
            while(combustibil_valid == 0)
            {
                printf("Doresti sa primesti informatii despre preturile la combustibil? [Da/Nu]");
                fgets(mesaj,300,stdin);
                mesaj[strlen(mesaj) - 1] = '\0';

                len = strlen(mesaj);
                
                if(strcmp(mesaj, "Exit") == 0)
                {
                    len = strlen("Exit");
                    if(write(sd,&len,sizeof(int)) == -1)
                    {
                        printf("Eroare la write(524).\n");
                        exit(1);
                    }
                    if(write(sd,"Exit", len) == -1)
                    {
                        printf("Eroare la write(529).\n");
                        exit(1);
                    }
                    printf("LA REVEDERE!\n");
                    exit(1);
                }
                if(write(sd,&len,sizeof(int)) == -1)
                {
                    printf("Eroare la write(537).\n");
                    exit(1);
                }
                if(write(sd,mesaj,len) == -1)
                {
                    printf("Eroare la write(542).\n");
                    exit(1);
                }
                if(read(sd,&len,sizeof(int)) == -1)
                {
                    printf("Eroare la read(547).\n");
                    exit(1);
                }
                if(read(sd,mesaj,len) == -1)
                {
                    printf("Eroare la read(552).\n");
                    exit(1);
                }
                mesaj[len]='\0';
                if(strcmp(mesaj,"Raspuns valid.") == 0)
                {
                    combustibil_valid = 1;
                }
                else
                {
                    printf("Comanda invalida!\n");
                }
            }
            if(read(sd,&len,sizeof(int)) == -1)
            {
                printf("Eroare la read(567).\n");
                exit(1);
            }
            if(read(sd,mesaj,len) == -1)
            {
                printf("Eroare la read(572).\n");
                exit(1);
            }
            mesaj[len] = '\0';
            if(strcmp(mesaj,"Cont creat.") == 0)
            {
                reusit = 1;
                printf("Cont creat cu succes!\n");
            }
            else
            {
                printf("Eroare la crearea contului. Va rugam reincercati.\n");
            }

        }
        
    }

    if(read(sd,&acc.vreme,sizeof(int)) == -1)
    {
        printf("Eroare la read(592).\n");
        exit(1);
    }
    if(read(sd,&acc.sport,sizeof(int)) == -1)
    {
        printf("Eroare la read(597).\n");
        exit(1);
    }
    if(read(sd,&acc.combustibil,sizeof(int)) == -1)
    {
        printf("Eroare la read(602).\n");
        exit(1);
    }

    if(acc.vreme == 1)
    {
        if(read(sd,&len,sizeof(int)) == -1)
        {
            printf("Eroare la write(610).\n");
            exit(1);
        }
        if(read(sd,mesaj,len) == -1)
        {
            printf("Eroare la write(615).\n");
            exit(1);
        }
        printf("%s\n",mesaj);
    }
    if(acc.sport == 1)
    {
        if(read(sd,&len,sizeof(int)) == -1)
        {
            printf("Eroare la write(624).\n");
            exit(1);
        }
        if(read(sd,mesaj,len) == -1)
        {
            printf("Eroare la write(629).\n");
            exit(1);
        }
        printf("%s\n",mesaj);
    }


    while(locatie_corecta == 0)
    {
        printf("INTRODUCETI LOCATIA DE PLECARE:\n-Mioritei\n-Piata Nord\n-Piata Tricolorului\n-Piata Mihai Eminescu\n-Orizont\n-Exit\n\n");
        fgets(mesaj,100,stdin);
        mesaj[strlen(mesaj) - 1] = '\0';
        if(strcmp(mesaj,"Mioritei") == 0 ||
            strcmp(mesaj,"Piata Tricolorului") == 0 ||
            strcmp(mesaj,"Piata Mihai Eminescu") == 0 ||
            strcmp(mesaj,"Orizont") == 0 ||
            strcmp(mesaj,"Piata Nord") == 0 )
        {
            locatie_corecta = 1;
        }
        else
        if(strcmp(mesaj,"Exit") == 0)
        {
            len = strlen(mesaj);
            if(write(sd,&len,sizeof(int)) == -1)
            {
                printf("Eroare la write(655).\n");
                exit(1);
            }
            if(write(sd,mesaj,len) == -1)
            {
                printf("Eroare la write(660).\n");
                exit(1);
            }
            printf("LA REVEDERE!\n");
            exit(1);
        }
        else
        {
            printf("INTRODUCETI O LOCATIE VALIDA!.\n\n");
        }
    }

    len = strlen(mesaj);
    if(write(sd,&len,sizeof(int)) == -1)
    {
        printf("Eroare la write(675).\n");
        exit(1);
    }
    if(write(sd,mesaj,len) == -1)
    {
        printf("Eroare la write(680).\n");
        exit(1);
    }

    if(read(sd,&len,sizeof(int)) == -1)
    {
        printf("Eroare la read(686).\n");
        exit(1);
    }
    if(read(sd,mesaj,len) == -1)
    {
        printf("Eroare la read(691).\n");
        exit(1);
    }
    mesaj[len] = '\0';
    printf("%s\n",mesaj);

    alege_strada(sd,mesaj,acc.strada);
    printf("%s\n",acc.strada);
    pthread_create(&operator,NULL,&cititor,(void *) sd);
    pthread_create(&viteza,NULL,&speed,(void *)sd);
    printf("\nINTRODUCETI O COMANDA:\nAccident\nPolitie\nTrafic\nVreme\nSport\nCombustibil\nExit\n\n");
    while(1)
    {
        strcpy(comanda,"comanda:");
        fgets(mesaj,100,stdin);
        mesaj[strlen(mesaj) - 1] = '\0';
        pthread_mutex_lock(&lacat);
        strcat(comanda,mesaj);
        len = strlen(comanda);
        if(write(sd,&len,sizeof(int)) == -1)
        {
            printf("Eroare la write(712).\n");
            exit(1);
        }
        if(write(sd,comanda,len) == -1)
        {
            printf("Eroare la write(717).\n");
            exit(1);
        }
        if(strcmp(mesaj,"Exit") == 0)
        {
            printf("LA REVEDERE!.\n");
            exit(1);
        }
        pthread_mutex_unlock(&lacat);
        
    }

}


void *cititor( void * arg)
{
    int sd;
    sd = (int) arg;
    int len2;
    char mesaj2[300];
    char var[10];
    while(1)
    {
        if(read(sd,&len2,sizeof(int)) == -1)
        {
            printf("Eroare la read(743).\n");
            exit(1);
        }
        if(read(sd,mesaj2,len2) == -1)
        {
            printf("Eroare la read(748).\n");
            exit(1);
        }
        mesaj2[len2] = '\0';
        strncpy(var,mesaj2,6);
        var[strlen(var)] = '\0';
        if(strcmp(var,"Alege:") == 0)
        {
            alege_strada(sd,mesaj2,acc.strada);
        }
        else
        {
            printf("%s\n",mesaj2);
        }
    }
}

void *speed( void * arg )
{
    int sd;
    sd = (int) arg;
    clock_t begin;
    double time_spent;
    unsigned int i;
    begin = clock();
    char viteza[20];
    char valoarechar[10];
    int valoare;
    strcpy(viteza,"viteza:");
    valoare = rand() % 80;
    sprintf(valoarechar,"%d",valoare);
    strcat(viteza,valoarechar);
    pthread_mutex_lock(&lacat);
    len = strlen(viteza);
    if(write(sd,&len,sizeof(int)) == -1)
    {
        printf("Eroare la write(784).\n");
        exit(1);
    }
    if(write(sd,viteza,len) == -1)
    {
        printf("Eroare la write(789).\n");
        exit(1);
    }
    pthread_mutex_unlock(&lacat);
    while(1)
    {
        time_spent = (double)(clock()-begin) / CLOCKS_PER_SEC;
        if(time_spent >= 60.0)
        {
            pthread_mutex_lock(&lacat);
            strcpy(viteza,"viteza:");
            valoare = rand() % 80;
            sprintf(valoarechar,"%d",valoare);
            strcat(viteza,valoarechar);
            len = strlen(viteza);
            if(write(sd,&len,sizeof(int)) == -1)
            {
                printf("Eroare la write(806).\n");
                exit(1);
            }
            if(write(sd,viteza,len) == -1)
            {
                printf("Eroare la write.(811)\n");
                exit(1);
            }
        
            begin = clock();
            pthread_mutex_unlock(&lacat);
        }
    }
}

void alege_strada(int sd, char *strada, char *alegere)
{
    strcpy(strada,strada+6);
    char vector_cuvinte[10][300];
    char *tok;
    int len;
    int number;
    int i = 0;
    tok = strtok(strada,"!");
    while(tok != NULL)
    {
        strcpy(vector_cuvinte[i],tok);
        tok=strtok(NULL,"!");
        i++; 
    }
    number = rand() % i;

    len = strlen(vector_cuvinte[number]);
    strcpy(alegere,vector_cuvinte[number]);
    pthread_mutex_lock(&lacat);
    if(write(sd,&len,sizeof(int)) == -1)
    {
        printf("Eroare la write(843).\n");
        exit(1);
    }
    if(write(sd,vector_cuvinte[number],len) == -1)
    {
        printf("Eroare la write(848).\n");
        exit(1);
    }
    pthread_mutex_unlock(&lacat);
}