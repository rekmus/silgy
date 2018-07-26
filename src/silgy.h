/* --------------------------------------------------------------------------
   Silgy Web App Engine
   Jurek Muszynski
   silgy.com
   Started: August 2015
-------------------------------------------------------------------------- */

#ifndef SILGY_H
#define SILGY_H

#ifdef _WIN32   /* Windows */
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#include <winsock2.h>
#include <ws2tcpip.h>
#include <psapi.h>
#define CLOCK_MONOTONIC 0   /* dummy */
#undef OUT
#endif  /* _WIN32 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <stdarg.h>
#include <errno.h>
#ifndef _WIN32
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/ipc.h>
#include <netdb.h>
#include <sys/shm.h>
#include <mqueue.h>
#endif
#include <sys/stat.h>
#include <signal.h>
#include <dirent.h>

#ifdef DBMYSQL
#include <mysql.h>
#include <mysqld_error.h>
#endif

#ifdef HTTPS
#include <openssl/ssl.h>
#endif

#ifdef __cplusplus
#include <iostream>
#include <cctype>
#else
#include <ctype.h>
typedef char                        bool;
#endif


#define WEB_SERVER_VERSION          "3.3"


/* for use with booleans */

#ifndef FALSE
#define FALSE                       ((char)0)
#endif
#ifndef TRUE
#define TRUE                        ((char)1)
#endif

#define OK                          0
#define SUCCEED                     OK
#define FAIL                        -1
#define EOS                         (char)0         /* End Of String */

/* log levels */

#define LOG_ALWAYS                  0               /* print always */
#define LOG_ERR                     1               /* print errors only */
#define LOG_WAR                     2               /* print errors and warnings */
#define LOG_INF                     3               /* print errors and warnings and info */
#define LOG_DBG                     4               /* for debug mode -- most detailed */

/* request macros */

#define REQ_METHOD(s)               (0==strcmp(conn[ci].method, s))
#define REQ_GET                     (0==strcmp(conn[ci].method, "GET"))
#define REQ_POST                    (0==strcmp(conn[ci].method, "POST"))
#define REQ_PUT                     (0==strcmp(conn[ci].method, "PUT"))
#define REQ_DELETE                  (0==strcmp(conn[ci].method, "DELETE"))
#define REQ_URI                     conn[ci].uri
#define REQ_DSK                     !conn[ci].mobile
#define REQ_MOB                     conn[ci].mobile
#define REQ_BOT                     conn[ci].bot
#define REQ_LANG                    conn[ci].lang

#define PROTOCOL                    (conn[ci].secure?"https":"http")
#define COLON_POSITION              (conn[ci].secure?5:4)

/* defaults */
#ifndef MEM_MEDIUM
#ifndef MEM_BIG
#ifndef MEM_HUGE
#ifndef MEM_SMALL
#define MEM_SMALL   /* default memory model */
#endif
#endif
#endif
#endif

#ifndef OUTFAST
#ifndef OUTCHECK
#define OUTCHECKREALLOC   /* default output type */
#endif
#endif

#ifndef QS_DEF_SQL_ESCAPE
#ifndef QS_DEF_DONT_ESCAPE
#define QS_DEF_HTML_ESCAPE   /* default query string security */
#endif
#endif

/* generate output as fast as possible */
#ifdef NOSTPCPY /* alas! */

    #define HOUT(s)                     (strcpy(conn[ci].p_curr_h, s), conn[ci].p_curr_h += strlen(s))
    #define OUTSS(s)                    (strcpy(conn[ci].p_curr_c, s), conn[ci].p_curr_c += strlen(s))
    #define OUT_BIN(data, len)          (len=(len>OUT_BUFSIZE?OUT_BUFSIZE:len), memcpy(conn[ci].p_curr_c, data, len), conn[ci].p_curr_c += len)

#else   /* faster */

    #define HOUT(s)                     (conn[ci].p_curr_h = stpcpy(conn[ci].p_curr_h, s))

    #ifdef OUTFAST
        #define OUTSS(s)                    (conn[ci].p_curr_c = stpcpy(conn[ci].p_curr_c, s))
        #define OUT_BIN(data, len)          (len=(len>OUT_BUFSIZE?OUT_BUFSIZE:len), memcpy(conn[ci].p_curr_c, data, len), conn[ci].p_curr_c += len)
    #else
        #ifdef OUTCHECK
            #define OUTSS(s)                    eng_out_check(ci, s)
            #define OUT_BIN(data, len)          (len=(len>OUT_BUFSIZE?OUT_BUFSIZE:len), memcpy(conn[ci].p_curr_c, data, len), conn[ci].p_curr_c += len)
        #else   /* OUTCHECKREALLOC */
            #define OUTSS(s)                    eng_out_check_realloc(ci, s)
            #define OUT_BIN(data, len)          eng_out_check_realloc_bin(ci, data, len)
        #endif
    #endif  /* OUTFAST */

#endif  /* NOSTPCPY */


#define OUTM(s, ...)                (sprintf(G_tmp, s, __VA_ARGS__), OUTSS(G_tmp))   /* OUT with multiple args */

#define CHOOSE_OUT(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, NAME, ...) NAME          /* single or multiple? */
#define OUT(...)                    CHOOSE_OUT(__VA_ARGS__, OUTM, OUTM, OUTM, OUTM, OUTM, OUTM, OUTM, OUTM, OUTM, OUTM, OUTM, OUTM, OUTSS)(__VA_ARGS__)

/* HTTP header -- resets respbuf! */
#define PRINT_HTTP_STATUS(st)       (sprintf(G_tmp, "HTTP/1.1 %d %s\r\n", st, get_http_descr(st)), HOUT(G_tmp))

/* date */
#define PRINT_HTTP_DATE             (sprintf(G_tmp, "Date: %s\r\n", M_resp_date), HOUT(G_tmp))

/* cache control */
#define PRINT_HTTP_NO_CACHE         HOUT("Cache-Control: private, must-revalidate, no-store, no-cache, max-age=0\r\n")
#define PRINT_HTTP_EXPIRES          (sprintf(G_tmp, "Expires: %s\r\n", M_expires), HOUT(G_tmp))
#define PRINT_HTTP_LAST_MODIFIED(s) (sprintf(G_tmp, "Last-Modified: %s\r\n", s), HOUT(G_tmp))

/* connection */
#define PRINT_HTTP_CONNECTION(ci)   (sprintf(G_tmp, "Connection: %s\r\n", conn[ci].keep_alive?"Keep-Alive":"close"), HOUT(G_tmp))

/* vary */
#define PRINT_HTTP_VARY_DYN         HOUT("Vary: Accept-Encoding, User-Agent\r\n")
#define PRINT_HTTP_VARY_STAT        HOUT("Vary: Accept-Encoding\r\n")
#define PRINT_HTTP_VARY_UIR         HOUT("Vary: Upgrade-Insecure-Requests\r\n")

/* content language */
#define PRINT_HTTP_LANGUAGE         HOUT("Content-Language: en-us\r\n")

/* framing */
#define PRINT_HTTP_FRAME_OPTIONS    HOUT("X-Frame-Options: SAMEORIGIN\r\n")

/* cookie */
#define PRINT_HTTP_COOKIE_A(ci)     (sprintf(G_tmp, "Set-Cookie: as=%s; HttpOnly\r\n", conn[ci].cookie_out_a), HOUT(G_tmp))
#define PRINT_HTTP_COOKIE_L(ci)     (sprintf(G_tmp, "Set-Cookie: ls=%s; HttpOnly\r\n", conn[ci].cookie_out_l), HOUT(G_tmp))
#define PRINT_HTTP_COOKIE_A_EXP(ci) (sprintf(G_tmp, "Set-Cookie: as=%s; Expires=%s; HttpOnly\r\n", conn[ci].cookie_out_a, conn[ci].cookie_out_a_exp), HOUT(G_tmp))
#define PRINT_HTTP_COOKIE_L_EXP(ci) (sprintf(G_tmp, "Set-Cookie: ls=%s; Expires=%s; HttpOnly\r\n", conn[ci].cookie_out_l, conn[ci].cookie_out_l_exp), HOUT(G_tmp))

/* content length */
#define PRINT_HTTP_CONTENT_LEN(len) (sprintf(G_tmp, "Content-Length: %d\r\n", len), HOUT(G_tmp))

/* identity */
#define PRINT_HTTP_SERVER           HOUT("Server: Silgy\r\n")

/* must be last! */
#define PRINT_HTTP_END_OF_HEADER    HOUT("\r\n")


#define IN_BUFSIZE                  8192            /* incoming request buffer length (8 kB) */
#define OUT_BUFSIZE                 262144          /* initial HTTP response buffer length (256 kB) */
#define TMP_BUFSIZE                 1048576         /* temporary string buffer size (1 MB) */
#define MAX_POST_DATA_BUFSIZE       16777216+1048576    /* max incoming POST data length (16+1 MB) */
#define MAX_LOG_STR_LEN             4095            /* max log string length */
#define MAX_METHOD_LEN              7               /* method length */
#define MAX_URI_LEN                 2047            /* max request URI length */
#define MAX_LABEL_LEN               63              /* max request label length */
#define MAX_VALUE_LEN               255             /* max request value length */
#define MAX_RESOURCE_LEN            63              /* max resource's name length -- as a first part of URI */
#define MAX_ID_LEN                  31              /* max id length -- as a second part of URI */
#define MAX_RESOURCES               10000           /* for M_auth_levels */
#define MAX_SQL_QUERY_LEN           1023            /* max SQL query length */

/* mainly memory usage */

#ifdef MEM_MEDIUM
#define MAX_CONNECTIONS             500             /* max TCP connections (5 per user session) */
#define MAX_SESSIONS                100             /* max user sessions */
#elif MEM_BIG
#define MAX_CONNECTIONS             2500            /* max TCP connections */
#define MAX_SESSIONS                500             /* max user sessions */
#elif MEM_HUGE
#define MAX_CONNECTIONS             10000           /* max TCP connections */
#define MAX_SESSIONS                2000            /* max user sessions */
#else   /* MEM_SMALL -- default */
#define MAX_CONNECTIONS             50              /* max TCP connections */
#define MAX_SESSIONS                10              /* max user sessions */
#endif

#define CONN_TIMEOUT                180             /* idle connection timeout in seconds */

#define USES_TIMEOUT                300             /* anonymous user session timeout in seconds */

#define CONN_STATE_DISCONNECTED         '0'
#define CONN_STATE_ACCEPTING            'a'
#define CONN_STATE_CONNECTED            '1'
#define CONN_STATE_READING_HEADER       'h'
#define CONN_STATE_READY_FOR_PARSE      'p'
#define CONN_STATE_READY_FOR_PROCESS    'P'
#define CONN_STATE_READING_DATA         'd'
#define CONN_STATE_WAITING_FOR_ASYNC    'A'
#define CONN_STATE_READY_TO_SEND_HEADER 'H'
#define CONN_STATE_READY_TO_SEND_BODY   'B'
#define CONN_STATE_SENDING_BODY         'S'

#ifdef __linux__
#define MONOTONIC_CLOCK_NAME            CLOCK_MONOTONIC_RAW
#else
#define MONOTONIC_CLOCK_NAME            CLOCK_MONOTONIC
#endif

#define MAX_BLACKLIST                   10000

/* UTF-8 */

#define CHAR_POUND                  "&#163;"
#define CHAR_COPYRIGHT              "&#169;"
#define CHAR_N_ACUTE                "&#324;"
#define CHAR_DOWN_ARROWHEAD1        "&#709;"
#define CHAR_LONG_DASH              "&#8212;"
#define CHAR_EURO                   "&#8364;"
#define CHAR_UP                     "&#8593;"
#define CHAR_DOWN                   "&#8595;"
#define CHAR_MINUS                  "&#8722;"
#define CHAR_VEL                    "&#8744;"
#define CHAR_VERTICAL_ELLIPSIS      "&#8942;"
#define CHAR_COUNTERSINK            "&#9013;"
#define CHAR_DOUBLE_TRIANGLE_U      "&#9195;"
#define CHAR_DOUBLE_TRIANGLE_D      "&#9196;"
#define CHAR_DOWN_TRIANGLE_B        "&#9660;"
#define CHAR_DOWN_TRIANGLE_W        "&#9661;"
#define CHAR_CLOSE                  "&#10005;"
#define CHAR_HEAVY_PLUS             "&#10133;"
#define CHAR_HEAVY_MINUS            "&#10134;"
#define CHAR_DOWN_ARROWHEAD2        "&#65088;"
#define CHAR_FULLW_PLUS             "&#65291;"

#define LOGIN_LEN                   63
#define EMAIL_LEN                   127
#define UNAME_LEN                   63

#define VIEW_DEFAULT                '0'
#define VIEW_DESKTOP                '1'
#define VIEW_MOBILE                 '2'

#define SESID_LEN                   15

#define EXPIRES_IN_DAYS             30              /* from app start for Expires HTTP reponse header for static resources */

/* authorization levels */

#define AUTH_LEVEL_NONE             '0'
#define AUTH_LEVEL_ANONYMOUS        '1'
#define AUTH_LEVEL_LOGGEDIN         '2'
#define AUTH_LEVEL_ADMIN            '3'

/* errors */

#define ERR_INVALID_REQUEST         -1
#define ERR_UNAUTHORIZED            -2
#define ERR_FORBIDDEN               -3
#define ERR_NOT_FOUND               -4
#define ERR_INT_SERVER_ERROR        -5
#define ERR_SERVER_TOOBUSY          -6
#define ERR_FILE_TOO_BIG            -7
#define ERR_REDIRECTION             -8

#define NOT_STATIC                  -1
#define MAX_STATICS                 1000            /* max static resources */

#define MAX_ASYNC                   20              /* max async responses */
#define ASYNC_STATE_FREE            '0'
#define ASYNC_STATE_SENT            '1'
#define ASYNC_STATE_RECEIVED        '2'
#define ASYNC_STATE_TIMEOUTED       '3'
#define ASYNC_REQ_MSG_SIZE          1024            /* async message size */
#define ASYNC_RES_MSG_SIZE          8192            /* async message size */
//#define ASYNC_RES_MSG_SIZE          16384           /* async message size */
//#define ASYNC_RES_MSG_SIZE            32768           /* async message size */
//#define ASYNC_RES_MSG_SIZE            102400          /* async message size -- 100 kB */
#define ASYNC_REQ_QUEUE             "/silgy_req"    /* request queue name */
#define ASYNC_RES_QUEUE             "/silgy_res"    /* response queue name */
#define ASYNC_MAX_TIMEOUT           1800            /* in seconds ==> 30 minutes */
#define S(s)                        (0==strcmp(service,s))
#define CALL_ASYNC(s,d,t)           eng_async_req(ci, s, d, TRUE, t)
#define CALL_ASYNC_NR(s,d)          eng_async_req(ci, s, d, FALSE, 0)


/* REST calls */

typedef struct {
    char    key[64];
    char    value[256];
} rest_header_t;

#define REST_MAX_HEADERS            100
#define REST_HEADERS_RESET          eng_rest_headers_reset()
#define REST_HEADER_SET(k,v)        eng_rest_header_set(k, v)
#define REST_HEADER_UNSET(k,v)      eng_rest_header_unset(k)

#define CALL_REST_RAW(req,res,m,u)  eng_rest_req(ci, &req, &res, m, u, FALSE)
#define CALL_REST_JSON(req,res,m,u) eng_rest_req(ci, &req, &res, m, u, TRUE)
/* aliases */
#define CALL_REST_HTTP(req,res,m,u) CALL_REST_RAW(req,res,m,u)
#define CALL_REST(req,res,m,u)      CALL_REST_JSON(req,res,m,u)

#define CALL_REST_DEFAULT_TIMEOUT   500     /* in ms -- to avoid blocking */


/* resource / content types */

/* incoming */

#define CONTENT_TYPE_URLENCODED     'U'
#define CONTENT_TYPE_MULTIPART      'L'

/* outgoing */

#define CONTENT_TYPE_UNSET          '-'
#define CONTENT_TYPE_USER           '+'
#define RES_TEXT                    'T'
#define RES_HTML                    'H'
#define RES_CSS                     'C'
#define RES_JS                      'S'
#define RES_GIF                     'G'
#define RES_JPG                     'J'
#define RES_ICO                     'I'
#define RES_PNG                     'P'
#define RES_BMP                     'B'
#define RES_PDF                     'A'
#define RES_AMPEG                   'M'
#define RES_EXE                     'X'
#define RES_ZIP                     'Z'


#define URI(uri)                    (0==strcmp(conn[ci].uri, uri))
#define REQ(res)                    (0==strcmp(conn[ci].resource, res))
#define ID(id)                      (0==strcmp(conn[ci].id, id))
#define US                          uses[conn[ci].usi]
#define AUS                         auses[conn[ci].usi]
#define HOST(s)                     eng_host(ci, s)

/* response macros */

#define RES_STATUS(s)               eng_set_res_status(ci, s)
#define RES_CONTENT_TYPE(s)         eng_set_res_content_type(ci, s)
#define RES_LOCATION(s, ...)        eng_set_res_location(ci, s, ##__VA_ARGS__)
#define RES_DONT_CACHE              conn[ci].dont_cache=TRUE
#define RES_CONTENT_DISPOSITION(s, ...) eng_set_res_content_disposition(ci, s, ##__VA_ARGS__)

#define REDIRECT_TO_LANDING         sprintf(conn[ci].location, "%s://%s", PROTOCOL, conn[ci].host)


/* HTTP status */

typedef struct {
    int     status;
    char    description[128];
} http_status_t;


/* date */

typedef struct {
    short   year;
    char    month;
    char    day;
} date_t;


/* connection */

typedef struct {
    /* what comes in */
#ifdef _WIN32   /* Windows */
    SOCKET  fd;                             /* file descriptor */
#else
    int     fd;                             /* file descriptor */
#endif  /* _WIN32 */
    bool    secure;                         /* https? */
    char    ip[INET_ADDRSTRLEN];            /* client IP */
    char    pip[INET_ADDRSTRLEN];           /* proxy IP */
    char    in[IN_BUFSIZE];                 /* the whole incoming request */
    char    method[MAX_METHOD_LEN+1];       /* HTTP method */
    long    was_read;                       /* request bytes read so far */
    bool    upgrade2https;                  /* Upgrade-Insecure-Requests = 1 */
    /* parsed HTTP request starts here */
    bool    head_only;                      /* request method = HEAD */
    bool    post;                           /* request method = POST */
    char    uri[MAX_URI_LEN+1];             /* requested URI string */
    char    resource[MAX_RESOURCE_LEN+1];   /* from URI */
    char    id[MAX_ID_LEN+1];               /* from URI */
    char    uagent[MAX_VALUE_LEN+1];        /* user agent string */
    bool    mobile;
    bool    keep_alive;
    char    referer[MAX_VALUE_LEN+1];
    long    clen;                           /* incoming & outgoing content length */
    char    *data;                          /* POST data */
    char    cookie_in_a[SESID_LEN+1];       /* anonymous */
    char    cookie_in_l[SESID_LEN+1];       /* logged in */
    char    host[64];
    char    website[64];
    char    lang[8];
    time_t  if_mod_since;
    char    in_ctype;                       /* content type */
    char    boundary[256];                  /* for POST multipart/form-data type */
    /* what goes out */
    char    header[1024];                   /* outgoing HTTP header */
#ifdef OUTCHECKREALLOC
    char    *out_data;                      /* body */
#else
    char    out_data[OUT_BUFSIZE];
#endif
    long    out_data_allocated;
    int     status;                         /* HTTP status */
    long    data_sent;                      /* how many body bytes has been sent */
    char    ctype;                          /* content type */
    char    ctypestr[256];                  /* user (custom) content type */
    char    cdisp[256];                     /* content disposition */
    time_t  modified;
    char    cookie_out_a[SESID_LEN+1];
    char    cookie_out_a_exp[32];           /* cookie expires */
    char    cookie_out_l[SESID_LEN+1];
    char    cookie_out_l_exp[32];           /* cookie expires */
    char    location[256];                  /* redirection */
    /* internal stuff */
    long    req;                            /* request count */
    struct timespec proc_start;
    char    conn_state;                     /* connection state (STATE_XXX) */
    char    *p_curr_h;                      /* current header pointer */
    char    *p_curr_c;                      /* current content pointer */
#ifdef HTTPS
    SSL     *ssl;
#endif
    int     ssl_err;
    char    auth_level;                     /* required authorization level */
    int     usi;                            /* user session index */
    int     static_res;                     /* static resource index in M_stat */
    time_t  last_activity;
    bool    bot;
    bool    expect100;
    bool    dont_cache;
} conn_t;


/* user session */

typedef struct {
    bool    logged;
    long    uid;
    char    login[LOGIN_LEN+1];
    char    email[EMAIL_LEN+1];
    char    name[UNAME_LEN+1];
    char    about[256];
    char    login_tmp[LOGIN_LEN+1];     /* while My Profile isn't saved */
    char    email_tmp[EMAIL_LEN+1];
    char    name_tmp[UNAME_LEN+1];
    char    about_tmp[256];
    char    sesid[SESID_LEN+1];
    char    ip[INET_ADDRSTRLEN];
    char    uagent[MAX_VALUE_LEN+1];
    char    referer[MAX_VALUE_LEN+1];
    char    lang[8];
    time_t  last_activity;
    char    additional[64];         /* password reset key */
} usession_t;


/* static resources */

typedef struct {
    char    name[256];
    char    type;
    char    *data;
    long    len;
    time_t  modified;
} stat_res_t;


/* counters */

typedef struct {
    long    req;        /* all parsed requests */
    long    req_dsk;    /* all requests with desktop UA */
    long    req_mob;    /* all requests with mobile UA */
    long    req_bot;    /* all requests with HTTP header indicating well-known search-engine bots */
    long    visits;     /* all visits to domain (Host=APP_DOMAIN) landing page (no action/resource), excl. bots that got 200 */
    long    visits_dsk; /* like visits -- desktop only */
    long    visits_mob; /* like visits -- mobile only */
    long    blocked;    /* attempts from blocked IP */
} counters_t;


/* asynchorous processing */

typedef struct {
    long    call_id;
    int     ci;
    char    service[32];
    char    response;
    char    data[ASYNC_REQ_MSG_SIZE-sizeof(long)-sizeof(int)-33];
} async_req_t;

typedef struct {
    long    call_id;
    int     ci;
    char    service[32];
    char    state;
    time_t  sent;
    int     timeout;
    char    data[ASYNC_RES_MSG_SIZE-sizeof(long)-(sizeof(int)*2)-sizeof(time_t)-33];
} async_res_t;


/* read from the config file */
extern char     G_logLevel;
extern int      G_httpPort;
extern int      G_httpsPort;
extern char     G_cipherList[256];
extern char     G_certFile[256];
extern char     G_certChainFile[256];
extern char     G_keyFile[256];
extern char     G_dbHost[128];
extern int      G_dbPort;
extern char     G_dbName[128];
extern char     G_dbUser[128];
extern char     G_dbPassword[128];
extern char     G_blockedIPList[256];
extern int      G_RESTTimeout;
extern char     G_test;
/* end of config params */
extern int      G_pid;                      /* pid */
extern char     G_appdir[256];              /* application root dir */
extern FILE     *G_log;                     /* log file handle */
extern long     G_days_up;                  /* web server's days up */
#ifndef ASYNC_SERVICE
extern conn_t   conn[MAX_CONNECTIONS];      /* HTTP connections & requests -- by far the most important structure around */
#endif
extern int      G_open_conn;                /* number of open connections */
extern char     G_tmp[TMP_BUFSIZE];         /* temporary string buffer */
#ifndef ASYNC_SERVICE
extern usession_t uses[MAX_SESSIONS+1];     /* user sessions -- they start from 1 */
#endif
extern int      G_sessions;                 /* number of active user sessions */
extern time_t   G_now;                      /* current time */
extern struct tm *G_ptm;                    /* human readable current time */
extern char     G_last_modified[32];        /* response header field with server's start time */
#ifdef DBMYSQL
extern MYSQL    *G_dbconn;                  /* database connection */
#endif
#ifndef _WIN32
/* asynchorous processing */
extern mqd_t    G_queue_req;                /* request queue */
extern mqd_t    G_queue_res;                /* response queue */
#ifdef ASYNC
extern async_res_t ares[MAX_ASYNC];         /* async response array */
extern long     G_last_call_id;             /* counter */
#endif
#endif
extern char     G_dt[20];                   /* datetime for database or log (YYYY-MM-DD hh:mm:ss) */
extern char     G_blacklist[MAX_BLACKLIST+1][INET_ADDRSTRLEN];
extern int      G_blacklist_cnt;            /* M_blacklist length */
extern counters_t G_cnts_today;             /* today's counters */
extern counters_t G_cnts_yesterday;         /* yesterday's counters */
extern counters_t G_cnts_day_before;        /* day before's counters */
/* SHM */
extern char     *G_shm_segptr;              /* SHM pointer */



#include "silgy_lib.h"

#ifdef USERS
#include "silgy_usr.h"
#endif

#include "silgy_app.h"




/* public engine functions */

#ifdef __cplusplus
extern "C" {
#endif
    void eng_set_param(const char *label, const char *value);
    void silgy_set_auth_level(const char *resource, char level);
    bool eng_uses_start(int ci);
    void eng_uses_close(int usi);
    void eng_uses_reset(int usi);
    void eng_async_req(int ci, const char *service, const char *data, char response, int timeout);
    void eng_rest_headers_reset(void);
    void eng_rest_header_set(const char *key, const char *value);
    void eng_rest_header_unset(const char *key);
    bool eng_rest_req(int ci, void *req, void *res, const char *method, const char *url, bool json);
    void silgy_add_to_static_res(const char *name, char *src);
    void eng_send_ajax_msg(int ci, int errcode);
    void eng_block_ip(const char *value, bool autoblocked);
    void eng_get_msg_str(int ci, char *dest, int errcode);
    bool eng_host(int ci, const char *host);
    void eng_set_res_status(int ci, int status);
    void eng_set_res_content_type(int ci, const char *str);
    void eng_set_res_location(int ci, const char *str, ...);
    void eng_set_res_content_disposition(int ci, const char *str, ...);
    void eng_out_check(int ci, const char *str);
    void eng_out_check_realloc(int ci, const char *str);
    void eng_out_check_realloc_bin(int ci, const char *data, long len);
#ifdef ASYNC_SERVICE
    bool service_init(void);
    void service_app_process_req(const char *service, const char *req, char *res);
    void service_done(void);
#endif
#ifdef __cplusplus
}   /* extern "C" */
#endif



#endif  /* SILGY_H */
