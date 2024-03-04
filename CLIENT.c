#include <unistd.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>

#define SERVER_FIFO_NAME "server_client_FIFO"
#define CLIENT_FIFO_NAME "client_server_FIFO"

int main(int argc, char *argv[])
{
    char mesaj_catreServer[300];
    int octeti;

    if (mkfifo(SERVER_FIFO_NAME, 0666) == -1 || mkfifo(CLIENT_FIFO_NAME, 0666) == -1)
    {
        if (errno != EEXIST)
        {
            printf("eroare creare fifo-uri\n");
            return 1;
        }
    }

    printf("(Astept sa citeasca serverul)\n");
    printf("COMENZI: login: | get-logged-users | get-proc-info: | logout | quit\n");

    // scriu si trimit catre server
    int server_to_client_fd = open(SERVER_FIFO_NAME, O_WRONLY);
    if (server_to_client_fd == -1)
    {
        printf("eroare deschidere fifo\n");
        return 1;
    }
    // primesc de la server si citesc
    int client_to_server_fd = open(CLIENT_FIFO_NAME, O_RDONLY);
    if (client_to_server_fd == -1)
    {
        printf("eroare deschidere fifo\n");
        return 1;
    }

    char mesaj_primit[1024];
    int logat = 0;

    while (fgets(mesaj_catreServer, sizeof(mesaj_catreServer), stdin) != NULL)
    {

        if ((octeti = write(server_to_client_fd, mesaj_catreServer, strlen(mesaj_catreServer))) == -1)
            perror("Problema la scriere in fifo");
        else

            if (read(client_to_server_fd, mesaj_primit, sizeof(mesaj_primit)) == -1)
            perror("nu a reusit clientul sa citeasca din server\n");
        else
        {

            if (strstr(mesaj_catreServer, "login:") && (strcmp(mesaj_primit, "1") == 0))
            {

                printf("Logat cu succes.\n");
                logat = 1;
            }
            else if (strstr(mesaj_catreServer, "logout") && (strcmp(mesaj_primit, "0") == 0))
            {
                printf("Delogat cu succes.\n");
                logat = 0;
            }
            else if (strstr(mesaj_catreServer, "quit") && (strcmp(mesaj_primit, "2") == 0))
            {
                break;
            }
            else if (strstr(mesaj_catreServer, "get-logged-users") && (strlen(mesaj_primit) > 0) && (logat == 1))
            {
                char *cuv = NULL;

                int var = 0;
                cuv = strtok(mesaj_primit, "|");

                while (cuv != NULL)
                {
                    if (var == 0)
                        printf("Username:");
                    else if (var == 1)
                        printf("Hostname for remote login");
                    else if (var == 2)
                        printf("Time entry was made:");
                    printf("%s", cuv);
                    if (var != 2)
                        printf("\n");
                    cuv = strtok(NULL, "|");
                    var++;
                }
            }
            else if (strstr(mesaj_catreServer, "get-proc-info:") && (strlen(mesaj_primit) > 0) && (logat == 1))
            {

                printf("%s\n", mesaj_primit);
            }
            else if ((strcmp(mesaj_primit, "3") == 0))
            {
                printf("Rescrie comanda.\n");
            }
        }
    }

    close(client_to_server_fd);
    close(server_to_client_fd);
    return 0;
}