
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
#include<sqlite3.h>

#define PORT 2909
extern int errno;

int spots[10];
int free_spots=0,rc=0;
sqlite3* db;
sqlite3_stmt *stmt;
char* err_msg=0;

static void *treat(void *);

typedef struct {
	pthread_t idThread; //id-ul thread-ului
	int thCount; //nr de conexiuni servite
}Thread;

Thread *threadsPool; //un array de structuri Thread

int sd; //descriptorul de socket de ascultare
int nthreads;//numarul de threaduri

pthread_mutex_t parking=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mlock=PTHREAD_MUTEX_INITIALIZER;              // variabila mutex ce va fi partajata de threaduri

static int callback(void *data, int argc, char **argv, char **azColName) {
    for (int i = 0; i < argc; i++) {
        printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
    }
    printf("\n");
    return 0;
}

void initializare_locuri(int spots[10])
{
    for(int i=0;i<10;i++)
        spots[i]=rand()%2; // 0 liber si 1 ocupat
}

int simulate_senzor(int spots[10],int spotId) // detecteaza daca este liber sau ocupat locul de parcare cautat
{
    pthread_mutex_lock(&parking);
    int result=(spots[spotId]==0);
    pthread_mutex_unlock(&parking);

    return result;
}

int freespots(int spots[10])
{
    int nr=0;
    pthread_mutex_lock(&parking);

    for(int i=0;i<10;i++)
        if(spots[i]==0)
            ++nr;

    pthread_mutex_unlock(&parking);
    return nr;
}

char* show_spots(int spots[10])
{
    char* mesaj=malloc(200);

    if(freespots(spots)==0)
        strcpy(mesaj,"Nu este niciun loc liber");
    else
    {
        strcpy(mesaj,"Urmatoarele locuri sunt libere: ");
        pthread_mutex_lock(&parking);

        for(int i=0;i<10;i++)
            if(spots[i]==0)
            {
                char temp[2];
                temp[0]=i+'0';
                strcat(mesaj,temp);
                strcat(mesaj," ");
            }

        pthread_mutex_unlock(&parking);
    }

    return mesaj;
}

void simulate_camera(int spots[10])
{
    for(int i=0;i<10;i++)
        spots[i]=rand()%2; // simulam camerele, care genereaza initial starea locurilor de parcare
}


void raspunde(int cl,int idThread);

int main (int argc, char *argv[])
{
  struct sockaddr_in server;	// structura folosita de server  	
  void threadCreate(int);

   if(argc<2)
   {
        fprintf(stderr,"Eroare: Primul argument este numarul de fire de executie...");
	exit(1);
   }

   initializare_locuri(&spots);
   simulate_camera(&spots);

    FILE* file=fopen("parking.db","r");

    if(file==NULL)
    {
        rc=sqlite3_open("parking.db",&db);

        if(rc!=SQLITE_OK)
        {
            printf("Eroare la deschiderea bazei de date");
            return 1;
        }
        printf("Baza de date a fost deschisa \n");

        const char* create="CREATE TABLE Parcare(Loc INT,Stare INT);";
        rc=sqlite3_exec(db,create,0,0,&err_msg);

        if(rc!=SQLITE_OK)
        {
            printf("Eroare la creare");
            return 1;
        }
        printf("Creare cu succes \n");

        for(int i=0;i<10;i++)
        {
            char insert[100];
            snprintf(insert,sizeof(insert),"INSERT INTO Parcare(Loc,Stare) VALUES(%d,%d);",i,spots[i]);
            rc=sqlite3_exec(db,insert,0,0,&err_msg);

            if(rc!=SQLITE_OK)
            {
                printf("Eroare la insert");
                return 1;
            }
        }
        //fclose(file);
    }
    else
    {
        rc=sqlite3_open("parking.db",&db);

        if(rc!=SQLITE_OK)
        {
            printf("Eroare la deschiderea bazei de date");
            return 1;
        }
        printf("Baza de date a fost deschisa \n");
        //fclose(file);
    }

    printf("da \n");
    const char* all="SELECT * FROM Parcare;";
    rc=sqlite3_exec(db,all,callback,0,&err_msg);

    if(rc!=SQLITE_OK)
    {
        printf("Eroare la deschiderea bazei de date");
        return 1;
    }

    const char* select="SELECT Stare FROM Parcare;";
    int j=-1;
    rc = sqlite3_prepare_v2(db, select, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }

    // Execute the query and process the results
    printf("Column data:\n");
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int value = sqlite3_column_int(stmt, 0); // 0 for the first column
        printf("valaore este: %d",value);
        spots[++j]=value;
    }

    // Finalize the statement and close the database
    sqlite3_finalize(stmt);

   nthreads=atoi(argv[1]);
   if(nthreads <=0)
	{
        fprintf(stderr,"Eroare: Numar de fire invalid...");
	exit(1);
	}
    threadsPool = calloc(sizeof(Thread),nthreads);

   /* crearea unui socket */
  if ((sd = socket (AF_INET, SOCK_STREAM, 0)) == -1)
    {
      perror ("[server]Eroare la socket().\n");
      return errno;
    }
  /* utilizarea optiunii SO_REUSEADDR */
  int on=1;
  setsockopt(sd,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on));
  
  /* pregatirea structurilor de date */
  bzero (&server, sizeof (server));

  /* umplem structura folosita de server */
  /* stabilirea familiei de socket-uri */
    server.sin_family = AF_INET;	
  /* acceptam orice adresa */
    server.sin_addr.s_addr = htonl (INADDR_ANY);
  /* utilizam un port utilizator */
    server.sin_port = htons (PORT);
  
  /* atasam socketul */
  if (bind (sd, (struct sockaddr *) &server, sizeof (struct sockaddr)) == -1)
    {
      perror ("[server]Eroare la bind().\n");
      return errno;
    }

  /* punem serverul sa asculte daca vin clienti sa se conecteze */
  if (listen (sd, 2) == -1)
    {
      perror ("[server]Eroare la listen().\n");
      return errno;
    }

   printf("Nr threaduri %d \n", nthreads); fflush(stdout);
   int i;
   for(i=0; i<nthreads;i++) threadCreate(i);

	
  /* servim in mod concurent clientii...folosind thread-uri */
  for ( ; ; ) 
  {
	printf ("[server]Asteptam la portul %d...\n",PORT);
        pause();				
  }
};
				
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
 		printf ("[thread]- %d - pornit...\n", (int) arg);fflush(stdout);

		for( ; ; )
		{
			int length = sizeof (from);
			pthread_mutex_lock(&mlock);
			//printf("Thread %d trezit\n",(int)arg);
			if ( (client = accept (sd, (struct sockaddr *) &from, &length)) < 0)
				{
	 			 perror ("[thread]Eroare la accept().\n");	  			
				}
			pthread_mutex_unlock(&mlock);
			threadsPool[(int)arg].thCount++;

			raspunde(client,(int)arg); //procesarea cererii
			/* am terminat cu acest client, inchidem conexiunea */
			close (client);			
		}	
}


void raspunde(int cl, int idThread)
{
    char mesaj[20];    // msajul primit de la client
    char trimitere[100] = {};
    int spotId = 0,ok=0,loc_ocupat=-1,locuri=0;
    bzero(trimitere, sizeof(trimitere));
   
    while (1) 
    {
        int bytes_read = read(cl, mesaj, sizeof(mesaj) - 1);

        if (bytes_read <= 0) {
            printf("[Thread %d] Clientul a închis conexiunea sau a apărut o eroare la read().\n", idThread);
            perror("[Thread]");
            break; 
        }

        mesaj[bytes_read] = '\0'; 
        printf("[Thread %d] Mesajul a fost primit de către server: %s\n", idThread, mesaj);

        spotId = 0; 
        bzero(trimitere, sizeof(trimitere)); 

        if (strstr(mesaj, "FIND")) {

            for (int i = 0; i < strlen(mesaj); i++)
                if (strchr("0123456789", mesaj[i]))
                    spotId = spotId * 10 + (mesaj[i] - '0');

            printf("Locul este %d\n", spotId);

            if (simulate_senzor(spots, spotId) == 1)
                sprintf(trimitere, "Locul %d este liber", spotId);
            else
                sprintf(trimitere, "Locul %d nu este liber", spotId);
        } else if (strstr(mesaj, "RESERVE")) {

            if(locuri==1)
                strcpy(trimitere,"Nu poti rezerva mai multe locuri in acelasi timp");
            else
            {
                for (int i = 0; i < strlen(mesaj); i++)
                     if (strchr("0123456789", mesaj[i]))
                        spotId = spotId * 10 + (mesaj[i] - '0');

                 printf("Locul este %d\n", spotId);

                if (simulate_senzor(spots, spotId) == 1) {
                    pthread_mutex_lock(&parking);
                    char update[100];
                    snprintf(update,sizeof(update),"UPDATE Parcare SET Stare=1 WHERE Loc=%d",spotId);
                    rc=sqlite3_exec(db,update,0,0,&err_msg);
                    
                    if(rc!=SQLITE_OK){
                        printf("Eroare la update \n");
                        return 1;
                    }
                    locuri=1;
                    loc_ocupat=spotId;
                    spots[spotId] = 1;
                    sprintf(trimitere, "Am rezervat locul %d", spotId);
                    pthread_mutex_unlock(&parking);
                } else
                    sprintf(trimitere, "Locul %d nu este liber", spotId);
            }
        } else if (strstr(mesaj, "FREE")) {

            for (int i = 0; i < strlen(mesaj); i++)
                if (strchr("0123456789", mesaj[i]))
                    spotId = spotId * 10 + (mesaj[i] - '0');

            printf("Locul este %d\n", spotId);
            pthread_mutex_lock(&parking);

            if(loc_ocupat==spotId)
            {
                char update[100];
                snprintf(update,sizeof(update),"UPDATE Parcare SET Stare=0 WHERE Loc=%d",spotId);
                rc=sqlite3_exec(db,update,0,0,&err_msg);
                    
                if(rc!=SQLITE_OK){
                    printf("Eroare la update \n");
                    return 1;
                }
                spots[spotId] = 0;
                locuri=0;
                sprintf(trimitere, "Am eliberat locul %d", spotId);
            }
            else
                strcpy(trimitere,"Nu poti elibera alt loc");
            
            pthread_mutex_unlock(&parking);      
        } else if (strstr(mesaj, "QUIT")) {

            printf("[Thread %d] Clientul a trimis comanda QUIT. Închidem conexiunea.\n", idThread);
            strcpy(trimitere, "Inchidem clientul");
            write(cl, trimitere, strlen(trimitere)); // Trimite mesajul final către client
            break; 
        } else if(strstr(mesaj,"NUMBER SPOTS")){
            int nr=freespots(spots);

            sprintf(trimitere,"Sunt %d locuri libere",nr);
        } else if(strstr(mesaj,"SHOW SPOTS")){
            char *mesaj=malloc(200);
            mesaj=show_spots(spots);
            strcpy(trimitere,mesaj);
            free(mesaj);
        } else if(strstr(mesaj,"DATABASE")){
            const char*select="SELECT * FROM Parcare;";            
            rc=sqlite3_exec(db,select,callback,0,&err_msg);

            if(rc!=SQLITE_OK)
            {
                printf("Eroare la deschiderea bazei de date");
                return 1;
            }

            strcpy(trimitere,"Am afisat in server baza de date");
        }else{
            sprintf(trimitere, "Comandă necunoscută: %s", mesaj);
        }

        if (spotId < 0 || spotId > 9) {
            sprintf(trimitere, "Locul %d nu există", spotId);
        }

        // Trimitem răspunsul către client
        int bytes_written = write(cl, trimitere, strlen(trimitere));
        if (bytes_written <= 0) {
            printf("[Thread %d] Eroare la write() către client.\n", idThread);
            perror("[Thread]");
            break; 
        }

        printf("[Thread %d] Mesajul a fost transmis către client: %s\n", idThread, trimitere);
    }
}
