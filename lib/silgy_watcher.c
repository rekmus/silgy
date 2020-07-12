/* --------------------------------------------------------------------------
   Restart Silgy app if dead
   Jurek Muszynski
-------------------------------------------------------------------------- */


#include "silgy.h"


#define STOP_COMMAND        "$SILGYDIR/bin/silgystop"
#define START_COMMAND       "$SILGYDIR/bin/silgystart"


int         G_httpPort;


static char M_watcherStopCmd[256];
static char M_watcherStartCmd[256];
static int  M_watcherWait;
static int  M_watcherLogRestart;


/* --------------------------------------------------------------------------
   Restart
-------------------------------------------------------------------------- */
static void restart()
{
    if ( M_watcherLogRestart > 0 )
    {
        G_logLevel = M_watcherLogRestart;
        log_start("watcher", FALSE);
    }

    ALWAYS_T("Restarting...");

    INF_T("Stopping...");
    INF_T(M_watcherStopCmd);
    system(M_watcherStopCmd);

    lib_update_time_globals();

    INF_T("Waiting %d second(s)...", M_watcherWait);
    sleep(M_watcherWait);

    lib_update_time_globals();

    INF_T("Starting...");
    INF_T(M_watcherStartCmd);
    system(M_watcherStartCmd);

#ifdef APP_ADMIN_EMAIL
    if ( strlen(APP_ADMIN_EMAIL) )
    {
        char message[1024];
        strcpy(message, "Silgy Watcher had to restart web server.");
        silgy_email(APP_ADMIN_EMAIL, "Silgy restart", message);
    }
#endif
}


/* --------------------------------------------------------------------------
   main
-------------------------------------------------------------------------- */
int main(int argc, char *argv[])
{
    char config[256];

    /* library init ------------------------------------------------------ */

    silgy_lib_init();

    sort_messages();

    G_initialized = 1;

    /* read the config file or set defaults ------------------------------ */

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
        G_logLevel = 0;   /* don't create log file */

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

    INF_T("Trying to connect...");

    G_RESTTimeout = 60000;   /* 60 seconds */

    char url[1024];

    sprintf(url, "127.0.0.1:%d", G_httpPort);

    REST_HEADER_SET("User-Agent", "Silgy Watcher Bot");

    if ( !CALL_REST_HTTP(NULL, NULL, "GET", url, 0) )
    {
        lib_update_time_globals();
        ERR_T("Couldn't connect");
        restart();
    }

    /* ------------------------------------------------------------------- */

    lib_update_time_globals();

    INF_T("silgy_watcher ended");

    log_finish();

    return EXIT_SUCCESS;
}
