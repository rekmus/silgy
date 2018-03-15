/* --------------------------------------------------------------------------
   Restart Silgy app if dead
   Jurek Muszynski
-------------------------------------------------------------------------- */

#include "silgy.h"


#define STOP_COMMAND  "sudo silgystop"
#define START_COMMAND "sudo silgystart"


char G_logLevel=4;
FILE *G_log;
struct tm *G_ptm;
int G_pid;
time_t G_now;
char G_appdir[256];
char G_dt[20];
char G_tmp[1048576];


void restart(void);


/* --------------------------------------------------------------------------
   main
-------------------------------------------------------------------------- */
int main(int argc, char *argv[])
{
    lib_get_app_dir();      // set G_appdir

    /* init time variables */

    G_now = time(NULL);
    G_ptm = gmtime(&G_now);
    sprintf(G_dt, "%d-%02d-%02d %02d:%02d:%02d", G_ptm->tm_year+1900, G_ptm->tm_mon+1, G_ptm->tm_mday, G_ptm->tm_hour, G_ptm->tm_min, G_ptm->tm_sec);

    /* start log */

    if ( !log_start("watch", TRUE) )
        return -1;



    ALWAYS("Trying to connect...");


    ALWAYS("Connected");

    ALWAYS("Sending request...");

    ALWAYS("Reading response...");

    ALWAYS("Response OK");

    log_finish();

    return 0;
}


/* --------------------------------------------------------------------------
   Restart
-------------------------------------------------------------------------- */
void restart()
{
    ALWAYS("Stopping...");
    system(STOP_COMMAND);

    ALWAYS("Waiting 1 second...");
    sleep(1);

    ALWAYS("Starting...");
    system(START_COMMAND);
}
