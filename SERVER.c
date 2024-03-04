#include <unistd.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <utmp.h>
#include <time.h>
#include <ctype.h>

#define SERVER_FIFO_NAME "server_client_FIFO"
#define CLIENT_FIFO_NAME "client_server_FIFO"

char mesaj_primit[30], mesaj_primit_gresit[30], mesaj_socket[256], mesaj_socket2[1024];

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

    printf("(Astept sa scrie clientul)\n");
    // primesc de la client si citesc
    int server_to_client_fd = open(SERVER_FIFO_NAME, O_RDONLY);

    if (server_to_client_fd == -1)
    {
        printf("eroare deschidere fifo\n");
        return 2;
    }
    // scriu trimit catre client
    int client_to_server_fd = open(CLIENT_FIFO_NAME, O_WRONLY);
    if (client_to_server_fd == -1)
    {
        printf("eroare deschidere fifo\n");
        return 2;
    }
    // pipe
    int fd_pipe1[2];
    // f[0]-read
    // f[1]-write
    if (pipe(fd_pipe1) == -1)
    {
        printf("an error opening pipe\n");
        return 1;
    }
    // socket
    int sockets[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == -1)
    {
        perror("socketpair");
        return 1;
    }
    FILE *file; // fisier pentru a accesa fisierul /proc/<pid>/status

    FILE *fisier; // fisier pentru accesa username-urile
    char linie[256];
    fisier = fopen("fisier_username.txt", "r");

    strcpy(mesaj_primit, "0"); // variabila care indica statusul de logare
    //=0 delogat
    //=1 logat
    do
    { // citire din client
        if ((octeti = read(server_to_client_fd, mesaj_catreServer, sizeof(mesaj_catreServer) - 1)) == -1)
            perror("Eroare la citirea din fifo");
        else
        {
            int sok1 = 0, sok2 = 0;
            mesaj_catreServer[octeti] = '\0';

            if ((strstr(mesaj_catreServer, "login:") && (strcmp(mesaj_primit, "0") == 0)) ||
                (strstr(mesaj_catreServer, "get-logged-users") && (strcmp(mesaj_primit, "1") == 0)) ||
                (strstr(mesaj_catreServer, "get-proc-info:")) && (strcmp(mesaj_primit, "1") == 0))
            {
                if (strstr(mesaj_catreServer, "get-logged-users") && (strcmp(mesaj_primit, "1") == 0))
                {
                    sok1 = 1;
                }
                if (strstr(mesaj_catreServer, "get-proc-info:") && (strcmp(mesaj_primit, "1") == 0))
                {
                    sok2 = 1;
                }

                strcpy(mesaj_primit_gresit, "0");
                char *username = strstr(mesaj_catreServer, "login:");

                if (username)
                {

                    memmove(username, username + strlen("login:"), strlen(username) - strlen("login:") + 1);
                }
                char *pidd = strstr(mesaj_catreServer, "get-proc-info:");
                if (pidd)
                {

                    memmove(pidd, pidd + strlen("get-proc-info:"), strlen(pidd) - strlen("get-proc-info:") + 1);
                }
                // login
                if (strcmp(mesaj_primit, "0") == 0)
                {
                    int copil1 = fork();
                    if (copil1 == -1)
                    {
                        printf("eroare fork\n");
                        return 2;
                    }

                    if (copil1 == 0) // copil
                    {
                        close(fd_pipe1[0]);

                        if (fisier == NULL)
                        {
                            perror("eroare la deschiderea fisierului\n");
                        }
                        // caut in fisier usernameul
                        char gasit_username[2];
                        strcpy(gasit_username, "0");

                        while (fgets(linie, sizeof(linie), fisier))
                        {
                            if (strcmp(linie, username) == 0)
                            {
                                strcpy(gasit_username, "1");
                                break;
                            }
                        }
                        fseek(fisier, 0, SEEK_SET);

                        if (write(fd_pipe1[1], &gasit_username, sizeof(gasit_username)) == -1)
                        {
                            printf("eroare in scrierea in pipe\n");
                            return 3;
                        }

                        close(fd_pipe1[1]);
                        exit(1);
                    }
                }
                // get-logged-users
                if ((strcmp(mesaj_primit, "1") == 0) && (sok1 == 1))
                {
                    int copil2 = fork();
                    if (copil2 == -1)
                    {
                        printf("eroare fork\n");
                        return 2;
                    }
                    if (copil2 == 0) // copil
                    {
                        mesaj_socket[strlen(mesaj_socket)] = '\0';
                        close(sockets[0]);

                        struct utmp *utmp_variabila;
                        setutent(); // deschide fișierul de jurnal utmp
                        while ((utmp_variabila = getutent()) != NULL)
                        {
                            if (utmp_variabila->ut_type == USER_PROCESS)
                            {
                                strncat(mesaj_socket, utmp_variabila->ut_user, UT_NAMESIZE);
                                strcat(mesaj_socket, "|");
                                strncat(mesaj_socket, utmp_variabila->ut_host, UT_NAMESIZE);
                                strcat(mesaj_socket, "|");
                                time_t entryTime = utmp_variabila->ut_tv.tv_sec;
                                strcat(mesaj_socket, ctime(&entryTime));
                            }
                        }
                        endutent(); // inchidere

                        if (write(sockets[1], mesaj_socket, sizeof(mesaj_socket)) < 0)
                            printf("eroare scriere socketpair\n");

                        close(sockets[1]);
                        exit(1);
                    }
                }
                if ((strcmp(mesaj_primit, "1") == 0) && (sok2 == 1))
                {
                    int pid = atoi(pidd);
                    char Cale[256];
                    // pentru cautarea in fisierul sursa /proc/<pid>/status
                    snprintf(Cale, sizeof(Cale), "/proc/%d/status", pid);

                    file = fopen(Cale, "r");
                    if (file == NULL)
                    {
                        perror("Eroare deschidere fisier");
                    }
                    char line_s[1024];

                    int copil3 = fork();
                    if (copil3 == -1)
                    {
                        printf("eroare fork\n");
                        return 2;
                    }

                    if (copil3 == 0) // copil
                    {
                        mesaj_socket2[strlen(mesaj_socket2)] = '\0';

                        close(sockets[0]);

                        while (fgets(line_s, sizeof(line_s), file))
                        {
                            // Afișează informațiile
                            if (strstr(line_s, "Name:") || strstr(line_s, "State:") || strstr(line_s, "PPid:") ||
                                strstr(line_s, "Uid:") || strstr(line_s, "VmSize:"))
                            {
                                strcat(mesaj_socket2, line_s);
                            }
                        }

                        mesaj_socket2[strlen(mesaj_socket2) - 1] = '\0';

                        if (write(sockets[1], mesaj_socket2, sizeof(mesaj_socket2)) < 0)
                            printf("eroare scriere socketpair\n");
                        else
                            printf("Am gasit informatiile procesului indicat.\n");

                        close(sockets[1]);
                        exit(1);
                    }
                }
                // parinte
                wait(NULL);
                wait(NULL);
                wait(NULL);
                // get-proc-info
                if ((strcmp(mesaj_primit, "1") == 0) && (sok2 == 1))
                {

                    if (read(sockets[0], mesaj_socket2, sizeof(mesaj_socket2)) == -1)
                        perror("eroare citire socketpair\n");

                    ssize_t octeti_s = write(client_to_server_fd, mesaj_socket2, strlen(mesaj_socket2));
                    if (octeti_s == -1)
                    {
                        perror("Eroare la scriere in FIFO");
                        exit(1);
                    }

                    /* else
                     {

                         printf("S-au scris %zd octeti in FIFO.\n", octeti_s);
                     }*/
                }
                // get-logged-users
                if ((strcmp(mesaj_primit, "1") == 0) && (sok1 == 1))
                {

                    if (read(sockets[0], mesaj_socket, sizeof(mesaj_socket)) == -1)
                        perror("eroare citire socketpair\n");

                    ssize_t octeti_s = write(client_to_server_fd, mesaj_socket, strlen(mesaj_socket));
                    if (octeti_s == -1)
                    {
                        perror("Eroare la scriere in FIFO");
                        exit(1);
                    }
                    /* else
                     {
                         printf("S-au scris %zd octeti in FIFO.\n", octeti_s);
                     }*/
                }
                // login
                if (strcmp(mesaj_primit, "0") == 0)
                {
                    if (read(fd_pipe1[0], &mesaj_primit, sizeof(mesaj_primit)) == -1)
                    {
                        printf("eroare in citirea din pipe\n");
                        return 3;
                    }
                    else
                    {

                        if (strcmp(mesaj_primit, "1") == 0)
                        { // scriu trimit catre client
                            if (write(client_to_server_fd, &mesaj_primit, sizeof(mesaj_primit)) == -1)
                                perror("nu am reusit sa scriu din server spre client\n");
                        }
                        else
                        {
                            printf("Username-ul nu a fost gasit. \n");
                            if (write(client_to_server_fd, &mesaj_primit, sizeof(mesaj_primit)) == -1)
                                perror("nu am reusit sa scriu din server spre client\n");
                        }
                    }
                }
            }

            else if (strstr(mesaj_catreServer, "logout") && (strcmp(mesaj_primit, "1") == 0))
            {
                strcpy(mesaj_primit_gresit, "0");

                strcpy(mesaj_primit, "0");
                printf("Urmeaza delogarea...\n");
                if (write(client_to_server_fd, &mesaj_primit, sizeof(mesaj_primit)) == -1)
                    perror("nu am reusit sa scriu din server spre client\n");
            }
            else if (strstr(mesaj_catreServer, "quit"))
            {
                strcpy(mesaj_primit_gresit, "0");
                strcpy(mesaj_primit, "2");
                if (write(client_to_server_fd, &mesaj_primit, sizeof(mesaj_primit)) == -1)
                    perror("nu am reusit sa scriu din server spre client\n");
                break;
            }
            else
            {
                printf("Comanda gresita!\n");
                strcpy(mesaj_primit_gresit, "3");
                if (write(client_to_server_fd, &mesaj_primit_gresit, sizeof(mesaj_primit_gresit)) == -1)
                    perror("nu am reusit sa scriu din server spre client\n");
            }
        }
    } while (octeti > 0);

    close(fd_pipe1[0]);
    close(fd_pipe1[1]);
    close(sockets[0]);
    close(sockets[1]);
    fclose(fisier);
    fclose(file);

    close(client_to_server_fd);
    close(server_to_client_fd);
    return 0;
}