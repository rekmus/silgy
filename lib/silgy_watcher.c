/* --------------------------------------------------------------------------
   Restart Silgy app if dead
   Jurek Muszynski
-------------------------------------------------------------------------- */

#include "silgy.h"

#define BUFSIZE       8196

#define STOP_COMMAND  "sudo $SILGYDIR/bin/silgystop"
#define START_COMMAND "sudo $SILGYDIR/bin/silgystart"


void restart(void);


/* --------------------------------------------------------------------------
   main
-------------------------------------------------------------------------- */
int main(int argc, char *argv[])
{
    char config[256];
    int  sockfd;
    int  conn;
    int  bytes;
    char buffer[BUFSIZE];
static struct sockaddr_in serv_addr;

    /* library init ------------------------------------------------------ */

    silgy_lib_init();

    /* read the config file or set defaults ------------------------------ */

    char exec_name[256];
    lib_get_exec_name(exec_name, argv[0]);

    if ( G_appdir[0] )
    {
        sprintf(config, "%s/bin/%s.conf", G_appdir, exec_name);
        if ( !lib_read_conf(config) )   /* no config file there */
        {
            sprintf(config, "%s.conf", exec_name);
            lib_read_conf(config);
        }
    }
    else    /* no SILGYDIR -- try current dir */
    {
        sprintf(config, "%s.conf", exec_name);
        lib_read_conf(config);
    }

    if ( !silgy_read_param_int("logLevel", &G_logLevel) )
        G_logLevel = 0;  /* don't create log file */

    /* start log --------------------------------------------------------- */

    if ( !log_start("watcher", G_test) )
		return EXIT_FAILURE;

    /* ------------------------------------------------------------------- */

    INF("Trying to connect...");

    if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 )
    {
        ERR("socket failed, errno = %d (%s)", errno, strerror(errno));
        log_finish();
		return EXIT_FAILURE;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serv_addr.sin_port = htons(80);

    if ( (conn=connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr))) < 0 )
    {
        ERR("connect failed, errno = %d (%s)", errno, strerror(errno));
        close(sockfd);
        INF("Restarting...");
        restart();
        log_finish();
        return EXIT_SUCCESS;
    }

    INF("Connected");

    /* ------------------------------------------------------------------- */

    INF("Sending request...");

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
        return EXIT_SUCCESS;
    }

    /* ------------------------------------------------------------------- */

    INF("Reading response...");

    bytes = read(sockfd, buffer, BUFSIZE);

    if ( bytes > 7 && 0==strncmp(buffer, "HTTP/1.1", 8) )
    {
        INF("Response OK");
    }
    else
    {
        WAR("Response NOT OK, restarting...");
        restart();
    }

    /* ------------------------------------------------------------------- */

    close(conn);
    close(sockfd);

    log_finish();

    return EXIT_SUCCESS;
}


/* --------------------------------------------------------------------------
   Restart
-------------------------------------------------------------------------- */
void restart()
{
#ifdef APP_ADMIN_EMAIL
    if ( strlen(APP_ADMIN_EMAIL) )
        sendemail(0, APP_ADMIN_EMAIL, "Silgy restart", "Silgy Watcher had to restart web server.");
#endif

    INF("Stopping...");
    system(STOP_COMMAND);

    INF("Waiting 1 second...");
    sleep(1);

    INF("Starting...");
    system(START_COMMAND);
}
