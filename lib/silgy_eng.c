/* --------------------------------------------------------------------------
   Silgy Web App Engine
   Jurek Muszynski
   silgy.com
   Started: August 2015
-----------------------------------------------------------------------------
   Main web engine module
-------------------------------------------------------------------------- */


#include <silgy.h>


#ifndef SILGY_SVC

#ifdef FD_MON_POLL
#ifndef _WIN32
#include <poll.h>
#endif
#endif  /* FD_MON_POLL */

#ifndef _WIN32
#include <zlib.h>
#endif


/* globals */

/* read from the config file */
int         G_httpPort=80;
int         G_httpsPort=443;
char        G_cipherList[1024]="";
char        G_certFile[256]="";
char        G_certChainFile[256]="";
char        G_keyFile[256]="";
char        G_dbHost[128]="";
int         G_dbPort=0;
char        G_dbName[128]="";
char        G_dbUser[128]="";
char        G_dbPassword[128]="";
int         G_usersRequireAccountActivation=0;
char        G_blockedIPList[256]="";
int         G_ASYNCId=0;
int         G_ASYNCDefTimeout=ASYNC_DEF_TIMEOUT;
/* end of config params */
unsigned    G_days_up=0;                /* web server's days up */
conn_t      conn[MAX_CONNECTIONS+1]={0}; /* HTTP connections & requests -- by far the most important structure around */
int         G_open_conn=0;              /* number of open connections */
int         G_open_conn_hwm=0;          /* highest number of open connections (high water mark) */
usession_t  uses[MAX_SESSIONS+1]={0};   /* user sessions -- they start from 1 */
ausession_t auses[MAX_SESSIONS+1]={0};  /* app user sessions, using the same index (usi) */
int         G_sessions=0;               /* number of active user sessions */
int         G_sessions_hwm=0;           /* highest number of active user sessions (high water mark) */
char        G_last_modified[32]="";     /* response header field with server's start time */

#ifdef DBMYSQL
MYSQL       *G_dbconn=NULL;             /* database connection */
#endif

/* asynchorous processing */
#ifndef _WIN32
char        G_req_queue_name[256]="";
char        G_res_queue_name[256]="";
mqd_t       G_queue_req={0};            /* request queue */
mqd_t       G_queue_res={0};            /* response queue */
#endif  /* _WIN32 */
int         G_async_req_data_size=ASYNC_REQ_MSG_SIZE-sizeof(async_req_hdr_t); /* how many bytes are left for data */
int         G_async_res_data_size=ASYNC_RES_MSG_SIZE-sizeof(async_res_hdr_t)-sizeof(int)*4; /* how many bytes are left for data */

bool        G_index_present=FALSE;      /* index.html present in res? */

char        G_blacklist[MAX_BLACKLIST+1][INET_ADDRSTRLEN];
int         G_blacklist_cnt=0;          /* G_blacklist length */
/* counters */
counters_t  G_cnts_today={0};           /* today's counters */
counters_t  G_cnts_yesterday={0};       /* yesterday's counters */
counters_t  G_cnts_day_before={0};      /* day before's counters */


/* locals */

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


/* authorization levels */

static struct {
    char    resource[MAX_RESOURCE_LEN+1];
    short   level;
    } M_auth_levels[MAX_RESOURCES] = {
        {"-", EOS}
    };

static char     *M_pidfile;                 /* pid file name */

#ifdef _WIN32   /* Windows */
static SOCKET   M_listening_fd=0;           /* The socket file descriptor for "listening" socket */
static SOCKET   M_listening_sec_fd=0;       /* The socket file descriptor for secure "listening" socket */
#else
static int      M_listening_fd=0;           /* The socket file descriptor for "listening" socket */
static int      M_listening_sec_fd=0;       /* The socket file descriptor for secure "listening" socket */
#endif  /* _WIN32 */

#ifdef HTTPS
static SSL_CTX  *M_ssl_ctx;
#endif

#ifdef HTTPS
#define LISTENING_FDS    2
#else
#define LISTENING_FDS    1
#endif

#ifdef FD_MON_SELECT
static fd_set       M_readfds={0};              /* Socket file descriptors we want to wake up for, using select() */
static fd_set       M_writefds={0};             /* Socket file descriptors we want to wake up for, using select() */
static int          M_highsock=0;               /* Highest #'d file descriptor, needed for select() */
#endif  /* FD_MON_SELECT */

#ifdef FD_MON_POLL

static struct pollfd M_pollfds[MAX_CONNECTIONS+LISTENING_FDS]={0};
static int          M_pollfds_cnt=0;
static int          M_poll_ci[MAX_CONNECTIONS+LISTENING_FDS]={0};

#endif  /* FD_MON_POLL */

static stat_res_t   M_stat[MAX_STATICS];        /* static resources */
static char         M_resp_date[32];            /* response header Date */
static char         M_expires_stat[32];         /* response header for static resources */
static char         M_expires_gen[32];          /* response header for generated resources */
static int          M_max_static=-1;            /* highest static resource M_stat index */
static bool         M_favicon_exists=FALSE;     /* special case statics */
static bool         M_robots_exists=FALSE;      /* -''- */
static bool         M_appleicon_exists=FALSE;   /* -''- */

#ifdef _WIN32   /* Windows */
WSADATA             wsa;
#endif

static bool         M_shutdown=FALSE;
static int          M_prev_minute;
static int          M_prev_day;
static time_t       M_last_housekeeping=0;

#ifdef ASYNC
static areq_t       areqs[MAX_ASYNC_REQS]={0};  /* async requests */
static unsigned     M_last_call_id=0;           /* counter */
static char         *M_async_shm=NULL;
#endif  /* ASYNC */


/* prototypes */

static bool housekeeping(void);
static void set_state(int ci, int bytes);
static void set_state_sec(int ci, int bytes);
static void read_conf(void);
static void respond_to_expect(int ci);
static void log_proc_time(int ci);
static void log_request(int ci);
static void close_conn(int ci);
static bool init(int argc, char **argv);
static void build_fd_sets(void);
static void accept_http();
static void accept_https();
static void read_blocked_ips(void);
static bool ip_blocked(const char *addr);
static int  first_free_stat(void);
static bool read_files(bool minify, bool first_scan, const char *path);
static int  is_static_res(int ci, const char *name);
static void process_req(int ci);
static void gen_response_header(int ci);
static void print_content_type(int ci, char type);
static bool a_usession_ok(int ci);
static void close_old_conn(void);
static void uses_close_timeouted(void);
static void close_uses(int usi, int ci);
static void reset_conn(int ci, char new_state);
static int  parse_req(int ci, int len);
static int  set_http_req_val(int ci, const char *label, const char *value);
static bool check_block_ip(int ci, const char *rule, const char *value);
static char *get_http_descr(int status_code);
static void dump_counters(void);
static void clean_up(void);
static void sigdisp(int sig);
static void gen_page_msg(int ci, int code);
static bool init_ssl(void);


/* --------------------------------------------------------------------------
   main
-------------------------------------------------------------------------- */
int main(int argc, char **argv)
{
    struct sockaddr_in serv_addr;
    unsigned    hit=0;
    int         reuse_addr=1;       /* Used so we can re-bind to our port while a previous connection is still in TIME_WAIT state */
    struct timeval timeout;         /* Timeout for select */
    int         sockets_ready;      /* Number of sockets ready for I/O */
    int         i=0;
    int         bytes=0;
    int         failed_select_cnt=0;
    int         j=0;
#ifdef DUMP
    time_t      dbg_last_time0=0;
    time_t      dbg_last_time1=0;
    time_t      dbg_last_time2=0;
    time_t      dbg_last_time3=0;
    time_t      dbg_last_time4=0;
    time_t      dbg_last_time5=0;
    time_t      dbg_last_time6=0;
    time_t      dbg_last_time7=0;
    time_t      dbg_last_time8=0;
    time_t      dbg_last_time9=0;
#endif  /* DUMP */

    if ( !init(argc, argv) )
    {
        ERR("init() failed, exiting");
        clean_up();
        return EXIT_FAILURE;
    }

    /* setup the network socket */

    DBG("Trying socket...");

#ifdef _WIN32   /* Windows */

    DBG("Initializing Winsock...");

    if ( WSAStartup(MAKEWORD(2,2), &wsa) != 0 )
    {
        ERR("WSAStartup failed. Error Code = %d", WSAGetLastError());
        clean_up();
        return EXIT_FAILURE;
    }

#endif  /* _WIN32 */

    if ( (M_listening_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0 )
    {
        ERR("socket failed, errno = %d (%s)", errno, strerror(errno));
        clean_up();
        return EXIT_FAILURE;
    }

    DBG("M_listening_fd = %d", M_listening_fd);

    /* So that we can re-bind to it without TIME_WAIT problems */
#ifdef _WIN32   /* Windows */
    setsockopt(M_listening_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse_addr, sizeof(reuse_addr));
#else
    setsockopt(M_listening_fd, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr));
#endif 

    /* Set socket to non-blocking */

    lib_setnonblocking(M_listening_fd);

    /* bind socket to a port */

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(G_httpPort);

    DBG("Trying bind to port %d...", G_httpPort);

    if ( bind(M_listening_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0 )
    {
        ERR("bind failed, errno = %d (%s)", errno, strerror(errno));
        clean_up();
        return EXIT_FAILURE;
    }

    /* listen to a port */

    DBG("Trying listen...\n");

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
#ifdef _WIN32   /* Windows */
    setsockopt(M_listening_sec_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse_addr, sizeof(reuse_addr));
#else
    setsockopt(M_listening_sec_fd, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr));
#endif

    /* Set socket to non-blocking */

    lib_setnonblocking(M_listening_sec_fd);

    /* bind socket to a port */

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(G_httpsPort);

    DBG("Trying bind to port %d...", G_httpsPort);

    if ( bind(M_listening_sec_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0 )
    {
        ERR("bind failed, errno = %d (%s)", errno, strerror(errno));
        clean_up();
        return EXIT_FAILURE;
    }

    /* listen to a port */

    DBG("Trying listen...\n");

    if ( listen(M_listening_sec_fd, SOMAXCONN) < 0 )
    {
        ERR("listen failed, errno = %d (%s)", errno, strerror(errno));
        clean_up();
        return EXIT_FAILURE;
    }

#endif  /* HTTPS */

//    addr_len = sizeof(cli_addr);

    /* log currently used memory */

    lib_log_memory();

    ALWAYS("\nWaiting for requests...\n");

    log_flush();

    M_prev_minute = G_ptm->tm_min;
    M_prev_day = G_ptm->tm_mday;

    /* main server loop ------------------------------------------------------------------------- */

#ifdef FD_MON_POLL

    M_pollfds[0].fd = M_listening_fd;
    M_pollfds[0].events = POLLIN;
    M_pollfds_cnt = 1;

#ifdef HTTPS
    M_pollfds[1].fd = M_listening_sec_fd;
    M_pollfds[1].events = POLLIN;
    M_pollfds_cnt = 2;
#endif

    int pi;     /* poll loop index */

#endif  /* FD_MON_POLL */

//  for ( ; hit<1000; ++hit )   /* test only */
    for ( ;; )
    {
        lib_update_time_globals();

#ifdef _WIN32   /* Windows */
        strftime(M_resp_date, 32, "%a, %d %b %Y %H:%M:%S GMT", G_ptm);
#else
        strftime(M_resp_date, 32, "%a, %d %b %Y %T GMT", G_ptm);
#endif  /* _WIN32 */

#ifdef ASYNC
        /* release timeout-ed */

        for ( j=0; j<MAX_ASYNC_REQS; ++j )
        {
            if ( areqs[j].state==ASYNC_STATE_SENT && areqs[j].sent < G_now-areqs[j].timeout )
            {
                DBG("Async request %d timeout-ed", j);
                conn[areqs[j].ci].async_err_code = ERR_ASYNC_TIMEOUT;
                conn[areqs[j].ci].status = 500;
                areqs[j].state = ASYNC_STATE_FREE;
                gen_response_header(areqs[j].ci);
            }
        }
#endif

        /* use your favourite fd monitoring */

#ifdef FD_MON_SELECT
        build_fd_sets();

        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        sockets_ready = select(M_highsock+1, &M_readfds, &M_writefds, NULL, &timeout);
#endif  /* FD_MON_SELECT */

#ifdef FD_MON_POLL
        sockets_ready = poll(M_pollfds, M_pollfds_cnt, 1000);
#endif  /* FD_MON_POLL */

#ifdef FD_MON_EPOLL
        ALWAYS("epoll not implemented yet!");
        clean_up();
        return EXIT_FAILURE;
#endif  /* FD_MON_EPOLL */

#ifdef _WIN32
        if ( M_shutdown ) break;
#endif
        if ( sockets_ready < 0 )
        {
            ERR_T("select failed, errno = %d (%s)", errno, strerror(errno));
            /* protect from infinite loop */
            if ( failed_select_cnt >= 10 )
            {
                ERR("select failed for the 10-th time, entering emergency reset");
                ALWAYS("Resetting all connections...");
                int k;
                for ( k=0; k<MAX_CONNECTIONS; ++k )
                    close_conn(k);
                failed_select_cnt = 0;
                ALWAYS("Waiting for 1 second...");
#ifdef _WIN32
                Sleep(1000);
#else
                sleep(1);
#endif
                continue;
            }
            else
            {
                ++failed_select_cnt;
                continue;
            }
        }
        else if ( sockets_ready == 0 )
        {
            /* we have some time now, let's do some housekeeping */
#ifdef DUMP
//            DBG("sockets_ready == 0");
#endif
            if ( !housekeeping() )
                return EXIT_FAILURE;
        }
        else    /* sockets_ready > 0 */
        {
#ifdef DUMP
            if ( G_now != dbg_last_time0 )   /* only once in a second */
            {
                DBG("    connected = %d", G_open_conn);
#ifdef FD_MON_POLL
                DBG("M_pollfds_cnt = %d", M_pollfds_cnt);
#endif
                DBG("sockets_ready = %d", sockets_ready);
                DBG("");
                dbg_last_time0 = G_now;
            }
#endif  /* DUMP */
#ifdef FD_MON_SELECT
            if ( FD_ISSET(M_listening_fd, &M_readfds) )   /* new http connection */
            {
#endif  /* FD_MON_SELECT */
#ifdef FD_MON_POLL
            if ( M_pollfds[0].revents & POLLIN )
            {
                M_pollfds[0].revents = 0;
#endif  /* FD_MON_POLL */
                accept_http();
                sockets_ready--;
            }
#ifdef HTTPS
#ifdef FD_MON_SELECT
            else if ( FD_ISSET(M_listening_sec_fd, &M_readfds) )   /* new https connection */
            {
#endif  /* FD_MON_SELECT */
#ifdef FD_MON_POLL
            else if ( M_pollfds[1].revents & POLLIN )
            {
                M_pollfds[1].revents = 0;
#endif  /* FD_MON_POLL */
                accept_https();
                sockets_ready--;
            }
#endif  /* HTTPS */
            else    /* existing connections have something going on on them ---------------------------------- */
            {
#ifdef FD_MON_SELECT
                for ( i=0; sockets_ready>0 && i<MAX_CONNECTIONS; ++i )
                {
#endif  /* FD_MON_SELECT */
#ifdef FD_MON_POLL
                for ( pi=LISTENING_FDS; sockets_ready>0 && pi<M_pollfds_cnt; ++pi )
                {
                    i = M_poll_ci[pi];   /* set conn array index */
#ifdef DUMP
                    if ( G_now != dbg_last_time1 )   /* only once in a second */
                    {
                        int l;
                        for ( l=0; l<M_pollfds_cnt; ++l )
                            DBG("ci=%d, pi=%d, M_pollfds[pi].revents = %d", M_poll_ci[l], l, M_pollfds[l].revents);
                        dbg_last_time1 = G_now;
                    }
#endif  /* DUMP */
#endif  /* FD_MON_POLL */
#ifdef FD_MON_SELECT
                    if ( FD_ISSET(conn[i].fd, &M_readfds) )     /* incoming data ready */
                    {
#endif  /* FD_MON_SELECT */
#ifdef FD_MON_POLL
                    if ( M_pollfds[pi].revents & POLLIN )
                    {
                        M_pollfds[pi].revents = 0;
#endif  /* FD_MON_POLL */
#ifdef DUMP
                        if ( G_now != dbg_last_time2 )   /* only once in a second */
                        {
                            DBG("ci=%d, fd=%d has incoming data, conn_state = %c", i, conn[i].fd, conn[i].conn_state);
                            dbg_last_time2 = G_now;
                        }
#endif  /* DUMP */
#ifdef HTTPS
                        if ( conn[i].secure )   /* HTTPS */
                        {
                            if ( conn[i].conn_state != CONN_STATE_READING_DATA )
                            {
#ifdef DUMP
                                DBG("Trying SSL_read from fd=%d (ci=%d)", conn[i].fd, i);
#endif
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
#ifdef DUMP
                                DBG("ci=%d, state == CONN_STATE_READING_DATA", i);
                                DBG("Trying SSL_read %u bytes of POST data from fd=%d (ci=%d)", conn[i].clen-conn[i].was_read, conn[i].fd, i);
#endif  /* DUMP */
                                bytes = SSL_read(conn[i].ssl, conn[i].in_data+conn[i].was_read, conn[i].clen-conn[i].was_read);

                                if ( bytes > 0 )
                                    conn[i].was_read += bytes;

                                set_state_sec(i, bytes);
                            }
                        }
                        else        /* HTTP */
#endif  /* HTTPS */
                        {
                            if ( conn[i].conn_state == CONN_STATE_CONNECTED )
                            {
#ifdef DUMP
                                DBG("ci=%d, state == CONN_STATE_CONNECTED", i);
                                DBG("Trying read from fd=%d (ci=%d)", conn[i].fd, i);
#endif  /* DUMP */
                                bytes = recv(conn[i].fd, conn[i].in, IN_BUFSIZE-1, 0);

                                if ( bytes > 0 )
                                    conn[i].in[bytes] = EOS;

                                set_state(i, bytes);    /* possibly:    CONN_STATE_DISCONNECTED (if error or closed by peer) */
                                                        /*              CONN_STATE_READY_FOR_PARSE */
                            }
                            else if ( conn[i].conn_state == CONN_STATE_READING_DATA )   /* POST */
                            {
#ifdef DUMP
                                DBG("ci=%d, state == CONN_STATE_READING_DATA", i);
                                DBG("Trying to read %u bytes of POST data from fd=%d (ci=%d)", conn[i].clen-conn[i].was_read, conn[i].fd, i);
#endif  /* DUMP */
                                bytes = recv(conn[i].fd, conn[i].in_data+conn[i].was_read, conn[i].clen-conn[i].was_read, 0);

                                if ( bytes > 0 )
                                    conn[i].was_read += bytes;

                                set_state(i, bytes);    /* possibly:    CONN_STATE_DISCONNECTED (if error or closed by peer) */
                                                        /*              CONN_STATE_READY_FOR_PROCESS */
                            }
                        }

                        sockets_ready--;
                    }
                    /* --------------------------------------------------------------------------------------- */
#ifdef FD_MON_SELECT
                    else if ( FD_ISSET(conn[i].fd, &M_writefds) )        /* ready for outgoing data */
                    {
#endif  /* FD_MON_SELECT */
#ifdef FD_MON_POLL
                    else if ( M_pollfds[pi].revents & POLLOUT )
                    {
                        M_pollfds[pi].revents = 0;
#endif  /* FD_MON_POLL */
#ifdef DUMP
                        if ( G_now != dbg_last_time3 )   /* only once in a second */
                        {
                            DBG("ci=%d, fd=%d is ready for outgoing data, conn_state = %c", i, conn[i].fd, conn[i].conn_state);
                            dbg_last_time3 = G_now;
                        }
#endif  /* DUMP */

#ifdef HTTPS
                        if ( conn[i].secure )   /* HTTPS */
                        {
                            if ( conn[i].conn_state == CONN_STATE_READY_TO_SEND_HEADER )
                            {
#ifdef DUMP
                                DBG("ci=%d, state == CONN_STATE_READY_TO_SEND_HEADER", i);
                                DBG("Trying SSL_write %u bytes to fd=%d (ci=%d)", conn[i].out_hlen, conn[i].fd, i);
#endif  /* DUMP */
#ifdef SEND_ALL_AT_ONCE
                                bytes = SSL_write(conn[i].ssl, conn[i].out_start, conn[i].out_len);
#ifdef DUMP
                                DBG("ci=%d, changing state to CONN_STATE_READY_TO_SEND_BODY", i);
#endif
                                conn[i].conn_state = CONN_STATE_READY_TO_SEND_BODY;
#else
                                bytes = SSL_write(conn[i].ssl, conn[i].out_header, conn[i].out_hlen);
#endif  /* SEND_ALL_AT_ONCE */
                                set_state_sec(i, bytes);
                            }
                            else if ( conn[i].conn_state == CONN_STATE_READY_TO_SEND_BODY || conn[i].conn_state == CONN_STATE_SENDING_BODY)
                            {
#ifdef DUMP
                                DBG("ci=%d, state == %s", i, conn[i].conn_state==CONN_STATE_READY_TO_SEND_BODY?"CONN_STATE_READY_TO_SEND_BODY":"CONN_STATE_SENDING_BODY");
#ifdef SEND_ALL_AT_ONCE
                                DBG("Trying SSL_write %u bytes to fd=%d (ci=%d)", conn[i].out_len, conn[i].fd, i);
#else
                                DBG("Trying SSL_write %u bytes to fd=%d (ci=%d)", conn[i].clen, conn[i].fd, i);
#endif
#endif  /* DUMP */
#ifdef SEND_ALL_AT_ONCE
                                bytes = SSL_write(conn[i].ssl, conn[i].out_start, conn[i].out_len);
#else
                                bytes = SSL_write(conn[i].ssl, conn[i].out_data, conn[i].clen);
#endif
                                set_state_sec(i, bytes);
                            }
                        }
                        else    /* HTTP */
#endif  /* HTTPS */
                        {
                            if ( conn[i].conn_state == CONN_STATE_READY_TO_SEND_HEADER )
                            {
#ifdef DUMP
                                DBG("ci=%d, state == CONN_STATE_READY_TO_SEND_HEADER", i);
#ifdef SEND_ALL_AT_ONCE
                                DBG("Trying to write %u bytes to fd=%d (ci=%d)", conn[i].out_len, conn[i].fd, i);
#else
                                DBG("Trying to write %u bytes to fd=%d (ci=%d)", conn[i].out_hlen, conn[i].fd, i);
#endif
#endif  /* DUMP */
#ifdef SEND_ALL_AT_ONCE
                                bytes = send(conn[i].fd, conn[i].out_start, conn[i].out_len, 0);
#ifdef DUMP
                                DBG("ci=%d, changing state to CONN_STATE_READY_TO_SEND_BODY", i);
#endif
                                conn[i].conn_state = CONN_STATE_READY_TO_SEND_BODY;
#else
                                bytes = send(conn[i].fd, conn[i].out_header, conn[i].out_hlen, 0);
#endif  /* SEND_ALL_AT_ONCE */
                                set_state(i, bytes);    /* possibly:    CONN_STATE_DISCONNECTED (if error or closed by peer) */
                                                        /*              CONN_STATE_READY_TO_SEND_BODY */
                            }
                            else if ( conn[i].conn_state == CONN_STATE_READY_TO_SEND_BODY || conn[i].conn_state == CONN_STATE_SENDING_BODY)
                            {
#ifdef DUMP
                                DBG("ci=%d, state == %s", i, conn[i].conn_state==CONN_STATE_READY_TO_SEND_BODY?"CONN_STATE_READY_TO_SEND_BODY":"CONN_STATE_SENDING_BODY");
#ifdef SEND_ALL_AT_ONCE
                                DBG("Trying to write %u bytes to fd=%d (ci=%d)", conn[i].out_len-conn[i].data_sent, conn[i].fd, i);
#else
                                DBG("Trying to write %u bytes to fd=%d (ci=%d)", conn[i].clen-conn[i].data_sent, conn[i].fd, i);
#endif
#endif  /* DUMP */
#ifdef SEND_ALL_AT_ONCE
                                bytes = send(conn[i].fd, conn[i].out_start+conn[i].data_sent, conn[i].out_len-conn[i].data_sent, 0);
#else
                                bytes = send(conn[i].fd, conn[i].out_data+conn[i].data_sent, conn[i].clen-conn[i].data_sent, 0);
#endif
                                set_state(i, bytes);    /* possibly:    CONN_STATE_DISCONNECTED (if error or closed by peer or !keep_alive) */
                                                        /*              CONN_STATE_SENDING_BODY (if data_sent < clen) */
                                                        /*              CONN_STATE_CONNECTED */
                            }
                        }

                        sockets_ready--;
                    }

                    /* --------------------------------------------------------------------------------------- */
                    /* after reading / writing it may be ready for parsing and processing ... */

                    if ( conn[i].conn_state == CONN_STATE_READY_FOR_PARSE )
                    {
                        conn[i].status = parse_req(i, bytes);
#ifdef HTTPS
#ifdef DOMAINONLY       /* redirect to final domain first */
                        if ( !conn[i].secure && conn[i].upgrade2https && 0!=strcmp(conn[i].host, APP_DOMAIN) )
                            conn[i].upgrade2https = FALSE;
#endif
#endif  /* HTTPS */
                        if ( conn[i].conn_state != CONN_STATE_READING_DATA )
                        {
#ifdef DUMP
                            DBG("ci=%d, changing state to CONN_STATE_READY_FOR_PROCESS", i);
#endif
                            conn[i].conn_state = CONN_STATE_READY_FOR_PROCESS;
                        }
                    }

                    /* received Expect: 100-continue before content */

                    if ( conn[i].expect100 )
                        respond_to_expect(i);

                    /* ready for processing */

                    if ( conn[i].conn_state == CONN_STATE_READY_FOR_PROCESS )
                    {
#ifdef _WIN32
                        clock_gettime_win(&conn[i].proc_start);
#else
                        clock_gettime(MONOTONIC_CLOCK_NAME, &conn[i].proc_start);
#endif
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

                        if ( conn[i].static_res == NOT_STATIC )   /* process request */
                            process_req(i);
#ifdef ASYNC
                        if ( conn[i].conn_state != CONN_STATE_WAITING_FOR_ASYNC )
#endif
                        gen_response_header(i);
                    }
                }
            }
        }

#ifdef DUMP
        if ( sockets_ready != 0 )
        {
            static time_t last_time=0;   /* prevent log overflow */

            if ( last_time != G_now )
            {
                DBG_T("sockets_ready should be 0 but currently %d", sockets_ready);
                last_time = G_now;
            }
        }
#endif  /* DUMP */

        /* async processing -- check on response queue */
#ifdef ASYNC
        async_res_t res;
#ifdef DUMP
        int mq_ret;
        if ( (mq_ret=mq_receive(G_queue_res, (char*)&res, ASYNC_RES_MSG_SIZE, NULL)) != -1 )    /* there's a response in the queue */
#else
        if ( mq_receive(G_queue_res, (char*)&res, ASYNC_RES_MSG_SIZE, NULL) != -1 )    /* there's a response in the queue */
#endif  /* DUMP */
        {
#ifdef DUMP
            DBG("res.chunk=%d", res.chunk);
            DBG("(unsigned short)res.chunk=%hd", (unsigned short)res.chunk);
#endif  /* DUMP */

            unsigned chunk_num = 0;
            chunk_num |= (unsigned short)res.chunk;

            DBG("ASYNC response received, chunk=%u", chunk_num);

            int  res_ai;
            int  res_ci;
            int  res_len;
            char *res_data;

            if ( ASYNC_CHUNK_IS_FIRST(res.chunk) )  /* get all the response's details */
            {
                DBG("res.ci=%d", res.ci);
                DBG("res.hdr.err_code = %d", res.hdr.err_code);
                DBG("res.hdr.status = %d", res.hdr.status);

                /* error code & status */

                conn[res.ci].async_err_code = res.hdr.err_code;
                conn[res.ci].status = res.hdr.status;

                /* update user session */

                memcpy(&uses[conn[res.ci].usi], &res.hdr.uses, sizeof(usession_t));
#ifndef ASYNC_EXCLUDE_AUSES
                memcpy(&auses[conn[res.ci].usi], &res.hdr.auses, sizeof(ausession_t));
#endif
                /* update connection details */

                conn[res.ci].ctype = res.hdr.ctype;
                strcpy(conn[res.ci].ctypestr, res.hdr.ctypestr);
                strcpy(conn[res.ci].cdisp, res.hdr.cdisp);
                strcpy(conn[res.ci].cookie_out_a, res.hdr.cookie_out_a);
                strcpy(conn[res.ci].cookie_out_a_exp, res.hdr.cookie_out_a_exp);
                strcpy(conn[res.ci].cookie_out_l, res.hdr.cookie_out_l);
                strcpy(conn[res.ci].cookie_out_l_exp, res.hdr.cookie_out_l_exp);
                strcpy(conn[res.ci].location, res.hdr.location);
                conn[res.ci].dont_cache = res.hdr.dont_cache;
                conn[res.ci].keep_content = res.hdr.keep_content;

                /* update REST stats */

                if ( res.hdr.rest_req > 0 )
                {
                    G_rest_status = res.hdr.rest_status;
                    G_rest_req += res.hdr.rest_req;
                    G_rest_elapsed += res.hdr.rest_elapsed;
                    G_rest_average = G_rest_elapsed / G_rest_req;
                }

                res_ai = res.ai;
                res_ci = res.ci;
                res_len = res.len;
                res_data = res.data;
            }
            else    /* 'data' chunk */
            {
                DBG("'data' chunk");

                async_res_data_t *resd = (async_res_data_t*)&res;

                res_ai = resd->ai;
                res_ci = resd->ci;
                res_len = resd->len;
                res_data = resd->data;
            }

            /* out data */

            if ( res_len > 0 )    /* chunk length */
            {
                DBG("res_len = %d", res_len);
#ifdef OUTCHECKREALLOC
                eng_out_check_realloc_bin(res_ci, res_data, res_len);
#else
                unsigned checked_len = res_len > OUT_BUFSIZE-OUT_HEADER_BUFSIZE ? OUT_BUFSIZE-OUT_HEADER_BUFSIZE : res_len;
                memcpy(conn[res_ci].p_content, res_data, checked_len);
                conn[res_ci].p_content += checked_len;
#endif
            }

            if ( ASYNC_CHUNK_IS_LAST(res.chunk) )
            {
                areqs[res_ai].state = ASYNC_STATE_FREE;
                gen_response_header(res_ci);
            }
        }
#ifdef DUMP
        else
        {
            static time_t last_time=0;   /* prevent log overflow */

            if ( last_time != G_now )
            {
                int wtf = errno;
                if ( wtf != EAGAIN )
                    ERR("mq_receive failed, errno = %d (%s)", wtf, strerror(wtf));
                last_time = G_now;
            }
        }
#endif  /* DUMP */

#endif  /* ASYNC */

        /* under heavy load there might never be that sockets_ready==0 */
        /* make sure it runs at least every 10 seconds */
#ifdef DUMP
//        DBG("M_last_housekeeping = %ld", M_last_housekeeping);
//        DBG("              G_now = %ld", G_now);
#endif
        if ( M_last_housekeeping < G_now-10 )
        {
            INF_T("M_last_housekeeping < G_now-10 ==> run housekeeping");
            if ( !housekeeping() )
                return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}


/* --------------------------------------------------------------------------
   Set values for Expires response headers
-------------------------------------------------------------------------- */
static void set_expiry_dates()
{
    time_t sometimeahead;

    sometimeahead = G_now + 3600*24*EXPIRES_STATICS;
    G_ptm = gmtime(&sometimeahead);
#ifdef _WIN32   /* Windows */
    strftime(M_expires_stat, 32, "%a, %d %b %Y %H:%M:%S GMT", G_ptm);
#else
    strftime(M_expires_stat, 32, "%a, %d %b %Y %T GMT", G_ptm);
#endif  /* _WIN32 */
    DBG("New M_expires_stat: %s", M_expires_stat);

    sometimeahead = G_now + 3600*24*EXPIRES_GENERATED;
    G_ptm = gmtime(&sometimeahead);
#ifdef _WIN32   /* Windows */
    strftime(M_expires_gen, 32, "%a, %d %b %Y %H:%M:%S GMT", G_ptm);
#else
    strftime(M_expires_gen, 32, "%a, %d %b %Y %T GMT", G_ptm);
#endif  /* _WIN32 */
    DBG("New M_expires_gen: %s", M_expires_gen);

    G_ptm = gmtime(&G_now);   /* reset to today */
}


/* --------------------------------------------------------------------------
   Close expired sessions etc...
-------------------------------------------------------------------------- */
static bool housekeeping()
{
#ifdef DUMP
//    DBG("housekeeping");
#endif

    /* close expired connections */
    if ( G_open_conn ) close_old_conn();

    /* close expired anonymous user sessions */
    if ( G_sessions ) uses_close_timeouted();

#ifdef DUMP
#ifndef DONT_RESCAN_RES
    if ( G_test )   /* kind of developer mode */
    {
        read_files(FALSE, FALSE, NULL);
        read_files(TRUE, FALSE, NULL);
    }
#endif  /* DONT_RESCAN_RES */
#endif  /* DUMP */

    if ( G_ptm->tm_min != M_prev_minute )
    {
#ifdef DUMP
        DBG("Once a minute");
#endif
        /* close expired logged in user sessions */
#ifdef USERS
        if ( G_sessions ) libusr_luses_close_timeouted();
#endif
        /* say something sometimes ... */
        ALWAYS_T("%d open connection(s) | %d user session(s)", G_open_conn, G_sessions);

        log_flush();

#ifndef DONT_RESCAN_RES    /* refresh static resources */
        read_files(FALSE, FALSE, NULL);
        read_files(TRUE, FALSE, NULL);
#endif  /* DONT_RESCAN_RES */

        /* start new log file every day */

        if ( G_ptm->tm_mday != M_prev_day )
        {
#ifdef DUMP
            DBG("Once a day");
#endif
            dump_counters();
            log_finish();
            if ( !log_start("", G_test) )
            {
                clean_up();
                return FALSE;
            }

            set_expiry_dates();

            if ( G_blockedIPList[0] )
            {
                /* update blacklist */
                read_blocked_ips();
            }

            /* copy & reset counters */
            memcpy(&G_cnts_day_before, &G_cnts_yesterday, sizeof(counters_t));
            memcpy(&G_cnts_yesterday, &G_cnts_today, sizeof(counters_t));
            memset(&G_cnts_today, 0, sizeof(counters_t));
            G_rest_req = 0;
            G_rest_elapsed = 0;
            G_rest_average = 0;

            /* log currently used memory */
            lib_log_memory();
            ++G_days_up;

            init_random_numbers();

            M_prev_day = G_ptm->tm_mday;
        }

        M_prev_minute = G_ptm->tm_min;
    }

    M_last_housekeeping = G_now;

    return TRUE;
}


/* --------------------------------------------------------------------------
   Set new connection state after read or write
-------------------------------------------------------------------------- */
static void set_state(int ci, int bytes)
{
#ifdef DUMP
        DBG("set_state ci=%d, bytes=%d", ci, bytes);
#endif
    if ( bytes <= 0 )
    {
        DBG("bytes = %d, errno = %d (%s), disconnecting slot %d\n", bytes, errno, strerror(errno), ci);
        close_conn(ci);
        return;
    }

    /* bytes > 0 */

    if ( conn[ci].conn_state == CONN_STATE_CONNECTED )  /* assume the whole header has been read */
    {
#ifdef DUMP
        DBG("ci=%d, changing state to CONN_STATE_READY_FOR_PARSE", ci);
#endif
        conn[ci].conn_state = CONN_STATE_READY_FOR_PARSE;
    }
    else if ( conn[ci].conn_state == CONN_STATE_READING_DATA )  /* it could have been received only partially */
    {
        if ( conn[ci].was_read < conn[ci].clen )
        {
            DBG("ci=%d, was_read=%u, continue receiving", ci, conn[ci].was_read);
        }
        else    /* data received */
        {
            conn[ci].in_data[conn[ci].was_read] = EOS;

            DBG("POST data received");

            /* ready for processing */
#ifdef DUMP
            DBG("ci=%d, changing state to CONN_STATE_READY_FOR_PROCESS", ci);
#endif
            conn[ci].conn_state = CONN_STATE_READY_FOR_PROCESS;
        }
    }
    else if ( conn[ci].conn_state == CONN_STATE_READY_TO_SEND_HEADER )  /* assume the whole header has been sent successfuly */
    {
        if ( conn[ci].clen > 0 )
        {
#ifdef DUMP
            DBG("ci=%d, changing state to CONN_STATE_READY_TO_SEND_BODY", ci);
#endif
            conn[ci].conn_state = CONN_STATE_READY_TO_SEND_BODY;
        }
        else    /* no body to send */
        {
            DBG("clen = 0");
            log_request(ci);
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
        conn[ci].data_sent += bytes;

#ifdef SEND_ALL_AT_ONCE
        if ( bytes < conn[ci].out_len )
#else
        if ( bytes < conn[ci].clen )
#endif
        {
#ifdef DUMP
            DBG("ci=%d, changing state to CONN_STATE_SENDING_BODY", ci);
#endif
            conn[ci].conn_state = CONN_STATE_SENDING_BODY;
        }
        else /* assuming the whole body has been sent at once */
        {
            log_request(ci);

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
        conn[ci].data_sent += bytes;

#ifdef SEND_ALL_AT_ONCE
        if ( conn[ci].data_sent < conn[ci].out_len )
#else
        if ( conn[ci].data_sent < conn[ci].clen )
#endif
        {
            DBG("ci=%d, data_sent=%u, continue sending", ci, conn[ci].data_sent);
        }
        else    /* body sent */
        {
            log_request(ci);

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
static void set_state_sec(int ci, int bytes)
{
    int     e;
    char    ec[256]="";
#ifdef HTTPS
    e = errno;

#ifdef DUMP
    DBG("set_state_sec ci=%d, bytes=%d", ci, bytes);
#endif
    conn[ci].ssl_err = SSL_get_error(conn[ci].ssl, bytes);

    if ( bytes <= 0 )
    {
        if ( conn[ci].ssl_err == SSL_ERROR_SYSCALL )
            sprintf(ec, ", errno = %d (%s)", e, strerror(e));

        DBG("bytes = %d, ssl_err = %d%s", bytes, conn[ci].ssl_err, ec);

        if ( conn[ci].ssl_err != SSL_ERROR_WANT_READ && conn[ci].ssl_err != SSL_ERROR_WANT_WRITE )
        {
            DBG("Closing connection\n");
            close_conn(ci);
        }

#ifdef FD_MON_POLL
        if ( conn[ci].ssl_err == SSL_ERROR_WANT_READ )
            M_pollfds[conn[ci].pi].events = POLLIN;
        else if ( conn[ci].ssl_err == SSL_ERROR_WANT_WRITE )
            M_pollfds[conn[ci].pi].events = POLLOUT;
#endif
        return;
    }

    /* bytes > 0 */

    /* we have no way of knowing if accept finished before reading actual request */
    if ( conn[ci].conn_state == CONN_STATE_ACCEPTING || conn[ci].conn_state == CONN_STATE_CONNECTED )   /* assume the whole header has been read */
    {
#ifdef DUMP
        DBG("Changing state to CONN_STATE_READY_FOR_PARSE");
#endif
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
            conn[ci].in_data[conn[ci].was_read] = EOS;

            DBG("POST data received");

            /* ready for processing */
#ifdef DUMP
            DBG("Changing state to CONN_STATE_READY_FOR_PROCESS");
#endif
            conn[ci].conn_state = CONN_STATE_READY_FOR_PROCESS;
        }
    }
    else if ( conn[ci].conn_state == CONN_STATE_READY_TO_SEND_HEADER )
    {
        if ( conn[ci].clen > 0 )
        {
#ifdef DUMP
            DBG("Changing state to CONN_STATE_READY_TO_SEND_BODY");
#endif
            conn[ci].conn_state = CONN_STATE_READY_TO_SEND_BODY;
        }
        else    /* no body to send */
        {
            DBG("clen = 0");
            log_request(ci);
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
        conn[ci].data_sent += bytes;

        log_request(ci);

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
static void read_conf()
{
    bool    conf_read=FALSE;

    /* set defaults */

    G_logLevel = 3;
    G_logToStdout = 0;
    G_logCombined = 0;
    G_httpPort = 80;
    G_httpsPort = 443;
    G_certFile[0] = EOS;
    G_certChainFile[0] = EOS;
    G_keyFile[0] = EOS;
    G_dbHost[0] = EOS;
    G_dbPort = 0;
    G_dbName[0] = EOS;
    G_dbUser[0] = EOS;
    G_dbPassword[0] = EOS;
    G_usersRequireAccountActivation = 0;
    G_blockedIPList[0] = EOS;
    G_ASYNCId = -1;
    G_ASYNCDefTimeout = ASYNC_DEF_TIMEOUT;
    G_RESTTimeout = CALL_REST_DEFAULT_TIMEOUT;
    G_test = 0;

    /* get the conf file path & name */

    if ( G_appdir[0] )
    {
        char conf_path[1024];
        sprintf(conf_path, "%s/bin/silgy.conf", G_appdir);
        if ( !(conf_read=lib_read_conf(conf_path)) )   /* no config file there */
            conf_read = lib_read_conf("silgy.conf");
    }
    else    /* no SILGYDIR -- try current dir */
    {
        conf_read = lib_read_conf("silgy.conf");
    }
    
    if ( conf_read )
    {
        silgy_read_param_int("logLevel", &G_logLevel);
        silgy_read_param_int("logToStdout", &G_logToStdout);
        silgy_read_param_int("logCombined", &G_logCombined);
        silgy_read_param_int("httpPort", &G_httpPort);
        silgy_read_param_int("httpsPort", &G_httpsPort);
        silgy_read_param_str("certFile", G_certFile);
        silgy_read_param_str("certChainFile", G_certChainFile);
        silgy_read_param_str("keyFile", G_keyFile);
        silgy_read_param_str("dbHost", G_dbHost);
        silgy_read_param_int("dbPort", &G_dbPort);
        silgy_read_param_str("dbName", G_dbName);
        silgy_read_param_str("dbUser", G_dbUser);
        silgy_read_param_str("dbPassword", G_dbPassword);
        silgy_read_param_int("usersRequireAccountActivation", &G_usersRequireAccountActivation);
        silgy_read_param_str("blockedIPList", G_blockedIPList);
        silgy_read_param_int("ASYNCId", &G_ASYNCId);
        silgy_read_param_int("ASYNCDefTimeout", &G_ASYNCDefTimeout);
        silgy_read_param_int("RESTTimeout", &G_RESTTimeout);
        silgy_read_param_int("test", &G_test);
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

    if ( conn[ci].clen >= MAX_PAYLOAD_SIZE-1 )   /* refuse */
    {
        INF("Sending 413");
#ifdef HTTPS
        if ( conn[ci].secure )
            bytes = SSL_write(conn[ci].ssl, reply_refuse, 41);
        else
#endif
            bytes = send(conn[ci].fd, reply_refuse, 41, 0);

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
            bytes = send(conn[ci].fd, reply_accept, 25, 0);

        if ( bytes < 25 ) ERR("write error, bytes = %d", bytes);
    }

    conn[ci].expect100 = FALSE;
}


/* --------------------------------------------------------------------------
   Log processing time
-------------------------------------------------------------------------- */
static void log_proc_time(int ci)
{
    conn[ci].elapsed = lib_elapsed(&conn[ci].proc_start);

    DBG("Processing time: %.3lf ms [%s]\n", conn[ci].elapsed, conn[ci].resource);

    G_cnts_today.elapsed += conn[ci].elapsed;
    G_cnts_today.average = G_cnts_today.elapsed / G_cnts_today.req;
}


/* --------------------------------------------------------------------------
   Log processing time
-------------------------------------------------------------------------- */
static void log_request(int ci)
{
    /* Use (almost) Combined Log Format */

    char logtime[64];
    strftime(logtime, 64, "%d/%b/%Y:%H:%M:%S +0000", G_ptm);

    if ( G_logCombined )
        INF("%s - - [%s] \"%s /%s %s\" %d %u \"%s\" \"%s\"  #%u  %.3lf ms%s", conn[ci].ip, logtime, conn[ci].method, conn[ci].uri, conn[ci].proto, conn[ci].status, conn[ci].clen, conn[ci].referer, conn[ci].uagent, conn[ci].req, conn[ci].elapsed, REQ_BOT?"  [bot]":"");
    else
        INF("%s - - [%s] \"%s /%s %s\" %d %u  #%u  %.3lf ms%s", conn[ci].ip, logtime, conn[ci].method, conn[ci].uri, conn[ci].proto, conn[ci].status, conn[ci].clen, conn[ci].req, conn[ci].elapsed, REQ_BOT?"  [bot]":"");
}


/* --------------------------------------------------------------------------
   Close connection
-------------------------------------------------------------------------- */
static void close_conn(int ci)
{
#ifdef DUMP
    DBG("Closing connection ci=%d, fd=%d", ci, conn[ci].fd);
#endif

#ifdef HTTPS
    if ( conn[ci].ssl )
    {
        SSL_free(conn[ci].ssl);
        conn[ci].ssl = NULL;
    }
#endif

#ifdef FD_MON_POLL  /* remove from monitored set */

    M_pollfds_cnt--;

    if ( conn[ci].pi != M_pollfds_cnt )    /* move the last one to just freed spot */
    {
        memcpy(&M_pollfds[conn[ci].pi], &M_pollfds[M_pollfds_cnt], sizeof(struct pollfd));
        /* update cross-references */
        M_poll_ci[conn[ci].pi] = M_poll_ci[M_pollfds_cnt];
        conn[M_poll_ci[M_pollfds_cnt]].pi = conn[ci].pi;
    }

#endif  /* FD_MON_POLL */

#ifdef _WIN32   /* Windows */
    closesocket(conn[ci].fd);
#else
    close(conn[ci].fd);
#endif  /* _WIN32 */
    reset_conn(ci, CONN_STATE_DISCONNECTED);

    G_open_conn--;
}


/* --------------------------------------------------------------------------
   Engine init
   Return TRUE if success
-------------------------------------------------------------------------- */
static bool init(int argc, char **argv)
{
    int i=0;

    /* init globals */

    G_days_up = 0;
    G_open_conn = 0;
    G_open_conn_hwm = 0;
    G_sessions = 0;
    G_sessions_hwm = 0;
    G_index_present = FALSE;
#ifdef DBMYSQL
    G_dbconn = NULL;
#endif

    /* counters */

    memset(&G_cnts_today, 0, sizeof(counters_t));
    memset(&G_cnts_yesterday, 0, sizeof(counters_t));
    memset(&G_cnts_day_before, 0, sizeof(counters_t));

    /* init Silgy library */

    silgy_lib_init();

#ifdef USERS
    libusr_init();
#endif

    /* read the config file or set defaults */

    read_conf();

    /* command line arguments */

    if ( argc > 1 )
    {
        G_httpPort = atoi(argv[1]);
        printf("Will be listening on the port %d...\n", G_httpPort);
    }

    /* start log --------------------------------------------------------- */

    char exec_name[256];
    lib_get_exec_name(exec_name, argv[0]);

    if ( !log_start("", G_test) )
        return FALSE;

    ALWAYS("Starting program");
    ALWAYS("");

#ifdef __linux__
    INF("This is Linux");
    INF("");
#endif

#ifdef _WIN32
    INF("This is Windows");
    INF("");
#endif

    ALWAYS("G_appdir [%s]", G_appdir);
    ALWAYS("logLevel = %d", G_logLevel);
    ALWAYS("logToStdout = %d", G_logToStdout);
    ALWAYS("logCombined = %d", G_logCombined);
    if ( argc > 1 )
    {
        ALWAYS("--------------------------------------------------------------------");
        WAR("httpPort = %d -- overwritten by a command line argument", G_httpPort);
        ALWAYS("--------------------------------------------------------------------");
    }
    else
        ALWAYS("httpPort = %d", G_httpPort);
    ALWAYS("httpsPort = %d", G_httpsPort);
    ALWAYS("dbHost [%s]", G_dbHost);
    ALWAYS("dbPort = %d", G_dbPort);
    ALWAYS("dbName [%s]", G_dbName);
    ALWAYS("ASYNCDefTimeout = %d", G_ASYNCDefTimeout);
    ALWAYS("RESTTimeout = %d", G_RESTTimeout);
    ALWAYS("test = %d", G_test);

    /* pid file ---------------------------------------------------------- */

    if ( !(M_pidfile=lib_create_pid_file("silgy_app")) )
        return FALSE;

    /* empty static resources list */

    for ( i=0; i<MAX_STATICS; ++i )
        strcpy(M_stat[i].name, "-");

    /* check endianness and some parameters */

    get_byteorder();

    ALWAYS("");
    ALWAYS_LINE_LONG;
    ALWAYS("");
    ALWAYS("System:");
    ALWAYS("-------");
#ifdef DUMP
    // SIZE_MAX is not defined in older GCC!
//    ALWAYS("              SIZE_MAX = %lu (%lu kB / %lu MB)", SIZE_MAX, SIZE_MAX/1024, SIZE_MAX/1024/1024);
#endif
    ALWAYS("            FD_SETSIZE = %d", FD_SETSIZE);
    ALWAYS("             SOMAXCONN = %d", SOMAXCONN);
    ALWAYS("");
    ALWAYS("Server:");
    ALWAYS("-------");
    ALWAYS("              SILGYDIR = %s", G_appdir);
    ALWAYS("    WEB_SERVER_VERSION = %s", WEB_SERVER_VERSION);
#ifdef MEM_TINY
    ALWAYS("          Memory model = MEM_TINY");
#endif
#ifdef MEM_SMALL
    ALWAYS("          Memory model = MEM_SMALL");
#endif
#ifdef MEM_MEDIUM
    ALWAYS("          Memory model = MEM_MEDIUM");
#endif
#ifdef MEM_LARGE
    ALWAYS("          Memory model = MEM_LARGE");
#endif
#ifdef MEM_XLARGE
    ALWAYS("          Memory model = MEM_XLARGE");
#endif
#ifdef MEM_XXLARGE
    ALWAYS("          Memory model = MEM_XXLARGE");
#endif
#ifdef MEM_XXXLARGE
    ALWAYS("          Memory model = MEM_XXXLARGE");
#endif
    ALWAYS("       MAX_CONNECTIONS = %d", MAX_CONNECTIONS);
    ALWAYS("          MAX_SESSIONS = %d", MAX_SESSIONS);
    ALWAYS("");
#ifdef FD_MON_SELECT
    ALWAYS("         FD monitoring = FD_MON_SELECT");
#endif
#ifdef FD_MON_POLL
    ALWAYS("         FD monitoring = FD_MON_POLL");
#endif
#ifdef FD_MON_EPOLL
    ALWAYS("         FD monitoring = FD_MON_EPOLL");
#endif
    ALWAYS("");
#ifdef SEND_ALL_AT_ONCE
    ALWAYS("    Response send mode = SEND_ALL_AT_ONCE");
    ALWAYS("");
#endif
    ALWAYS("          CONN_TIMEOUT = %d seconds", CONN_TIMEOUT);
    ALWAYS("          USES_TIMEOUT = %d seconds", USES_TIMEOUT);
#ifdef USERS
    ALWAYS("         LUSES_TIMEOUT = %d seconds", LUSES_TIMEOUT);
#endif
    ALWAYS("");
    ALWAYS("           OUT_BUFSIZE = %lu B (%lu kB / %0.2lf MB)", OUT_BUFSIZE, OUT_BUFSIZE/1024, (double)OUT_BUFSIZE/1024/1024);
    ALWAYS("");
    ALWAYS("           conn's size = %lu B (%lu kB / %0.2lf MB)", sizeof(conn), sizeof(conn)/1024, (double)sizeof(conn)/1024/1024);
    ALWAYS("            uses' size = %lu B (%lu kB / %0.2lf MB)", sizeof(uses), sizeof(uses)/1024, (double)sizeof(uses)/1024/1024);
    ALWAYS("");
    ALWAYS("    ASYNC_REQ_MSG_SIZE = %d B", ASYNC_REQ_MSG_SIZE);
    ALWAYS("    ASYNC req.hdr size = %lu B (%lu kB / %0.2lf MB)", sizeof(async_req_hdr_t), sizeof(async_req_hdr_t)/1024, (double)sizeof(async_req_hdr_t)/1024/1024);
    ALWAYS(" G_async_req_data_size = %lu B (%lu kB / %0.2lf MB)", G_async_req_data_size, G_async_req_data_size/1024, (double)G_async_req_data_size/1024/1024);
    ALWAYS("    ASYNC req     size = %lu B (%lu kB / %0.2lf MB)", sizeof(async_req_t), sizeof(async_req_t)/1024, (double)sizeof(async_req_t)/1024/1024);
    ALWAYS("");
    ALWAYS("    ASYNC_RES_MSG_SIZE = %d B", ASYNC_RES_MSG_SIZE);
    ALWAYS("    ASYNC res.hdr size = %lu B (%lu kB / %0.2lf MB)", sizeof(async_res_hdr_t), sizeof(async_res_hdr_t)/1024, (double)sizeof(async_res_hdr_t)/1024/1024);
    ALWAYS(" G_async_res_data_size = %lu B (%lu kB / %0.2lf MB)", G_async_res_data_size, G_async_res_data_size/1024, (double)G_async_res_data_size/1024/1024);
    ALWAYS("    ASYNC res     size = %lu B (%lu kB / %0.2lf MB)", sizeof(async_res_t), sizeof(async_res_t)/1024, (double)sizeof(async_res_t)/1024/1024);
    ALWAYS("");
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
    ALWAYS("         APP_LOGIN_URI = %s", APP_LOGIN_URI);
    if ( DEF_RES_AUTH_LEVEL == AUTH_LEVEL_NONE )
        ALWAYS("    DEF_RES_AUTH_LEVEL = AUTH_LEVEL_NONE");
    else if ( DEF_RES_AUTH_LEVEL == AUTH_LEVEL_ANONYMOUS )
        ALWAYS("    DEF_RES_AUTH_LEVEL = AUTH_LEVEL_ANONYMOUS");
    else if ( DEF_RES_AUTH_LEVEL == AUTH_LEVEL_LOGGEDIN )
        ALWAYS("    DEF_RES_AUTH_LEVEL = AUTH_LEVEL_LOGGEDIN");
    else if ( DEF_RES_AUTH_LEVEL == AUTH_LEVEL_USER )
        ALWAYS("    DEF_RES_AUTH_LEVEL = AUTH_LEVEL_USER");
    else if ( DEF_RES_AUTH_LEVEL == AUTH_LEVEL_CUSTOMER )
        ALWAYS("    DEF_RES_AUTH_LEVEL = AUTH_LEVEL_CUSTOMER");
    else if ( DEF_RES_AUTH_LEVEL == AUTH_LEVEL_STAFF )
        ALWAYS("    DEF_RES_AUTH_LEVEL = AUTH_LEVEL_STAFF");
    else if ( DEF_RES_AUTH_LEVEL == AUTH_LEVEL_MODERATOR )
        ALWAYS("    DEF_RES_AUTH_LEVEL = AUTH_LEVEL_MODERATOR");
    else if ( DEF_RES_AUTH_LEVEL == AUTH_LEVEL_ADMIN )
        ALWAYS("    DEF_RES_AUTH_LEVEL = AUTH_LEVEL_ADMIN");
    else if ( DEF_RES_AUTH_LEVEL == AUTH_LEVEL_ROOT )
        ALWAYS("    DEF_RES_AUTH_LEVEL = AUTH_LEVEL_ROOT");
#ifdef APP_ADMIN_EMAIL
    ALWAYS("       APP_ADMIN_EMAIL = %s", APP_ADMIN_EMAIL);
#endif
#ifdef APP_CONTACT_EMAIL
    ALWAYS("     APP_CONTACT_EMAIL = %s", APP_CONTACT_EMAIL);
#endif
#ifdef USERS
    ALWAYS("");
#ifdef USERSBYEMAIL
    ALWAYS(" Users' authentication = USERSBYEMAIL");
#else
    ALWAYS(" Users' authentication = USERSBYLOGIN");
#endif
#endif  /* USERS */
//    ALWAYS("");
//    ALWAYS("           auses' size = %lu B (%lu kB / %0.2lf MB)", sizeof(auses), sizeof(auses)/1024, (double)sizeof(auses)/1024/1024);
//    ALWAYS("");
    ALWAYS_LINE_LONG;
    ALWAYS("");

#ifdef DUMP
    WAR("DUMP is enabled, this file may grow big quickly!");
    ALWAYS("");
#endif  /* DUMP */


    /* ensure the message sizes are sufficient */

#ifdef ASYNC
    if ( sizeof(async_req_hdr_t) > ASYNC_REQ_MSG_SIZE )
    {
        ERR("sizeof(async_req_hdr_t) > ASYNC_REQ_MSG_SIZE, increase APP_ASYNC_REQ_MSG_SIZE");
        return FALSE;
    }

    if ( sizeof(async_res_hdr_t) > ASYNC_RES_MSG_SIZE )
    {
        ERR("sizeof(async_res_hdr_t) > ASYNC_RES_MSG_SIZE, increase APP_ASYNC_RES_MSG_SIZE");
        return FALSE;
    }
#endif


#ifdef DBMYSQL

    DBG("Trying lib_open_db...");

    if ( !lib_open_db() )
    {
        ERR("lib_open_db failed");
        return FALSE;
    }

    ALWAYS("Database connected");

#endif  /* DBMYSQL */


    /* custom init
       Among others, that may contain generating statics, like css and js */

    if ( !silgy_app_init(argc, argv) )
    {
        ERR("app_init() failed");
        return FALSE;
    }

    DBG("app_init() OK");

    /* read static resources */

    if ( !read_files(FALSE, TRUE, NULL) )   /* normal */
    {
        ERR("read_files() failed");
        return FALSE;
    }

    DBG("read_files(FALSE) OK");

    if ( !read_files(TRUE, TRUE, NULL) )    /* minified */
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

    uint8_t sha1_res1[SHA1_DIGEST_SIZE];
    char    sha1_res2[64];
    char    sha1_res3[64];

    DBG("");
    DBG("Trying libSHA1...\n");
    DBG("Expected: [A9993E36 4706816A BA3E2571 7850C26C 9CD0D89D]");

    libSHA1((unsigned char*)"abc", 3, sha1_res1);

    digest_to_hex(sha1_res1, sha1_res2);
    DBG("     Got: [%s]\n", sha1_res2);

    /* fill the M_random_numbers up */

    init_random_numbers();

    /* calculate Expires and Last-Modified header fields for static resources */

#ifdef _WIN32   /* Windows */
    strftime(G_last_modified, 32, "%a, %d %b %Y %H:%M:%S GMT", G_ptm);
#else
    strftime(G_last_modified, 32, "%a, %d %b %Y %T GMT", G_ptm);
#endif  /* _WIN32 */
    DBG("Now is: %s\n", G_last_modified);

    set_expiry_dates();

    /* handle signals ---------------------------------------------------- */

    signal(SIGINT,  sigdisp);   /* Ctrl-C */
    signal(SIGTERM, sigdisp);
#ifndef _WIN32
    signal(SIGQUIT, sigdisp);   /* Ctrl-\ */
    signal(SIGTSTP, sigdisp);   /* Ctrl-Z */

    signal(SIGPIPE, SIG_IGN);   /* ignore SIGPIPE */
#endif

    /* initialize SSL connection ----------------------------------------- */

#ifdef HTTPS
    if ( !init_ssl() )
    {
        ERR("init_ssl failed");
        return FALSE;
    }
#endif

    /* init conn array --------------------------------------------------- */

    for ( i=0; i<MAX_CONNECTIONS; ++i )
    {
#ifdef OUTCHECKREALLOC
        if ( !(conn[i].out_data_alloc = (char*)malloc(OUT_BUFSIZE)) )
        {
            ERR("malloc for conn[%d].out_data failed", i);
            return FALSE;
        }
#endif  /* OUTCHECKREALLOC */

        conn[i].out_data_allocated = OUT_BUFSIZE;
        conn[i].in_data = NULL;
        reset_conn(i, CONN_STATE_DISCONNECTED);

#ifdef HTTPS
        conn[i].ssl = NULL;
#endif
        conn[i].req = 0;
    }

    /* read blocked IPs list --------------------------------------------- */

    read_blocked_ips();

#ifdef ASYNC
    ALWAYS("\nOpening message queues...\n");

#ifdef APP_ASYNC_ID
    if ( G_ASYNCId > -1 )
    {
        sprintf(G_req_queue_name, "%s_%d__%d", ASYNC_REQ_QUEUE, APP_ASYNC_ID, G_ASYNCId);
        sprintf(G_res_queue_name, "%s_%d__%d", ASYNC_RES_QUEUE, APP_ASYNC_ID, G_ASYNCId);
    }
    else
    {
        sprintf(G_req_queue_name, "%s_%d", ASYNC_REQ_QUEUE, APP_ASYNC_ID);
        sprintf(G_res_queue_name, "%s_%d", ASYNC_RES_QUEUE, APP_ASYNC_ID);
    }
#else
    if ( G_ASYNCId > -1 )
    {
        sprintf(G_req_queue_name, "%s__%d", ASYNC_REQ_QUEUE, G_ASYNCId);
        sprintf(G_res_queue_name, "%s__%d", ASYNC_RES_QUEUE, G_ASYNCId);
    }
    else
    {
        strcpy(G_req_queue_name, ASYNC_REQ_QUEUE);
        strcpy(G_res_queue_name, ASYNC_RES_QUEUE);
    }
#endif

    struct mq_attr attr={0};

    attr.mq_maxmsg = ASYNC_MQ_MAXMSG;

    /* ------------------------------------------------------------------- */

    if ( mq_unlink(G_req_queue_name) == 0 )
        INF("Message queue %s removed from system", G_req_queue_name);

    attr.mq_msgsize = ASYNC_REQ_MSG_SIZE;

    G_queue_req = mq_open(G_req_queue_name, O_WRONLY | O_CREAT | O_NONBLOCK, 0664, &attr);
    if (G_queue_req < 0)
        ERR("mq_open for req failed, errno = %d (%s)", errno, strerror(errno));
    else
        INF("mq_open %s OK", G_req_queue_name);

    /* ------------------------------------------------------------------- */

    if ( mq_unlink(G_res_queue_name) == 0 )
        INF("Message queue %s removed from system", G_res_queue_name);

    attr.mq_msgsize = ASYNC_RES_MSG_SIZE;   /* larger buffer */

    G_queue_res = mq_open(G_res_queue_name, O_RDONLY | O_CREAT | O_NONBLOCK, 0664, &attr);
    if (G_queue_res < 0)
        ERR("mq_open for res failed, errno = %d (%s)", errno, strerror(errno));
    else
        INF("mq_open %s OK", G_res_queue_name);

    /* ------------------------------------------------------------------- */

    for (i=0; i<MAX_ASYNC_REQS; ++i)
        areqs[i].state = ASYNC_STATE_FREE;

    M_last_call_id = 0;

    INF("");

#endif  /* ASYNC */

    return TRUE;
}


/* --------------------------------------------------------------------------
   Build select list
   This is on the latency critical path
   Try to minimize number of steps
-------------------------------------------------------------------------- */
static void build_fd_sets()
{
#ifdef FD_MON_SELECT
    FD_ZERO(&M_readfds);
    FD_ZERO(&M_writefds);

    FD_SET(M_listening_fd, &M_readfds);
#ifdef HTTPS
    FD_SET(M_listening_sec_fd, &M_readfds);
    M_highsock = M_listening_sec_fd;
#else
    M_highsock = M_listening_fd;
#endif  /* HTTPS */

    int i;
    int remain = G_open_conn;

    for ( i=0; remain>0 && i<MAX_CONNECTIONS; ++i )
    {
        if ( conn[i].conn_state == CONN_STATE_DISCONNECTED ) continue;

#ifdef HTTPS
        if ( conn[i].secure )
        {
            /* reading */

            if ( conn[i].conn_state == CONN_STATE_CONNECTED
                    || conn[i].conn_state == CONN_STATE_READING_DATA
                    || conn[i].ssl_err == SSL_ERROR_WANT_READ )
            {
                FD_SET(conn[i].fd, &M_readfds);
            }

            /* writing */

            else if ( conn[i].conn_state == CONN_STATE_READY_TO_SEND_HEADER
                    || conn[i].conn_state == CONN_STATE_READY_TO_SEND_BODY
                    || conn[i].conn_state == CONN_STATE_SENDING_BODY
#ifdef ASYNC
                    || conn[i].conn_state == CONN_STATE_WAITING_FOR_ASYNC
#endif
                    || conn[i].ssl_err == SSL_ERROR_WANT_WRITE )
            {
                FD_SET(conn[i].fd, &M_writefds);
            }
        }
        else
        {
#endif  /* HTTPS */
            /* reading */

            if ( conn[i].conn_state == CONN_STATE_CONNECTED
                    || conn[i].conn_state == CONN_STATE_READING_DATA )
            {
                FD_SET(conn[i].fd, &M_readfds);
            }

            /* writing */

            else if ( conn[i].conn_state == CONN_STATE_READY_TO_SEND_HEADER
                    || conn[i].conn_state == CONN_STATE_READY_TO_SEND_BODY
#ifdef ASYNC
                    || conn[i].conn_state == CONN_STATE_WAITING_FOR_ASYNC
#endif
                    || conn[i].conn_state == CONN_STATE_SENDING_BODY )
            {
                FD_SET(conn[i].fd, &M_writefds);
            }
#ifdef HTTPS
        }
#endif

        if ( conn[i].fd > M_highsock )
            M_highsock = conn[i].fd;

        remain--;
    }
#ifdef DUMP
    if ( remain )
        DBG_T("remain should be 0 but currently %d", remain);
#endif
#endif  /* FD_MON_SELECT */
}


/* --------------------------------------------------------------------------
   Handle a brand new connection
   we've got fd and IP here for conn array
-------------------------------------------------------------------------- */
static void accept_http()
{
    int         i;
    int         connection;
    struct sockaddr_in cli_addr;
    socklen_t   addr_len;
    char        remote_addr[INET_ADDRSTRLEN]="";

    /* We have a new connection coming in! We'll
       try to find a spot for it in conn array  */

    addr_len = sizeof(cli_addr);

    connection = accept(M_listening_fd, (struct sockaddr*)&cli_addr, &addr_len);

    if ( connection < 0 )
    {
        ERR("accept failed, errno = %d (%s)", errno, strerror(errno));
        return;
    }

    /* get the remote address */
#ifdef _WIN32   /* Windows */
    strcpy(remote_addr, inet_ntoa(cli_addr.sin_addr));
#else
    inet_ntop(AF_INET, &(cli_addr.sin_addr), remote_addr, INET_ADDRSTRLEN);
#endif

    if ( G_blockedIPList[0] && ip_blocked(remote_addr) )
    {
        ++G_cnts_today.blocked;
#ifdef _WIN32   /* Windows */
        closesocket(connection);
#else
        close(connection);
#endif  /* _WIN32 */
        return;
    }

    lib_setnonblocking(connection);

    /* find a free slot in conn */

    for ( i=0; i<MAX_CONNECTIONS; ++i )
    {
        if ( conn[i].conn_state == CONN_STATE_DISCONNECTED )    /* free connection slot -- we'll use it */
        {
            DBG("\nConnection accepted: %s, slot=%d, fd=%d", remote_addr, i, connection);
            conn[i].fd = connection;
            conn[i].secure = FALSE;

            if ( ++G_open_conn > G_open_conn_hwm )
                G_open_conn_hwm = G_open_conn;

            strcpy(conn[i].ip, remote_addr);        /* possibly client IP */
            strcpy(conn[i].pip, remote_addr);       /* possibly proxy IP */
#ifdef DUMP
            DBG("Changing state to CONN_STATE_CONNECTED");
#endif
            conn[i].conn_state = CONN_STATE_CONNECTED;
            conn[i].last_activity = G_now;

#ifdef FD_MON_POLL  /* add connection to monitored set */
            /* reference ... */
            conn[i].pi = M_pollfds_cnt;
            /* ... each other to avoid unnecessary looping */
            M_poll_ci[M_pollfds_cnt] = i;
            M_pollfds[M_pollfds_cnt].fd = connection;
            M_pollfds[M_pollfds_cnt].events = POLLIN;
            M_pollfds[M_pollfds_cnt].revents = 0;
            ++M_pollfds_cnt;
#endif  /* FD_MON_POLL */

            connection = -1;                        /* mark as OK */
            break;
        }
    }

    if (connection != -1)   /* none was free */
    {
        WAR("No room left for new client, sending 503");

        int bytes = send(connection, "HTTP/1.1 503 Service Unavailable\r\n\r\n", 36, 0);

        if ( bytes < 36 )
            ERR("write error, bytes = %d of 36", bytes);
#ifdef _WIN32   /* Windows */
        closesocket(connection);
#else
        close(connection);
#endif  /* _WIN32 */
    }
}


/* --------------------------------------------------------------------------
   Handle a brand new connection
   we've got fd and IP here for conn array
-------------------------------------------------------------------------- */
static void accept_https()
{
#ifdef HTTPS
    int         i;
    int         connection;
    struct sockaddr_in cli_addr;
    socklen_t   addr_len;
    char        remote_addr[INET_ADDRSTRLEN]="";
    int         ret, ssl_err;

    /* We have a new connection coming in! We'll
       try to find a spot for it in conn array  */

    addr_len = sizeof(cli_addr);

    connection = accept(M_listening_sec_fd, (struct sockaddr*)&cli_addr, &addr_len);

    if ( connection < 0 )
    {
        ERR("accept failed, errno = %d (%s)", errno, strerror(errno));
        return;
    }

    /* get the remote address */
#ifdef _WIN32   /* Windows */
    strcpy(remote_addr, inet_ntoa(cli_addr.sin_addr));
#else
    inet_ntop(AF_INET, &(cli_addr.sin_addr), remote_addr, INET_ADDRSTRLEN);
#endif

    if ( G_blockedIPList[0] && ip_blocked(remote_addr) )
    {
        ++G_cnts_today.blocked;
#ifdef _WIN32   /* Windows */
        closesocket(connection);
#else
        close(connection);
#endif  /* _WIN32 */
        return;
    }

    lib_setnonblocking(connection);

    /* find a free slot in conn */

    for ( i=0; i<MAX_CONNECTIONS; ++i )
    {
        if ( conn[i].conn_state == CONN_STATE_DISCONNECTED )    /* free connection slot -- we'll use it */
        {
            DBG("\nSecure connection accepted: %s, slot=%d, fd=%d", remote_addr, i, connection);
            conn[i].fd = connection;
            conn[i].secure = TRUE;

            if ( ++G_open_conn > G_open_conn_hwm )
                G_open_conn_hwm = G_open_conn;

#ifdef FD_MON_POLL  /* add connection to monitored set */
            /* reference ... */
            conn[i].pi = M_pollfds_cnt;
            /* ... each other to avoid unnecessary looping */
            M_poll_ci[M_pollfds_cnt] = i;
            M_pollfds[M_pollfds_cnt].fd = connection;
            M_pollfds[M_pollfds_cnt].events = POLLIN;
            M_pollfds[M_pollfds_cnt].revents = 0;
            ++M_pollfds_cnt;
#endif  /* FD_MON_POLL */

            conn[i].ssl = SSL_new(M_ssl_ctx);

            if ( !conn[i].ssl )
            {
                ERR("SSL_new failed");
                close_conn(i);
                return;
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

            ret = SSL_accept(conn[i].ssl);   /* handshake here */

            if ( ret <= 0 )
            {
                conn[i].ssl_err = SSL_get_error(conn[i].ssl, ret);

                if ( conn[i].ssl_err != SSL_ERROR_WANT_READ && conn[i].ssl_err != SSL_ERROR_WANT_WRITE )
                {
                    DBG("SSL_accept failed, ssl_err = %d", conn[i].ssl_err);
                    close_conn(i);
                    return;
                }
            }

#ifdef FD_MON_POLL
            if ( conn[i].ssl_err == SSL_ERROR_WANT_WRITE )
                M_pollfds[conn[i].pi].events = POLLOUT;
#endif
            strcpy(conn[i].ip, remote_addr);        /* possibly client IP */
            strcpy(conn[i].pip, remote_addr);       /* possibly proxy IP */
#ifdef DUMP
            DBG("Changing state to CONN_STATE_ACCEPTING");
#endif
            conn[i].conn_state = CONN_STATE_ACCEPTING;
            conn[i].last_activity = G_now;
            connection = -1;                        /* mark as OK */
            break;
        }
    }

    if (connection != -1)   /* none was free */
    {
        WAR("No room left for new client, sending 503");

        int bytes = send(connection, "HTTP/1.1 503 Service Unavailable\r\n\r\n", 36, 0);

        if ( bytes < 36 )
            ERR("write error, bytes = %d of 36", bytes);
#ifdef _WIN32   /* Windows */
        closesocket(connection);
#else
        close(connection);
#endif  /* _WIN32 */
    }
#endif
}


/* --------------------------------------------------------------------------
   Read list of blocked IPs from the file
-------------------------------------------------------------------------- */
static void read_blocked_ips()
{
    char    fname[1024];
    FILE    *h_file=NULL;
    int     c=0;
    int     i=0;
    char    now_value=1;
    char    now_comment=0;
    char    value[64]="";

    if ( G_blockedIPList[0] == EOS ) return;

    INF("Updating blocked IPs list");

    /* open the file */

    if ( G_blockedIPList[0] == '/' )    /* full path */
        strcpy(fname, G_blockedIPList);
    else if ( G_appdir[0] )
        sprintf(fname, "%s/bin/%s", G_appdir, G_blockedIPList);
    else
        strcpy(fname, G_blockedIPList);

    if ( NULL == (h_file=fopen(fname, "r")) )
    {
        WAR("Couldn't open %s\n", fname);
        return;
    }

    G_blacklist_cnt = 0;

    /* parse the file */

    while ( EOF != (c=fgetc(h_file)) )
    {
        if ( c == ' ' || c == '\t' || c == '\r' ) continue;  /* omit whitespaces */

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
   Read static resources from disk
   Read all the files from G_appdir/res or resmin directory
   path is a relative path uder `res` or `resmin`
-------------------------------------------------------------------------- */
static bool read_files(bool minify, bool first_scan, const char *path)
{
    int     i;
    char    resdir[STATIC_PATH_LEN];        /* full path to res */
    char    ressubdir[STATIC_PATH_LEN];     /* full path to res/subdir */
    char    namewpath[STATIC_PATH_LEN];     /* full path including file name */
    char    resname[STATIC_PATH_LEN];       /* relative path including file name */
    DIR     *dir;
    struct dirent *dirent;
    FILE    *fd;
    char    *data_tmp=NULL;
    char    *data_tmp_min=NULL;
    struct stat fstat;
    char    mod_time[32];

#ifndef _WIN32
    if ( G_appdir[0] == EOS ) return TRUE;
#endif

    if ( first_scan && !path ) DBG("");

#ifdef DUMP
//    DBG_LINE_LONG;
//    DBG("read_files, minify = %s", minify?"TRUE":"FALSE");
#endif

#ifdef _WIN32   /* be more forgiving */

    if ( G_appdir[0] )
    {
        if ( minify )
            sprintf(resdir, "%s/resmin", G_appdir);
        else
            sprintf(resdir, "%s/res", G_appdir);
    }
    else    /* no SILGYDIR */
    {
        if ( minify )
            strcpy(resdir, "../resmin");
        else
            strcpy(resdir, "../res");
    }

#else /* Linux -- don't fool around */

    if ( minify )
        sprintf(resdir, "%s/resmin", G_appdir);
    else
        sprintf(resdir, "%s/res", G_appdir);

#endif  /* _WIN32 */

#ifdef DUMP
//    DBG("resdir [%s]", resdir);
#endif

    if ( !path )     /* highest level */
    {
        strcpy(ressubdir, resdir);
    }
    else    /* recursive call */
    {
        sprintf(ressubdir, "%s/%s", resdir, path);
    }

#ifdef DUMP
//    DBG("ressubdir [%s]", ressubdir);
#endif

    if ( (dir=opendir(ressubdir)) == NULL )
    {
//#ifdef DUMP
        if ( first_scan )
            DBG("Couldn't open directory [%s]", ressubdir);
//#endif
        return TRUE;    /* don't panic, just no external resources will be used */
    }

    /* ------------------------------------------------------------------- */
    /* check removed files */

    if ( !first_scan && !path )   /* on the highest level only */
    {
#ifdef DUMP
//        DBG("Checking removed files...");
#endif
        for ( i=0; i<=M_max_static; ++i )
        {
            if ( M_stat[i].name[0]==EOS ) continue;  /* already removed */

            if ( minify && M_stat[i].source != STATIC_SOURCE_RESMIN ) continue;

            if ( !minify && M_stat[i].source != STATIC_SOURCE_RES ) continue;
#ifdef DUMP
//            DBG("Checking %s...", M_stat[i].name);
#endif
            char fullpath[STATIC_PATH_LEN];
            sprintf(fullpath, "%s/%s", resdir, M_stat[i].name);

            if ( !lib_file_exists(fullpath) )
            {
                INF("Removing %s from static resources", M_stat[i].name);

                if ( 0==strcmp(M_stat[i].name, "index.html") )
                    G_index_present = FALSE;

                M_stat[i].name[0] = EOS;
                free(M_stat[i].data);
            }
        }
    }

    /* ------------------------------------------------------------------- */
#ifdef DUMP
//    DBG("Reading %sfiles", first_scan?"":"new ");
#endif
    /* read the files into memory */

    while ( (dirent=readdir(dir)) )
    {
        if ( dirent->d_name[0] == '.' )  /* skip ".", ".." and hidden files */
            continue;

        /* ------------------------------------------------------------------- */
        /* resource name */

        if ( !path )
            strcpy(resname, dirent->d_name);
        else
            sprintf(resname, "%s/%s", path, dirent->d_name);

#ifdef DUMP
//        if ( first_scan )
//            DBG("resname [%s]", resname);
#endif

        /* ------------------------------------------------------------------- */
        /* additional file info */

        sprintf(namewpath, "%s/%s", resdir, resname);

#ifdef DUMP
//        if ( first_scan )
//            DBG("namewpath [%s]", namewpath);
#endif

        if ( stat(namewpath, &fstat) != 0 )
        {
            ERR("stat for [%s] failed, errno = %d (%s)", namewpath, errno, strerror(errno));
            closedir(dir);
            return FALSE;
        }

        /* ------------------------------------------------------------------- */

        if ( S_ISDIR(fstat.st_mode) )   /* directory */
        {
#ifdef DUMP
//            if ( first_scan )
//                DBG("Reading subdirectory [%s]...", dirent->d_name);
#endif
            read_files(minify, first_scan, resname);
            continue;
        }
        else if ( !S_ISREG(fstat.st_mode) )    /* skip if not a regular file nor directory */
        {
#ifdef DUMP
            if ( first_scan )
                DBG("[%s] is not a regular file", resname);
#endif
            continue;
        }

        /* ------------------------------------------------------------------- */
        /* already read? */

        bool reread = FALSE;

        if ( !first_scan )
        {
            bool exists = FALSE;

            for ( i=0; i<=M_max_static; ++i )
            {
                if ( M_stat[i].name[0]==EOS ) continue;  /* removed */

                if ( minify && M_stat[i].source != STATIC_SOURCE_RESMIN ) continue;

                if ( !minify && M_stat[i].source != STATIC_SOURCE_RES ) continue;

                /* ------------------------------------------------------------------- */

                if ( 0==strcmp(M_stat[i].name, resname) )
                {
#ifdef DUMP
//                    DBG("%s already read", resname);
#endif
                    if ( M_stat[i].modified == fstat.st_mtime )
                    {
#ifdef DUMP
//                        DBG("Not modified");
#endif
                        exists = TRUE;
                    }
                    else
                    {
                        INF("%s has been modified", resname);
                        reread = TRUE;
                    }

                    break;
                }
            }

            if ( exists ) continue;  /* not modified */
        }

        /* find the first unused slot in M_stat array */

        if ( !reread )
        {
            i = first_free_stat();
            /* file name */
            strcpy(M_stat[i].name, resname);
        }

        /* last modified */

        M_stat[i].modified = fstat.st_mtime;

        /* size and content */

#ifdef _WIN32   /* Windows */
        if ( NULL == (fd=fopen(namewpath, "rb")) )
#else
        if ( NULL == (fd=fopen(namewpath, "r")) )
#endif  /* _WIN32 */
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
                    ERR("Couldn't allocate %u bytes for %s", M_stat[i].len, M_stat[i].name);
                    fclose(fd);
                    closedir(dir);
                    return FALSE;
                }

                if ( NULL == (data_tmp_min=(char*)malloc(M_stat[i].len+1)) )
                {
                    ERR("Couldn't allocate %u bytes for %s", M_stat[i].len, M_stat[i].name);
                    fclose(fd);
                    closedir(dir);
                    return FALSE;
                }

                fread(data_tmp, M_stat[i].len, 1, fd);
                *(data_tmp+M_stat[i].len) = EOS;

                M_stat[i].len = silgy_minify(data_tmp_min, data_tmp);   /* new length */
            }

            /* allocate the final destination */

            if ( reread )
                free(M_stat[i].data);

#ifdef SEND_ALL_AT_ONCE
            if ( NULL == (M_stat[i].data=(char*)malloc(M_stat[i].len+1+OUT_HEADER_BUFSIZE)) )
            {
                ERR("Couldn't allocate %u bytes for %s", M_stat[i].len+1+OUT_HEADER_BUFSIZE, M_stat[i].name);
#else
            if ( NULL == (M_stat[i].data=(char*)malloc(M_stat[i].len+1)) )
            {
                ERR("Couldn't allocate %u bytes for %s", M_stat[i].len+1, M_stat[i].name);
#endif  /* SEND_ALL_AT_ONCE */
                fclose(fd);
                closedir(dir);
                return FALSE;
            }

            if ( minify )
            {
#ifdef SEND_ALL_AT_ONCE
                memcpy(M_stat[i].data+OUT_HEADER_BUFSIZE, data_tmp_min, M_stat[i].len+1);
#else
                memcpy(M_stat[i].data, data_tmp_min, M_stat[i].len+1);
#endif
                free(data_tmp);
                free(data_tmp_min);
                data_tmp = NULL;
                data_tmp_min = NULL;

                M_stat[i].source = STATIC_SOURCE_RESMIN;
            }
            else
            {
#ifdef SEND_ALL_AT_ONCE
                fread(M_stat[i].data+OUT_HEADER_BUFSIZE, M_stat[i].len, 1, fd);
#else
                fread(M_stat[i].data, M_stat[i].len, 1, fd);
#endif

                M_stat[i].source = STATIC_SOURCE_RES;
            }

            fclose(fd);

            if ( !reread )
            {
                M_stat[i].type = get_res_type(M_stat[i].name);

                if ( 0==strcmp(M_stat[i].name, "index.html") )
                    G_index_present = TRUE;
            }

            /* log file info */

            if ( G_logLevel > LOG_INF )
            {
                G_ptm = gmtime(&M_stat[i].modified);
                sprintf(mod_time, "%d-%02d-%02d %02d:%02d:%02d", G_ptm->tm_year+1900, G_ptm->tm_mon+1, G_ptm->tm_mday, G_ptm->tm_hour, G_ptm->tm_min, G_ptm->tm_sec);
                G_ptm = gmtime(&G_now);     /* set it back */
                DBG("%s %s\t\t%u bytes", lib_add_spaces(M_stat[i].name, 28), mod_time, M_stat[i].len);
            }
        }

#ifdef DUMP
//      if ( minify )   /* temporarily */
//          DBG("minified %s: [%s]", M_stat[i].name, M_stat[i].data);
#endif  /* DUMP */
    }

    closedir(dir);

    if ( first_scan && !path ) DBG("");

    return TRUE;
}


/* --------------------------------------------------------------------------
   Find first free slot in M_stat
-------------------------------------------------------------------------- */
static int first_free_stat()
{
    int i=0;

    for ( i=0; i<MAX_STATICS; ++i )
    {
        if ( M_stat[i].name[0]=='-' || M_stat[i].name[0]==EOS )
        {
            if ( i > M_max_static ) M_max_static = i;
            return i;
        }
    }

    ERR("MAX_STATICS reached (%d)! You can set/increase APP_MAX_STATICS in silgy_app.h.", MAX_STATICS);

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
   Main new request processing
   Request received over current conn is already parsed
-------------------------------------------------------------------------- */
static void process_req(int ci)
{
    int ret=OK;

    DBG("process_req, ci=%d", ci);

    /* ------------------------------------------------------------------------ */

#ifdef DUMP
    if ( conn[ci].post && conn[ci].in_data )
        log_long(conn[ci].in_data, conn[ci].was_read, "POST data");
#endif

    /* ------------------------------------------------------------------------ */

#ifdef SEND_ALL_AT_ONCE  /* make room for a header */
    conn[ci].p_content = conn[ci].out_data + OUT_HEADER_BUFSIZE;
#else
    conn[ci].p_content = conn[ci].out_data;
#endif

    /* ------------------------------------------------------------------------ */

    conn[ci].location[COLON_POSITION] = '-';    /* no protocol here yet */

    /* ------------------------------------------------------------------------ */

    if ( conn[ci].status != 200 )
        return;

    /* ------------------------------------------------------------------------ */
    /* Generate HTML content before header -- to know its size & type --------- */

    /* ------------------------------------------------------------------------ */
    /* authorization check / log in from cookies ------------------------------ */

#ifdef USERS
    if ( conn[ci].cookie_in_l[0] )  /* logged in sesid cookie present */
    {
        ret = libusr_luses_ok(ci);     /* is it valid? */

        if ( ret == OK )    /* valid sesid -- user logged in */
            DBG("User logged in from cookie");
        else if ( ret != ERR_INT_SERVER_ERROR && ret != ERR_SERVER_TOOBUSY )   /* dodged sesid... or session expired */
            WAR("Invalid ls cookie");
    }

    if ( LOGGED && conn[ci].required_auth_level > uses[conn[ci].usi].auth_level )
    {
        WAR("Insufficient user authorization level, returning 404");
        ret = ERR_NOT_FOUND;
        RES_DONT_CACHE;
    }
    else if ( !LOGGED && conn[ci].required_auth_level > AUTH_LEVEL_ANONYMOUS )  /* redirect to login page */
    {
        INF("auth_level > AUTH_LEVEL_ANONYMOUS required, redirecting to login");
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

    if ( conn[ci].required_auth_level==AUTH_LEVEL_ANONYMOUS && !REQ_BOT && !conn[ci].head_only && !LOGGED )    /* anonymous user session required */
#else
    if ( conn[ci].required_auth_level==AUTH_LEVEL_ANONYMOUS && !REQ_BOT && !conn[ci].head_only )
#endif  /* USERS */
    {
        if ( !conn[ci].cookie_in_a[0] || !a_usession_ok(ci) )       /* valid anonymous sesid cookie not present */
        {
            ret = eng_uses_start(ci, NULL);
        }
    }

    /* ------------------------------------------------------------------------ */
    /* process request -------------------------------------------------------- */

    if ( ret == OK )
    {
        if ( !conn[ci].location[0] )
            silgy_app_main(ci);  /* main application called here */
    }

    /* ------------------------------------------------------------------------ */

    conn[ci].last_activity = G_now;
    if ( conn[ci].usi ) US.last_activity = G_now;

#ifdef ASYNC
    if ( conn[ci].conn_state == CONN_STATE_WAITING_FOR_ASYNC ) return;
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

    if ( ret==ERR_REDIRECTION || conn[ci].status==400 || conn[ci].status==401 || conn[ci].status==403 || conn[ci].status==404 || conn[ci].status==500 || conn[ci].status==503 )
    {
#ifdef USERS
        if ( conn[ci].usi && !LOGGED ) close_uses(conn[ci].usi, ci);
#else
        if ( conn[ci].usi ) close_uses(conn[ci].usi, ci);
#endif
        if ( !conn[ci].keep_content )   /* reset out buffer pointer as it could have contained something already */
        {
#ifdef SEND_ALL_AT_ONCE
            conn[ci].p_content = conn[ci].out_data + OUT_HEADER_BUFSIZE;
#else
            conn[ci].p_content = conn[ci].out_data;
#endif
            if ( ret == OK )   /* RES_STATUS could be used, show the proper message */
            {
                if ( conn[ci].status == 400 )
                    ret = ERR_INVALID_REQUEST;
                else if ( conn[ci].status == 401 )
                    ret = ERR_UNAUTHORIZED;
                else if ( conn[ci].status == 403 )
                    ret = ERR_FORBIDDEN;
                else if ( conn[ci].status == 404 )
                    ret = ERR_NOT_FOUND;
                else if ( conn[ci].status == 500 )
                    ret = ERR_INT_SERVER_ERROR;
                else if ( conn[ci].status == 503 )
                    ret = ERR_SERVER_TOOBUSY;
            }

            gen_page_msg(ci, ret);
        }

        RES_DONT_CACHE;
    }
}


/* --------------------------------------------------------------------------
   Straight from Mark Adler!

   Compress buf[0..len-1] in place into buf[0..*max-1].  *max must be greater
   than or equal to len.  Return Z_OK on success, Z_BUF_ERROR if *max is not
   enough output space, Z_MEM_ERROR if there is not enough memory, or
   Z_STREAM_ERROR if *strm is corrupted (e.g. if it wasn't initialized or if it
   was inadvertently written over).  If Z_OK is returned, *max is set to the
   actual size of the output.  If Z_BUF_ERROR is returned, then *max is
   unchanged and buf[] is filled with *max bytes of uncompressed data (which is
   not all of it, but as much as would fit).

   Incompressible data will require more output space than len, so max should
   be sufficiently greater than len to handle that case in order to avoid a
   Z_BUF_ERROR. To assure that there is enough output space, max should be
   greater than or equal to the result of deflateBound(strm, len).

   strm is a deflate stream structure that has already been successfully
   initialized by deflateInit() or deflateInit2().  That structure can be
   reused across multiple calls to deflate_inplace().  This avoids unnecessary
   memory allocations and deallocations from the repeated use of deflateInit()
   and deflateEnd().
-------------------------------------------------------------------------- */
#ifndef _WIN32
static int deflate_inplace(z_stream *strm, unsigned char *buf, unsigned len, unsigned *max)
{
    int ret;                    /* return code from deflate functions */
    unsigned have;              /* number of bytes in temp[] */
    unsigned char *hold;        /* allocated buffer to hold input data */
    unsigned char temp[11];     /* must be large enough to hold zlib or gzip
                                   header (if any) and one more byte -- 11
                                   works for the worst case here, but if gzip
                                   encoding is used and a deflateSetHeader()
                                   call is inserted in this code after the
                                   deflateReset(), then the 11 needs to be
                                   increased to accomodate the resulting gzip
                                   header size plus one */

    /* initialize deflate stream and point to the input data */
    ret = deflateReset(strm);
    if (ret != Z_OK)
        return ret;
    strm->next_in = buf;
    strm->avail_in = len;

    /* kick start the process with a temporary output buffer -- this allows
       deflate to consume a large chunk of input data in order to make room for
       output data there */
    if (*max < len)
        *max = len;
    strm->next_out = temp;
    strm->avail_out = sizeof(temp) > *max ? *max : sizeof(temp);
    ret = deflate(strm, Z_FINISH);
    if (ret == Z_STREAM_ERROR)
        return ret;

    /* if we can, copy the temporary output data to the consumed portion of the
       input buffer, and then continue to write up to the start of the consumed
       input for as long as possible */
    have = strm->next_out - temp;
    if (have <= (strm->avail_in ? len - strm->avail_in : *max)) {
        memcpy(buf, temp, have);
        strm->next_out = buf + have;
        have = 0;
        while (ret == Z_OK) {
            strm->avail_out = strm->avail_in ? strm->next_in - strm->next_out :
                                               (buf + *max) - strm->next_out;
            ret = deflate(strm, Z_FINISH);
        }
        if (ret != Z_BUF_ERROR || strm->avail_in == 0) {
            *max = strm->next_out - buf;
            return ret == Z_STREAM_END ? Z_OK : ret;
        }
    }
    /* the output caught up with the input due to insufficiently compressible
       data -- copy the remaining input data into an allocated buffer and
       complete the compression from there to the now empty input buffer (this
       will only occur for long incompressible streams, more than ~20 MB for
       the default deflate memLevel of 8, or when *max is too small and less
       than the length of the header plus one byte) */
    hold = (unsigned char*)strm->zalloc(strm->opaque, strm->avail_in, 1);
    if (hold == Z_NULL)
        return Z_MEM_ERROR;
    memcpy(hold, strm->next_in, strm->avail_in);
    strm->next_in = hold;
    if (have) {
        memcpy(buf, temp, have);
        strm->next_out = buf + have;
    }
    strm->avail_out = (buf + *max) - strm->next_out;
    ret = deflate(strm, Z_FINISH);
    strm->zfree(strm->opaque, hold);
    *max = strm->next_out - buf;
    return ret == Z_OK ? Z_BUF_ERROR : (ret == Z_STREAM_END ? Z_OK : ret);
}
#endif  /* _WIN32 */


/* --------------------------------------------------------------------------
   Generate HTTP response header
-------------------------------------------------------------------------- */
static void gen_response_header(int ci)
{
    DBG("gen_response_header, ci=%d", ci);

#ifdef SEND_ALL_AT_ONCE
    char out_header[OUT_HEADER_BUFSIZE];
    conn[ci].p_header = out_header;
#else
    conn[ci].p_header = conn[ci].out_header;
#endif

    PRINT_HTTP_STATUS(conn[ci].status);

    if ( conn[ci].status == 301 || conn[ci].status == 303 )     /* redirection */
    {
        DBG("Redirecting");

        /*
           1 - upgrade 2 https, keep URI (301)
           2 - app new page version, ignore URI, use location (303)
           3 - redirect to final domain, keep URI (301)
        */
#ifdef HTTPS
        if ( conn[ci].upgrade2https )   /* (1) */
        {
            PRINT_HTTP_VARY_UIR;    /* Upgrade-Insecure-Requests */
            sprintf(G_tmp, "Location: https://%s/%s\r\n", conn[ci].host, conn[ci].uri);
        }
        else if ( conn[ci].location[0] == 'h'        /* (2) full address already present */
#else
             if ( conn[ci].location[0] == 'h'        /* (2) full address already present */
#endif  /* HTTPS */
                    && conn[ci].location[1] == 't'
                    && conn[ci].location[2] == 't'
                    && conn[ci].location[3] == 'p' )
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

        conn[ci].clen = 0;
    }
    else if ( conn[ci].status == 304 )   /* not modified since */
    {
#ifdef DUMP
        DBG("Not Modified");
#endif
        if ( conn[ci].static_res == NOT_STATIC )
        {
            PRINT_HTTP_LAST_MODIFIED(G_last_modified);
        }
        else    /* static res */
        {
            PRINT_HTTP_LAST_MODIFIED(time_epoch2http(M_stat[conn[ci].static_res].modified));
        }

        conn[ci].clen = 0;
    }
    else    /* normal response with content: 2xx, 4xx, 5xx */
    {
#ifdef DUMP
        DBG("Normal response");
#endif
        if ( conn[ci].dont_cache )   /* dynamic content */
        {
            PRINT_HTTP_VARY_DYN;
            PRINT_HTTP_NO_CACHE;
        }
        else    /* static content */
        {
            PRINT_HTTP_VARY_STAT;

            if ( conn[ci].static_res == NOT_STATIC )   /* generated -- moderate caching */
            {
                if ( conn[ci].modified )
                    PRINT_HTTP_LAST_MODIFIED(time_epoch2http(conn[ci].modified));
                else
                    PRINT_HTTP_LAST_MODIFIED(G_last_modified);

                if ( EXPIRES_GENERATED > 0 )
                    PRINT_HTTP_EXPIRES_GENERATED;
            }
            else    /* static resource -- aggressive caching */
            {
                PRINT_HTTP_LAST_MODIFIED(time_epoch2http(M_stat[conn[ci].static_res].modified));

                if ( EXPIRES_STATICS > 0 )
                    PRINT_HTTP_EXPIRES_STATICS;
            }
        }

        if ( conn[ci].static_res == NOT_STATIC )    /* determine the content length */
#ifdef SEND_ALL_AT_ONCE
            conn[ci].clen = conn[ci].p_content - conn[ci].out_data - OUT_HEADER_BUFSIZE;
#else
            conn[ci].clen = conn[ci].p_content - conn[ci].out_data;
#endif
        else
            conn[ci].clen = M_stat[conn[ci].static_res].len;

        /* compress? ------------------------------------------------------------------ */

#ifndef _WIN32  /* just too much headache */
        if ( conn[ci].static_res==NOT_STATIC && conn[ci].clen > COMPRESS_TRESHOLD && conn[ci].accept_deflate && (conn[ci].ctype==RES_HTML || conn[ci].ctype==RES_TEXT || conn[ci].ctype==RES_JSON || conn[ci].ctype==RES_BMP) && !UA_IE )
        {
            DBG("Compressing content");

            int ret;
static z_stream strm;
static bool first=TRUE;

            if ( first )
            {
                strm.zalloc = Z_NULL;
                strm.zfree = Z_NULL;
                strm.opaque = Z_NULL;

                ret = deflateInit(&strm, COMPRESS_LEVEL);

                if ( ret != Z_OK )
                {
                    ERR("deflateInit failed, ret = %d", ret);
                    return;
                }

                first = FALSE;
            }

            unsigned max = conn[ci].clen;

#ifdef SEND_ALL_AT_ONCE
            ret = deflate_inplace(&strm, (unsigned char*)conn[ci].out_data+OUT_HEADER_BUFSIZE, conn[ci].clen, &max);
#else
            ret = deflate_inplace(&strm, (unsigned char*)conn[ci].out_data, conn[ci].clen, &max);
#endif
//            (void)deflateEnd(&strm);

            if ( ret == Z_OK )
            {
                DBG("Compression success, old len=%u, new len=%u", conn[ci].clen, max);
                conn[ci].clen = max;
                PRINT_HTTP_CONTENT_ENCODING_DEFLATE;
            }
            else
            {
                ERR("deflate_inplace failed, ret = %d", ret);
            }
        }
#endif  /* _WIN32 */

        /* ---------------------------------------------------------------------------- */
    }

    /* Date */

    PRINT_HTTP_DATE;

    /* Connection */

    PRINT_HTTP_CONNECTION(ci);

    /* Content-Length */

    PRINT_HTTP_CONTENT_LEN(conn[ci].clen);

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

#ifdef HTTPS
#ifndef NO_HSTS
    if ( !G_test && !conn[ci].secure )
        PRINT_HTTP_HSTS;
#endif
#endif  /* HTTPS */

#ifndef NO_IDENTITY
    PRINT_HTTP_SERVER;
#endif

    /* ------------------------------------------------------------- */

    PRINT_HTTP_END_OF_HEADER;

    /* header length */

#ifdef SEND_ALL_AT_ONCE
    conn[ci].out_hlen = conn[ci].p_header - out_header;
#else
    conn[ci].out_hlen = conn[ci].p_header - conn[ci].out_header;
#endif

#ifdef DUMP
    DBG("ci=%d, out_hlen = %u", ci, conn[ci].out_hlen);
#endif

    DBG("Response status: %d", conn[ci].status);

#ifdef DUMP     /* low-level tests */
    DBG("ci=%d, Changing state to CONN_STATE_READY_TO_SEND_HEADER", ci);
#endif
    conn[ci].conn_state = CONN_STATE_READY_TO_SEND_HEADER;

#ifdef FD_MON_POLL
    M_pollfds[conn[ci].pi].events = POLLOUT;
#endif

#if OUT_HEADER_BUFSIZE-1 <= MAX_LOG_STR_LEN
#ifdef SEND_ALL_AT_ONCE
    DBG("\nResponse header:\n\n[%s]\n", out_header);
#else
    DBG("\nResponse header:\n\n[%s]\n", conn[ci].out_header);
#endif
#else
#ifdef SEND_ALL_AT_ONCE
    log_long(out_header, conn[ci].out_hlen, "\nResponse header");
#else
    log_long(conn[ci].out_header, conn[ci].out_hlen, "\nResponse header");
#endif
#endif  /* OUT_HEADER_BUFSIZE-1 <= MAX_LOG_STR_LEN */

#ifdef SEND_ALL_AT_ONCE
    /* ----------------------------------------------------------------- */
    /* try to send everything at once */
    /* copy response header just before the content */

    conn[ci].out_start = conn[ci].out_data + (OUT_HEADER_BUFSIZE - conn[ci].out_hlen);
    memcpy(conn[ci].out_start, out_header, conn[ci].out_hlen);
    conn[ci].out_len = conn[ci].out_hlen + conn[ci].clen;

    /* ----------------------------------------------------------------- */
#endif

#ifdef DUMP     /* low-level tests */
    if ( G_logLevel>=LOG_DBG && conn[ci].clen > 0 && !conn[ci].head_only && conn[ci].static_res == NOT_STATIC
            && (conn[ci].ctype==RES_TEXT || conn[ci].ctype==RES_JSON) )
#ifdef SEND_ALL_AT_ONCE
        log_long(conn[ci].out_data+OUT_HEADER_BUFSIZE, conn[ci].clen, "Content to send");
#else
        log_long(conn[ci].out_data, conn[ci].clen, "Content to send");
#endif
#endif  /* DUMP */

    /* ----------------------------------------------------------------- */

    conn[ci].last_activity = G_now;
    if ( conn[ci].usi ) US.last_activity = G_now;

    log_proc_time(ci);
}


/* --------------------------------------------------------------------------
   Print Content-Type to response header
-------------------------------------------------------------------------- */
static void print_content_type(int ci, char type)
{
    char http_type[256];

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
    else if ( type == RES_SVG )
        strcpy(http_type, "image/svg+xml");
    else if ( type == RES_JSON )
        strcpy(http_type, "application/json");
    else if ( type == RES_PDF )
        strcpy(http_type, "application/pdf");
    else if ( type == RES_AMPEG )
        strcpy(http_type, "audio/mpeg");
    else if ( type == RES_EXE )
        strcpy(http_type, "application/x-msdownload");
    else if ( type == RES_ZIP )
        strcpy(http_type, "application/zip");
    else
        strcpy(http_type, "text/plain");

    sprintf(G_tmp, "Content-Type: %s\r\n", http_type);
    HOUT(G_tmp);
}


/* --------------------------------------------------------------------------
   Verify IP & User-Agent against sesid in uses (anonymous users)
   Return true if session exists
-------------------------------------------------------------------------- */
static bool a_usession_ok(int ci)
{
    DBG("a_usession_ok");

    if ( conn[ci].usi )   /* existing connection */
    {
        if ( uses[conn[ci].usi].sesid[0]
                && !uses[conn[ci].usi].logged
                && 0==strcmp(conn[ci].cookie_in_a, uses[conn[ci].usi].sesid)
                && 0==strcmp(conn[ci].uagent, uses[conn[ci].usi].uagent) )
        {
            DBG("Anonymous session found, usi=%d, sesid [%s]", conn[ci].usi, uses[conn[ci].usi].sesid);
            return TRUE;
        }
        else    /* session was closed -- it should never happen */
        {
            WAR("usi > 0 and no session!");
            conn[ci].usi = 0;
        }
    }
    else    /* fresh connection */
    {
        int i;

        for ( i=1; i<=MAX_SESSIONS; ++i )
        {
            if ( uses[i].sesid[0]
                    && !uses[i].logged
                    && 0==strcmp(conn[ci].cookie_in_a, uses[i].sesid)
                    && 0==strcmp(conn[ci].uagent, uses[i].uagent) )
            {
                DBG("Anonymous session found, usi=%d, sesid [%s]", i, uses[i].sesid);
                conn[ci].usi = i;
                return TRUE;
            }
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

    for ( i=0; G_open_conn>0 && i<MAX_CONNECTIONS; ++i )
    {
        if ( conn[i].conn_state != CONN_STATE_DISCONNECTED && conn[i].last_activity < last_allowed )
        {
            DBG("Closing timeouted connection %d", i);
            close_conn(i);
        }
    }
}


/* --------------------------------------------------------------------------
   Close timeouted anonymous user sessions
-------------------------------------------------------------------------- */
static void uses_close_timeouted()
{
    int     i;
    time_t  last_allowed;

    last_allowed = G_now - USES_TIMEOUT;

    for ( i=1; G_sessions>0 && i<=MAX_SESSIONS; ++i )
    {
        if ( uses[i].sesid[0] && !uses[i].logged && uses[i].last_activity < last_allowed )
            close_uses(i, NOT_CONNECTED);
    }
}


/* --------------------------------------------------------------------------
   Close anonymous user session
-------------------------------------------------------------------------- */
static void close_uses(int usi, int ci)
{
    DBG("Closing anonymous session, usi=%d, sesid [%s]", usi, uses[usi].sesid);

    if ( ci != NOT_CONNECTED )   /* still connected */
        silgy_app_session_done(ci);
    else    /* trick to maintain consistency across silgy_app_xxx functions */
    {       /* that use ci for everything -- even to get user session data */
        conn[CLOSING_SESSION_CI].usi = usi;
        silgy_app_session_done(CLOSING_SESSION_CI);
    }

    /* reset session data */

    memset(&uses[usi], 0, sizeof(usession_t));
    memset(&auses[usi], 0, sizeof(ausession_t));

    G_sessions--;

    if ( ci != NOT_CONNECTED )   /* still connected */
        conn[ci].usi = 0;

    DBG("%d session(s) remaining", G_sessions);
}


/* --------------------------------------------------------------------------
   Reset connection after processing request
-------------------------------------------------------------------------- */
static void reset_conn(int ci, char new_state)
{
#ifdef DUMP
    DBG("Resetting connection ci=%d, fd=%d, new state == %s\n", ci, conn[ci].fd, new_state==CONN_STATE_CONNECTED?"CONN_STATE_CONNECTED":"CONN_STATE_DISCONNECTED");
#endif

    conn[ci].conn_state = new_state;

    conn[ci].status = 200;
    conn[ci].method[0] = EOS;
    conn[ci].head_only = FALSE;
    conn[ci].post = FALSE;
    if ( conn[ci].in_data )
    {
        free(conn[ci].in_data);
        conn[ci].in_data = NULL;
    }
    conn[ci].was_read = 0;
    conn[ci].upgrade2https = FALSE;
    conn[ci].data_sent = 0;
    conn[ci].resource[0] = EOS;
    conn[ci].req1[0] = EOS;
    conn[ci].req2[0] = EOS;
    conn[ci].req3[0] = EOS;
    conn[ci].uagent[0] = EOS;
    conn[ci].mobile = FALSE;
    conn[ci].referer[0] = EOS;
    conn[ci].keep_alive = FALSE;
    conn[ci].proto[0] = EOS;
    conn[ci].clen = 0;
    conn[ci].cookie_in_a[0] = EOS;
    conn[ci].cookie_in_l[0] = EOS;
    conn[ci].host[0] = EOS;
    strcpy(conn[ci].website, APP_WEBSITE);
    conn[ci].lang[0] = EOS;
    conn[ci].if_mod_since = 0;
    conn[ci].in_ctypestr[0] = EOS;
    conn[ci].in_ctype = CONTENT_TYPE_UNSET;
    conn[ci].boundary[0] = EOS;
    conn[ci].authorization[0] = EOS;
    conn[ci].required_auth_level = DEF_RES_AUTH_LEVEL;

    conn[ci].out_data = conn[ci].out_data_alloc;

    if ( new_state == CONN_STATE_DISCONNECTED )
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
    REQ_BOT = FALSE;
    conn[ci].expect100 = FALSE;
    conn[ci].dont_cache = FALSE;
    conn[ci].keep_content = FALSE;
    conn[ci].accept_deflate = FALSE;

#ifdef ASYNC
    conn[ci].service[0] = EOS;
    conn[ci].async_err_code = OK;
//    conn[ci].ai = -1;
#endif

    if ( new_state == CONN_STATE_CONNECTED )
    {
#ifdef FD_MON_POLL
        M_pollfds[conn[ci].pi].events = POLLIN;
#endif
        conn[ci].last_activity = G_now;
        if ( conn[ci].usi ) US.last_activity = G_now;
    }
}


/* --------------------------------------------------------------------------
   Parse HTTP request
   Return HTTP status code
-------------------------------------------------------------------------- */
static int parse_req(int ci, int len)
{
    int  ret=200;

    /* --------------------------------------------

    Shortest valid request:

    GET / HTTP/1.1      15 including \n +
    Host: 1.1.1.1       14 including \n = 29

    -------------------------------------------- */

    DBG("parse_req, ci=%d", ci);

    conn[ci].req = ++G_cnts_today.req;    /* for reporting processing time at the end */

    DBG("\n------------------------------------------------\n %s  Request %u\n------------------------------------------------\n", G_dt, conn[ci].req);

//  if ( conn[ci].conn_state != STATE_SENDING ) /* ignore Range requests for now */
//      conn[ci].conn_state = STATE_RECEIVED;   /* by default */

    if ( len < 14 ) /* ignore any junk */
    {
        DBG("request len < 14, ignoring");
        return 400;  /* Bad Request */
    }

    /* look for end of header */

    char *p_hend = strstr(conn[ci].in, "\r\n\r\n");

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
                return 400;  /* Bad Request */
            }
        }
    }

    int hlen = p_hend - conn[ci].in;    /* HTTP header length including first of the last new line characters to simplify parsing algorithm in the third 'for' loop below */

#ifdef DUMP
    DBG("hlen = %d", hlen);
#endif

    log_long(conn[ci].in, hlen, "Incoming buffer");     /* IN_BUFSIZE > MAX_LOG_STR_LEN! */

    ++hlen;     /* HTTP header length including first of the last new line characters to simplify parsing algorithm in the third 'for' loop below */

    /* parse the header -------------------------------------------------------------------------- */

    int i;

    for ( i=0; i<hlen; ++i )    /* the first line is special -- consists of more than one token */
    {                                   /* the very first token is a request method */
        if ( isalpha(conn[ci].in[i]) )
        {
            if ( i < MAX_METHOD_LEN )
                conn[ci].method[i] = conn[ci].in[i];
            else
            {
                ERR("Method too long, ignoring");
                return 400;  /* Bad Request */
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
                WAR("Method [%s] not allowed, ignoring", conn[ci].method);
                return 405;
            }

            break;
        }
    }

    /* only for low-level tests ------------------------------------- */
//  DBG("method: [%s]", conn[ci].method);
    /* -------------------------------------------------------------- */

    i += 2;     /* skip " /" */
    int j=0;

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

#ifdef APP_ROOT_URI
    /*
       If i.e. APP_ROOT_URI: "app"
       then with URL: example.com/app/something
       we want conn[ci].uri to be "something"

       Initial conn[ci].uri: app/something
       root_uri_len = 3
    */
    int root_uri_len = strlen(APP_ROOT_URI);
    if ( 0==strncmp(conn[ci].uri, APP_ROOT_URI, root_uri_len) )
    {
        char tmp[MAX_URI_LEN+1];
        strcpy(tmp, conn[ci].uri+root_uri_len+1);
#ifdef DUMP
        DBG("tmp: [%s]", tmp);
#endif
        strcpy(conn[ci].uri, tmp);
    }
#endif  /* APP_ROOT_URI */

    /* only for low-level tests ------------------------------------- */
#ifdef DUMP
    DBG("URI: [%s]", conn[ci].uri);
#endif
    /* -------------------------------------------------------------- */

    j = 0;
    while ( i < hlen && conn[ci].in[i] != '\r' && conn[ci].in[i] != '\n' )
    {
        if ( conn[ci].in[i] != ' ' && j < 15 )
            conn[ci].proto[j++] = conn[ci].in[i];
        ++i;
    }
    conn[ci].proto[j] = EOS;
#ifdef DUMP
//    DBG("proto [%s]", conn[ci].proto);
#endif

    /* -------------------------------------------------------------- */

    char flg_data=FALSE;
    char now_label=TRUE;
    char now_value=FALSE;
    char was_cr=FALSE;
    char label[MAX_LABEL_LEN+1];
    char value[MAX_VALUE_LEN+1];

    while ( i < hlen && conn[ci].in[i] != '\n' ) ++i;

    j = 0;

    for ( i; i<hlen; ++i )   /* next lines */
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
                now_value = FALSE;
                if ( j == 0 )
                    WAR("Value of %s is empty!", label);
                else
                    if ( (ret=set_http_req_val(ci, label, value+1)) != 200 ) return ret;
            }
            now_label = TRUE;
            j = 0;
        }
        else if ( now_label && conn[ci].in[i] == ':' )   /* end of label, start of value */
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
                return 400;  /* Bad Request */
            }
        }
        else if ( now_value )   /* value */
        {
            value[j++] = conn[ci].in[i];

            if ( j == MAX_VALUE_LEN )   /* truncate here */
            {
                DBG("Truncating %s's value", label);
                value[j] = EOS;
#ifdef DUMP
                DBG("value: [%s]", value);
#endif
                if ( (ret=set_http_req_val(ci, label, value+1)) != 200 ) return ret;
                now_value = FALSE;
            }
        }
    }

    /* behave as one good web server ------------------------------------- */

#ifndef DONT_LOOK_FOR_INDEX

    if ( conn[ci].uri[0]==EOS && G_index_present && REQ_GET )
    {
        INF("Serving index.html");
        strcpy(conn[ci].uri, "index.html");
    }

#endif  /* DONT_LOOK_FOR_INDEX */

    /* split URI and resource / id --------------------------------------- */

    if ( conn[ci].uri[0] )  /* if not empty */
    {
        if ( (0==strcmp(conn[ci].uri, "favicon.ico") && !M_favicon_exists)
                || (0==strcmp(conn[ci].uri, "robots.txt") && !M_robots_exists)
                || (0==strcmp(conn[ci].uri, "apple-touch-icon.png") && !M_appleicon_exists) )
            return 404;     /* Not Found */

        /* cut query string off */

        char uri[MAX_URI_LEN+1];
        int  uri_i=0;
        while ( conn[ci].uri[uri_i] != EOS && conn[ci].uri[uri_i] != '?' )
        {
            uri[uri_i] = conn[ci].uri[uri_i];
            ++uri_i;
        }

        uri[uri_i] = EOS;

        DBG("uri w/o qs [%s]", uri);

        /* tokenize */

        const char slash[]="/";
        char *token;

        token = strtok(uri, slash);

        /* resource (REQ0) */

        if ( token )
            strncpy(conn[ci].resource, token, MAX_RESOURCE_LEN);
        else
            strncpy(conn[ci].resource, uri, MAX_RESOURCE_LEN);

        conn[ci].resource[MAX_RESOURCE_LEN] = EOS;

        /* REQ1 */

        if ( token && (token=strtok(NULL, slash)) )
        {
            strncpy(conn[ci].req1, token, MAX_RESOURCE_LEN);
            conn[ci].req1[MAX_RESOURCE_LEN] = EOS;

            /* REQ2 */

            if ( token=strtok(NULL, slash) )
            {
                strncpy(conn[ci].req2, token, MAX_RESOURCE_LEN);
                conn[ci].req2[MAX_RESOURCE_LEN] = EOS;

                /* REQ3 */

                if ( token=strtok(NULL, slash) )
                {
                    strncpy(conn[ci].req3, token, MAX_RESOURCE_LEN);
                    conn[ci].req3[MAX_RESOURCE_LEN] = EOS;
                }
            }
        }

        DBG("REQ0 [%s]", conn[ci].resource);
        DBG("REQ1 [%s]", conn[ci].req1);
        DBG("REQ2 [%s]", conn[ci].req2);
        DBG("REQ3 [%s]", conn[ci].req3);

        conn[ci].static_res = is_static_res(ci, conn[ci].uri);    /* statics --> set the flag!!! */
        /* now, it may have set conn[ci].status to 304 */

        if ( conn[ci].static_res != NOT_STATIC )    /* static resource */
            conn[ci].out_data = M_stat[conn[ci].static_res].data;
    }

    /* get the required authorization level for this resource */

    if ( conn[ci].static_res == NOT_STATIC )
    {
        i = 0;
        while ( M_auth_levels[i].resource[0] != '-' )
        {
            if ( REQ(M_auth_levels[i].resource) )
            {
                conn[ci].required_auth_level = M_auth_levels[i].level;
                break;
            }
            ++i;
        }
    }
    else    /* don't do any checks for static resources */
    {
        conn[ci].required_auth_level = AUTH_LEVEL_NONE;
    }

    /* ignore Range requests for now -------------------------------------------- */

/*  if ( conn[ci].conn_state == STATE_SENDING )
    {
        DBG("conn_state == STATE_SENDING, this request will be ignored");
        return 200;
    } */

    DBG("bot = %s", REQ_BOT?"TRUE":"FALSE");

    /* update request counters -------------------------------------------------- */

    if ( REQ_BOT )
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

        len = conn[ci].in+len - p_hend;   /* remaining request length -- likely a content */

#ifdef DUMP
        DBG("Remaining request length (content) = %d", len);
#endif

        if ( len > conn[ci].clen )
            return 400;     /* Bad Request */

        /* copy so far received POST data from conn[ci].in to conn[ci].in_data */

        if ( NULL == (conn[ci].in_data=(char*)malloc(conn[ci].clen+1)) )
        {
            ERR("Couldn't allocate %u bytes for POST data", conn[ci].clen);
            return 500;     /* Internal Sever Error */
        }

        memcpy(conn[ci].in_data, p_hend, len);
        conn[ci].was_read = len;    /* if POST then was_read applies to data section only! */

        if ( len < conn[ci].clen )      /* the whole content not received yet */
        {                               /* this is the only case when conn_state != received */
            DBG("The whole content not received yet, len=%d", len);
#ifdef DUMP
            DBG("Changing state to CONN_STATE_READING_DATA");
#endif
            conn[ci].conn_state = CONN_STATE_READING_DATA;
            return ret;
        }
        else    /* the whole content received with the header at once */
        {
            conn[ci].in_data[len] = EOS;
            DBG("POST data received with header");
        }
    }

    if ( conn[ci].status == 304 )   /* Not Modified */
        return 304;
    else
        return ret;
}


/* --------------------------------------------------------------------------
   Set request properties read from HTTP request header
   Caller is responsible for ensuring value length > 0
   Return HTTP status code
-------------------------------------------------------------------------- */
static int set_http_req_val(int ci, const char *label, const char *value)
{
    char     new_value[MAX_VALUE_LEN+1];
    char     ulabel[MAX_LABEL_LEN+1];
    char     uvalue[MAX_VALUE_LEN+1];
    char     *p;
    int      i;

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

/*      if ( !REQ_BOT &&
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
            REQ_BOT = TRUE;
        } */

        if ( !REQ_BOT &&
                (strstr(uvalue, "BOT")
                || strstr(uvalue, "SCAN")
                || strstr(uvalue, "CRAWLER")
                || strstr(uvalue, "SURDOTLY")
                || strstr(uvalue, "BAIDU")
                || strstr(uvalue, "ZGRAB")
                || strstr(uvalue, "DOMAINSONO")
                || strstr(uvalue, "NETCRAFT")
                || 0==strncmp(uvalue, "CURL", 4)
                || 0==strncmp(uvalue, "BUBING", 6)
                || 0==strncmp(uvalue, "CLOUD MAPPING", 13)
                || 0==strcmp(uvalue, "TELESPHOREO")
                || 0==strcmp(uvalue, "MAGIC BROWSER")) )
        {
            REQ_BOT = TRUE;
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
        /* it can be 'unknown' */

        char tmp[INET_ADDRSTRLEN+1];
        int len = strlen(value);
        i = 0;

        while ( i<len && (value[i]=='.' || isdigit(value[i])) && i<INET_ADDRSTRLEN )
        {
            tmp[i] = value[i];
            ++i;
        }

        tmp[i] = EOS;

        DBG("%s's value: [%s]", label, tmp);

        if ( strlen(tmp) > 6 )
            strcpy(conn[ci].ip, tmp);
    }
    else if ( 0==strcmp(ulabel, "CONTENT-LENGTH") )
    {
        conn[ci].clen = atoi(value);
        if ( conn[ci].clen < 0 || (!conn[ci].post && conn[ci].clen >= IN_BUFSIZE) || (conn[ci].post && conn[ci].clen >= MAX_PAYLOAD_SIZE-1) )
        {
            ERR("Request too long, clen = %u, sending 413", conn[ci].clen);
            return 413;
        }
        DBG("conn[ci].clen = %u", conn[ci].clen);
    }
    else if ( 0==strcmp(ulabel, "ACCEPT-ENCODING") )    /* gzip, deflate, br */
    {
        strcpy(uvalue, upper(value));
        if ( strstr(uvalue, "DEFLATE") )
            conn[ci].accept_deflate = TRUE;
        DBG("accept_deflate = %d", conn[ci].accept_deflate);
    }
    else if ( 0==strcmp(ulabel, "ACCEPT-LANGUAGE") )    /* en-US en-GB pl-PL */
    {
        i = 0;
        while ( value[i] != EOS && value[i] != ',' && value[i] != ';' && i < LANG_LEN )
        {
            conn[ci].lang[i] = toupper(value[i]);
            ++i;
        }

        conn[ci].lang[i] = EOS;

        DBG("conn[ci].lang: [%s]", conn[ci].lang);
    }
    else if ( 0==strcmp(ulabel, "CONTENT-TYPE") )
    {
        strcpy(conn[ci].in_ctypestr, value);

        int len = strlen(value);

        if ( len > 32 && 0==strncmp(value, "application/x-www-form-urlencoded", 33) )
        {
            conn[ci].in_ctype = CONTENT_TYPE_URLENCODED;
        }
        else if ( len > 18 && 0==strncmp(value, "multipart/form-data", 19) )
        {
            conn[ci].in_ctype = CONTENT_TYPE_MULTIPART;

            if ( p=(char*)strstr(value, "boundary=") )
            {
                strcpy(conn[ci].boundary, p+9);
                DBG("boundary: [%s]", conn[ci].boundary);
            }
        }
        else if ( len > 23 && 0==strncmp(value, "application/octet-stream", 24) )
        {
            conn[ci].in_ctype = CONTENT_TYPE_OCTET_STREAM;
        }
    }
    else if ( 0==strcmp(ulabel, "AUTHORIZATION") )
    {
        strcpy(conn[ci].authorization, value);
    }
    else if ( 0==strcmp(ulabel, "FROM") )
    {
        strcpy(uvalue, upper(value));
        if ( !REQ_BOT && (strstr(uvalue, "GOOGLEBOT") || strstr(uvalue, "BINGBOT") || strstr(uvalue, "YANDEX") || strstr(uvalue, "CRAWLER")) )
            REQ_BOT = TRUE;
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
   It doesn't really change anything security-wise but just saves bandwidth
-------------------------------------------------------------------------- */
static bool check_block_ip(int ci, const char *rule, const char *value)
{
    if ( G_test ) return FALSE;    /* don't block for tests */

#ifdef BLACKLISTAUTOUPDATE
    if ( (rule[0]=='H' && conn[ci].post && 0==strcmp(value, APP_IP))        /* Host */
            || (rule[0]=='U' && 0==strcmp(value, "Mozilla/5.0 Jorgee"))     /* User-Agent */
            || (rule[0]=='R' && 0==strcmp(value, "wp-login.php"))           /* Resource */
            || (rule[0]=='R' && 0==strcmp(value, "wp-config.php"))          /* Resource */
            || (rule[0]=='R' && 0==strcmp(value, "administrator"))          /* Resource */
            || (rule[0]=='R' && 0==strcmp(value, "phpmyadmin"))             /* Resource */
            || (rule[0]=='R' && 0==strcmp(value, "java.php"))               /* Resource */
            || (rule[0]=='R' && 0==strcmp(value, "logon.php"))              /* Resource */
            || (rule[0]=='R' && 0==strcmp(value, "log.php"))                /* Resource */
            || (rule[0]=='R' && 0==strcmp(value, "hell.php"))               /* Resource */
            || (rule[0]=='R' && 0==strcmp(value, "shell.php"))              /* Resource */
            || (rule[0]=='R' && 0==strcmp(value, "tty.php"))                /* Resource */
            || (rule[0]=='R' && 0==strcmp(value, "cmd.php"))                /* Resource */
            || (rule[0]=='R' && 0==strcmp(value, ".env"))                   /* Resource */
            || (rule[0]=='R' && strstr(value, "setup.php")) )               /* Resource */
    {
        eng_block_ip(conn[ci].ip, TRUE);
        conn[ci].keep_alive = FALSE;    /* disconnect */
        return TRUE;
    }
#endif

    return FALSE;
}


/* --------------------------------------------------------------------------
   Return HTTP status description
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
    ALWAYS("            req: %u", G_cnts_today.req);
    ALWAYS("        req_dsk: %u", G_cnts_today.req_dsk);
    ALWAYS("        req_mob: %u", G_cnts_today.req_mob);
    ALWAYS("        req_bot: %u", G_cnts_today.req_bot);
    ALWAYS("         visits: %u", G_cnts_today.visits);
    ALWAYS("     visits_dsk: %u", G_cnts_today.visits_dsk);
    ALWAYS("     visits_mob: %u", G_cnts_today.visits_mob);
    ALWAYS("        blocked: %u", G_cnts_today.blocked);
//       DBG("        elapsed: %.3lf ms", G_cnts_today.elapsed);
    ALWAYS("        average: %.3lf ms", G_cnts_today.average);
    ALWAYS("connections HWM: %d", G_open_conn_hwm);
    ALWAYS("   sessions HWM: %d", G_sessions_hwm);
    ALWAYS("");
}


/* --------------------------------------------------------------------------
   Clean up
-------------------------------------------------------------------------- */
static void clean_up()
{
    char command[1024];

    M_shutdown = TRUE;

    ALWAYS("");
    ALWAYS("Cleaning up...\n");
    lib_log_memory();
    dump_counters();

    silgy_app_done();

    if ( access(M_pidfile, F_OK) != -1 )
    {
        DBG("Removing pid file...");
#ifdef _WIN32   /* Windows */
        sprintf(command, "del %s", M_pidfile);
#else
        sprintf(command, "rm %s", M_pidfile);
#endif
        system(command);
    }

#ifdef DBMYSQL
    lib_close_db();
#endif
#ifdef HTTPS
    SSL_CTX_free(M_ssl_ctx);
    EVP_cleanup();
#endif
#ifdef ASYNC
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
#endif

#ifdef _WIN32   /* Windows */
    WSACleanup();
#endif  /* _WIN32 */

    silgy_lib_done();
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
   Generic message page
-------------------------------------------------------------------------- */
static void gen_page_msg(int ci, int code)
{
    DBG("gen_page_msg");

#ifdef APP_ERROR_PAGE

    silgy_app_error_page(ci, code);

#else

    OUT("<!DOCTYPE html>");
    OUT("<html>");
    OUT("<head>");
    OUT("<title>%s</title>", APP_WEBSITE);
    if ( REQ_MOB )  // if mobile request
        OUT("<meta name=\"viewport\" content=\"width=device-width\">");
    OUT("</head>");
    OUT("<body><p>%s</p></body>", silgy_message(code));
    OUT("</html>");

#endif  /* APP_ERROR_PAGE */
}


/* --------------------------------------------------------------------------
   Init SSL for a server
-------------------------------------------------------------------------- */
static bool init_ssl()
{
#ifdef HTTPS
    const SSL_METHOD *method;
    /*
       From Hynek Schlawack's blog:
       https://hynek.me/articles/hardening-your-web-servers-ssl-ciphers
       https://www.ssllabs.com/ssltest
       Last update: 2019-04-08
       Qualys says Forward Secrecy isn't enabled
    */
//    char ciphers[1024]="ECDH+AESGCM:ECDH+CHACHA20:DH+AESGCM:ECDH+AES256:DH+AES256:ECDH+AES128:DH+AES:RSA+AESGCM:RSA+AES:!aNULL:!MD5:!DSS";

    /*
       https://www.digicert.com/ssl-support/ssl-enabling-perfect-forward-secrecy.htm
       Last update: 2019-04-18
    */
    char ciphers[1024]="EECDH+ECDSA+AESGCM EECDH+aRSA+AESGCM EECDH+ECDSA+SHA384 EECDH+ECDSA+SHA256 EECDH+aRSA+SHA384 EECDH+aRSA+SHA256 EECDH+aRSA+RC4 EECDH EDH+aRSA RC4 !aNULL !eNULL !LOW !3DES !MD5 !EXP !PSK !SRP !DSS !RC4";

    DBG("init_ssl");

    /* libssl init */
    SSL_library_init();
    SSL_load_error_strings();

    /* libcrypto init */
    OpenSSL_add_all_algorithms();
    ERR_load_crypto_strings();

    G_ssl_lib_initialized = TRUE;

    method = SSLv23_server_method();    /* negotiate the highest protocol version supported by both the server and the client */

    M_ssl_ctx = SSL_CTX_new(method);    /* create new context from method */

    if ( M_ssl_ctx == NULL )
    {
        ERR("SSL_CTX_new failed");
        return FALSE;
    }

    const long flags = SSL_OP_ALL | SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_TLSv1;
    SSL_CTX_set_options(M_ssl_ctx, flags);

    /* support ECDH using the most appropriate shared curve */

    if ( SSL_CTX_set_ecdh_auto(M_ssl_ctx, 1) <= 0 )
    {
        ERR("SSL_CTX_set_ecdh_auto failed");
        return FALSE;
    }

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
   Set required authorization level for the resource
-------------------------------------------------------------------------- */
void silgy_set_auth_level(const char *resource, short level)
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
int eng_uses_start(int ci, const char *sesid)
{
    int     i;
    char    new_sesid[SESID_LEN+1];

    DBG("eng_uses_start");

    if ( G_sessions == MAX_SESSIONS )
    {
        WAR("User sessions exhausted");
        return ERR_SERVER_TOOBUSY;
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

    if ( sesid )
    {
        strcpy(new_sesid, sesid);
    }
    else    /* generate sesid */
    {
        silgy_random(new_sesid, SESID_LEN);
    }

    INF("Starting new session, usi=%d, sesid [%s]", conn[ci].usi, new_sesid);

    /* add record to uses */

    strcpy(US.sesid, new_sesid);
    strcpy(US.ip, conn[ci].ip);
    strcpy(US.uagent, conn[ci].uagent);
    strcpy(US.referer, conn[ci].referer);
    strcpy(US.lang, conn[ci].lang);

    lib_set_datetime_formats(US.lang);

    /* custom session init */

    if ( !silgy_app_session_init(ci) )
    {
        close_uses(conn[ci].usi, ci);
        return ERR_INT_SERVER_ERROR;
    }

    /* set 'as' cookie */

    strcpy(conn[ci].cookie_out_a, new_sesid);

    DBG("%d user session(s)", G_sessions);

    if ( G_sessions > G_sessions_hwm )
        G_sessions_hwm = G_sessions;

    return OK;
}


/* --------------------------------------------------------------------------
   Send asynchronous request
-------------------------------------------------------------------------- */
void eng_async_req(int ci, const char *service, const char *data, char response, int timeout, int size)
{
#ifdef ASYNC

    async_req_t req;

    if ( M_last_call_id >= 1000000000 ) M_last_call_id = 0;

    req.hdr.call_id = ++M_last_call_id;
    req.hdr.ci = ci;

    if ( service )
        strcpy(req.hdr.service, service);
    else
        req.hdr.service[0] = EOS;

    req.hdr.response = response;

    /* conn */

    strcpy(req.hdr.ip, conn[ci].ip);
    strcpy(req.hdr.method, conn[ci].method);
    req.hdr.post = conn[ci].post;
    strcpy(req.hdr.uri, conn[ci].uri);
    strcpy(req.hdr.resource, conn[ci].resource);
    strcpy(req.hdr.uagent, conn[ci].uagent);
    req.hdr.mobile = conn[ci].mobile;
    req.hdr.clen = conn[ci].clen;

    /* For POST, the payload can be in the data space of the message,
       or -- if it's bigger -- in the shared memory */

    if ( conn[ci].post )
    {
        if ( conn[ci].clen < G_async_req_data_size )
        {
            DBG("Payload fits in msg");

            memcpy(req.data, conn[ci].in_data, conn[ci].clen+1);
            req.hdr.payload_location = ASYNC_PAYLOAD_MSG;
        }
        else    /* ASYNC_PAYLOAD_SHM */
        {
            DBG("Payload requires SHM");

            if ( !M_async_shm )
            {
                if ( (M_async_shm=lib_shm_create(MAX_PAYLOAD_SIZE, 0)) == NULL )
                {
                    ERR("Couldn't create SHM");
                    return;
                }

                M_async_shm[MAX_PAYLOAD_SIZE-1] = 0;
            }

            /* use the last byte as a simple semaphore */

            while ( M_async_shm[MAX_PAYLOAD_SIZE-1] )
            {
                WAR("Waiting for the SHM segment to be freed by silgy_svc process");
                msleep(100);   /* temporarily */
            }

            memcpy(M_async_shm, conn[ci].in_data, conn[ci].clen+1);
            M_async_shm[MAX_PAYLOAD_SIZE-1] = 1;
            req.hdr.payload_location = ASYNC_PAYLOAD_SHM;
        }
    }

    strcpy(req.hdr.host, conn[ci].host);
    strcpy(req.hdr.website, conn[ci].website);
    strcpy(req.hdr.lang, conn[ci].lang);
    req.hdr.in_ctype = conn[ci].in_ctype;
    strcpy(req.hdr.boundary, conn[ci].boundary);
    req.hdr.status = conn[ci].status;
    req.hdr.ctype = conn[ci].ctype;

    /* pass user session */

    if ( conn[ci].usi )
    {
        memcpy(&req.hdr.uses, &US, sizeof(usession_t));
#ifndef ASYNC_EXCLUDE_AUSES
        memcpy(&req.hdr.auses, &AUS, sizeof(ausession_t));
#endif
    }
    else
    {
        memset(&req.hdr.uses, 0, sizeof(usession_t));
#ifndef ASYNC_EXCLUDE_AUSES
        memset(&req.hdr.auses, 0, sizeof(ausession_t));
#endif
    }

    /* counters */

    memcpy(&req.hdr.cnts_today, &G_cnts_today, sizeof(counters_t));
    memcpy(&req.hdr.cnts_yesterday, &G_cnts_yesterday, sizeof(counters_t));
    memcpy(&req.hdr.cnts_day_before, &G_cnts_day_before, sizeof(counters_t));

    req.hdr.days_up = G_days_up;
    req.hdr.open_conn = G_open_conn;
    req.hdr.open_conn_hwm = G_open_conn_hwm;
    req.hdr.sessions = G_sessions;
    req.hdr.sessions_hwm = G_sessions_hwm;

    req.hdr.blacklist_cnt = G_blacklist_cnt;

    /* other */

    strcpy(req.hdr.cookie_out_a, conn[ci].cookie_out_a);
    strcpy(req.hdr.cookie_out_a_exp, conn[ci].cookie_out_a_exp);
    strcpy(req.hdr.cookie_out_l, conn[ci].cookie_out_l);
    strcpy(req.hdr.cookie_out_l_exp, conn[ci].cookie_out_l_exp);

    strcpy(req.hdr.last_modified, G_last_modified);


    bool found=0;

    if ( response )     /* we will wait */
    {
        /* add to areqs (async response array) */

        int j;

        for ( j=0; j<MAX_ASYNC_REQS; ++j )
        {
            if ( areqs[j].state == ASYNC_STATE_FREE )    /* free slot */
            {
                DBG("free slot %d found in areqs", j);
                areqs[j].ci = ci;
                areqs[j].state = ASYNC_STATE_SENT;
                areqs[j].sent = G_now;
                if ( timeout < 0 ) timeout = 0;
                if ( timeout == 0 || timeout > ASYNC_MAX_TIMEOUT ) timeout = ASYNC_MAX_TIMEOUT;
                areqs[j].timeout = timeout;
                req.hdr.ai = j;
                found = 1;
                break;
            }
        }

        if ( found )
        {
            /* set request state */
#ifdef DUMP
            DBG("Changing state to CONN_STATE_WAITING_FOR_ASYNC");
#endif
            conn[ci].conn_state = CONN_STATE_WAITING_FOR_ASYNC;

#ifdef FD_MON_POLL
            M_pollfds[conn[ci].pi].events = POLLOUT;
#endif
        }
        else
        {
            ERR("areqs is full");
        }
    }

    if ( found || !response )
    {
        DBG("Sending a message on behalf of ci=%d, call_id=%u, service [%s]", ci, req.hdr.call_id, req.hdr.service);
        if ( mq_send(G_queue_req, (char*)&req, ASYNC_REQ_MSG_SIZE, 0) != 0 )
            ERR("mq_send failed, errno = %d (%s)", errno, strerror(errno));
    }

#endif
}


/* --------------------------------------------------------------------------
   Set internal (generated) static resource data & size
-------------------------------------------------------------------------- */
void silgy_add_to_static_res(const char *name, const char *src)
{
    int i;

    i = first_free_stat();

    strcpy(M_stat[i].name, name);

    M_stat[i].len = strlen(src);   /* internal are text based */

#ifdef SEND_ALL_AT_ONCE

    if ( NULL == (M_stat[i].data=(char*)malloc(M_stat[i].len+1+OUT_HEADER_BUFSIZE)) )
    {
        ERR("Couldn't allocate %u bytes for %s", M_stat[i].len+1+OUT_HEADER_BUFSIZE, M_stat[i].name);
        return;
    }

    strcpy(M_stat[i].data+OUT_HEADER_BUFSIZE, src);

#else

    if ( NULL == (M_stat[i].data=(char*)malloc(M_stat[i].len+1)) )
    {
        ERR("Couldn't allocate %u bytes for %s", M_stat[i].len+1, M_stat[i].name);
        return;
    }

    strcpy(M_stat[i].data, src);

#endif  /* SEND_ALL_AT_ONCE */

    M_stat[i].type = get_res_type(M_stat[i].name);
    M_stat[i].modified = G_now;
    M_stat[i].source = STATIC_SOURCE_INTERNAL;

    INF("%s (%u bytes)", M_stat[i].name, M_stat[i].len);
}


/* --------------------------------------------------------------------------
   Add to blocked IP
-------------------------------------------------------------------------- */
void eng_block_ip(const char *value, bool autoblocked)
{
    char    fname[1024];
    char    comm[1024];

    if ( G_blockedIPList[0] == EOS ) return;

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
        strcpy(fname, G_blockedIPList);
    else if ( G_appdir[0] )
        sprintf(fname, "%s/bin/%s", G_appdir, G_blockedIPList);
    else
        strcpy(fname, G_blockedIPList);

    sprintf(comm, "echo \"%s\t# %sblocked on %s\" >> %s", value, autoblocked?"auto":"", G_dt, fname);
    system(comm);

    WAR("IP %s blacklisted", value);
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
   Write string to output buffer with buffer overwrite protection
-------------------------------------------------------------------------- */
void eng_out_check(int ci, const char *str)
{
    int available = OUT_BUFSIZE - (conn[ci].p_content - conn[ci].out_data);

    if ( strlen(str) < available )  /* the whole string will fit */
    {
        conn[ci].p_content = stpcpy(conn[ci].p_content, str);
    }
    else    /* let's write only what we can. WARNING: no UTF-8 checking is done here! */
    {
        conn[ci].p_content = stpncpy(conn[ci].p_content, str, available-1);
        *conn[ci].p_content = EOS;
    }
}


/* --------------------------------------------------------------------------
   Write string to output buffer with buffer resizing if necessary
-------------------------------------------------------------------------- */
void eng_out_check_realloc(int ci, const char *str)
{
    if ( strlen(str) < conn[ci].out_data_allocated - (conn[ci].p_content-conn[ci].out_data) )    /* the whole string will fit */
    {
        conn[ci].p_content = stpcpy(conn[ci].p_content, str);
    }
    else    /* resize output buffer and try again */
    {
        unsigned used = conn[ci].p_content - conn[ci].out_data;
        char *tmp = (char*)realloc(conn[ci].out_data_alloc, conn[ci].out_data_allocated*2);
        if ( !tmp )
        {
            ERR("Couldn't reallocate output buffer for ci=%d, tried %u bytes", ci, conn[ci].out_data_allocated*2);
            return;
        }
        conn[ci].out_data_alloc = tmp;
        conn[ci].out_data = conn[ci].out_data_alloc;
        conn[ci].out_data_allocated = conn[ci].out_data_allocated * 2;
        conn[ci].p_content = conn[ci].out_data + used;
        INF("Reallocated output buffer for ci=%d, new size = %u bytes", ci, conn[ci].out_data_allocated);
        eng_out_check_realloc(ci, str);     /* call itself! */
    }
}


/* --------------------------------------------------------------------------
   Write binary data to output buffer with buffer resizing if necessary
-------------------------------------------------------------------------- */
void eng_out_check_realloc_bin(int ci, const char *data, int len)
{
    if ( len < conn[ci].out_data_allocated - (conn[ci].p_content - conn[ci].out_data) )    /* the whole data will fit */
    {
        memcpy(conn[ci].p_content, data, len);
        conn[ci].p_content += len;
    }
    else    /* resize output buffer and try again */
    {
        unsigned used = conn[ci].p_content - conn[ci].out_data;
        char *tmp = (char*)realloc(conn[ci].out_data_alloc, conn[ci].out_data_allocated*2);
        if ( !tmp )
        {
            ERR("Couldn't reallocate output buffer for ci=%d, tried %u bytes", ci, conn[ci].out_data_allocated*2);
            return;
        }
        conn[ci].out_data_alloc = tmp;
        conn[ci].out_data = conn[ci].out_data_alloc;
        conn[ci].out_data_allocated = conn[ci].out_data_allocated * 2;
        conn[ci].p_content = conn[ci].out_data + used;
        INF("Reallocated output buffer for ci=%d, new size = %u bytes", ci, conn[ci].out_data_allocated);
        eng_out_check_realloc_bin(ci, data, len);       /* call itself! */
    }
}


/* --------------------------------------------------------------------------
   Return request header value
-------------------------------------------------------------------------- */
char *eng_get_header(int ci, const char *header)
{
static char value[MAX_VALUE_LEN+1];
    char uheader[MAX_LABEL_LEN+1];

    strcpy(uheader, upper(header));

    if ( 0==strcmp(uheader, "CONTENT-TYPE") )
    {
        strcpy(value, conn[ci].in_ctypestr);
        return value;
    }
    else if ( 0==strcmp(uheader, "AUTHORIZATION") )
    {
        strcpy(value, conn[ci].authorization);
        return value;
    }
    else
    {
        return NULL;
    }
}


/* --------------------------------------------------------------------------
   REST call -- pass request header value from the original request
-------------------------------------------------------------------------- */
void eng_rest_header_pass(int ci, const char *key)
{
    char value[MAX_VALUE_LEN+1];

    strcpy(value, eng_get_header(ci, key));

    if ( value[0] )
        REST_HEADER_SET(key, value);
}


/* --------------------------------------------------------------------------
   Blacklist IP
-------------------------------------------------------------------------- */
static void do_add2blocked(int ci)
{
    QSVAL   ip;
    char    comm[1024];

    INF("do_add2blocked");

    OUT_HTML_HEADER;

    if ( G_blacklist_cnt > MAX_BLACKLIST-1 )
    {
        WAR("G_blacklist_cnt at max (%d)!", MAX_BLACKLIST);
        OUT("<p class=m50>ERROR: Blacklist already full!</p>");
    }
    else    /* some rudimentary validation */
    {
        if ( !QS("ip", ip) )
        {
            ERR("ip expected in URI");
            OUT("<p class=m50>ERROR: ip expected in URI!</p>");
        }
        else if ( strlen(ip) > INET_ADDRSTRLEN-1 )
        {
            ERR("ip too long");
            OUT("<p class=m50>ERROR: ip too long!</p>");
        }
        else if ( strlen(ip) < 7 )
        {
            ERR("ip too short");
            OUT("<p class=m50>ERROR: ip too short!</p>");
        }
        else if ( !isdigit(ip[0]) )
        {
            ERR("ip does not start with digit");
            OUT("<p class=m50>ERROR: ip does not start with digit!</p>");
        }
        else
        {
            eng_block_ip(ip, FALSE);
            WAR("IP %s manually blacklisted", ip);
            OUT("<p class=m50>IP %s blacklisted.</p>", ip);
        }
    }

    OUT("<p class=m15><a href=\"/\"><< Back to Main</a></p>");

    OUT_HTML_FOOTER;

    RES_DONT_CACHE;
}



#else   /* SILGY_SVC ====================================================================================== */



char        G_service[SVC_NAME_LEN+1];
int         G_error_code=OK;
int         G_ASYNCId=-1;
char        G_req_queue_name[256]="";
char        G_res_queue_name[256]="";
mqd_t       G_queue_req={0};            /* request queue */
mqd_t       G_queue_res={0};            /* response queue */
int         G_async_req_data_size=ASYNC_REQ_MSG_SIZE-sizeof(async_req_hdr_t); /* how many bytes are left for data */
int         G_async_res_data_size=ASYNC_RES_MSG_SIZE-sizeof(async_res_hdr_t)-sizeof(int)*4; /* how many bytes are left for data */
int         G_usersRequireAccountActivation=0;
async_req_t req;
async_res_t res;
#ifdef OUTCHECKREALLOC
char        *out_data=NULL;
unsigned    out_data_allocated;
#endif
char        *p_content=NULL;
conn_t      conn[MAX_CONNECTIONS+1]={0}; /* request details */
int         ci=0;
usession_t  uses[MAX_SESSIONS+1]={0};   /* user sessions -- they start from 1 */
ausession_t auses[MAX_SESSIONS+1]={0};  /* app user sessions, using the same index (usi) */

/* counters */

counters_t  G_cnts_today={0};           /* today's counters */
counters_t  G_cnts_yesterday={0};       /* yesterday's counters */
counters_t  G_cnts_day_before={0};      /* day before's counters */

unsigned    G_days_up=0;                /* web server's days up */
int         G_open_conn=0;              /* number of open connections */
int         G_open_conn_hwm=0;          /* highest number of open connections (high water mark) */
int         G_sessions=0;               /* number of active user sessions */
int         G_sessions_hwm=0;           /* highest number of active user sessions (high water mark) */
int         G_blacklist_cnt=0;          /* G_blacklist length */
char        G_last_modified[32]="";     /* response header field with server's start time */


#ifdef DBMYSQL
MYSQL       *G_dbconn=NULL;             /* database connection */
char        G_dbHost[128]="";
int         G_dbPort=0;
char        G_dbName[128]="";
char        G_dbUser[128]="";
char        G_dbPassword[128]="";
#endif


static char *M_pidfile;                 /* pid file name */
static char *M_async_shm=NULL;


static void sigdisp(int sig);
static void clean_up(void);


/* --------------------------------------------------------------------------
   main
-------------------------------------------------------------------------- */
int main(int argc, char *argv[])
{
    char config[256];

    /* library init ------------------------------------------------------ */

    silgy_lib_init();

#ifdef USERS
    libusr_init();
#endif

    /* read the config file or set defaults ------------------------------ */

    char exec_name[256];
    lib_get_exec_name(exec_name, argv[0]);

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

    if ( !silgy_read_param_int("logLevel", &G_logLevel) )
        G_logLevel = 3;  /* info */

    if ( !silgy_read_param_int("logToStdout", &G_logToStdout) )
        G_logToStdout = 0;

    if ( !silgy_read_param_int("ASYNCId", &G_ASYNCId) )
        G_ASYNCId = -1;

    if ( !silgy_read_param_int("RESTTimeout", &G_RESTTimeout) )
        G_RESTTimeout = CALL_REST_DEFAULT_TIMEOUT;

#ifdef DBMYSQL
    silgy_read_param_str("dbHost", G_dbHost);
    silgy_read_param_int("dbPort", &G_dbPort);
    silgy_read_param_str("dbName", G_dbName);
    silgy_read_param_str("dbUser", G_dbUser);
    silgy_read_param_str("dbPassword", G_dbPassword);
#endif  /* DBMYSQL */
#ifdef USERS
    silgy_read_param_int("usersRequireAccountActivation", &G_usersRequireAccountActivation);
#endif  /* USERS */

    /* start log --------------------------------------------------------- */

    char logprefix[64];

    sprintf(logprefix, "s_%d", G_pid);

    if ( !log_start(logprefix, G_test) )
        return EXIT_FAILURE;

    /* pid file ---------------------------------------------------------- */

    if ( !(M_pidfile=lib_create_pid_file(logprefix)) )
        return EXIT_FAILURE;

    /* fill the M_random_numbers up */

    init_random_numbers();

    /* handle signals ---------------------------------------------------- */

    signal(SIGINT,  sigdisp);   /* Ctrl-C */
    signal(SIGTERM, sigdisp);
#ifndef _WIN32
    signal(SIGQUIT, sigdisp);   /* Ctrl-\ */
    signal(SIGTSTP, sigdisp);   /* Ctrl-Z */

    signal(SIGPIPE, SIG_IGN);   /* ignore SIGPIPE */
#endif

    /* init dummy conn structure ----------------------------------------- */

    strcpy(conn[0].host, APP_DOMAIN);
    strcpy(conn[0].website, APP_WEBSITE);

    if ( !(conn[0].in_data = (char*)malloc(G_async_req_data_size)) )
    {
        ERR("malloc for conn[0].in_data failed");
        return EXIT_FAILURE;
    }

    conn[0].in_data_allocated = G_async_req_data_size;

#ifdef OUTCHECKREALLOC

    if ( !(out_data = (char*)malloc(OUT_BUFSIZE)) )
    {
        ERR("malloc for out_data failed");
        return EXIT_FAILURE;
    }

    out_data_allocated = OUT_BUFSIZE;

#endif  /* OUTCHECKREALLOC */

    /* open database ----------------------------------------------------- */

#ifdef DBMYSQL

    DBG("Trying lib_open_db...");

    if ( !lib_open_db() )
    {
        ERR("lib_open_db failed");
        clean_up();
        return EXIT_FAILURE;
    }

    ALWAYS("Database connected");

#endif  /* DBMYSQL */


    /* open queues ------------------------------------------------------- */

#ifdef APP_ASYNC_ID
    if ( G_ASYNCId > -1 )
    {
        sprintf(G_req_queue_name, "%s_%d__%d", ASYNC_REQ_QUEUE, APP_ASYNC_ID, G_ASYNCId);
        sprintf(G_res_queue_name, "%s_%d__%d", ASYNC_RES_QUEUE, APP_ASYNC_ID, G_ASYNCId);
    }
    else
    {
        sprintf(G_req_queue_name, "%s_%d", ASYNC_REQ_QUEUE, APP_ASYNC_ID);
        sprintf(G_res_queue_name, "%s_%d", ASYNC_RES_QUEUE, APP_ASYNC_ID);
    }
#else
    if ( G_ASYNCId > -1 )
    {
        sprintf(G_req_queue_name, "%s__%d", ASYNC_REQ_QUEUE, G_ASYNCId);
        sprintf(G_res_queue_name, "%s__%d", ASYNC_RES_QUEUE, G_ASYNCId);
    }
    else
    {
        strcpy(G_req_queue_name, ASYNC_REQ_QUEUE);
        strcpy(G_res_queue_name, ASYNC_RES_QUEUE);
    }
#endif

    G_queue_req = mq_open(G_req_queue_name, O_RDONLY, NULL, NULL);

    if ( G_queue_req < 0 )
    {
        ERR("mq_open for req failed, errno = %d (%s)", errno, strerror(errno));
        clean_up();
        return EXIT_FAILURE;
    }

    INF("mq_open %s OK", G_req_queue_name);

    G_queue_res = mq_open(G_res_queue_name, O_WRONLY, NULL, NULL);

    if ( G_queue_res < 0 )
    {
        ERR("mq_open for res failed, errno = %d (%s)", errno, strerror(errno));
        clean_up();
        return EXIT_FAILURE;
    }

    INF("mq_open %s OK", G_res_queue_name);

    /* ------------------------------------------------------------------- */

    if ( !silgy_svc_init() )
    {
        ERR("silgy_svc_init failed");
        clean_up();
        return EXIT_FAILURE;
    }

    /* ------------------------------------------------------------------- */

    int prev_day = G_ptm->tm_mday;

    INF("\nWaiting...\n");

    while (1)
    {
        G_rest_req = 0;
        G_rest_elapsed = 0;
        G_rest_average = 0;

        if ( mq_receive(G_queue_req, (char*)&req, ASYNC_REQ_MSG_SIZE, NULL) != -1 )
        {
            lib_update_time_globals();
            
            /* start new log file every day */

            if ( G_ptm->tm_mday != prev_day )
            {
                log_finish();

                if ( !log_start(logprefix, G_test) )
                {
                    clean_up();
                    return EXIT_FAILURE;
                }

                prev_day = G_ptm->tm_mday;

                init_random_numbers();
            }

            DBG_T("Message received");

            if ( G_logLevel > LOG_INF )
                DBG_T("ci=%d, service [%s], call_id=%u", req.hdr.ci, req.hdr.service, req.hdr.call_id);
            else
                INF_T("%s called (call_id=%u)", req.hdr.service, req.hdr.call_id);

            res.ai = req.hdr.ai;
            res.ci = req.hdr.ci;
            strcpy(G_service, req.hdr.service);

            /* request details */

            strcpy(conn[0].ip, req.hdr.ip);
            strcpy(conn[0].method, req.hdr.method);
            conn[0].post = req.hdr.post;
            strcpy(conn[0].uri, req.hdr.uri);
            strcpy(conn[0].resource, req.hdr.resource);
            strcpy(conn[0].uagent, req.hdr.uagent);
            conn[0].mobile = req.hdr.mobile;
            conn[0].clen = req.hdr.clen;

            /* For POST, the payload can be in the data space of the message,
               or -- if it's bigger -- in the shared memory */

            if ( req.hdr.post )
            {
                if ( req.hdr.payload_location == ASYNC_PAYLOAD_MSG )
                {
                    memcpy(conn[0].in_data, req.data, req.hdr.clen+1);
                }
                else    /* ASYNC_PAYLOAD_SHM */
                {
                    if ( conn[0].in_data_allocated < req.hdr.clen+1 )
                    {
                        char *tmp = (char*)realloc(conn[0].in_data, req.hdr.clen+1);
                        if ( !tmp )
                        {
                            ERR("Couldn't realloc in_data, tried %u bytes", req.hdr.clen+1);
                            continue;
                        }
                        conn[0].in_data = tmp;
                        conn[0].in_data_allocated = req.hdr.clen+1;
                        INF("Reallocated in_data, new size = %u bytes", req.hdr.clen+1);
                    }

                    if ( !M_async_shm )
                    {
                        if ( (M_async_shm=lib_shm_create(MAX_PAYLOAD_SIZE, 0)) == NULL )
                        {
                            ERR("Couldn't attach to SHM");
                            continue;
                        }
                    }

                    memcpy(conn[0].in_data, M_async_shm, req.hdr.clen+1);

                    /* mark it as free */
                    M_async_shm[MAX_PAYLOAD_SIZE-1] = 0;
                }
            }

            strcpy(conn[0].host, req.hdr.host);
            strcpy(conn[0].website, req.hdr.website);
            strcpy(conn[0].lang, req.hdr.lang);
            conn[0].in_ctype = req.hdr.in_ctype;
            strcpy(conn[0].boundary, req.hdr.boundary);
            conn[0].status = req.hdr.status;
            conn[0].ctype = req.hdr.ctype;

            /* ----------------------------------------------------------- */

            DBG("Processing...");

            /* response data */

#ifdef OUTCHECKREALLOC
            p_content = out_data;
#else
            p_content = res.data;
#endif

            /* user session */

            memcpy(&uses[1], &req.hdr.uses, sizeof(usession_t));
#ifndef ASYNC_EXCLUDE_AUSES
            memcpy(&auses[1], &req.hdr.auses, sizeof(ausession_t));
#endif
            if ( uses[1].sesid[0] )
                conn[0].usi = 1;    /* user session present */
            else
                conn[0].usi = 0;    /* no session */

            /* counters */

            memcpy(&G_cnts_today, &req.hdr.cnts_today, sizeof(counters_t));
            memcpy(&G_cnts_yesterday, &req.hdr.cnts_yesterday, sizeof(counters_t));
            memcpy(&G_cnts_day_before, &req.hdr.cnts_day_before, sizeof(counters_t));
            
            G_days_up = req.hdr.days_up;
            G_open_conn = req.hdr.open_conn;
            G_open_conn_hwm = req.hdr.open_conn_hwm;
            G_sessions = req.hdr.sessions;
            G_sessions_hwm = req.hdr.sessions_hwm;

            G_blacklist_cnt = req.hdr.blacklist_cnt;

            /* other */

            strcpy(conn[0].cookie_out_a, req.hdr.cookie_out_a);
            strcpy(conn[0].cookie_out_a_exp, req.hdr.cookie_out_a_exp);
            strcpy(conn[0].cookie_out_l, req.hdr.cookie_out_l);
            strcpy(conn[0].cookie_out_l_exp, req.hdr.cookie_out_l_exp);

            strcpy(G_last_modified, req.hdr.last_modified);

            /* ----------------------------------------------------------- */

            silgy_svc_main();

            /* ----------------------------------------------------------- */

            if ( req.hdr.response )
            {
                DBG_T("Sending response...");

                res.hdr.err_code = G_error_code;

                res.hdr.status = conn[0].status;
                res.hdr.ctype = conn[0].ctype;
                strcpy(res.hdr.ctypestr, conn[0].ctypestr);
                strcpy(res.hdr.cdisp, conn[0].cdisp);
                strcpy(res.hdr.cookie_out_a, conn[0].cookie_out_a);
                strcpy(res.hdr.cookie_out_a_exp, conn[0].cookie_out_a_exp);
                strcpy(res.hdr.cookie_out_l, conn[0].cookie_out_l);
                strcpy(res.hdr.cookie_out_l_exp, conn[0].cookie_out_l_exp);
                strcpy(res.hdr.location, conn[0].location);
                res.hdr.dont_cache = conn[0].dont_cache;
                res.hdr.keep_content = conn[0].keep_content;

                res.hdr.rest_status = G_rest_status;
                res.hdr.rest_req = G_rest_req;          /* only for this async call */
                res.hdr.rest_elapsed = G_rest_elapsed;  /* only for this async call */

                /* user session */

                memcpy(&res.hdr.uses, &uses[1], sizeof(usession_t));
#ifndef ASYNC_EXCLUDE_AUSES
                memcpy(&res.hdr.auses, &auses[1], sizeof(ausession_t));
#endif
                /* data */

                async_res_data_t resd;   /* different struct for more data */
                unsigned data_len, chunk_num=0, data_sent;
#ifdef OUTCHECKREALLOC
                data_len = p_content - out_data;
#else
                data_len = p_content - res.data;
#endif
                DBG("data_len = %u", data_len);

                res.chunk = ASYNC_CHUNK_FIRST;

                G_async_res_data_size = ASYNC_RES_MSG_SIZE-sizeof(async_res_hdr_t)-sizeof(int)*4;

                if ( data_len <= G_async_res_data_size )
                {
                    res.len = data_len;
#ifdef OUTCHECKREALLOC
                    memcpy(res.data, out_data, res.len);
#endif
                    res.chunk |= ASYNC_CHUNK_LAST;
                }
#ifdef OUTCHECKREALLOC
                else    /* we'll need more than one chunk */
                {
                    /* 0-th chunk */

                    res.len = G_async_res_data_size;
                    memcpy(res.data, out_data, res.len);

                    /* prepare the new struct for chunks > 0 */

                    resd.ai = req.hdr.ai;
                    resd.ci = req.hdr.ci;

                    G_async_res_data_size = ASYNC_RES_MSG_SIZE-sizeof(int)*4;
                }

                /* send first chunk (res) */

                DBG("Sending 0-th chunk, chunk data length = %d", res.len);

                if ( mq_send(G_queue_res, (char*)&res, ASYNC_RES_MSG_SIZE, 0) != 0 )
                    ERR("mq_send failed, errno = %d (%s)", errno, strerror(errno));

                data_sent = res.len;

                DBG("data_sent = %u", data_sent);

                /* next chunks if required (resd) */

                while ( data_sent < data_len )
                {
                    resd.chunk = ++chunk_num;

                    if ( data_len-data_sent <= G_async_res_data_size )   /* last chunk */
                    {
                        DBG("data_len-data_sent = %d, last chunk...", data_len-data_sent);
                        resd.len = data_len - data_sent;
                        resd.chunk |= ASYNC_CHUNK_LAST;
                    }
                    else
                    {
                        resd.len = G_async_res_data_size;
                    }

                    memcpy(resd.data, out_data+data_sent, resd.len);

                    DBG("Sending %u-th chunk, chunk data length = %d", chunk_num, resd.len);

                    if ( mq_send(G_queue_res, (char*)&resd, ASYNC_RES_MSG_SIZE, 0) != 0 )
                        ERR("mq_send failed, errno = %d (%s)", errno, strerror(errno));

                    data_sent += resd.len;

                    DBG("data_sent = %u", data_sent);
                }

#endif  /* OUTCHECKREALLOC */

                DBG("Sent\n");
            }
            else
            {
                DBG("Response not required\n");
            }

            /* ----------------------------------------------------------- */

            log_flush();
        }
    }

    clean_up();

    return EXIT_SUCCESS;
}


/* --------------------------------------------------------------------------
   Write string to output buffer with buffer overwrite protection
-------------------------------------------------------------------------- */
void svc_out_check(const char *str)
{
    int available = G_async_res_data_size - (p_content - res.data);

    if ( strlen(str) < available )  /* the whole string will fit */
    {
        p_content = stpcpy(p_content, str);
    }
    else    /* let's write only what we can. WARNING: no UTF-8 checking is done here! */
    {
        p_content = stpncpy(p_content, str, available-1);
        *p_content = EOS;
    }
}


/* --------------------------------------------------------------------------
   Write string to output buffer with buffer resizing if necessary
-------------------------------------------------------------------------- */
void svc_out_check_realloc(const char *str)
{
    if ( strlen(str) < out_data_allocated - (p_content - out_data) )    /* the whole string will fit */
    {
        p_content = stpcpy(p_content, str);
    }
    else    /* resize output buffer and try again */
    {
        unsigned used = p_content - out_data;
        char *tmp = (char*)realloc(out_data, out_data_allocated*2);
        if ( !tmp )
        {
            ERR("Couldn't reallocate output buffer, tried %u bytes", out_data_allocated*2);
            return;
        }
        out_data = tmp;
        out_data_allocated = out_data_allocated * 2;
        p_content = out_data + used;
        INF("Reallocated output buffer, new size = %u bytes", out_data_allocated);
        svc_out_check_realloc(str);     /* call itself! */
    }
}


/* --------------------------------------------------------------------------
   Write binary data to output buffer with buffer resizing if necessary
-------------------------------------------------------------------------- */
void svc_out_check_realloc_bin(const char *data, int len)
{
    if ( len < out_data_allocated - (p_content - out_data) )    /* the whole data will fit */
    {
        memcpy(p_content, data, len);
        p_content += len;
    }
    else    /* resize output buffer and try again */
    {
        unsigned used = p_content - out_data;
        char *tmp = (char*)realloc(out_data, out_data_allocated*2);
        if ( !tmp )
        {
            ERR("Couldn't reallocate output buffer, tried %u bytes", out_data_allocated*2);
            return;
        }
        out_data = tmp;
        out_data_allocated = out_data_allocated * 2;
        p_content = out_data + used;
        INF("Reallocated output buffer, new size = %u bytes", out_data_allocated);
        svc_out_check_realloc_bin(data, len);       /* call itself! */
    }
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
    char command[1024];

    ALWAYS("");
    ALWAYS("Cleaning up...\n");
    lib_log_memory();

    silgy_svc_done();

    if ( access(M_pidfile, F_OK) != -1 )
    {
        DBG("Removing pid file...");
#ifdef _WIN32   /* Windows */
        sprintf(command, "del %s", M_pidfile);
#else
        sprintf(command, "rm %s", M_pidfile);
#endif
        system(command);
    }

#ifdef DBMYSQL
    lib_close_db();
#endif

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

    silgy_lib_done();
}

#endif  /* SILGY_SVC */
