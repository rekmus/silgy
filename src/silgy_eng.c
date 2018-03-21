/* --------------------------------------------------------------------------
   Silgy Web App Engine
   Jurek Muszynski
   silgy.com
   Started: August 2015
-----------------------------------------------------------------------------
   Main web engine module
-------------------------------------------------------------------------- */


#include "silgy.h"


http_status_t   M_http_status[]={
        {200, "OK"},
        {201, "Created"},
        {206, "Partial Content"},
        {301, "Moved Permanently"},
        {302, "Found"},
        {303, "See Other"},
        {304, "Not Modified"},
        {400, "Bad Request"},
        {401, "Unauthorized"},
        {403, "Forbidden"},
        {404, "Not Found"},
        {405, "Method Not Allowed\r\nAllow: GET, POST, PUT, DELETE, OPTIONS, HEAD"},
        {413, "Request Entity Too Large"},
        {414, "Request-URI Too Long"},
        {416, "Range Not Satisfiable"},
        {500, "Internal Server Error"},
        {501, "Not Implemented"},
        {503, "Service Unavailable"},
        { -1, ""}
    };


static struct {                         /* default auth level is set in app.h -- no need to set those */
    char    resource[MAX_RESOURCE_LEN+1];
    char    level;
    }       M_auth_levels[MAX_RESOURCES] = {
        {"-", EOS}
    };

/* globals */

/* read from the config file */
int         G_httpPort;
int         G_httpsPort;
char        G_cipherList[256];
char        G_certFile[256];
char        G_certChainFile[256];
char        G_keyFile[256];
char        G_dbName[128];
char        G_dbUser[128];
char        G_dbPassword[128];
char        G_blockedIPList[256];
/* end of config params */
long        G_days_up;                  /* web server's days up */
#ifndef ASYNC_SERVICE
conn_t      conn[MAX_CONNECTIONS];      /* HTTP connections & requests -- by far the most important structure around */
#endif
int         G_open_conn;                /* number of open connections */
#ifndef ASYNC_SERVICE
usession_t  uses[MAX_SESSIONS+1];       /* user sessions -- they start from 1 */
#endif
int         G_sessions;                 /* number of active user sessions */
char        G_last_modified[32];        /* response header field with server's start time */
#ifdef DBMYSQL
MYSQL       *G_dbconn;                  /* database connection */
#endif
/* asynchorous processing */
mqd_t       G_queue_req;                /* request queue */
mqd_t       G_queue_res;                /* response queue */
#ifdef ASYNC
async_res_t ares[MAX_ASYNC];            /* async response array */
long        G_last_call_id;             /* counter */
#endif
char        G_blacklist[MAX_BLACKLIST+1][INET_ADDRSTRLEN];
int         G_blacklist_cnt;            /* M_blacklist length */
counters_t  G_cnts_today;               /* today's counters */
counters_t  G_cnts_yesterday;           /* yesterday's counters */
counters_t  G_cnts_day_before;          /* day before's counters */

/* locals */

static char         *M_pidfile;                 /* pid file name */
static int          M_listening_fd=0;           /* The socket file descriptor for our "listening" socket */
static int          M_listening_sec_fd=0;       /* The socket file descriptor for secure "listening" socket */
#ifdef HTTPS
static SSL_CTX      *M_ssl_ctx;
#endif
static fd_set       M_readfds={0};              /* Socket file descriptors we want to wake up for, using select() */
static fd_set       M_writefds={0};             /* Socket file descriptors we want to wake up for, using select() */
static int          M_highsock=0;               /* Highest #'d file descriptor, needed for select() */
static stat_res_t   M_stat[MAX_STATICS];        /* static resources */
static char         M_resp_date[32];            /* response header field Date */
static char         M_expires[32];              /* response header field one month ahead for static resources */
static bool         M_favicon_exists=FALSE;     /* special case statics */
static bool         M_robots_exists=FALSE;      /* -''- */
static bool         M_appleicon_exists=FALSE;   /* -''- */

/* prototypes */

static void set_state(int ci, long bytes);
static void set_state_sec(int ci, long bytes);
static bool read_conf(void);
static void respond_to_expect(int ci);
static void log_proc_time(int ci);
static void close_conn(int ci);
static bool init(int argc, char **argv);
static void setnonblocking(int sock);
static void build_select_list(void);
static void accept_http();
static void accept_https();
static bool read_blocked_ips(void);
static bool ip_blocked(const char *addr);
static int first_free_stat(void);
static bool read_files(bool minify);
static int is_static_res(int ci, const char *name);
static bool open_db(void);
static void process_req(int ci);
static void gen_response_header(int ci);
static void print_content_type(int ci, char type);
static bool a_usession_ok(int ci);
static void close_old_conn(void);
static void close_uses_timeout(void);
static void close_a_uses(int usi);
static void reset_conn(int ci, char conn_state);
static int parse_req(int ci, long len);
static int set_http_req_val(int ci, const char *label, const char *value);
static bool check_block_ip(int ci, const char *rule, const char *value);
static char *get_http_descr(int status_code);
static void dump_counters(void);
static void clean_up(void);
static void sigdisp(int sig);
static void gen_page_msg(int ci, int msg);
static bool init_ssl(void);


/* --------------------------------------------------------------------------
   main
-------------------------------------------------------------------------- */
int main(int argc, char **argv)
{

static struct   sockaddr_in serv_addr;      /* static = initialised to zeros */
static struct   sockaddr_in cli_addr;       /* static = initialised to zeros */
unsigned int    addr_len=0;
    int         prev_day=0;
unsigned long   hit=0;
    char        remote_addr[INET_ADDRSTRLEN]=""; /* remote address */
    int         reuse_addr=1;               /* Used so we can re-bind to our port while a previous connection is still in TIME_WAIT state */
struct timeval  timeout;                    /* Timeout for select */
    int         readsocks=0;                /* Number of sockets ready for reading */
    int         i=0;                        /* Current item in conn_sockets for for loops */
    int         time_elapsed=0;             /* time unit, currently 250 ms */
    time_t      sometimeahead;
    long        bytes=0;
    int         failed_select_cnt=0;
    int         j=0;

    if ( !init(argc, argv) )
    {
        if ( G_log )
            ERR("init() failed, exiting");
        else
            printf("init() failed, exiting.\n");

        clean_up();
        return EXIT_FAILURE;
    }

    /* create new log file every day */

    prev_day = G_ptm->tm_mday;

    /* setup the network socket */

    DBG("Trying socket...");

    if ( (M_listening_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0 )
    {
        ERR("socket failed, errno = %d (%s)", errno, strerror(errno));
        clean_up();
        return EXIT_FAILURE;
    }

    DBG("M_listening_fd = %d", M_listening_fd);

    /* So that we can re-bind to it without TIME_WAIT problems */
    setsockopt(M_listening_fd, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr));

    /* Set socket to non-blocking with our setnonblocking routine */
    setnonblocking(M_listening_fd);

    /* bind socket to a port */

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(G_httpPort);

    DBG("Trying bind...");

    if ( bind(M_listening_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0 )
    {
        ERR("bind failed, errno = %d (%s)", errno, strerror(errno));
        clean_up();
        return EXIT_FAILURE;
    }

    /* listen to a port */

    DBG("Trying listen...");

    if ( listen(M_listening_fd, SOMAXCONN) < 0 )
    {
        ERR("listen failed, errno = %d (%s)", errno, strerror(errno));
        clean_up();
        return EXIT_FAILURE;
    }

    /* repeat everything for port 443 */

#ifdef HTTPS

    DBG("Trying socket for secure connections...");

    if ( (M_listening_sec_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0 )
    {
        ERR("socket failed, errno = %d (%s)", errno, strerror(errno));
        clean_up();
        return EXIT_FAILURE;
    }

    DBG("M_listening_sec_fd = %d", M_listening_sec_fd);

    /* So that we can re-bind to it without TIME_WAIT problems */
    setsockopt(M_listening_sec_fd, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr));

    /* Set socket to non-blocking with our setnonblocking routine */
    setnonblocking(M_listening_sec_fd);

    /* bind socket to a port */

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(G_httpsPort);

    DBG("Trying bind...");

    if ( bind(M_listening_sec_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0 )
    {
        ERR("bind failed, errno = %d (%s)", errno, strerror(errno));
        clean_up();
        return EXIT_FAILURE;
    }

    /* listen to a port */

    DBG("Trying listen...");

    if ( listen(M_listening_sec_fd, SOMAXCONN) < 0 )
    {
        ERR("listen failed, errno = %d (%s)", errno, strerror(errno));
        clean_up();
        return EXIT_FAILURE;
    }

    M_highsock = M_listening_sec_fd;

#else

    M_highsock = M_listening_fd;

#endif

    addr_len = sizeof(cli_addr);

    if ( G_dbName[0] )
    {
        DBG("Trying open_db...");

        if ( !open_db() )
        {
            ERR("open_db failed");
            clean_up();
            return EXIT_FAILURE;
        }

        ALWAYS("Database connected");
    }

    /* log currently used memory */

    lib_log_memory();

    ALWAYS("Waiting for requests...\n");

    fflush(G_log);


    /* main server loop ------------------------------------------------------------------------- */

//  for ( ; hit<1000; ++hit )   /* test only */
    for ( ;; )
    {
        build_select_list();

        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        G_now = time(NULL);
        G_ptm = gmtime(&G_now);
        strftime(M_resp_date, 32, "%a, %d %b %Y %X %Z", G_ptm);
        sprintf(G_dt, "%d-%02d-%02d %02d:%02d:%02d", G_ptm->tm_year+1900, G_ptm->tm_mon+1, G_ptm->tm_mday, G_ptm->tm_hour, G_ptm->tm_min, G_ptm->tm_sec);
#ifdef EVERY_SECOND
        app_every_second();
#endif
#ifdef ASYNC
        /* mark timeout-ed */

        for ( j=0; j<MAX_ASYNC; ++j )
        {
            if ( ares[j].state==ASYNC_STATE_SENT && ares[j].sent < G_now-ares[j].timeout )
            {
                DBG("Async request %d timeout-ed", j);
                ares[j].state = ASYNC_STATE_TIMEOUTED;
            }
        }
#endif
        readsocks = select(M_highsock+1, &M_readfds, &M_writefds, NULL, &timeout);

        if (readsocks < 0)
        {
            ERR("select failed, errno = %d (%s)", errno, strerror(errno));
            /* protect from infinite loop */
            if ( failed_select_cnt >= 100 )
            {
                ERR("select failed 100-th time, check your server logic!");
                break;
            }
            else
            {
                ++failed_select_cnt;
                continue;
            }
        }
        else if (readsocks == 0)
        {
            /* we have some time now, let's do some housekeeping */

            if ( G_open_conn ) close_old_conn();
            if ( G_sessions ) close_uses_timeout();

            if ( time_elapsed >= 60 )   /* say something sometimes ... */
            {
                ALWAYS("[%s] %d open connection(s) | %d user session(s)", G_dt+11, G_open_conn, G_sessions);
                time_elapsed = 0;
#ifdef USERS
                if ( G_sessions ) libusr_close_luses_timeout();     /* tidy up cache */
#endif
                fflush(G_log);

                /* start new log file every day */

                if ( G_ptm->tm_mday != prev_day )
                {
                    dump_counters();
                    log_finish();
                    if ( !log_start("", G_test) )
                        return EXIT_FAILURE;
                    prev_day = G_ptm->tm_mday;

                    /* set new Expires date */
                    sometimeahead = G_now + 3600*24*EXPIRES_IN_DAYS;
                    G_ptm = gmtime(&sometimeahead);
                    strftime(M_expires, 32, "%a, %d %b %Y %X %Z", G_ptm);
                    ALWAYS("New M_expires: %s", M_expires);
                    G_ptm = gmtime(&G_now); /* make sure G_ptm is up to date */

                    if ( G_blockedIPList[0] )
                    {
                        /* update blacklist */
                        read_blocked_ips();
                    }

                    /* copy & reset counters */
                    memcpy(&G_cnts_day_before, &G_cnts_yesterday, sizeof(counters_t));
                    memcpy(&G_cnts_yesterday, &G_cnts_today, sizeof(counters_t));
                    memset(&G_cnts_today, 0, sizeof(counters_t));

                    /* log currently used memory */
                    lib_log_memory();
                    ++G_days_up;
                }
            }
        }
        else    /* readsocks > 0 */
        {
            if (FD_ISSET(M_listening_fd, &M_readfds))
            {
                accept_http();
            }
#ifdef HTTPS
            else if (FD_ISSET(M_listening_sec_fd, &M_readfds))
            {
                accept_https();
            }
#endif
            else    /* existing connections have something going on on them --------------------------------- */
            {
                for (i=0; i<MAX_CONNECTIONS; ++i)
                {
                    /* --------------------------------------------------------------------------------------- */
                    if ( FD_ISSET(conn[i].fd, &M_readfds) )     /* incoming data ready */
                    {
//                      DBG("\nfd=%d has incoming data ready to read", conn[i].fd);
#ifdef HTTPS
                        if ( conn[i].secure )   /* HTTPS */
                        {
//                          DBG("secure, state=%c, pending=%d", conn[i].conn_state, SSL_pending(conn[i].ssl));

                            if ( conn[i].conn_state != CONN_STATE_READING_DATA )
                            {
//                              DBG("Trying SSL_read from fd=%d", conn[i].fd);
                                bytes = SSL_read(conn[i].ssl, conn[i].in, IN_BUFSIZE-1);
                                if ( bytes > 1 )
                                    conn[i].in[bytes] = EOS;
                                else if ( bytes == 1 )  /* when browser splits the request to prevent BEAST attack */
                                {
                                    bytes = SSL_read(conn[i].ssl, conn[i].in+1, IN_BUFSIZE-2) + 1;
                                    if ( bytes > 1 )
                                        conn[i].in[bytes] = EOS;
                                }
                                set_state_sec(i, bytes);
                            }
                            else    /* POST */
                            {
//                              DBG("state == CONN_STATE_READING_DATA");
//                              DBG("Trying to read %ld bytes of POST data from fd=%d", conn[i].clen-conn[i].was_read, conn[i].fd);
                                bytes = SSL_read(conn[i].ssl, conn[i].data+conn[i].was_read, conn[i].clen-conn[i].was_read);
                                if ( bytes > 0 )
                                    conn[i].was_read += bytes;
                                set_state_sec(i, bytes);
                            }
                        }
                        else        /* HTTP */
#endif
                        {
//                          DBG("not secure, state=%c", conn[i].conn_state);

                            if ( conn[i].conn_state == CONN_STATE_CONNECTED )
                            {
//                              DBG("state == CONN_STATE_CONNECTED");
//                              DBG("Trying read from fd=%d", conn[i].fd);
                                bytes = read(conn[i].fd, conn[i].in, IN_BUFSIZE-1);
                                if ( bytes > 0 )
                                    conn[i].in[bytes] = EOS;
                                set_state(i, bytes);    /* possibly:    CONN_STATE_DISCONNECTED (if error or closed by peer) */
                                                        /*              CONN_STATE_READY_FOR_PARSE */
                            }
                            else if ( conn[i].conn_state == CONN_STATE_READING_DATA )   /* POST */
                            {
//                              DBG("state == CONN_STATE_READING_DATA");
//                              DBG("Trying to read %ld bytes of POST data from fd=%d", conn[i].clen-conn[i].was_read, conn[i].fd);
                                bytes = read(conn[i].fd, conn[i].data+conn[i].was_read, conn[i].clen-conn[i].was_read);
                                conn[i].was_read += bytes;
                                set_state(i, bytes);    /* possibly:    CONN_STATE_DISCONNECTED (if error or closed by peer) */
                                                        /*              CONN_STATE_READY_FOR_PROCESS */
                            }
                        }
                    }

                    /* --------------------------------------------------------------------------------------- */
                    if ( FD_ISSET(conn[i].fd, &M_writefds) )        /* ready for outgoing data */
                    {
//                      DBG("fd=%d is ready for outgoing data", conn[i].fd);

                        /* async processing */
#ifdef ASYNC
                        if ( conn[i].conn_state == CONN_STATE_WAITING_FOR_ASYNC )
                        {
                            for ( j=0; j<MAX_ASYNC; ++j )
                            {
                                if ( (ares[j].state==ASYNC_STATE_RECEIVED || ares[j].state==ASYNC_STATE_TIMEOUTED) && ares[j].ci == i )
                                {
                                    if ( ares[j].state == ASYNC_STATE_RECEIVED )
                                    {
                                        DBG("Async response in an array for ci=%d, processing", i);
                                        app_async_done(i, ares[j].service, ares[j].data, FALSE);
                                    }
                                    else if ( ares[j].state == ASYNC_STATE_TIMEOUTED )
                                    {
                                        DBG("Async response done as timeout-ed for ci=%d", i);
                                        app_async_done(i, ares[j].service, "", TRUE);
                                    }
                                    gen_response_header(i);
                                    ares[j].state = ASYNC_STATE_FREE;
                                    break;
                                }
                            }
                        }
#endif
#ifdef HTTPS
                        if ( conn[i].secure )   /* HTTPS */
                        {
//                          DBG("secure, state=%c", conn[i].conn_state);

                            if ( conn[i].conn_state == CONN_STATE_READY_TO_SEND_HEADER )
                            {
//                              DBG("state == CONN_STATE_READY_TO_SEND_HEADER");
//                              DBG("Trying to write %ld bytes to fd=%d", strlen(conn[i].header), conn[i].fd);
                                bytes = SSL_write(conn[i].ssl, conn[i].header, strlen(conn[i].header));
                                set_state_sec(i, bytes);
                            }
                            else if ( conn[i].conn_state == CONN_STATE_READY_TO_SEND_BODY || conn[i].conn_state == CONN_STATE_SENDING_BODY)
                            {
//                              DBG("state == %s", conn[i].conn_state==CONN_STATE_READY_TO_SEND_BODY?"CONN_STATE_READY_TO_SEND_BODY":"CONN_STATE_SENDING_BODY");
//                              DBG("Trying to write %ld bytes to fd=%d", conn[i].clen, conn[i].fd);
                                if ( conn[i].static_res == NOT_STATIC )
                                    bytes = SSL_write(conn[i].ssl, conn[i].out_data, conn[i].clen);
                                else
                                    bytes = SSL_write(conn[i].ssl, M_stat[conn[i].static_res].data, conn[i].clen);
//                              conn[i].data_sent += bytes;
                                set_state_sec(i, bytes);
                            }
                        }
                        else    /* HTTP */
#endif
                        {
//                          DBG("not secure, state=%c", conn[i].conn_state);

                            if ( conn[i].conn_state == CONN_STATE_READY_TO_SEND_HEADER )
                            {
//                              DBG("state == CONN_STATE_READY_TO_SEND_HEADER");
//                              DBG("Trying to write %ld bytes to fd=%d", strlen(conn[i].header), conn[i].fd);
                                bytes = write(conn[i].fd, conn[i].header, strlen(conn[i].header));
                                set_state(i, bytes);    /* possibly:    CONN_STATE_DISCONNECTED (if error or closed by peer) */
                                                        /*              CONN_STATE_READY_TO_SEND_BODY */
                            }
                            else if ( conn[i].conn_state == CONN_STATE_READY_TO_SEND_BODY || conn[i].conn_state == CONN_STATE_SENDING_BODY)
                            {
//                              DBG("state == %s", conn[i].conn_state==CONN_STATE_READY_TO_SEND_BODY?"CONN_STATE_READY_TO_SEND_BODY":"CONN_STATE_SENDING_BODY");
//                              DBG("Trying to write %ld bytes to fd=%d", conn[i].clen-conn[i].data_sent, conn[i].fd);
                                if ( conn[i].static_res == NOT_STATIC )
                                    bytes = write(conn[i].fd, conn[i].out_data+conn[i].data_sent, conn[i].clen-conn[i].data_sent);
                                else
                                    bytes = write(conn[i].fd, M_stat[conn[i].static_res].data+conn[i].data_sent, conn[i].clen-conn[i].data_sent);
                                conn[i].data_sent += bytes;
                                set_state(i, bytes);    /* possibly:    CONN_STATE_DISCONNECTED (if error or closed by peer or !keep_alive) */
                                                        /*              CONN_STATE_SENDING_BODY (if data_sent < clen) */
                                                        /*              CONN_STATE_CONNECTED */
                            }
                        }
                    }

                    /* --------------------------------------------------------------------------------------- */
                    /* after reading / writing it may be ready for parsing and processing ... */

                    if ( conn[i].conn_state == CONN_STATE_READY_FOR_PARSE )
                    {
                        clock_gettime(MONOTONIC_CLOCK_NAME, &conn[i].proc_start);

                        conn[i].status = parse_req(i, bytes);
#ifdef HTTPS
#ifdef DOMAINONLY       /* redirect to final domain first */
                        if ( !conn[i].secure && conn[i].upgrade2https && 0!=strcmp(conn[i].host, APP_DOMAIN) )
                            conn[i].upgrade2https = FALSE;
#endif
#endif
                        if ( conn[i].conn_state != CONN_STATE_READING_DATA )
                            conn[i].conn_state = CONN_STATE_READY_FOR_PROCESS;
                    }

                    /* received Expect: 100-continue before content */

                    if ( conn[i].expect100 )
                        respond_to_expect(i);

                    /* ready for processing */

                    if ( conn[i].conn_state == CONN_STATE_READY_FOR_PROCESS )
                    {
#ifdef HTTPS
                        if ( conn[i].upgrade2https && conn[i].status==200 )
                            conn[i].status = 301;
#endif
                        /* update visits counter */
                        if ( !conn[i].resource[0] && conn[i].status==200 && !conn[i].bot && !conn[i].head_only && 0==strcmp(conn[i].host, APP_DOMAIN) )
                        {
                            ++G_cnts_today.visits;
                            if ( conn[i].mobile )
                                ++G_cnts_today.visits_mob;
                            else
                                ++G_cnts_today.visits_dsk;
                        }

                        /* process request */
                        process_req(i);
                        gen_response_header(i);
                    }
                }
            }
        }

        /* async processing -- check on response queue */
#ifdef ASYNC
        async_res_t res;

        if ( mq_receive(G_queue_res, (char*)&res, ASYNC_RES_MSG_SIZE, 0) != -1 )    /* there's a response in a queue */
        {
            DBG("Message received!");
            DBG("res.call_id = %ld", res.call_id);
            DBG("res.ci = %d", res.ci);
            DBG("res.service [%s]", res.service);

            for ( j=0; j<MAX_ASYNC; ++j )
            {
                if ( ares[j].call_id == res.call_id )
                {
                    DBG("ares record found");
                    memcpy(&ares[j], (char*)&res, ASYNC_RES_MSG_SIZE);
                    ares[j].state = ASYNC_STATE_RECEIVED;
                    break;
                }
            }
        }

        /* free timeout-ed */

        for ( j=0; j<MAX_ASYNC; ++j )
        {
            if ( ares[j].state==ASYNC_STATE_TIMEOUTED )     /* apparently closed browser connection */
            {
                ares[j].state = ASYNC_STATE_FREE;
            }
        }
#endif
        ++time_elapsed;
    }

    clean_up();

    return EXIT_SUCCESS;
}


/* --------------------------------------------------------------------------
   Set new connection state after read or write
-------------------------------------------------------------------------- */
static void set_state(int ci, long bytes)
{
    if ( bytes <= 0 )   /* read failure stop now */
    {
        DBG("bytes = %ld, errno = %d (%s), disconnecting slot %d\n", bytes, errno, strerror(errno), ci);
        close_conn(ci);
        return;
    }

    /* bytes > 0 */

    DBG("bytes = %ld", bytes);

    if ( conn[ci].conn_state == CONN_STATE_CONNECTED )  /* assume the whole header has been read */
    {
//      DBG("Changing state to CONN_STATE_READY_FOR_PARSE");
        conn[ci].conn_state = CONN_STATE_READY_FOR_PARSE;
    }
    else if ( conn[ci].conn_state == CONN_STATE_READING_DATA )  /* it could have been received only partially */
    {
        if ( conn[ci].was_read < conn[ci].clen )
        {
            DBG("Continue receiving");
        }
        else    /* data received */
        {
            conn[ci].data[conn[ci].was_read] = EOS;
#ifdef DUMP     /* low-level tests */
//          log_long(conn[ci].data, conn[ci].was_read, "POST data received");
#else
            DBG("POST data received");
#endif
//          DBG("Changing state to CONN_STATE_READY_FOR_PROCESS");
            conn[ci].conn_state = CONN_STATE_READY_FOR_PROCESS;
        }
    }
    else if ( conn[ci].conn_state == CONN_STATE_READY_TO_SEND_HEADER )  /* assume the whole header has been sent successfuly */
    {
        if ( conn[ci].clen > 0 )
        {
//          DBG("Changing state to CONN_STATE_READY_TO_SEND_BODY");
            conn[ci].conn_state = CONN_STATE_READY_TO_SEND_BODY;
        }
        else /* no body to send */
        {
            DBG("clen = 0");
            log_proc_time(ci);
            if ( conn[ci].keep_alive )
            {
                DBG("End of processing, reset_conn\n");
                reset_conn(ci, CONN_STATE_CONNECTED);
            }
            else
            {
                DBG("End of processing, close_conn\n");
                close_conn(ci);
            }
        }
    }
    else if ( conn[ci].conn_state == CONN_STATE_READY_TO_SEND_BODY )    /* it could have been sent only partially */
    {
        if ( bytes < conn[ci].clen )
        {
//          DBG("Changing state to CONN_STATE_SENDING_BODY");
            conn[ci].conn_state = CONN_STATE_SENDING_BODY;
        }
        else /* assuming the whole body has been sent at once */
        {
            log_proc_time(ci);
            if ( conn[ci].keep_alive )
            {
                DBG("End of processing, reset_conn\n");
                reset_conn(ci, CONN_STATE_CONNECTED);
            }
            else
            {
                DBG("End of processing, close_conn\n");
                close_conn(ci);
            }
        }
    }
    else if ( conn[ci].conn_state == CONN_STATE_SENDING_BODY )
    {
        if ( conn[ci].data_sent < conn[ci].clen )
        {
            DBG("Continue sending");
        }
        else    /* body sent */
        {
            log_proc_time(ci);
            if ( conn[ci].keep_alive )
            {
                DBG("End of processing, reset_conn\n");
                reset_conn(ci, CONN_STATE_CONNECTED);
            }
            else
            {
                DBG("End of processing, close_conn\n");
                close_conn(ci);
            }
        }
    }
}


/* --------------------------------------------------------------------------
   Set new connection state after read or write for secure connections
-------------------------------------------------------------------------- */
static void set_state_sec(int ci, long bytes)
{
    int     e;
    char    ec[64]="";
#ifdef HTTPS
    e = errno;

    conn[ci].ssl_err = SSL_get_error(conn[ci].ssl, bytes);

    if ( bytes <= 0 )
    {
        if ( conn[ci].ssl_err == SSL_ERROR_SYSCALL )
            sprintf(ec, ", errno = %d (%s)", e, strerror(e));

        DBG("bytes = %ld, ssl_err = %d%s", bytes, conn[ci].ssl_err, ec);

        if ( conn[ci].ssl_err != SSL_ERROR_WANT_READ && conn[ci].ssl_err != SSL_ERROR_WANT_WRITE )
        {
            DBG("Closing connection\n");
            close_conn(ci);
        }

        return;
    }

    /* bytes > 0 */

    DBG("bytes = %ld", bytes);

    // we have no way of knowing if accept finished before reading actual request
    if ( conn[ci].conn_state == CONN_STATE_ACCEPTING || conn[ci].conn_state == CONN_STATE_CONNECTED )   /* assume the whole header has been read */
    {
//      DBG("Changing state to CONN_STATE_READY_FOR_PARSE");
        conn[ci].conn_state = CONN_STATE_READY_FOR_PARSE;
    }
    else if ( conn[ci].conn_state == CONN_STATE_READING_DATA )
    {
        if ( conn[ci].was_read < conn[ci].clen )
        {
            DBG("Continue receiving");
        }
        else    /* data received */
        {
            conn[ci].data[conn[ci].was_read] = EOS;
#ifdef DUMP     /* low-level tests */
//          log_long(conn[ci].data, conn[ci].was_read, "POST data received");
#else
            DBG("POST data received");
#endif
//          DBG("Changing state to CONN_STATE_READY_FOR_PROCESS");
            conn[ci].conn_state = CONN_STATE_READY_FOR_PROCESS;
        }
    }
    else if ( conn[ci].conn_state == CONN_STATE_READY_TO_SEND_HEADER )
    {
        if ( conn[ci].clen > 0 )
        {
//          DBG("Changing state to CONN_STATE_READY_TO_SEND_BODY");
            conn[ci].conn_state = CONN_STATE_READY_TO_SEND_BODY;
        }
        else /* no body to send */
        {
            DBG("clen = 0");
            log_proc_time(ci);
            if ( conn[ci].keep_alive )
            {
                DBG("End of processing, reset_conn\n");
                reset_conn(ci, CONN_STATE_CONNECTED);
            }
            else
            {
                DBG("End of processing, close_conn\n");
                close_conn(ci);
            }
        }
    }
    else if ( conn[ci].conn_state == CONN_STATE_READY_TO_SEND_BODY || conn[ci].conn_state == CONN_STATE_SENDING_BODY )
    {
        log_proc_time(ci);
        if ( conn[ci].keep_alive )
        {
            DBG("End of processing, reset_conn\n");
            reset_conn(ci, CONN_STATE_CONNECTED);
        }
        else
        {
            DBG("End of processing, close_conn\n");
            close_conn(ci);
        }
    }
#endif
}


/* --------------------------------------------------------------------------
   Read & parse conf file and set global parameters
-------------------------------------------------------------------------- */
static bool read_conf()
{
    char    *p_conf_path=NULL;
    char    conf_path[256];

    /* set defaults */

    G_logLevel = 4;
    G_httpPort = 80;
    G_httpsPort = 443;
    G_certFile[0] = EOS;
    G_certChainFile[0] = EOS;
    G_keyFile[0] = EOS;
    G_dbName[0] = EOS;
    G_dbUser[0] = EOS;
    G_dbPassword[0] = EOS;
    G_blockedIPList[0] = EOS;
    G_test = 0;

    /* get the conf file path & name */

    if ( NULL != (p_conf_path=getenv("SILGY_CONF")) )
    {
        return lib_read_conf(p_conf_path);
    }
    else    /* no SILGY_CONF -- try default */
    {
        sprintf(conf_path, "%s/bin/silgy.conf", G_appdir);
        printf("SILGY_CONF not set, trying %s...\n", conf_path);
        return lib_read_conf(conf_path);
    }
}


/* --------------------------------------------------------------------------
   Respond to Expect: header
-------------------------------------------------------------------------- */
static void respond_to_expect(int ci)
{
    static char reply_accept[]="HTTP/1.1 100 Continue\r\n\r\n";
    static char reply_refuse[]="HTTP/1.1 413 Request Entity Too Large\r\n\r\n";
    int bytes;

    if ( conn[ci].clen >= MAX_POST_DATA_BUFSIZE )   /* refuse */
    {
        INF("Sending 413");
#ifdef HTTPS
        if ( conn[ci].secure )
            bytes = SSL_write(conn[ci].ssl, reply_refuse, 41);
        else
#endif
            bytes = write(conn[ci].fd, reply_refuse, 41);

        if ( bytes < 41 ) ERR("write error, bytes = %d", bytes);
    }
    else    /* accept */
    {
        INF("Sending 100");

#ifdef HTTPS
        if ( conn[ci].secure )
            bytes = SSL_write(conn[ci].ssl, reply_accept, 25);
        else
#endif
            bytes = write(conn[ci].fd, reply_accept, 25);

        if ( bytes < 25 ) ERR("write error, bytes = %d", bytes);
    }

    conn[ci].expect100 = FALSE;
}


/* --------------------------------------------------------------------------
   Log processing time
-------------------------------------------------------------------------- */
static void log_proc_time(int ci)
{
    DBG("Processing time: %.3lf ms [%s]\n", lib_elapsed(&conn[ci].proc_start), conn[ci].resource);

    if ( G_logLevel < LOG_DBG )
    {
        ALWAYS("[%s] #%ld %s %s  %s  %d  %.3lf ms%s", G_dt+11, conn[ci].req, conn[ci].method, conn[ci].uri, (conn[ci].static_res==NOT_STATIC && !conn[ci].post)?conn[ci].referer:"", conn[ci].status, lib_elapsed(&conn[ci].proc_start), conn[ci].bot?" [bot]":"");
    }
}


/* --------------------------------------------------------------------------
   Close connection
-------------------------------------------------------------------------- */
static void close_conn(int ci)
{
#ifdef HTTPS
    if ( conn[ci].secure )
        SSL_free(conn[ci].ssl);
#endif
    close(conn[ci].fd);
    reset_conn(ci, CONN_STATE_DISCONNECTED);
}


/* --------------------------------------------------------------------------
  engine init
  return TRUE if success
-------------------------------------------------------------------------- */
static bool init(int argc, char **argv)
{
    time_t      sometimeahead;
    int         i=0;

    /* libSHA1 test */

    uint8_t sha1_res1[SHA1_DIGEST_SIZE];
    char    sha1_res2[64];
    char    sha1_res3[64];

    /* init globals */

    G_pid = getpid();
    G_days_up = 0;
    G_open_conn = 0;
    G_sessions = 0;

    /* counters */

    memset(&G_cnts_today, 0, sizeof(counters_t));
    memset(&G_cnts_yesterday, 0, sizeof(counters_t));
    memset(&G_cnts_day_before, 0, sizeof(counters_t));

#ifdef DBMYSQL
    G_dbconn = NULL;
#endif

    /* command line arguments */

    /* nothing here anymore -- everything's gone to the conf file */

    /* app root dir */

    lib_get_app_dir();      // set G_appdir

    /* read the config file or set defaults */

    read_conf();

    /* init time variables */

    G_now = time(NULL);
    G_ptm = gmtime(&G_now);
    sprintf(G_dt, "%d-%02d-%02d %02d:%02d:%02d", G_ptm->tm_year+1900, G_ptm->tm_mon+1, G_ptm->tm_mday, G_ptm->tm_hour, G_ptm->tm_min, G_ptm->tm_sec);

    /* start log */

    if ( !log_start("", G_test) )
        return FALSE;

    ALWAYS("Starting program");
    ALWAYS("");

    ALWAYS("sizeof(time_t) = %d", sizeof(time_t));
    ALWAYS("G_now = %ld", G_now);
    ALWAYS("");

#ifdef __linux__
    INF("This is Linux");
    INF("");
#endif

    ALWAYS("G_appdir [%s]", G_appdir);
    ALWAYS("logLevel = %d", G_logLevel);
    ALWAYS("httpPort = %d", G_httpPort);
    ALWAYS("httpsPort = %d", G_httpsPort);
    ALWAYS("G_dbName [%s]", G_dbName);
    ALWAYS("G_test = %d", G_test);

    /* pid file --------------------------------------------------------------------------- */

    if ( !(M_pidfile=lib_create_pid_file(argv[0])) )
        return FALSE;

    /* empty static resources list */

    strcpy(M_stat[0].name, "-");

    /* check endianness and some parameters */

    get_byteorder();

    ALWAYS("");
    ALWAYS("----------------------------------------------------------------------------------------------");
    ALWAYS("");
    ALWAYS("System:");
    ALWAYS("-------");
    ALWAYS("              SIZE_MAX = %lu (%lu kB / %lu MB)", SIZE_MAX, SIZE_MAX/1024, SIZE_MAX/1024/1024);
    ALWAYS("            FD_SETSIZE = %d", FD_SETSIZE);
    ALWAYS("             SOMAXCONN = %d", SOMAXCONN);
    ALWAYS("");
    ALWAYS("Server:");
    ALWAYS("-------");
    ALWAYS("              SILGYDIR = %s", G_appdir);
    ALWAYS("    WEB_SERVER_VERSION = %s", WEB_SERVER_VERSION);
#ifdef MEM_SMALL
    ALWAYS("          Memory model = MEM_SMALL");
#endif
#ifdef MEM_MEDIUM
    ALWAYS("          Memory model = MEM_MEDIUM");
#endif
#ifdef MEM_BIG
    ALWAYS("          Memory model = MEM_BIG");
#endif
#ifdef MEM_HUGE
    ALWAYS("          Memory model = MEM_HUGE");
#endif
    ALWAYS("       MAX_CONNECTIONS = %d", MAX_CONNECTIONS);
    ALWAYS("          MAX_SESSIONS = %d", MAX_SESSIONS);
    ALWAYS("          CONN_TIMEOUT = %d seconds", CONN_TIMEOUT);
    ALWAYS("          USES_TIMEOUT = %d seconds", USES_TIMEOUT);
#ifdef USERS
    ALWAYS("         LUSES_TIMEOUT = %d seconds", LUSES_TIMEOUT);
#endif
    ALWAYS("");
    ALWAYS("           conn's size = %lu B (%lu kB / %0.2lf MB)", sizeof(conn), sizeof(conn)/1024, (double)sizeof(conn)/1024/1024);
    ALWAYS("            uses' size = %lu B (%lu kB / %0.2lf MB)", sizeof(uses), sizeof(uses)/1024, (double)sizeof(uses)/1024/1024);
    ALWAYS("");
    ALWAYS("           OUT_BUFSIZE = %lu B (%lu kB / %0.2lf MB)", OUT_BUFSIZE, OUT_BUFSIZE/1024, (double)OUT_BUFSIZE/1024/1024);
#ifdef OUTFAST
    ALWAYS("           Output type = OUTFAST");
#endif
#ifdef OUTCHECK
    ALWAYS("           Output type = OUTCHECK");
#endif
#ifdef OUTCHECKREALLOC
    ALWAYS("           Output type = OUTCHECKREALLOC");
#endif

#ifdef QS_DEF_SQL_ESCAPE
    ALWAYS(" Query string security = QS_DEF_SQL_ESCAPE");
#endif
#ifdef QS_DEF_DONT_ESCAPE
    ALWAYS(" Query string security = QS_DEF_DONT_ESCAPE");
#endif
#ifdef QS_DEF_HTML_ESCAPE
    ALWAYS(" Query string security = QS_DEF_HTML_ESCAPE");
#endif
    ALWAYS("");
    ALWAYS("Program:");
    ALWAYS("--------");
    ALWAYS("           APP_WEBSITE = %s", APP_WEBSITE);
    ALWAYS("            APP_DOMAIN = %s", APP_DOMAIN);
    ALWAYS("           APP_VERSION = %s", APP_VERSION);
    ALWAYS("         APP_COPYRIGHT = %s", APP_COPYRIGHT);
    ALWAYS("         APP_LOGIN_URI = %s", APP_LOGIN_URI);
    if ( APP_DEF_AUTH_LEVEL == AUTH_LEVEL_NONE )
        ALWAYS("    APP_DEF_AUTH_LEVEL = AUTH_LEVEL_NONE");
    else if ( APP_DEF_AUTH_LEVEL == AUTH_LEVEL_ANONYMOUS )
        ALWAYS("    APP_DEF_AUTH_LEVEL = AUTH_LEVEL_ANONYMOUS");
    else if ( APP_DEF_AUTH_LEVEL == AUTH_LEVEL_LOGGEDIN )
        ALWAYS("    APP_DEF_AUTH_LEVEL = AUTH_LEVEL_LOGGEDIN");
    else if ( APP_DEF_AUTH_LEVEL == AUTH_LEVEL_ADMIN )
        ALWAYS("    APP_DEF_AUTH_LEVEL = AUTH_LEVEL_ADMIN");
    ALWAYS("       APP_ADMIN_EMAIL = %s", APP_ADMIN_EMAIL);
    ALWAYS("     APP_CONTACT_EMAIL = %s", APP_CONTACT_EMAIL);
    ALWAYS("");
    ALWAYS("           auses' size = %lu B (%lu kB / %0.2lf MB)", sizeof(auses), sizeof(auses)/1024, (double)sizeof(auses)/1024/1024);
    ALWAYS("");
    ALWAYS("----------------------------------------------------------------------------------------------");
    ALWAYS("");

    /* custom init
       Among others, that may contain generating statics, like css and js */

    if ( !app_init(argc, argv) )
    {
        ERR("app_init() failed");
        return FALSE;
    }

    DBG("app_init() OK");

    /* read static resources */

    if ( !read_files(FALSE) )   /* normal */
    {
        ERR("read_files() failed");
        return FALSE;
    }

    DBG("read_files(FALSE) OK");

    if ( !read_files(TRUE) )    /* minified */
    {
        ERR("read_files() for minified failed");
        return FALSE;
    }

    DBG("read_files(TRUE) OK");

    /* special case statics -- check if present */

    for ( i=0; M_stat[i].name[0] != '-'; ++i )
    {
        if ( 0==strcmp(M_stat[i].name, "favicon.ico") )
        {
            M_favicon_exists = TRUE;
            break;
        }
    }

    for ( i=0; M_stat[i].name[0] != '-'; ++i )
    {
        if ( 0==strcmp(M_stat[i].name, "robots.txt") )
        {
            M_robots_exists = TRUE;
            break;
        }
    }

    for ( i=0; M_stat[i].name[0] != '-'; ++i )
    {
        if ( 0==strcmp(M_stat[i].name, "apple-touch-icon.png") )
        {
            M_appleicon_exists = TRUE;
            break;
        }
    }

    DBG("Standard icons OK");

    /* libSHA1 test */

    DBG("");
    DBG("Trying libSHA1...\n");
    DBG("Expected: [A9993E36 4706816A BA3E2571 7850C26C 9CD0D89D]");

    libSHA1((unsigned char*)"abc", 3, sha1_res1);

    digest_to_hex(sha1_res1, sha1_res2);
    DBG("     Got: [%s]\n", sha1_res2);

    /* calculate Expires and Last-Modified header fields for static resources */

    strftime(G_last_modified, 32, "%a, %d %b %Y %X %Z", G_ptm);
    DBG("Now is: %s\n", G_last_modified);

    sometimeahead = G_now + 3600*24*EXPIRES_IN_DAYS;
    G_ptm = gmtime(&sometimeahead);
    strftime(M_expires, 32, "%a, %d %b %Y %X %Z", G_ptm);
    DBG("M_expires: %s\n", M_expires);

    G_ptm = gmtime(&G_now); /* reset to today */

    /* handle signals */

    signal(SIGINT,  sigdisp);   /* Ctrl-C */
    signal(SIGTERM, sigdisp);
    signal(SIGQUIT, sigdisp);   /* Ctrl-\ */
    signal(SIGTSTP, sigdisp);   /* Ctrl-Z */

    /* initialize SSL connection */

#ifdef HTTPS
    if ( !init_ssl() )
    {
        ERR("init_ssl failed");
        return FALSE;
    }
#endif

    /* init conn array */

    for (i=0; i<MAX_CONNECTIONS; ++i)
    {
#ifdef OUTCHECKREALLOC
        if ( !(conn[i].out_data = (char*)malloc(OUT_BUFSIZE)) )
        {
            ERR("malloc for conn[%d].out_data failed", i);
            return FALSE;
        }
#endif
        conn[i].out_data_allocated = OUT_BUFSIZE;
        reset_conn(i, CONN_STATE_DISCONNECTED);
        conn[i].req = 0;
    }

    /* init user sessions */

//  memset(&uses, 0, sizeof(uses));

    for (i=0; i<MAX_SESSIONS+1; ++i)
    {
        eng_uses_reset(i);
        app_uses_reset(i);
    }

    /* read blocked IPs list */

    if ( G_blockedIPList[0] )
        read_blocked_ips();

#ifdef ASYNC
    ALWAYS("\nOpening message queues...\n");

    struct mq_attr attr;

    attr.mq_maxmsg = 100;
    attr.mq_msgsize = ASYNC_REQ_MSG_SIZE;
    attr.mq_flags = 0;
    attr.mq_curmsgs = 0;

    G_queue_req = mq_open(ASYNC_REQ_QUEUE, O_WRONLY | O_CREAT | O_NONBLOCK, 0664, &attr);
    if (G_queue_req < 0)
        ERR("mq_open for req failed, errno = %d (%s)", errno, strerror(errno));

    attr.mq_msgsize = ASYNC_RES_MSG_SIZE;   /* larger buffer */

    G_queue_res = mq_open(ASYNC_RES_QUEUE, O_RDONLY | O_CREAT | O_NONBLOCK, 0664, &attr);
    if (G_queue_res < 0)
        ERR("mq_open for res failed, errno = %d (%s)", errno, strerror(errno));

    for (i=0; i<MAX_ASYNC; ++i)
        ares[i].state = ASYNC_STATE_FREE;

    G_last_call_id = 0;

#endif

    return TRUE;
}


/* --------------------------------------------------------------------------
  set socket as non-blocking
-------------------------------------------------------------------------- */
static void setnonblocking(int sock)
{
    int opts;

    opts = fcntl(sock, F_GETFL);

    if (opts < 0)
    {
        ERR("fcntl(F_GETFL) failed");
        exit(EXIT_FAILURE);
    }

    opts = (opts | O_NONBLOCK);

    if (fcntl(sock, F_SETFL, opts) < 0)
    {
        ERR("fcntl(F_SETFL) failed");
        exit(EXIT_FAILURE);
    }
}


/* --------------------------------------------------------------------------
  build select list
-------------------------------------------------------------------------- */
static void build_select_list()
{
    int i;

    FD_ZERO(&M_readfds);
    FD_ZERO(&M_writefds);

    FD_SET(M_listening_fd, &M_readfds);
#ifdef HTTPS
    FD_SET(M_listening_sec_fd, &M_readfds);
#endif

    G_open_conn = 0;

    for ( i=0; i<MAX_CONNECTIONS; ++i )
    {
        if ( conn[i].conn_state != CONN_STATE_DISCONNECTED )
        {
            FD_SET(conn[i].fd, &M_readfds);

            /* only for certain states */

#ifdef HTTPS
            if ( conn[i].secure )
            {
                if ( conn[i].conn_state == CONN_STATE_READY_TO_SEND_HEADER
                        || conn[i].conn_state == CONN_STATE_READY_TO_SEND_BODY
                        || conn[i].conn_state == CONN_STATE_SENDING_BODY
                        || conn[i].ssl_err == SSL_ERROR_WANT_WRITE )
                {
                    FD_SET(conn[i].fd, &M_writefds);
                }
            }
            else
            {
#endif
                if ( conn[i].conn_state != CONN_STATE_CONNECTED
                        && conn[i].conn_state != CONN_STATE_READING_DATA )
                    FD_SET(conn[i].fd, &M_writefds);
#ifdef HTTPS
            }
#endif
            if (conn[i].fd > M_highsock)
                M_highsock = conn[i].fd;
            ++G_open_conn;
        }
    }
}


/* --------------------------------------------------------------------------
  handle a brand new connection
  we've got fd and IP here for conn array
-------------------------------------------------------------------------- */
static void accept_http()
{
    int     i;          /* current item in conn_sockets for for loops */
    int     connection; /* socket file descriptor for incoming connections */
static struct   sockaddr_in cli_addr;   /* static = initialised to zeros */
    socklen_t   addr_len;
    char    remote_addr[INET_ADDRSTRLEN]="";    /* remote address */
    long    bytes;

    /* We have a new connection coming in! We'll
       try to find a spot for it in conn_sockets.  */

    addr_len = sizeof(cli_addr);

    /* connection is a fd that accept gives us that we'll be communicating through now with the remote client */
    /* this fd will become our conn id for the whole connection's life (that is, until one of the sides close()-s) */

    connection = accept(M_listening_fd, (struct sockaddr*)&cli_addr, &addr_len);

    if (connection < 0)
    {
        ERR("accept failed, errno = %d (%s)", errno, strerror(errno));
        return;
    }

    /* get the remote address */

    inet_ntop(AF_INET, &(cli_addr.sin_addr), remote_addr, INET_ADDRSTRLEN);

    if ( G_blockedIPList[0] && ip_blocked(remote_addr) )
    {
        ++G_cnts_today.blocked;
        close(connection);
        return;
    }

    setnonblocking(connection);

    /* find a free slot in conn */

    for (i=0; (i<MAX_CONNECTIONS) && (connection != -1); ++i)
    {
        if ( conn[i].conn_state == CONN_STATE_DISCONNECTED )    /* free connection slot -- we'll use it */
        {
            DBG("\nConnection accepted: %s, slot=%d, fd=%d", remote_addr, i, connection);
            conn[i].fd = connection;
            conn[i].secure = FALSE;
            strcpy(conn[i].ip, remote_addr);        /* possibly client IP */
            strcpy(conn[i].pip, remote_addr);       /* possibly proxy IP */
            conn[i].conn_state = CONN_STATE_CONNECTED;
            conn[i].last_activity = G_now;
            connection = -1;                        /* mark as OK */
        }
    }

    if (connection != -1)   /* none was free */
    {
        /* No room left in the queue! */
        WAR("No room left for new client, sending 503");
        bytes = write(connection, "HTTP/1.1 503 Service Unavailable\r\n\r\n", 36);
        if ( bytes < 36 )
            ERR("write error, bytes = %d of 36", bytes);
        close(connection);
    }
}


/* --------------------------------------------------------------------------
  handle a brand new connection
  we've got fd and IP here for conn array
-------------------------------------------------------------------------- */
static void accept_https()
{
#ifdef HTTPS
    int     i;          /* current item in conn_sockets for for loops */
    int     connection; /* socket file descriptor for incoming connections */
static struct   sockaddr_in cli_addr;   /* static = initialised to zeros */
    socklen_t   addr_len;
    char    remote_addr[INET_ADDRSTRLEN]="";    /* remote address */
    long    bytes;
    int     ret, ssl_err;

    /* We have a new connection coming in! We'll
       try to find a spot for it in conn_sockets.  */

    addr_len = sizeof(cli_addr);

    /* connection is a fd that accept gives us that we'll be communicating through now with the remote client */
    /* this fd will become our conn id for the whole connection's life (that is, until one of the sides close()-s) */

    connection = accept(M_listening_sec_fd, (struct sockaddr*)&cli_addr, &addr_len);

    if (connection < 0)
    {
        ERR("accept failed, errno = %d (%s)", errno, strerror(errno));
        return;
    }

    /* get the remote address */

    inet_ntop(AF_INET, &(cli_addr.sin_addr), remote_addr, INET_ADDRSTRLEN);

    if ( G_blockedIPList[0] && ip_blocked(remote_addr) )
    {
        ++G_cnts_today.blocked;
        close(connection);
        return;
    }

    setnonblocking(connection);

    /* find a free slot in conn */

    for (i=0; (i<MAX_CONNECTIONS) && (connection != -1); ++i)
    {
        if ( conn[i].conn_state == CONN_STATE_DISCONNECTED )    /* free connection slot -- we'll use it */
        {
            DBG("\nSecure connection accepted: %s, slot=%d, fd=%d", remote_addr, i, connection);
            conn[i].fd = connection;
            conn[i].secure = TRUE;

            conn[i].ssl = SSL_new(M_ssl_ctx);

            if ( !conn[i].ssl )
            {
                ERR("SSL_new failed");
                close_conn(i);
                break;
            }

            /* SSL_set_fd() sets the file descriptor fd as the input/output facility
               for the TLS/SSL (encrypted) side of ssl. fd will typically be the socket
               file descriptor of a network connection.
               When performing the operation, a socket BIO is automatically created to
               interface between the ssl and fd. The BIO and hence the SSL engine inherit
               the behaviour of fd. If fd is non-blocking, the ssl will also have non-blocking behaviour.
               If there was already a BIO connected to ssl, BIO_free() will be called
               (for both the reading and writing side, if different). */

            ret = SSL_set_fd(conn[i].ssl, connection);

            if ( ret <= 0 )
            {
                ERR("SSL_set_fd failed, ret = %d", ret);
                close_conn(i);
                return;
            }

            ret = SSL_accept(conn[i].ssl);  /* handshake here */

            if ( ret <= 0 )
            {
                conn[i].ssl_err = SSL_get_error(conn[i].ssl, ret);

                if ( conn[i].ssl_err != SSL_ERROR_WANT_READ && conn[i].ssl_err != SSL_ERROR_WANT_WRITE )
                {
                    ERR("SSL_accept failed, ssl_err = %d", conn[i].ssl_err);
                    close_conn(i);
                    return;
                }
            }

            strcpy(conn[i].ip, remote_addr);        /* possibly client IP */
            strcpy(conn[i].pip, remote_addr);       /* possibly proxy IP */
            conn[i].conn_state = CONN_STATE_ACCEPTING;
            conn[i].last_activity = G_now;
            connection = -1;                        /* mark as OK */
        }
    }

    if (connection != -1)   /* none was free */
    {
        /* No room left in the queue! */
        WAR("No room left for new client, closing");
        close(connection);
    }
#endif
}


/* --------------------------------------------------------------------------
   Read list of blocked IPs from the file
-------------------------------------------------------------------------- */
static bool read_blocked_ips()
{
    char    fname[256];
    FILE    *h_file=NULL;
    int     c=0;
    int     i=0;
    char    now_value=1;
    char    now_comment=0;
    char    value[64]="";

    INF("Updating blocked IPs list");

    /* open the file */

    if ( G_blockedIPList[0] == '/' )    /* full path */
        strcpy(fname, G_blockedIPList);
    else    /* just a file name */
        sprintf(fname, "%s/bin/%s", G_appdir, G_blockedIPList);

    if ( NULL == (h_file=fopen(fname, "r")) )
    {
        ERR("Error opening %s\n", fname);
        return FALSE;
    }

    G_blacklist_cnt = 0;

    /* parse the file */

    while ( EOF != (c=fgetc(h_file)) )
    {
        if ( c == ' ' || c == ' ' || c == '\r' ) continue;  /* omit whitespaces */

        if ( c == '\n' )    /* end of value or end of comment or empty line */
        {
            if ( now_value && i )   /* end of value */
            {
                value[i] = EOS;
                if ( !ip_blocked(value) )   /* avoid duplicates */
                {
                    strcpy(G_blacklist[G_blacklist_cnt++], value);
                    if ( G_blacklist_cnt == MAX_BLACKLIST )
                    {
                        WAR("Blacklist full! (%d IPs)", G_blacklist_cnt);
                        now_value = 0;
                        break;
                    }
                }
                i = 0;
            }
            now_value = 1;
            now_comment = 0;
        }
        else if ( now_comment )
        {
            continue;
        }
        else if ( c == '#' )    /* possible end of value */
        {
            if ( now_value && i )   /* end of value */
            {
                value[i] = EOS;
                strcpy(G_blacklist[G_blacklist_cnt++], value);
                if ( G_blacklist_cnt == MAX_BLACKLIST )
                {
                    WAR("Blacklist full! (%d IPs)", G_blacklist_cnt);
                    now_value = 0;
                    break;
                }
                i = 0;
            }
            now_value = 0;
            now_comment = 1;
        }
        else if ( now_value )   /* value */
        {
            if ( i < INET_ADDRSTRLEN-1 )
                value[i++] = c;
        }
    }

    if ( now_value && i )   /* end of value */
    {
        value[i] = EOS;
        strcpy(G_blacklist[G_blacklist_cnt++], value);
    }

    if ( NULL != h_file )
        fclose(h_file);

    ALWAYS("%d IPs blacklisted", G_blacklist_cnt);

    /* show the list */

/*  DBG("");
    for ( i=0; i<G_blacklist_cnt; ++i )
        DBG("%s", G_blacklist[i]);
    DBG("");*/

    return TRUE;
}


/* --------------------------------------------------------------------------
   Return TRUE if addr is on our blacklist
-------------------------------------------------------------------------- */
static bool ip_blocked(const char *addr)
{
    int i;

    for ( i=0; i<G_blacklist_cnt; ++i )
    {
        if ( 0==strcmp(G_blacklist[i], addr) )
            return TRUE;
    }

    return FALSE;
}


/* --------------------------------------------------------------------------
  read static resources from disk
  read all the files from G_appdir/res directory
-------------------------------------------------------------------------- */
static bool read_files(bool minify)
{
    int     i=0;
    char    resdir[256]="";
    DIR     *dir;
struct dirent *dirent;
    char    namewpath[256]="";
    FILE    *fd;
    char    *data_tmp=NULL;
struct stat fstat;
    char    mod_time[32];

    DBG("read_files, minify = %s\n", minify?"TRUE":"FALSE");

    if ( minify )
        sprintf(resdir, "%s/resmin", G_appdir);
    else
        sprintf(resdir, "%s/res", G_appdir);

    if ( (dir=opendir(resdir)) == NULL )
    {
        WAR("Couldn't open directory %s", resdir);
        return TRUE;    /* don't panic, just no external resources will be used */
    }

#ifdef _DIRENT_HAVE_D_TYPE
    INF("_DIRENT_HAVE_D_TYPE is defined");  /* we could use d_type in the future? */
#endif

    /* find the first unused slot in M_stat array */

    i = first_free_stat();

    /* read the files into memory */

    while ( (dirent=readdir(dir)) )
    {
        if ( dirent->d_name[0] == '.' ) /* skip ".", ".." and hidden files */
            continue;

        strcpy(M_stat[i].name, dirent->d_name);

        if ( minify )
            sprintf(namewpath, "%s/resmin/%s", G_appdir, M_stat[i].name);
        else
            sprintf(namewpath, "%s/res/%s", G_appdir, M_stat[i].name);

        if ( NULL == (fd=fopen(namewpath, "r")) )
            ERR("Couldn't open %s", namewpath);
        else
        {
            fseek(fd, 0, SEEK_END);     /* determine the file size */

            M_stat[i].len = ftell(fd);

            rewind(fd);

            if ( minify )
            {
                /* we don't know the minified size yet -- read file into temp buffer */

                if ( NULL == (data_tmp=(char*)malloc(M_stat[i].len+1)) )
                {
                    ERR("Couldn't allocate %ld bytes for %s!!!", M_stat[i].len, M_stat[i].name);
                    fclose(fd);
                    closedir(dir);
                    return FALSE;
                }
                else    /* OK */
                {
                    rewind(fd);
                }

                fread(data_tmp, M_stat[i].len, 1, fd);
                data_tmp[M_stat[i].len] = EOS;

                /* can we use the same buffer again? */
                M_stat[i].len = lib_minify(data_tmp, data_tmp); /* new length */
            }

            /* allocate the final destination */

            if ( NULL == (M_stat[i].data=(char*)malloc(M_stat[i].len+1)) )
            {
                ERR("Couldn't allocate %ld bytes for %s!!!", M_stat[i].len+1, M_stat[i].name);
                fclose(fd);
                closedir(dir);
                return FALSE;
            }

            if ( minify )
            {
                memcpy(M_stat[i].data, data_tmp, M_stat[i].len+1);
                free(data_tmp);
                data_tmp = NULL;
            }
            else
            {
                fread(M_stat[i].data, M_stat[i].len, 1, fd);
            }

            fclose(fd);

            M_stat[i].type = get_res_type(M_stat[i].name);

            /* last modified */

            if ( stat(namewpath, &fstat) == 0 )
                M_stat[i].modified = fstat.st_mtime;
            else
            {
                ERR("stat failed, errno = %d (%s)", errno, strerror(errno));
                return FALSE;
            }

            G_ptm = gmtime(&M_stat[i].modified);
            sprintf(mod_time, "%d-%02d-%02d %02d:%02d:%02d", G_ptm->tm_year+1900, G_ptm->tm_mon+1, G_ptm->tm_mday, G_ptm->tm_hour, G_ptm->tm_min, G_ptm->tm_sec);
            ALWAYS("%s %s\t\t%ld Bytes", lib_add_spaces(M_stat[i].name, 28), mod_time, M_stat[i].len);
        }

//      if ( minify )   /* temporarily */
//      {
//          DBG("minified %s: [%s]", M_stat[i].name, M_stat[i].data);
//      }

        ++i;
    }

    closedir(dir);

    strcpy(M_stat[i].name, "-");    /* end of list */

    G_ptm = gmtime(&G_now);     /* set it back */

    DBG("");

    return TRUE;
}


/* --------------------------------------------------------------------------
  find first free slot in M_stat
-------------------------------------------------------------------------- */
static int first_free_stat()
{
    int i=0;

    for ( i=0; i<MAX_STATICS; ++i )
        if ( 0==strcmp(M_stat[i].name, "-") )
            return i;

    ERR("Big trouble, ran out of statics! i = %d", i);

    return -1;  /* nothing's free, we ran out of statics! */
}


/* --------------------------------------------------------------------------
   Return M_stat array index if name is on statics' list
-------------------------------------------------------------------------- */
static int is_static_res(int ci, const char *name)
{
    int i;

    for ( i=0; M_stat[i].name[0] != '-'; ++i )
    {
        if ( 0==strcmp(M_stat[i].name, name) )
        {
//          DBG("It is static");
            if ( conn[ci].if_mod_since >= M_stat[i].modified )
            {
//              DBG("Not Modified");
                conn[ci].status = 304;  /* Not Modified */
            }
            return i;
        }
    }

    return -1;
}


/* --------------------------------------------------------------------------
   Open database connection
-------------------------------------------------------------------------- */
static bool open_db()
{
#ifdef DBMYSQL
    if ( NULL == (G_dbconn=mysql_init(NULL)) )
    {
        ERR("Error %u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
        return FALSE;
    }
#ifdef DBMYSQLRECONNECT
    my_bool reconnect=1;
    mysql_options(G_dbconn, MYSQL_OPT_RECONNECT, &reconnect);
#endif
    if ( NULL == mysql_real_connect(G_dbconn, NULL, G_dbUser, G_dbPassword, G_dbName, 0, NULL, 0) )
    {
        ERR("Error %u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
        return FALSE;
    }
#endif
    return TRUE;
}


/* --------------------------------------------------------------------------
   Main new request processing
   Request received over current conn is already parsed
-------------------------------------------------------------------------- */
static void process_req(int ci)
{
    int     ret=OK;

    DBG("process_req, ci=%d", ci);

    conn[ci].p_curr_c = conn[ci].out_data;

    conn[ci].location[COLON_POSITION] = '-';    /* no protocol here yet */

    /* ------------------------------------------------------------------------ */
    /* Generate HTML content before header -- to know its size & type --------- */

    /* ------------------------------------------------------------------------ */
    /* authorization check / log in from cookies ------------------------------ */

    if ( conn[ci].static_res == NOT_STATIC && conn[ci].status == 200 )
    {
#ifdef USERS
        if ( conn[ci].cookie_in_l[0] )  /* logged in sesid cookie present */
        {
            ret = libusr_l_usession_ok(ci);     /* is it valid? */

            if ( ret == OK )    /* valid sesid -- user logged in */
                DBG("User logged in from cookie");
            else if ( ret != ERR_INT_SERVER_ERROR && ret != ERR_SERVER_TOOBUSY )    /* dodged sesid... or session expired */
                WAR("Invalid ls cookie");
        }

        if ( !LOGGED && conn[ci].auth_level == AUTH_LEVEL_LOGGEDIN )    /* redirect to login page */
        {
            INF("AUTH_LEVEL_LOGGEDIN required, redirecting to login");
            ret = ERR_REDIRECTION;
            if ( !strlen(APP_LOGIN_URI) )   /* login page = landing page */
                sprintf(conn[ci].location, "%s://%s", PROTOCOL, conn[ci].host);
            else
                strcpy(conn[ci].location, APP_LOGIN_URI);
        }
        else    /* login not required for this URI */
        {
            ret = OK;
        }

        if ( !LOGGED && conn[ci].auth_level == AUTH_LEVEL_ANONYMOUS && !conn[ci].bot && !conn[ci].head_only )       /* anonymous user session required */
#else
        if ( conn[ci].auth_level == AUTH_LEVEL_ANONYMOUS && !conn[ci].bot && !conn[ci].head_only )
#endif
        {
            if ( !conn[ci].cookie_in_a[0] || !a_usession_ok(ci) )       /* valid anonymous sesid cookie not present */
            {
                if ( !eng_uses_start(ci) )  /* start new anonymous user session */
                    ret = ERR_SERVER_TOOBUSY;   /* user sessions exhausted */
            }
        }

        /* ------------------------------------------------------------------------ */
        /* process request -------------------------------------------------------- */

        if ( ret == OK )
        {
            if ( !conn[ci].location[0] )
                ret = app_process_req(ci);  /* main application called here */
        }

        conn[ci].last_activity = G_now;
        if ( conn[ci].usi ) US.last_activity = G_now;

#ifdef ASYNC
        if ( conn[ci].conn_state == CONN_STATE_WAITING_FOR_ASYNC )
        {
            return;
        }
#endif
        /* ------------------------------------------------------------------------ */

        if ( conn[ci].location[0] || ret == ERR_REDIRECTION )   /* redirection has a priority */
            conn[ci].status = 303;
        else if ( ret == ERR_INVALID_REQUEST )
            conn[ci].status = 400;
        else if ( ret == ERR_UNAUTHORIZED )
            conn[ci].status = 401;
        else if ( ret == ERR_FORBIDDEN )
            conn[ci].status = 403;
        else if ( ret == ERR_NOT_FOUND )
            conn[ci].status = 404;
        else if ( ret == ERR_INT_SERVER_ERROR )
            conn[ci].status = 500;
        else if ( ret == ERR_SERVER_TOOBUSY )
            conn[ci].status = 503;

        if ( ret==ERR_REDIRECTION || ret==ERR_INVALID_REQUEST || ret==ERR_UNAUTHORIZED || ret==ERR_FORBIDDEN || ret==ERR_NOT_FOUND || ret==ERR_INT_SERVER_ERROR || ret==ERR_SERVER_TOOBUSY )
        {
            conn[ci].p_curr_c = conn[ci].out_data;      /* reset out buffer pointer as it could have contained something already */
#ifdef USERS
            if ( conn[ci].usi && !LOGGED ) close_a_uses(conn[ci].usi);
#else
            if ( conn[ci].usi ) close_a_uses(conn[ci].usi);
#endif
            gen_page_msg(ci, ret);
        }
    }
}


/* --------------------------------------------------------------------------
   Generate HTTP response header
-------------------------------------------------------------------------- */
static void gen_response_header(int ci)
{
    DBG("gen_response_header, ci=%d", ci);

    conn[ci].p_curr_h = conn[ci].header;

    conn[ci].clen = 0;

    PRINT_HTTP_STATUS(conn[ci].status);

    if ( conn[ci].status == 301 || conn[ci].status == 303 )     /* redirection */
    {
        DBG("Redirecting");

        /*
           1 - upgrade 2 https, keep URI (301)
           2 - app new page version, ignore URI, use location (303)
           3 - redirect to final domain, keep URI (301)
        */

        if ( conn[ci].upgrade2https )   /* (1) */
        {
            PRINT_HTTP_VARY_UIR;    /* Upgrade-Insecure-Requests */
            sprintf(G_tmp, "Location: https://%s/%s\r\n", conn[ci].host, conn[ci].uri);
        }
        else if ( conn[ci].location[COLON_POSITION] == ':' )        /* (2) full address already present */
        {
            sprintf(G_tmp, "Location: %s\r\n", conn[ci].location);
        }
        else if ( conn[ci].location[0] )        /* (2) */
        {
            sprintf(G_tmp, "Location: %s://%s/%s\r\n", PROTOCOL, conn[ci].host, conn[ci].location);
        }
        else if ( conn[ci].uri[0] ) /* (3) URI */
        {
#ifdef DOMAINONLY
            sprintf(G_tmp, "Location: %s://%s/%s\r\n", PROTOCOL, G_test?conn[ci].host:APP_DOMAIN, conn[ci].uri);
#else
            sprintf(G_tmp, "Location: %s://%s/%s\r\n", PROTOCOL, conn[ci].host, conn[ci].uri);
#endif
        }
        else    /* (3) No URI */
        {
#ifdef DOMAINONLY
            sprintf(G_tmp, "Location: %s://%s\r\n", PROTOCOL, G_test?conn[ci].host:APP_DOMAIN);
#else
            sprintf(G_tmp, "Location: %s://%s\r\n", PROTOCOL, conn[ci].host);
#endif
        }
        HOUT(G_tmp);

        PRINT_HTTP_CONTENT_LEN(0);      /* zero content */
    }
    else if ( conn[ci].status == 304 )      /* not modified since */
    {
        DBG("Not Modified");
        if ( conn[ci].static_res == NOT_STATIC )
        {
            PRINT_HTTP_LAST_MODIFIED(G_last_modified);
        }
        else    /* static res */
        {
            PRINT_HTTP_LAST_MODIFIED(time_epoch2http(M_stat[conn[ci].static_res].modified));
        }
        PRINT_HTTP_CONTENT_LEN(0);      /* zero content */
    }
    else    /* normal response with content */
    {
        DBG("Normal response");

        if ( conn[ci].dont_cache )  /* dynamic content */
        {
            PRINT_HTTP_VARY_DYN;
            PRINT_HTTP_NO_CACHE;
        }
        else
        {
            PRINT_HTTP_VARY_STAT;
            if ( conn[ci].static_res == NOT_STATIC )
            {
                if ( conn[ci].modified )
                    PRINT_HTTP_LAST_MODIFIED(time_epoch2http(conn[ci].modified));
                else
                    PRINT_HTTP_LAST_MODIFIED(G_last_modified);
            }
            else    /* static res */
            {
                PRINT_HTTP_LAST_MODIFIED(time_epoch2http(M_stat[conn[ci].static_res].modified));
            }
            PRINT_HTTP_EXPIRES;
        }

        if ( conn[ci].static_res == NOT_STATIC )
            conn[ci].clen = conn[ci].p_curr_c - conn[ci].out_data;
        else
            conn[ci].clen = M_stat[conn[ci].static_res].len;

        PRINT_HTTP_CONTENT_LEN(conn[ci].clen);
    }

    /* Date */

    PRINT_HTTP_DATE;

    /* Connection */

    PRINT_HTTP_CONNECTION(ci);

    /* Cookie */

    if ( conn[ci].static_res == NOT_STATIC && (conn[ci].status == 200 || conn[ci].status == 303) && !conn[ci].head_only )
    {
        if ( conn[ci].cookie_out_l[0] )         /* logged in cookie has been produced */
        {
            if ( conn[ci].cookie_out_l_exp[0] )
            {
                PRINT_HTTP_COOKIE_L_EXP(ci);    /* with expiration date */
            }
            else
            {
                PRINT_HTTP_COOKIE_L(ci);
            }
        }

        if ( conn[ci].cookie_out_a[0] )         /* anonymous cookie has been produced */
        {
            if ( conn[ci].cookie_out_a_exp[0] )
            {
                PRINT_HTTP_COOKIE_A_EXP(ci);    /* with expiration date */
            }
            else
            {
                PRINT_HTTP_COOKIE_A(ci);
            }
        }
    }

    /* Content-Type */

    if ( conn[ci].clen == 0 )   /* don't set for these */
    {                   /* this covers 301, 303 and 304 */
    }
    else if ( conn[ci].static_res != NOT_STATIC )   /* static resource */
    {
        print_content_type(ci, M_stat[conn[ci].static_res].type);
    }
    else if ( conn[ci].ctype == CONTENT_TYPE_USER )
    {
        sprintf(G_tmp, "Content-Type: %s\r\n", conn[ci].ctypestr);
        HOUT(G_tmp);
    }
    else if ( conn[ci].ctype != CONTENT_TYPE_UNSET )
    {
        print_content_type(ci, conn[ci].ctype);
    }

    if ( conn[ci].cdisp[0] )
    {
        sprintf(G_tmp, "Content-Disposition: %s\r\n", conn[ci].cdisp);
        HOUT(G_tmp);
    }

#ifndef NO_IDENTITY
    PRINT_HTTP_SERVER;
#endif

    PRINT_HTTP_END_OF_HEADER;

    DBG("Response status: %d", conn[ci].status);

//  DBG("Changing state to CONN_STATE_READY_TO_SEND_HEADER");
    conn[ci].conn_state = CONN_STATE_READY_TO_SEND_HEADER;

    DBG("\nResponse header:\n\n[%s]\n", conn[ci].header);

#ifdef DUMP     /* low-level tests */
    if ( G_logLevel>=LOG_DBG && conn[ci].clen > 0 && !conn[ci].head_only && conn[ci].static_res == NOT_STATIC && (conn[ci].ctype == CONTENT_TYPE_UNSET || conn[ci].ctype == RES_TEXT || conn[ci].ctype == RES_HTML) )
        log_long(conn[ci].out_data, conn[ci].clen, "Sent");
#endif

    conn[ci].last_activity = G_now;
    if ( conn[ci].usi ) US.last_activity = G_now;
}


/* --------------------------------------------------------------------------
   Print Content-Type to response header
-------------------------------------------------------------------------- */
static void print_content_type(int ci, char type)
{
    char    http_type[32]="text/plain";     /* default */

    if ( type == RES_HTML )
        strcpy(http_type, "text/html; charset=utf-8");
    else if ( type == RES_CSS )
        strcpy(http_type, "text/css");
    else if ( type == RES_JS )
        strcpy(http_type, "application/javascript");
    else if ( type == RES_GIF )
        strcpy(http_type, "image/gif");
    else if ( type == RES_JPG )
        strcpy(http_type, "image/jpeg");
    else if ( type == RES_ICO )
        strcpy(http_type, "image/x-icon");
    else if ( type == RES_PNG )
        strcpy(http_type, "image/png");
    else if ( type == RES_BMP )
        strcpy(http_type, "image/bmp");
    else if ( type == RES_PDF )
        strcpy(http_type, "application/pdf");
    else if ( type == RES_AMPEG )
        strcpy(http_type, "audio/mpeg");
    else if ( type == RES_EXE )
        strcpy(http_type, "application/x-msdownload");
    else if ( type == RES_ZIP )
        strcpy(http_type, "application/zip");

    sprintf(G_tmp, "Content-Type: %s\r\n", http_type);
    HOUT(G_tmp);
}


/* --------------------------------------------------------------------------
   Verify IP & User-Agent against sesid in uses (anonymous users)
   Return user session array index if all ok
-------------------------------------------------------------------------- */
static bool a_usession_ok(int ci)
{
    int i;

    for (i=1; i<=MAX_SESSIONS; ++i)
    {
        if ( uses[i].sesid[0] && !uses[i].logged && 0==strcmp(conn[ci].cookie_in_a, uses[i].sesid)
/*              && 0==strcmp(conn[ci].ip, uses[i].ip) */
                && 0==strcmp(conn[ci].uagent, uses[i].uagent) )
        {
            DBG("Anonymous session found, usi=%d, sesid [%s]", i, uses[i].sesid);
            conn[ci].usi = i;
            return TRUE;
        }
    }

    /* not found */
    return FALSE;
}


/* --------------------------------------------------------------------------
   Close timeouted connections
-------------------------------------------------------------------------- */
static void close_old_conn()
{
    int     i;
    time_t  last_allowed;

    last_allowed = G_now - CONN_TIMEOUT;

    for (i=0; i<MAX_CONNECTIONS; ++i)
    {
        if ( conn[i].conn_state != CONN_STATE_DISCONNECTED && conn[i].last_activity < last_allowed )
        {
            DBG("Closing timeouted connection %d", i);
            close_conn(i);
        }
    }
}


/* --------------------------------------------------------------------------
  close timeouted anonymous user sessions
-------------------------------------------------------------------------- */
static void close_uses_timeout()
{
    int     i;
    time_t  last_allowed;

    last_allowed = G_now - USES_TIMEOUT;

    for (i=1; i<=MAX_SESSIONS; ++i)
    {
        if ( uses[i].sesid[0] && !uses[i].logged && uses[i].last_activity < last_allowed )
            close_a_uses(i);
    }
}


/* --------------------------------------------------------------------------
  close anonymous user session
-------------------------------------------------------------------------- */
static void close_a_uses(int usi)
{
    DBG("Closing anonymous session, usi=%d, sesid [%s]", usi, uses[usi].sesid);
    eng_uses_close(usi);
}


/* --------------------------------------------------------------------------
  reset connection after processing request
-------------------------------------------------------------------------- */
static void reset_conn(int ci, char conn_state)
{
    conn[ci].status = 200;
    conn[ci].conn_state = conn_state;
    conn[ci].method[0] = EOS;
    conn[ci].head_only = FALSE;
    conn[ci].post = FALSE;
    if ( conn[ci].data )
    {
        free(conn[ci].data);
        conn[ci].data = NULL;
    }
    conn[ci].was_read = 0;
    conn[ci].upgrade2https = FALSE;
    conn[ci].data_sent = 0;
    conn[ci].resource[0] = EOS;
    conn[ci].id[0] = EOS;
    conn[ci].uagent[0] = EOS;
    conn[ci].mobile = FALSE;
    conn[ci].referer[0] = EOS;
    conn[ci].keep_alive = FALSE;
    conn[ci].clen = 0;
    conn[ci].cookie_in_a[0] = EOS;
    conn[ci].cookie_in_l[0] = EOS;
    conn[ci].host[0] = EOS;
    strcpy(conn[ci].website, APP_WEBSITE);
    conn[ci].lang[0] = EOS;
    conn[ci].if_mod_since = 0;
    conn[ci].in_ctype = CONTENT_TYPE_URLENCODED;
    conn[ci].boundary[0] = EOS;
    conn[ci].auth_level = APP_DEF_AUTH_LEVEL;
    conn[ci].usi = 0;
    conn[ci].static_res = NOT_STATIC;
    conn[ci].ctype = RES_HTML;
    conn[ci].cdisp[0] = EOS;
    conn[ci].modified = 0;
    conn[ci].cookie_out_a[0] = EOS;
    conn[ci].cookie_out_a_exp[0] = EOS;
    conn[ci].cookie_out_l[0] = EOS;
    conn[ci].cookie_out_l_exp[0] = EOS;
    conn[ci].location[0] = EOS;
    conn[ci].bot = FALSE;
    conn[ci].expect100 = FALSE;
    conn[ci].dont_cache = FALSE;
}


/* --------------------------------------------------------------------------
  parse HTTP request
  return HTTP status code
-------------------------------------------------------------------------- */
static int parse_req(int ci, long len)
{
    int     ret=200;
    long    hlen;
    char    *p_hend=NULL;
    long    i;
    long    j=0;
    char    flg_data=FALSE;
    char    now_label=TRUE;
    char    now_value=FALSE;
    char    was_cr=FALSE;
    char    label[MAX_LABEL_LEN+1];
    char    value[MAX_VALUE_LEN+1];
    char    *p_question=NULL;

    /* --------------------------------------------

    Shortest valid request:

    GET / HTTP/1.1      15 including \n +
    Host: 1.1.1.1       14 including \n = 29

    -------------------------------------------- */

    DBG("parse_req, ci=%d", ci);

    ++G_cnts_today.req;
    conn[ci].req = G_cnts_today.req;    /* superfluous? */

    DBG("\n------------------------------------------------\n %s  Request %lu\n------------------------------------------------\n", G_dt, conn[ci].req);

//  if ( conn[ci].conn_state != STATE_SENDING ) /* ignore Range requests for now */
//      conn[ci].conn_state = STATE_RECEIVED;   /* by default */

    if ( len < 14 ) /* ignore any junk */
    {
        DBG("request len < 14, ignoring");
        return 400; /* Bad Request */
    }

    /* look for end of header */

    p_hend = strstr(conn[ci].in, "\r\n\r\n");

    if ( !p_hend )
    {
        p_hend = strstr(conn[ci].in, "\n\n");

        if ( !p_hend )
        {
            if ( 0 == strncmp(conn[ci].in, "GET / HTTP/1.", 13) )   /* temporary solution for good looking partial requests */
            {
                strcat(conn[ci].in, "\n");  /* for values reading algorithm */
                p_hend = conn[ci].in + len;
            }
            else
            {
                DBG("Request syntax error, ignoring");
                return 400; /* Bad Request */
            }
        }
    }

    hlen = p_hend - conn[ci].in;    /* HTTP header length including first of the last new line characters to simplify parsing algorithm in the third 'for' loop below */

    /* temporarily insert EOS at the end of header to avoid logging POST data */

    char eoh = conn[ci].in[hlen];
    conn[ci].in[hlen] = EOS;
    DBG("Incoming buffer:\n\n[%s]\n", conn[ci].in);
    conn[ci].in[hlen] = eoh;

    ++hlen;     /* HTTP header length including first of the last new line characters to simplify parsing algorithm in the third 'for' loop below */

    /* parse the header -------------------------------------------------------------------------- */

    for ( i=0; i<hlen; ++i )    /* the first line is special -- consists of more than one token */
    {                                   /* the very first token is a request method */
        if ( isalpha(conn[ci].in[i]) )
        {
            if ( i < MAX_METHOD_LEN )
                conn[ci].method[i] = conn[ci].in[i];
            else
            {
                ERR("Method too long, ignoring");
                return 400; /* Bad Request */
            }
        }
        else    /* most likely space = end of method */
        {
            conn[ci].method[i] = EOS;

            /* check against the list of allowed methods */

            if ( 0==strcmp(conn[ci].method, "GET") )
            {
                /* just go ahead */
            }
            else if ( 0==strcmp(conn[ci].method, "POST") || 0==strcmp(conn[ci].method, "PUT") || 0==strcmp(conn[ci].method, "DELETE") )
            {
                conn[ci].post = TRUE;   /* read payload */
            }
            else if ( 0==strcmp(conn[ci].method, "OPTIONS") )
            {
                /* just go ahead */
            }
            else if ( 0==strcmp(conn[ci].method, "HEAD") )
            {
                conn[ci].head_only = TRUE;  /* send only a header */
            }
            else
            {
                ERR("Method [%s] not allowed, ignoring", conn[ci].method);
                return 405;
            }

            break;
        }
    }

    /* only for low-level tests ------------------------------------- */
//  DBG("method: [%s]", conn[ci].method);
    /* -------------------------------------------------------------- */

    i += 2;     /* skip " /" */

    for ( i; i<hlen; ++i )  /* URI */
    {
        if ( conn[ci].in[i] != ' ' && conn[ci].in[i] != '\t' )
        {
            if ( j < MAX_URI_LEN )
                conn[ci].uri[j++] = conn[ci].in[i];
            else
            {
                ERR("URI too long, ignoring");
                return 414; /* Request-URI Too Long */
            }
        }
        else    /* end of URI */
        {
            conn[ci].uri[j] = EOS;
            break;
        }
    }

    /* only for low-level tests ------------------------------------- */
//  DBG("URI: [%s]", conn[ci].uri);
    /* -------------------------------------------------------------- */

    while ( i < hlen && conn[ci].in[i] != '\n' ) ++i;   /* go to the next line */
    j = 0;

    for ( i; i<hlen; ++i )  /* next lines */
    {
        if ( !now_value && (conn[ci].in[i] == ' ' || conn[ci].in[i] == '\t') )  /* omit whitespaces */
            continue;

        if ( conn[ci].in[i] == '\n' && was_cr )
        {
            was_cr = FALSE;
            continue;   /* value has already been saved in a previous loop go */
        }

        if ( conn[ci].in[i] == '\r' )
            was_cr = TRUE;

        if ( conn[ci].in[i] == '\r' || conn[ci].in[i] == '\n' ) /* end of value. Caution: \n only if continue above is in place! */
        {
            if ( now_value )
            {
                value[j] = EOS;
                if ( j == 0 )
                    WAR("Value of %s is empty!", label);
                else
                    if ( (ret=set_http_req_val(ci, label, value+1)) != 200 ) return ret;
            }
            now_label = TRUE;
            now_value = FALSE;
            j = 0;
        }
        else if ( now_label && conn[ci].in[i] == ':' )  /* end of label, start of value */
        {
            label[j] = EOS;
            now_label = FALSE;
            now_value = TRUE;
            j = 0;
        }
        else if ( now_label )   /* label */
        {
            if ( j < MAX_LABEL_LEN )
                label[j++] = conn[ci].in[i];
            else
            {
                label[j] = EOS;
                WAR("Label [%s] too long, ignoring", label);
                return 400; /* Bad Request */
            }
        }
        else if ( now_value )   /* value */
        {
            value[j++] = conn[ci].in[i];
            if ( j == MAX_VALUE_LEN )   /* truncate here */
            {
                WAR("Truncating %s's value", label);
                value[j] = EOS;
                if ( (ret=set_http_req_val(ci, label, value+1)) != 200 ) return ret;
                now_value = FALSE;
            }
        }
    }

    /* split URI and resource / id ---------------------------------------------- */

    if ( conn[ci].uri[0] )  /* if not empty */
    {
        if ( (0==strcmp(conn[ci].uri, "favicon.ico") && !M_favicon_exists)
                || (0==strcmp(conn[ci].uri, "robots.txt") && !M_robots_exists)
                || (0==strcmp(conn[ci].uri, "apple-touch-icon.png") && !M_appleicon_exists) )
            return 404;     /* Not Found */

        strncpy(conn[ci].resource, conn[ci].uri, MAX_RESOURCE_LEN);
        conn[ci].resource[MAX_RESOURCE_LEN] = EOS;

        if ( p_question=strchr(conn[ci].resource, '/') )    /* there's an id part of URI */
        {
            conn[ci].resource[p_question-conn[ci].resource] = EOS;

            strncpy(conn[ci].id, ++p_question, MAX_ID_LEN);
            conn[ci].id[MAX_ID_LEN] = EOS;

            if ( p_question=strchr(conn[ci].id, '?') )
                conn[ci].id[p_question-conn[ci].id] = EOS;
        }
        else if ( p_question=strchr(conn[ci].resource, '?') )   /* no id but query string may be present */
        {
            conn[ci].resource[p_question-conn[ci].resource] = EOS;
        }

        DBG("resource: [%s]", conn[ci].resource);
        DBG("id: [%s]", conn[ci].id);

        conn[ci].static_res = is_static_res(ci, conn[ci].resource);     /* statics --> set the flag!!! */
        /* now, it may have set conn[ci].status to 304 */
    }

    /* get the required authorization level for this resource */

    if ( conn[ci].static_res == NOT_STATIC )
    {
        i = 0;
        while ( M_auth_levels[i].resource[0] != '-' )
        {
            if ( REQ(M_auth_levels[i].resource) )
            {
                conn[ci].auth_level = M_auth_levels[i].level;
                break;
            }
            ++i;
        }
    }
    else    /* don't do any checks for static resources */
    {
        conn[ci].auth_level = AUTH_LEVEL_NONE;
    }

    /* ignore Range requests for now -------------------------------------------- */

/*  if ( conn[ci].conn_state == STATE_SENDING )
    {
        DBG("conn_state == STATE_SENDING, this request will be ignored");
        return 200;
    } */

    DBG("bot = %s", conn[ci].bot?"TRUE":"FALSE");

    /* update request counters -------------------------------------------------- */

    if ( conn[ci].bot )
        ++G_cnts_today.req_bot;

    if ( conn[ci].mobile )
        ++G_cnts_today.req_mob;
    else
        ++G_cnts_today.req_dsk;

    /* Block IP? ---------------------------------------------------------------- */

#ifdef BLACKLISTAUTOUPDATE
        if ( check_block_ip(ci, "Resource", conn[ci].resource) )
            return 403;     /* Forbidden */
#endif

#ifdef DOMAINONLY
        if ( !G_test && 0!=strcmp(conn[ci].host, APP_DOMAIN) )
            return 301;     /* Moved permanently */
#endif

    /* handle the POST content -------------------------------------------------- */

    if ( conn[ci].post && conn[ci].clen > 0 )
    {
        /* i = number of request characters read so far */

        /* p_hend will now point to the content */

        if ( 0==strncmp(p_hend, "\r\n\r\n", 4) )
            p_hend += 4;
        else    /* was "\n\n" */
            p_hend += 2;

        len = conn[ci].in+len - p_hend;         /* remaining request length -- likely a content */

        if ( len > conn[ci].clen )
            return 400;     /* Bad Request */

        /* copy so far received POST data from conn[ci].in to conn[ci].data */

        if ( NULL == (conn[ci].data=(char*)malloc(conn[ci].clen+1)) )
        {
            ERR("Couldn't allocate %d bytes for POST data!!!", conn[ci].clen);
            return 500;     /* Internal Sever Error */
        }

        memcpy(conn[ci].data, p_hend, len);
        conn[ci].was_read = len;    /* if POST then was_read applies to data section only! */

        if ( len < conn[ci].clen )      /* the whole content not received yet */
        {                               /* this is the only case when conn_state != received */
            DBG("The whole content not received yet");
            conn[ci].conn_state = CONN_STATE_READING_DATA;
            return ret;
        }
        else    /* the whole content received with the header at once */
        {
            conn[ci].data[len] = EOS;
            DBG("POST data received with header");
        }
    }

    if ( conn[ci].status == 304 )   /* Not Modified */
        return 304;
    else
        return ret;
}


/* --------------------------------------------------------------------------
  set request properties read from HTTP request header
  caller is responsible for ensuring value length > 0
  return HTTP status code
-------------------------------------------------------------------------- */
static int set_http_req_val(int ci, const char *label, const char *value)
{
    int     len;
    char    new_value[MAX_VALUE_LEN+1];
    char    ulabel[MAX_LABEL_LEN+1];
    char    uvalue[MAX_VALUE_LEN+1];
    char    *p;
    int     i;

    /* only for low-level tests ------------------------------------- */
//  DBG("label: [%s], value: [%s]", label, value);
    /* -------------------------------------------------------------- */

    strcpy(ulabel, upper(label));

    if ( 0==strcmp(ulabel, "HOST") )
    {
#ifdef BLACKLISTAUTOUPDATE
        if ( check_block_ip(ci, "Host", value) )
            return 403;     /* Forbidden */
#endif
        strcpy(conn[ci].host, value);
    }
    else if ( 0==strcmp(ulabel, "USER-AGENT") )
    {
#ifdef BLACKLISTAUTOUPDATE
        if ( check_block_ip(ci, "User-Agent", value) )
            return 403;     /* Forbidden */
#endif
        strcpy(conn[ci].uagent, value);
        strcpy(uvalue, upper(value));
        if ( strstr(uvalue, "ANDROID") || strstr(uvalue, "IPHONE") || strstr(uvalue, "SYMBIAN") || strstr(uvalue, "BLACKBERRY") || strstr(uvalue, "MOBILE") )
        {
            conn[ci].mobile = TRUE;
        }

        DBG("mobile = %s", conn[ci].mobile?"TRUE":"FALSE");

/*      if ( !conn[ci].bot &&
                (strstr(uvalue, "ADSBOT")
                || strstr(uvalue, "BAIDU")
                || strstr(uvalue, "UPTIMEBOT")
                || strstr(uvalue, "SEMRUSHBOT")
                || strstr(uvalue, "SEZNAMBOT")
                || strstr(uvalue, "SCANBOT")
                || strstr(uvalue, "SYSSCAN")
                || strstr(uvalue, "DOMAINSONO")
                || strstr(uvalue, "SURDOTLY")
                || strstr(uvalue, "DOTBOT")
                || strstr(uvalue, "ALPHABOT")
                || strstr(uvalue, "AHREFSBOT")
                || strstr(uvalue, "CRAWLER")
                || 0==strncmp(uvalue, "MASSCAN", 7)
                || 0==strncmp(uvalue, "CURL", 4)
                || 0==strncmp(uvalue, "CCBOT", 5)
                || 0==strcmp(uvalue, "TELESPHOREO")
                || 0==strcmp(uvalue, "MAGIC BROWSER")) )
        {
            conn[ci].bot = TRUE;
        } */

        if ( !conn[ci].bot &&
                (strstr(uvalue, "BOT")
                || strstr(uvalue, "SCAN")
                || strstr(uvalue, "CRAWLER")
                || strstr(uvalue, "DOMAINSONO")
                || strstr(uvalue, "SURDOTLY")
                || strstr(uvalue, "BAIDU")
                || 0==strncmp(uvalue, "CURL", 4)
                || 0==strcmp(uvalue, "TELESPHOREO")
                || 0==strcmp(uvalue, "MAGIC BROWSER")) )
        {
            conn[ci].bot = TRUE;
        }
    }
    else if ( 0==strcmp(ulabel, "CONNECTION") )
    {
        strcpy(uvalue, upper(value));
        if ( 0==strcmp(uvalue, "KEEP-ALIVE") )
            conn[ci].keep_alive = TRUE;
    }
    else if ( 0==strcmp(ulabel, "COOKIE") )
    {
        if ( strlen(value) < SESID_LEN+3 ) return 200;  /* no valid cookie but request still OK */

        /* parse cookies, set anonymous and / or logged in sesid */

        if ( NULL != (p=(char*)strstr(value, "as=")) )  /* anonymous sesid present? */
        {
            p += 3;
            if ( strlen(p) >= SESID_LEN )
            {
                strncpy(conn[ci].cookie_in_a, p, SESID_LEN);
                conn[ci].cookie_in_a[SESID_LEN] = EOS;
            }
        }
        if ( NULL != (p=(char*)strstr(value, "ls=")) )  /* logged in sesid present? */
        {
            p += 3;
            if ( strlen(p) >= SESID_LEN )
            {
                strncpy(conn[ci].cookie_in_l, p, SESID_LEN);
                conn[ci].cookie_in_l[SESID_LEN] = EOS;
            }
        }
    }
    else if ( 0==strcmp(ulabel, "REFERER") )
    {
        strcpy(conn[ci].referer, value);
//      if ( !conn[ci].uri[0] && value[0] )
//          INF("Referer: [%s]", value);
    }
    else if ( 0==strcmp(ulabel, "X-FORWARDED-FOR") )    /* keep first IP as client IP */
    {
        len = strlen(value);
        i = 0;

        while ( i<len && (value[i]=='.' || isdigit(value[i])) && i<INET_ADDRSTRLEN )
        {
            conn[ci].ip[i] = value[i];
            ++i;
        }

        conn[ci].ip[i] = EOS;

        DBG("%s's value: [%s]", label, conn[ci].ip);
    }
    else if ( 0==strcmp(ulabel, "CONTENT-LENGTH") )
    {
        conn[ci].clen = atol(value);
        if ( conn[ci].clen < 0 || (!conn[ci].post && conn[ci].clen >= IN_BUFSIZE) || (conn[ci].post && conn[ci].clen >= MAX_POST_DATA_BUFSIZE) )
        {
            ERR("Request too long, clen = %ld, sending 413", conn[ci].clen);
            return 413;
        }
        DBG("conn[ci].clen = %ld", conn[ci].clen);
    }
    else if ( 0==strcmp(ulabel, "ACCEPT-LANGUAGE") )    /* en-US en-GB pl-PL */
    {
        i = 0;
        while ( value[i] != EOS && value[i] != ',' && value[i] != ';' && i < 7 )
        {
            conn[ci].lang[i] = value[i];
            ++i;
        }

        conn[ci].lang[i] = EOS;

        DBG("conn[ci].lang: [%s]", conn[ci].lang);
    }
    else if ( 0==strcmp(ulabel, "CONTENT-TYPE") )
    {
        len = strlen(value);
        if ( len > 18 && 0==strncmp(value, "multipart/form-data", 19) )
        {
            conn[ci].in_ctype = CONTENT_TYPE_MULTIPART;
//          DBG("%s's value: [%s]", label, value);
            if ( p=(char*)strstr(value, "boundary=") )
            {
                strcpy(conn[ci].boundary, p+9);
                DBG("boundary: [%s]", conn[ci].boundary);
            }
        }
    }
    else if ( 0==strcmp(ulabel, "FROM") )
    {
        strcpy(uvalue, upper(value));
        if ( !conn[ci].bot && (strstr(uvalue, "GOOGLEBOT") || strstr(uvalue, "BINGBOT") || strstr(uvalue, "YANDEX") || strstr(uvalue, "CRAWLER")) )
            conn[ci].bot = TRUE;
    }
    else if ( 0==strcmp(ulabel, "IF-MODIFIED-SINCE") )
    {
        conn[ci].if_mod_since = time_http2epoch(value);
    }
    else if ( !conn[ci].secure && !G_test && 0==strcmp(ulabel, "UPGRADE-INSECURE-REQUESTS") && 0==strcmp(value, "1") )
    {
        DBG("Client wants to upgrade to HTTPS");
        conn[ci].upgrade2https = TRUE;
    }
    else if ( 0==strcmp(ulabel, "EXPECT") )
    {
        if ( 0==strcmp(value, "100-continue") )
            conn[ci].expect100 = TRUE;
    }

    return 200;
}


/* --------------------------------------------------------------------------
   Check the rules and block IP if matches
   Return TRUE if blocked
-------------------------------------------------------------------------- */
static bool check_block_ip(int ci, const char *rule, const char *value)
{
    if ( G_test ) return FALSE;     // don't block for tests

    if ( (rule[0]=='H' && conn[ci].post && 0==strcmp(value, APP_IP))        /* Host */
            || (rule[0]=='U' && 0==strcmp(value, "Mozilla/5.0 Jorgee"))     /* User-Agent */
            || (rule[0]=='R' && 0==strcmp(value, "wp-login.php"))           /* Resource */
            || (rule[0]=='R' && 0==strcmp(value, "administrator"))          /* Resource */
            || (rule[0]=='R' && 0==strcmp(value, "phpmyadmin"))             /* Resource */
            || (rule[0]=='R' && strstr(value, "setup.php")) )               /* Resource */
    {
        eng_block_ip(conn[ci].ip, TRUE);
        conn[ci].keep_alive = FALSE;    /* disconnect */
        return TRUE;
    }

    return FALSE;
}


/* --------------------------------------------------------------------------
  return HTTP status description
-------------------------------------------------------------------------- */
static char *get_http_descr(int status_code)
{
    int i;

    for ( i=0; M_http_status[i].status != -1; ++i )
    {
        if ( M_http_status[i].status == status_code )  /* found */
            return (char*)&(M_http_status[i].description);
    }

    return NULL;
}


/* --------------------------------------------------------------------------
   Dump counters
-------------------------------------------------------------------------- */
static void dump_counters()
{
    ALWAYS("");
    ALWAYS("Counters:\n");
    ALWAYS("       req: %ld", G_cnts_today.req);
    ALWAYS("   req_dsk: %ld", G_cnts_today.req_dsk);
    ALWAYS("   req_mob: %ld", G_cnts_today.req_mob);
    ALWAYS("   req_bot: %ld", G_cnts_today.req_bot);
    ALWAYS("    visits: %ld", G_cnts_today.visits);
    ALWAYS("visits_dsk: %ld", G_cnts_today.visits_dsk);
    ALWAYS("visits_mob: %ld", G_cnts_today.visits_mob);
    ALWAYS("   blocked: %ld", G_cnts_today.blocked);
    ALWAYS("");
}


/* --------------------------------------------------------------------------
   Clean up
-------------------------------------------------------------------------- */
static void clean_up()
{
    char    command[128]="";

    if ( G_log )
    {
        ALWAYS("");
        log_write_time(LOG_ALWAYS, "Cleaning up...\n");
        lib_log_memory();
        dump_counters();
    }

    app_done();

    if ( access(M_pidfile, F_OK) != -1 )
    {
        if (G_log) DBG("Removing pid file...");
        sprintf(command, "rm %s", M_pidfile);
        system(command);
    }

#ifdef DBMYSQL
    if ( G_dbconn )
        mysql_close(G_dbconn);
#endif
#ifdef HTTPS
    SSL_CTX_free(M_ssl_ctx);
    EVP_cleanup();
#endif
#ifdef ASYNC
    if (G_queue_req)
    {
        mq_close(G_queue_req);
        mq_unlink(ASYNC_REQ_QUEUE);
    }
    if (G_queue_res)
    {
        mq_close(G_queue_res);
        mq_unlink(ASYNC_RES_QUEUE);
    }
#endif

    log_finish();
}


/* --------------------------------------------------------------------------
   Signal response
-------------------------------------------------------------------------- */
static void sigdisp(int sig)
{
    DBG("Exiting due to receiving signal: %d", sig);
    clean_up();
    exit(1);
}


/* --------------------------------------------------------------------------
  generic message page
-------------------------------------------------------------------------- */
static void gen_page_msg(int ci, int msg)
{
    char    str[1024];

    DBG("gen_page_msg");

    if ( app_gen_page_msg(ci, msg) ) return;    /* if custom message page has been generated */

    eng_get_msg_str(ci, str, msg);

    OUT("<!DOCTYPE html>");
    OUT("<html>");
    OUT("<head><title>Web Server Message</title></head>");
    OUT("<body>%s</body>", str);
    OUT("</html>");
}


/* --------------------------------------------------------------------------
  init SSL
-------------------------------------------------------------------------- */
static bool init_ssl()
{
#ifdef HTTPS
#ifdef __linux__
    const SSL_METHOD    *method;
#else
    SSL_METHOD  *method;
#endif
    /*
       From Hynek Schlawack's blog:
       https://hynek.me/articles/hardening-your-web-servers-ssl-ciphers
       https://www.ssllabs.com/ssltest
    */
    char                ciphers[256]="ECDH+AESGCM:DH+AESGCM:ECDH+AES256:DH+AES256:ECDH+AES128:DH+AES:RSA+AESGCM:RSA+AES:!aNULL:!MD5:!DSS";

    DBG("init_ssl");

    SSL_library_init();
    OpenSSL_add_all_algorithms();       /* load & register all cryptos, etc. */
    SSL_load_error_strings();           /* load all error messages */

    method = SSLv23_server_method();    /* negotiate the highest protocol version supported by both the server and the client */
//  method = TLS_server_method();       /* negotiate the highest protocol version supported by both the server and the client */
//  method = TLSv1_2_server_method();   /* TLS v.1.2 only */

    M_ssl_ctx = SSL_CTX_new(method);    /* create new context from method */

    if ( M_ssl_ctx == NULL )
    {
        ERR("SSL_CTX_new failed");
        return FALSE;
    }

    /* support ECDH using the most appropriate shared curve */

//  if ( SSL_CTX_set_ecdh_auto(M_ssl_ctx, 1) <= 0 )     /* undefined reference?? */
/*  {
        ERR("SSL_CTX_set_ecdh_auto failed");
        return FALSE;
    } */

    const long flags = SSL_OP_ALL | SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3;
    SSL_CTX_set_options(M_ssl_ctx, flags);

    if ( G_cipherList[0] )
        strcpy(ciphers, G_cipherList);

    ALWAYS("        Using ciphers: [%s]", ciphers);

    SSL_CTX_set_cipher_list(M_ssl_ctx, ciphers);

    /* set the local certificate */

    ALWAYS("    Using certificate: [%s]", G_certFile);

    if ( SSL_CTX_use_certificate_file(M_ssl_ctx, G_certFile, SSL_FILETYPE_PEM) <= 0 )
    {
        ERR("SSL_CTX_use_certificate_file failed");
        return FALSE;
    }

    if ( G_certChainFile[0] )   /* set the chain file */
    {
        ALWAYS("Using cert chain file: [%s]", G_certChainFile);

        if ( SSL_CTX_load_verify_locations(M_ssl_ctx, G_certChainFile, NULL) <= 0 )
        {
            ERR("SSL_CTX_load_verify_locations failed");
            return FALSE;
        }
    }

   /* set the private key from KeyFile (may be the same as CertFile) */

    ALWAYS("    Using private key: [%s]", G_keyFile);

    if ( SSL_CTX_use_PrivateKey_file(M_ssl_ctx, G_keyFile, SSL_FILETYPE_PEM) <= 0 )
    {
        ERR("SSL_CTX_use_PrivateKey_file failed");
        return FALSE;
    }

    /* verify private key */

    if ( !SSL_CTX_check_private_key(M_ssl_ctx) )
    {
        ERR("Private key does not match the public certificate");
        return FALSE;
    }
#endif
    return TRUE;
}





/* ============================================================================================================= */
/* PUBLIC ENGINE FUNCTIONS (callbacks)                                                                           */
/* ============================================================================================================= */


/* --------------------------------------------------------------------------
   Set global parameters read from conf file
   lib_read_conf() callback
-------------------------------------------------------------------------- */
void eng_set_param(const char *label, const char *value)
{
    if ( PARAM("logLevel") )
        G_logLevel = atoi(value);
    else if ( PARAM("httpPort") )
        G_httpPort = atoi(value);
    else if ( PARAM("httpsPort") )
        G_httpsPort = atoi(value);
    else if ( PARAM("cipherList") )
        strcpy(G_cipherList, value);
    else if ( PARAM("certFile") )
        strcpy(G_certFile, value);
    else if ( PARAM("certChainFile") )
        strcpy(G_certChainFile, value);
    else if ( PARAM("keyFile") )
        strcpy(G_keyFile, value);
//  else if ( PARAM("dbHost") )
//      strcpy(G_dbHost, value);
//  else if ( PARAM("dbPort") )
//      G_dbPort = atoi(value);
    else if ( PARAM("dbName") )
        strcpy(G_dbName, value);
    else if ( PARAM("dbUser") )
        strcpy(G_dbUser, value);
    else if ( PARAM("dbPassword") )
        strcpy(G_dbPassword, value);
    else if ( PARAM("blockedIPList") )
        strcpy(G_blockedIPList, value);
    else if ( PARAM("test") )
        G_test = atoi(value);
}


/* --------------------------------------------------------------------------
   Set required authorization level for the resource
-------------------------------------------------------------------------- */
void eng_set_auth_level(const char *resource, char level)
{
static int current=0;

    if ( current > MAX_RESOURCES-2 )
        return;

    strcpy(M_auth_levels[current].resource, resource);
    M_auth_levels[current].level = level;

    strcpy(M_auth_levels[++current].resource, "-");
}


/* --------------------------------------------------------------------------
   Start new anonymous user session
-------------------------------------------------------------------------- */
bool eng_uses_start(int ci)
{
    int     i;
    char    sesid[SESID_LEN+1];

    DBG("eng_uses_start");

    if ( G_sessions == MAX_SESSIONS )
    {
        WAR("User sessions exhausted");
        return FALSE;
    }

    ++G_sessions;   /* start from 1 */

    /* find first free slot */

    for ( i=1; i<=MAX_SESSIONS; ++i )
    {
        if ( uses[i].sesid[0] == EOS )
        {
            conn[ci].usi = i;
            break;
        }
    }

    /* generate sesid */

    get_random_str(sesid, SESID_LEN);

    INF("Starting new session, usi=%d, sesid [%s]", conn[ci].usi, sesid);

    /* add record to uses */

    strcpy(US.sesid, sesid);
    strcpy(US.ip, conn[ci].ip);
    strcpy(US.uagent, conn[ci].uagent);
    strcpy(US.referer, conn[ci].referer);
    strcpy(US.lang, conn[ci].lang);

    lib_set_datetime_formats(US.lang);

    /* custom session init */

    app_uses_init(ci);

    /* set 'as' cookie */

    strcpy(conn[ci].cookie_out_a, sesid);

    DBG("%d user session(s)", G_sessions);

    return TRUE;
}


/* --------------------------------------------------------------------------
   Close user session
-------------------------------------------------------------------------- */
void eng_uses_close(int usi)
{
    eng_uses_reset(usi);
    app_uses_reset(usi);

    G_sessions--;

    DBG("%d session(s) remaining", G_sessions);
}


/* --------------------------------------------------------------------------
   Reset user session
-------------------------------------------------------------------------- */
void eng_uses_reset(int usi)
{
    uses[usi].logged = FALSE;
    uses[usi].uid = 0;
    uses[usi].login[0] = EOS;
    uses[usi].email[0] = EOS;
    uses[usi].name[0] = EOS;
    uses[usi].login_tmp[0] = EOS;
    uses[usi].email_tmp[0] = EOS;
    uses[usi].name_tmp[0] = EOS;
    uses[usi].sesid[0] = EOS;
    uses[usi].ip[0] = EOS;
    uses[usi].uagent[0] = EOS;
    uses[usi].referer[0] = EOS;
    uses[usi].lang[0] = EOS;
    uses[usi].additional[0] = EOS;
}


/* --------------------------------------------------------------------------
   Send asynchronous request
-------------------------------------------------------------------------- */
void eng_async_req(int ci, const char *service, const char *data, char response, int timeout)
{
#ifdef ASYNC

    async_req_t req;

    if ( G_last_call_id > 10000000 ) G_last_call_id = 0;

    req.call_id = ++G_last_call_id;
    req.ci = ci;
    if ( service ) strcpy(req.service, service);
    req.response = response;
    if ( data ) strcpy(req.data, data);

    DBG("Sending a message on behalf of ci=%d, call_id=%ld, service [%s]", ci, req.call_id, req.service);

    mq_send(G_queue_req, (char*)&req, ASYNC_REQ_MSG_SIZE, 0);

    if ( response )     /* we will wait */
    {
        /* add to ares (async response array) */

        int j;

        for ( j=0; j<MAX_ASYNC; ++j )
        {
            if ( ares[j].state == ASYNC_STATE_FREE )        /* free slot */
            {
                DBG("free slot %d found in ares", j);
                ares[j].call_id = req.call_id;
                ares[j].ci = ci;
                strcpy(ares[j].service, service);
                ares[j].state = ASYNC_STATE_SENT;
                ares[j].sent = G_now;
                if ( timeout < 0 ) timeout = 0;
                if ( timeout == 0 || timeout > ASYNC_MAX_TIMEOUT ) timeout = ASYNC_MAX_TIMEOUT;
                ares[j].timeout = timeout;
                break;
            }
        }

        /* set request state */

        conn[ci].conn_state = CONN_STATE_WAITING_FOR_ASYNC;
    }
#endif
}


/* --------------------------------------------------------------------------
   Set internal (generated) static resource data & size
-------------------------------------------------------------------------- */
void eng_add_to_static_res(const char *name, char *data)
{
    int i;

    i = first_free_stat();

    strcpy(M_stat[i].name, name);
    M_stat[i].data = data;
    M_stat[i].len = strlen(data);   /* internal are text based */
    M_stat[i].type = get_res_type(M_stat[i].name);
    M_stat[i].modified = G_now;

    ALWAYS("%s (%ld Bytes)", M_stat[i].name, M_stat[i].len);

    strcpy(M_stat[++i].name, "-");
}


/* --------------------------------------------------------------------------
   Add to blocked IP
-------------------------------------------------------------------------- */
void eng_block_ip(const char *value, bool autoblocked)
{
    char    file[256];
    char    comm[512];

    if ( G_blacklist_cnt > MAX_BLACKLIST-1 )
    {
        WAR("G_blacklist_cnt at max (%d)!", MAX_BLACKLIST);
        return;
    }

    if ( ip_blocked(value) )
    {
        DBG("%s already blocked", value);
        return;
    }

    strcpy(G_blacklist[G_blacklist_cnt++], value);

    if ( G_blockedIPList[0] == '/' )    /* full path */
        strcpy(file, G_blockedIPList);
    else    /* just a file name */
        sprintf(file, "%s/bin/%s", G_appdir, G_blockedIPList);

    sprintf(comm, "echo \"%s\t# %sblocked on %s\" >> %s", value, autoblocked?"auto":"", G_dt, file);
    system(comm);

    WAR("IP %s blacklisted", value);
}


/* --------------------------------------------------------------------------
  Get error description for user.

  There are 3 groups of messages:
  < 0               -- server errors
  > 0 && < 1000     -- user library errors/messages
  >= 1000           -- app errors/messages
-------------------------------------------------------------------------- */
void eng_get_msg_str(int ci, char *dest, int errcode)
{
    if ( errcode == OK )
        strcpy(dest, "OK");
    else if ( errcode == ERR_INT_SERVER_ERROR )
//      strcpy(dest, "Apologies, this is our fault. This service is still under intense development and the problem will probably be solved in a few hours.");
        strcpy(dest, "Apologies, this is our fault. Please try again later.");
    else if ( errcode == ERR_SERVER_TOOBUSY )
        strcpy(dest, "Apologies, we are experiencing very high demand right now, please try again in a few minutes.");
    else if ( errcode == ERR_INVALID_REQUEST )
        strcpy(dest, "Invalid HTTP request");
    else if ( errcode == ERR_NOT_FOUND )
        strcpy(dest, "The page you're trying to access does not exist here.");
    else if ( errcode == ERR_UNAUTHORIZED )
        strcpy(dest, "I'm sorry but you don't have permission to see this.");
    else if ( errcode == ERR_FILE_TOO_BIG )
        strcpy(dest, "I'm sorry but your file is too big.");
#ifdef USERS
    else if ( errcode < 1000 )
        libusr_get_msg_str(ci, dest, errcode);
#endif
    else
        app_get_msg_str(ci, dest, errcode);
}


/* --------------------------------------------------------------------------
   Return true if host matches
-------------------------------------------------------------------------- */
bool eng_host(int ci, const char *host)
{
    char uhost[64];
    char conn_uhost[64];

    strcpy(uhost, upper(host));
    strcpy(conn_uhost, upper(conn[ci].host));

    return (0==strcmp(conn_uhost, uhost));
}


/* --------------------------------------------------------------------------
   Set response status
-------------------------------------------------------------------------- */
void eng_set_res_status(int ci, int status)
{
    conn[ci].status = status;
}


/* --------------------------------------------------------------------------
   Set response content type
-------------------------------------------------------------------------- */
void eng_set_res_content_type(int ci, const char *str)
{
    conn[ci].ctype = CONTENT_TYPE_USER;
    strcpy(conn[ci].ctypestr, str);
}


/* --------------------------------------------------------------------------
   Set location
-------------------------------------------------------------------------- */
void eng_set_res_location(int ci, const char *str, ...)
{
    va_list     plist;

    va_start(plist, str);
    vsprintf(conn[ci].location, str, plist);
    va_end(plist);
}


/* --------------------------------------------------------------------------
   Set response content disposition
-------------------------------------------------------------------------- */
void eng_set_res_content_disposition(int ci, const char *str, ...)
{
    va_list     plist;

    va_start(plist, str);
    vsprintf(conn[ci].cdisp, str, plist);
    va_end(plist);
}


/* --------------------------------------------------------------------------
   Write string to output buffer with buffer overwrite protection
-------------------------------------------------------------------------- */
void eng_out_check(int ci, const char *str)
{
    int available = OUT_BUFSIZE - (conn[ci].p_curr_c - conn[ci].out_data);

    if ( strlen(str) < available )  /* the whole string will fit */
    {
        conn[ci].p_curr_c = stpcpy(conn[ci].p_curr_c, str);
    }
    else    /* let's write only what we can. WARNING: no UTF-8 checking is done here! */
    {
        conn[ci].p_curr_c = stpncpy(conn[ci].p_curr_c, str, available-1);
        *conn[ci].p_curr_c = EOS;
    }
}


/* --------------------------------------------------------------------------
   Write string to output buffer with buffer resizing if necessary
-------------------------------------------------------------------------- */
void eng_out_check_realloc(int ci, const char *str)
{
    if ( strlen(str) < conn[ci].out_data_allocated - (conn[ci].p_curr_c-conn[ci].out_data) )    /* the whole string will fit */
    {
        conn[ci].p_curr_c = stpcpy(conn[ci].p_curr_c, str);
    }
    else    /* resize output buffer and try again */
    {
        long used = conn[ci].p_curr_c - conn[ci].out_data;
        char *tmp = (char*)realloc(conn[ci].out_data, conn[ci].out_data_allocated*2);
        if ( !tmp )
        {
            ERR("Couldn't reallocate output buffer for ci=%d, tried %ld bytes", ci, conn[ci].out_data_allocated*2);
            return;
        }
        conn[ci].out_data = tmp;
        conn[ci].out_data_allocated = conn[ci].out_data_allocated * 2;
        conn[ci].p_curr_c = conn[ci].out_data + used;
        INF("Reallocated output buffer for ci=%d, new size = %ld bytes", ci, conn[ci].out_data_allocated);
        eng_out_check_realloc(ci, str);     /* call itself! */
    }
}


/* --------------------------------------------------------------------------
   Write binary data to output buffer with buffer resizing if necessary
-------------------------------------------------------------------------- */
void eng_out_check_realloc_bin(int ci, const char *data, long len)
{
    if ( len < conn[ci].out_data_allocated - (conn[ci].p_curr_c-conn[ci].out_data) )    /* the whole data will fit */
    {
        memcpy(conn[ci].p_curr_c, data, len);
        conn[ci].p_curr_c += len;
    }
    else    /* resize output buffer and try again */
    {
        long used = conn[ci].p_curr_c - conn[ci].out_data;
        char *tmp = (char*)realloc(conn[ci].out_data, conn[ci].out_data_allocated*2);
        if ( !tmp )
        {
            ERR("Couldn't reallocate output buffer for ci=%d, tried %ld bytes", ci, conn[ci].out_data_allocated*2);
            return;
        }
        conn[ci].out_data = tmp;
        conn[ci].out_data_allocated = conn[ci].out_data_allocated * 2;
        conn[ci].p_curr_c = conn[ci].out_data + used;
        INF("Reallocated output buffer for ci=%d, new size = %ld bytes", ci, conn[ci].out_data_allocated);
        eng_out_check_realloc_bin(ci, data, len);       /* call itself! */
    }
}
