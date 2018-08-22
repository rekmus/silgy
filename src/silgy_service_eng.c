/* --------------------------------------------------------------------------
   Silgy Web App
   Jurek Muszynski
   Started: August 2015
-----------------------------------------------------------------------------
   Service engine module
-------------------------------------------------------------------------- */


#include "silgy.h"


char        G_req_queue_name[256];
char        G_res_queue_name[256];
mqd_t       G_queue_req;                /* request queue */
mqd_t       G_queue_res;                /* response queue */


static char *M_pidfile;                 /* pid file name */


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

    lib_update_time_globals();

    sprintf(config, "%s.conf", argv[0]);

    if ( !lib_read_conf(config) )
    {
        sprintf(config, "%s/bin/%s.conf", G_appdir, argv[0]);
        lib_read_conf(config);
    }

    if ( !silgy_read_param_int("logLevel", &G_logLevel) )
        G_logLevel = 3;  /* info */

    /* start log --------------------------------------------------------- */

    if ( G_logLevel && !log_start(argv[0], G_test) )
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

#ifdef APP_ASYNC_ID
    sprintf(G_req_queue_name, "%s_%d", ASYNC_REQ_QUEUE, APP_ASYNC_ID);
    sprintf(G_res_queue_name, "%s_%d", ASYNC_RES_QUEUE, APP_ASYNC_ID);
#else
    strcpy(G_req_queue_name, ASYNC_REQ_QUEUE);
    strcpy(G_res_queue_name, ASYNC_RES_QUEUE);
#endif

    struct mq_attr attr={0};

    attr.mq_maxmsg = ASYNC_MQ_MAXMSG;
    attr.mq_msgsize = ASYNC_REQ_MSG_SIZE;

	G_queue_req = mq_open(G_req_queue_name, O_RDONLY, 0664, &attr);

	if ( G_queue_req < 0 )
	{
		ERR("mq_open for req failed, errno = %d (%s)", errno, strerror(errno));
		clean_up();
		return EXIT_FAILURE;
	}

    INF("G_queue_req open OK");

    attr.mq_msgsize = ASYNC_RES_MSG_SIZE;   /* larger buffer */

	G_queue_res = mq_open(G_res_queue_name, O_WRONLY, 0664, &attr);

	if ( G_queue_res < 0 )
	{
		ERR("mq_open for res failed, errno = %d (%s)", errno, strerror(errno));
		clean_up();
		return EXIT_FAILURE;
	}

    INF("G_queue_res open OK");

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

    INF("Waiting...\n");

    while (1)
    {
        if ( mq_receive(G_queue_req, (char*)&req, ASYNC_REQ_MSG_SIZE, NULL) != -1 )
        {
            lib_update_time_globals();
            DBG_T("Message received");
            if ( G_logLevel > LOG_INF )
                DBG_T("ci = %d, service [%s], call_id = %ld", req.ci, req.service, req.call_id);
            else
                INF_T("%s called (id=%ld)", req.service, req.call_id);
            res.call_id = req.call_id;
            res.ci = req.ci;
            strcpy(res.service, req.service);
            /* ----------------------------------------------------------- */
            DBG("Processing...");
            service_app_process_req(req.service, req.data, res.data);
            /* ----------------------------------------------------------- */
            if ( req.response )
            {
                DBG("Sending response...");
                mq_send(G_queue_res, (char*)&res, ASYNC_RES_MSG_SIZE, NULL);
                DBG("Sent\n");
            }
            else
            {
                DBG("Response not required\n");
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
    lib_update_time_globals();
    ALWAYS("");
    ALWAYS_T("Exiting due to receiving signal: %d", sig);
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
        ALWAYS("Cleaning up...\n");
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
    {
        mq_close(G_queue_req);
        mq_unlink(G_req_queue_name);
    }
    if (G_queue_res)
    {
        mq_close(G_queue_res);
        mq_unlink(G_res_queue_name);
    }

    log_finish();
}
