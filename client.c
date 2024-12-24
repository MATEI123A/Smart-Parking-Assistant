#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>

/* codul de eroare returnat de anumite apeluri */
extern int errno;

/* portul de conectare la server */
int port;

int main(int argc, char *argv[])
{
    int sd;                        // descriptorul de socket
    struct sockaddr_in server;     // structura folosita pentru conectare
    char buf[100], mesaj[100];
    int bytes_read;

    /* exista toate argumentele in linia de comanda? */
    if (argc != 3)
    {
        printf("Sintaxa: %s <adresa_server> <port>\n", argv[0]);
        return -1;
    }

    /* stabilim portul */
    port = atoi(argv[2]);

    /* cream socketul */
    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("Eroare la socket().\n");
        return errno;
    }

    /* umplem structura folosita pentru realizarea conexiunii cu serverul */
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(argv[1]);
    server.sin_port = htons(port);

    /* ne conectam la server */
    if (connect(sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1)
    {
        perror("[client]Eroare la connect().\n");
        return errno;
    }

    printf("[client]Introduceti o comanda: ");
    fflush(stdout);
    if (fgets(buf, sizeof(buf), stdin) == NULL) // Utilizează fgets pentru a citi de la stdin
    {
        perror("[client]Eroare la citire comanda.\n");
        return errno;
    }

    // trimit mesajul spre server
    int bytes_written = write(sd, buf, strlen(buf));
    if (bytes_written < 0)
    {
        perror("[client]Eroare la write() către server.\n");
        return errno;
    }
    printf("[client]Am trimis: %s", buf);

    /* Citirea răspunsului */
    bzero(mesaj,sizeof(mesaj));
    
    while (1)
    {
        bytes_read = read(sd, mesaj, sizeof(mesaj) - 1);
        if (bytes_read > 0)
        {
            mesaj[bytes_read] = '\0'; // Terminator de șir
            printf("[client]Mesajul primit este: %s\n", mesaj);

            if (strstr(mesaj, "Inchidem clientul") != NULL)
                break;

            bzero(mesaj,sizeof(mesaj));
            bzero(buf,sizeof(buf));

            printf("[client]Introduceti o comanda ");
            fflush(stdout);

            if (fgets(buf, sizeof(buf), stdin) == NULL) //  citim de la tastatura
            {
                perror("[client]Eroare la citire comanda.\n");
                return errno;
            }

            bytes_written = write(sd, buf, strlen(buf));
            
            if (bytes_written < 0)
            {
                perror("[client]Eroare la write() către server.\n");
                return errno;
            }

            printf("[client]Am trimis: %s", buf);
       
        }
        else if (bytes_read < 0)
        {
            perror("[client]Eroare la read() de la server.\n");
            return errno;
        }
    }

    /* inchidem conexiunea, am terminat */
    close(sd);
    printf("[client]Conexiunea a fost închisă.\n");
    return 0;
}
