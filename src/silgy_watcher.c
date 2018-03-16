/* --------------------------------------------------------------------------
   Restart Silgy app if dead
   Jurek Muszynski
-------------------------------------------------------------------------- */

#include "silgy.h"

#define BUFSIZE       8196

#define STOP_COMMAND  "sudo $SILGYDIR/bin/silgystop"
#define START_COMMAND "sudo $SILGYDIR/bin/silgystart"


char G_logLevel=4;
FILE *G_log;
struct tm *G_ptm;
int G_pid;
time_t G_now;
char G_appdir[256];
char G_dt[20];
char G_tmp[1048576];
char *G_shm_segptr;


void restart(void);


/* --------------------------------------------------------------------------
   main
-------------------------------------------------------------------------- */
int main(int argc, char *argv[])
{
    int sockfd;
    int conn;
    int bytes;
    char buffer[BUFSIZE];
    static struct sockaddr_in serv_addr;

    lib_get_app_dir();      // set G_appdir

    /* init time variables */

    G_now = time(NULL);
    G_ptm = gmtime(&G_now);
    sprintf(G_dt, "%d-%02d-%02d %02d:%02d:%02d", G_ptm->tm_year+1900, G_ptm->tm_mon+1, G_ptm->tm_mday, G_ptm->tm_hour, G_ptm->tm_min, G_ptm->tm_sec);

    /* start log */

    if ( !log_start("watch", TRUE) )
        return -1;

    /* -------------------------------------------------------------------------- */

    ALWAYS("Trying to connect...");

    if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 )
    {
        ERR("socket failed, errno = %d (%s)", errno, strerror(errno));
        exit(1);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serv_addr.sin_port = htons(80);

    if ( (conn=connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr))) < 0 )
    {
        ERR("connect failed, errno = %d (%s)", errno, strerror(errno));
        close(sockfd);
        ALWAYS("Restarting...");
        restart();
        log_finish();
        return 0;
    }

    ALWAYS("Connected");

    /* -------------------------------------------------------------------------- */

    ALWAYS("Sending request...");

    char *p=buffer;     /* stpcpy is more convenient and faster than strcat */

    p = stpcpy(p, "GET / HTTP/1.1\r\n");
    p = stpcpy(p, "Host: 127.0.0.1\r\n");
    p = stpcpy(p, "User-Agent: Silgy Watcher (Bot)\r\n");   /* don't bother Silgy with creating a user session */
    p = stpcpy(p, "Connection: close\r\n");
    p = stpcpy(p, "\r\n");

    bytes = write(sockfd, buffer, strlen(buffer));

    if ( bytes < 18 )
    {
        ERR("write failed, errno = %d (%s)", errno, strerror(errno));
        close(conn);
        close(sockfd);
        log_finish();
        return 0;
    }

    /* -------------------------------------------------------------------------- */

    ALWAYS("Reading response...");

    bytes = read(sockfd, buffer, BUFSIZE);

    if ( bytes > 7 && 0==strncmp(buffer, "HTTP/1.1", 8) )
    {
        ALWAYS("Response OK");
    }
    else
    {
        ALWAYS("Response NOT OK, restarting...");
        restart();
    }

    /* -------------------------------------------------------------------------- */

    close(conn);
    close(sockfd);

    log_finish();

    return 0;
}


/* --------------------------------------------------------------------------
   Restart
-------------------------------------------------------------------------- */
void restart()
{
    if ( strlen(APP_ADMIN_EMAIL) )
        sendemail(0, APP_ADMIN_EMAIL, "Silgy restart", "Silgy Watcher had to restart web server.");

    ALWAYS("Stopping...");
    system(STOP_COMMAND);

    ALWAYS("Waiting 1 second...");
    sleep(1);

    ALWAYS("Starting...");
    system(START_COMMAND);
}
