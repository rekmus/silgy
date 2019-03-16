/* --------------------------------------------------------------------------
   Restart Silgy app if dead
   Jurek Muszynski
-------------------------------------------------------------------------- */


#include "silgy.h"


#define BUFSIZE         8196

#define REASON_CONNECT  1
#define REASON_WRITE    2
#define REASON_READ     3

#define STOP_COMMAND    "sudo $SILGYDIR/bin/silgystop"
#define START_COMMAND   "sudo $SILGYDIR/bin/silgystart"


int         G_httpPort;
/* counters */
counters_t  G_cnts_today;               /* today's counters */
counters_t  G_cnts_yesterday;           /* yesterday's counters */
counters_t  G_cnts_day_before;          /* day before's counters */


static char M_watcherStopCmd[256];
static char M_watcherStartCmd[256];
static int  M_watcherWait;
static int  M_watcherLogRestart;


void restart(char reason);


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

//    if ( G_appdir[0] )
//    {
//        sprintf(config, "%s/bin/%s.conf", G_appdir, exec_name);
//        if ( !lib_read_conf(config) )   /* no config file there */
//        {
//            sprintf(config, "%s.conf", exec_name);
//            lib_read_conf(config);
//        }
//    }
//    else    /* no SILGYDIR -- try current dir */
//    {
//        sprintf(config, "%s.conf", exec_name);
//        lib_read_conf(config);
//    }

    if ( G_appdir[0] )
    {
        sprintf(config, "%s/bin/silgy.conf", G_appdir);
        if ( !lib_read_conf(config) )   /* no config file there */
        {
            strcpy(config, "silgy.conf");
            lib_read_conf(config);
        }
    }
    else    /* no SILGYDIR -- try current dir */
    {
        strcpy(config, "silgy.conf");
        lib_read_conf(config);
    }

    /* ------------------------------------------------------------------- */

    if ( !silgy_read_param_int("watcherLogLevel", &G_logLevel) )
        G_logLevel = 0;  /* don't create log file */

    if ( !silgy_read_param_int("watcherLogToStdout", &G_logToStdout) )
        G_logToStdout = 0;

    if ( !silgy_read_param_int("httpPort", &G_httpPort) )
        G_httpPort = 80;

    if ( !silgy_read_param_str("watcherStopCmd", M_watcherStopCmd) )
        strcpy(M_watcherStopCmd, STOP_COMMAND);

    if ( !silgy_read_param_str("watcherStartCmd", M_watcherStartCmd) )
        strcpy(M_watcherStartCmd, START_COMMAND);

    if ( !silgy_read_param_int("watcherWait", &M_watcherWait) )
        M_watcherWait = 10;

    if ( !silgy_read_param_int("watcherLogRestart", &M_watcherLogRestart) )
        M_watcherLogRestart = 3;

    /* start log --------------------------------------------------------- */

    if ( !log_start("watcher", FALSE) )
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
    serv_addr.sin_port = htons(G_httpPort);

    if ( (conn=connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr))) < 0 )
    {
        ERR("connect failed, errno = %d (%s)", errno, strerror(errno));
        close(sockfd);
        restart(REASON_CONNECT);
        log_finish();
        return EXIT_SUCCESS;
    }

    INF("Connected");

    /* ------------------------------------------------------------------- */

    INF("Sending request...");

    char *p=buffer;     /* stpcpy is more convenient and faster than strcat */

    p = stpcpy(p, "GET / HTTP/1.1\r\n");
    p = stpcpy(p, "Host: 127.0.0.1\r\n");
    p = stpcpy(p, "User-Agent: Silgy Watcher Bot\r\n");   /* don't bother Silgy with creating a user session */
    p = stpcpy(p, "Connection: close\r\n");
    p = stpcpy(p, "\r\n");

    bytes = write(sockfd, buffer, strlen(buffer));

    if ( bytes < 18 )
    {
        ERR("write failed, errno = %d (%s)", errno, strerror(errno));
        close(conn);
        close(sockfd);
        restart(REASON_WRITE);
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
        ERR("read failed, errno = %d (%s)", errno, strerror(errno));
        close(conn);
        close(sockfd);
        restart(REASON_READ);
        log_finish();
        return EXIT_SUCCESS;
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
void restart(char reason)
{
    if ( M_watcherLogRestart > 0 )
    {
        G_logLevel = M_watcherLogRestart;
        log_start("watcher", FALSE);
    }

    char reason_desc[256];

    if ( reason == REASON_CONNECT )
        strcpy(reason_desc, "Couldn't connect");
    else if ( reason == REASON_WRITE )
        strcpy(reason_desc, "Couldn't send the request");
    else if ( reason == REASON_READ )
        strcpy(reason_desc, "Couldn't read the response");
    else
        strcpy(reason_desc, "Unknown reason!");

    ALWAYS(reason_desc);

    ALWAYS("Restarting...");

    INF("Stopping...");
    INF(M_watcherStopCmd);
    system(M_watcherStopCmd);

    INF("Waiting %d second(s)...", M_watcherWait);
    sleep(M_watcherWait);

    INF("Starting...");
    INF(M_watcherStartCmd);
    system(M_watcherStartCmd);

#ifdef APP_ADMIN_EMAIL
    if ( strlen(APP_ADMIN_EMAIL) )
    {
        char message[1024];
        sprintf(message, "Silgy Watcher had to restart web server due to: %s", reason_desc);
        silgy_email(APP_ADMIN_EMAIL, "Silgy restart", message);
    }
#endif
}
