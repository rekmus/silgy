/* --------------------------------------------------------------------------
   Silgy Web App
   Jurek Muszynski
   Started: August 2015
-----------------------------------------------------------------------------
   Service engine module
-------------------------------------------------------------------------- */


#include "silgy.h"


mqd_t           G_queue_req;                /* request queue */
mqd_t           G_queue_res;                /* response queue */


static char     *M_pidfile;                 /* pid file name */


static void sigdisp(int sig);
static void clean_up(void);


/* --------------------------------------------------------------------------
   main
-------------------------------------------------------------------------- */
int main(int argc, char *argv[])
{
    char config[256];

    G_pid = getpid();

    /* set G_appdir ------------------------------------------------------ */

    lib_get_app_dir();

    /* init time variables ----------------------------------------------- */

    G_now = time(NULL);
    G_ptm = gmtime(&G_now);
    sprintf(G_dt, "%d-%02d-%02d %02d:%02d:%02d", G_ptm->tm_year+1900, G_ptm->tm_mon+1, G_ptm->tm_mday, G_ptm->tm_hour, G_ptm->tm_min, G_ptm->tm_sec);

    sprintf(config, "%s/bin/silgy_service.conf", G_appdir);
    lib_read_conf(config);

    if ( !silgy_read_param_int("logLevel", &G_logLevel) )
        G_logLevel = 3;  /* info */

    /* start log --------------------------------------------------------- */

    if ( G_logLevel && !log_start("service", G_test) )
		return EXIT_FAILURE;

    /* pid file ---------------------------------------------------------- */

    if ( !(M_pidfile=lib_create_pid_file(argv[0])) )
		return EXIT_FAILURE;

	/* handle signals ---------------------------------------------------- */

#ifndef _WIN32
	signal(SIGINT,  sigdisp);	/* Ctrl-C */
	signal(SIGTERM, sigdisp);
	signal(SIGQUIT, sigdisp);	/* Ctrl-\ */
	signal(SIGTSTP, sigdisp);	/* Ctrl-Z */
#endif

    /* open queues ------------------------------------------------------- */

	G_queue_req = mq_open(ASYNC_REQ_QUEUE, O_RDONLY);

	if ( G_queue_req < 0 )
	{
		ERR("mq_open for req failed, errno = %d (%s)", errno, strerror(errno));
		clean_up();
		return EXIT_FAILURE;
	}

    DBG("G_queue_req open OK");

	G_queue_res = mq_open(ASYNC_RES_QUEUE, O_WRONLY);

	if ( G_queue_res < 0 )
	{
		ERR("mq_open for res failed, errno = %d (%s)", errno, strerror(errno));
		clean_up();
		return EXIT_FAILURE;
	}

    DBG("G_queue_res open OK");

    /* ------------------------------------------------------------------- */

	if ( !service_init() )
	{
		ERR("service_init failed");
		clean_up();
		return EXIT_FAILURE;
    }

    /* ------------------------------------------------------------------- */

    async_req_t req;
    async_res_t res;

    INF("Waiting...");

    while (1)
    {
        if ( mq_receive(G_queue_req, (char*)&req, ASYNC_REQ_MSG_SIZE, 0) != -1 )
        {
            DBG("Message received");
            DBG("ci = %d, service [%s], call_id = %ld", req.ci, req.service, req.call_id);
            res.call_id = req.call_id;
            res.ci = req.ci;
            strcpy(res.service, req.service);
            DBG("Processing...");
            service_app_process_req(req.service, req.data, res.data);
            if ( req.response )
            {
                DBG("Sending response...");
                mq_send(G_queue_res, (char*)&res, ASYNC_RES_MSG_SIZE, 0);
                DBG("Sent\n");
            }
            else
            {
                DBG("Response not required");
            }
        }
    }

	clean_up();

	return EXIT_SUCCESS;
}


/* --------------------------------------------------------------------------
   Signal response
-------------------------------------------------------------------------- */
static void sigdisp(int sig)
{
    INF("Exiting due to receiving signal: %d", sig);
    clean_up();
    exit(1);
}


/* --------------------------------------------------------------------------
   Clean up
-------------------------------------------------------------------------- */
static void clean_up()
{
    char    command[256];

    if ( G_log )
    {
        ALWAYS("");
        log_write_time(LOG_ALWAYS, "Cleaning up...\n");
        lib_log_memory();
    }

    service_done();

    if ( access(M_pidfile, F_OK) != -1 )
    {
        if (G_log) DBG("Removing pid file...");
#ifdef _WIN32   /* Windows */
        sprintf(command, "del %s", M_pidfile);
#else
        sprintf(command, "rm %s", M_pidfile);
#endif
        system(command);
    }

	if (G_queue_req)
		mq_close(G_queue_req);

	if (G_queue_res)
		mq_close(G_queue_res);

    log_finish();
}
