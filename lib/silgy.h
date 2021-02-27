/* --------------------------------------------------------------------------
   Silgy Web App Engine
   Jurek Muszynski
   silgy.com
   Started: August 2015
-------------------------------------------------------------------------- */

#ifndef SILGY_H
#define SILGY_H

#ifdef _WIN32   /* Windows */
#ifdef _MSC_VER /* Microsoft compiler */
/* libraries */
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "psapi.lib")   /* GetProcessMemoryInfo */
/* __VA_ARGS__ issue */
#define EXPAND_VA(x) x
/* access function */
#define F_OK    0       /* test for existence of file */
#define X_OK    0x01    /* test for execute or search permission */
#define W_OK    0x02    /* test for write permission */
#define R_OK    0x04    /* test for read permission */
#endif  /* _MSC_VER */
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0501 /* Windows XP or higher required */
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
#endif  /* _WIN32 */

#include <sys/stat.h>
#include <signal.h>
#include <dirent.h>

#ifdef __cplusplus
#include <iostream>
#include <cctype>
#else   /* C */
#include <ctype.h>
typedef char                            bool;
#define false                           ((char)0)
#define true                            ((char)1)
#endif  /* __cplusplus */


#define WEB_SERVER_VERSION              "4.5.9"
/* alias */
#define SILGY_VERSION                   WEB_SERVER_VERSION


#ifndef FALSE
#define FALSE                           false
#endif
#ifndef TRUE
#define TRUE                            true
#endif


/* pure C string type */

typedef char str1k[1024];
typedef char str2k[1024*2];
typedef char str4k[1024*4];
typedef char str8k[1024*8];
typedef char str16k[1024*16];
typedef char str32k[1024*32];
typedef char str64k[1024*64];
typedef char str128k[1024*128];
typedef char str256k[1024*256];


#define OK                              0
#define SUCCEED                         OK
#define FAIL                            -1

#define EOS                             ((char)0)       /* End Of String */


/* log levels */

#define LOG_ALWAYS                      0               /* print always */
#define LOG_ERR                         1               /* print errors only */
#define LOG_WAR                         2               /* print errors and warnings */
#define LOG_INF                         3               /* print errors and warnings and info */
#define LOG_DBG                         4               /* for debug mode -- most detailed */


#define MAX_URI_VAL_LEN                 255             /* max value length received in URI -- sufficient for 99% cases */
#define MAX_LONG_URI_VAL_LEN            65535           /* max long value length received in URI -- 64 kB - 1 B */

#define QSBUF                           MAX_URI_VAL_LEN+1
#define QS_BUF                          QSBUF

#define WEBSITE_LEN                     63
#define CONTENT_TYPE_LEN                63
#define CONTENT_DISP_LEN                127

#define LOGIN_LEN                       30
#define EMAIL_LEN                       120
#define UNAME_LEN                       120
#define PHONE_LEN                       30
#define ABOUT_LEN                       250


#define VIEW_DEFAULT                    '0'
#define VIEW_DESKTOP                    '1'
#define VIEW_MOBILE                     '2'


#define SQLBUF                          4096            /* SQL query buffer size */



/* UTF-8 */

#define CHAR_POUND                      "&#163;"
#define CHAR_COPYRIGHT                  "&#169;"
#define CHAR_N_ACUTE                    "&#324;"
#define CHAR_DOWN_ARROWHEAD1            "&#709;"
#define CHAR_LONG_DASH                  "&#8212;"
#define CHAR_EURO                       "&#8364;"
#define CHAR_UP                         "&#8593;"
#define CHAR_DOWN                       "&#8595;"
#define CHAR_MINUS                      "&#8722;"
#define CHAR_VEL                        "&#8744;"
#define CHAR_VERTICAL_ELLIPSIS          "&#8942;"
#define CHAR_COUNTERSINK                "&#9013;"
#define CHAR_DOUBLE_TRIANGLE_U          "&#9195;"
#define CHAR_DOUBLE_TRIANGLE_D          "&#9196;"
#define CHAR_DOWN_TRIANGLE_B            "&#9660;"
#define CHAR_DOWN_TRIANGLE_W            "&#9661;"
#define CHAR_CLOSE                      "&#10005;"
#define CHAR_HEAVY_PLUS                 "&#10133;"
#define CHAR_HEAVY_MINUS                "&#10134;"
#define CHAR_DOWN_ARROWHEAD2            "&#65088;"
#define CHAR_FULLW_PLUS                 "&#65291;"
#define CHAR_LESS_EQUAL                 "&#8804;"
#define CHAR_GREATER_EQUAL              "&#8805;"


#ifndef SILGY_SVC
#ifndef SILGY_WATCHER
#define SILGY_APP
#endif
#endif


#include "silgy_app.h"


/* select() vs poll() vs epoll() */

#ifdef _WIN32
#define FD_MON_SELECT   /* WSAPoll doesn't seem to be a reliable alternative */
#undef FD_MON_POLL
#undef FD_MON_EPOLL
#ifdef ASYNC
#undef ASYNC
#endif
#else
#ifndef FD_MON_SELECT
#define FD_MON_POLL
#endif
#endif  /* _WIN32 */


#ifdef SILGY_WATCHER
#define SILGY_CLIENT
#ifdef HTTPS
#undef HTTPS
#endif
#endif

#ifdef SILGY_CLIENT
#ifdef DBMYSQL
#undef DBMYSQL
#endif
#ifdef USERS
#undef USERS
#endif
#endif  /* SILGY_CLIENT */


#ifdef DBMYSQL
#include <mysql.h>
#include <mysqld_error.h>
#endif

#ifdef HTTPS
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif


/* request macros */

#define REQ_METHOD                  conn[ci].method
#define REQ_GET                     (0==strcmp(conn[ci].method, "GET"))
#define REQ_POST                    (0==strcmp(conn[ci].method, "POST"))
#define REQ_PUT                     (0==strcmp(conn[ci].method, "PUT"))
#define REQ_DELETE                  (0==strcmp(conn[ci].method, "DELETE"))
#define REQ_OPTIONS                 (0==strcmp(conn[ci].method, "OPTIONS"))
#define REQ_URI                     conn[ci].uri
#define REQ_CONTENT_TYPE            conn[ci].in_ctypestr
#define REQ_DSK                     !conn[ci].mobile
#define REQ_MOB                     conn[ci].mobile
#define REQ_BOT                     conn[ci].bot
#define REQ_LANG                    conn[ci].lang

#define PROTOCOL                    (conn[ci].secure?"https":"http")
#define COLON_POSITION              (conn[ci].secure?5:4)


/* defaults */

#ifndef MEM_TINY
#ifndef MEM_MEDIUM
#ifndef MEM_LARGE
#ifndef MEM_XLARGE
#ifndef MEM_XXLARGE
#ifndef MEM_XXXLARGE
#ifndef MEM_XXXXLARGE
#ifndef MEM_SMALL
#define MEM_SMALL   /* default memory model */
#endif
#endif
#endif
#endif
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

#ifdef SILGY_SVC

    #ifdef OUTFAST
        #define OUTSS(str)                  (p_content = stpcpy(p_content, str))
        #define OUT_BIN(data, len)          (len=(len>G_async_res_data_size?G_async_res_data_size:len), memcpy(p_content, data, len), p_content += len)
    #elif defined (OUTCHECK)
        #define OUTSS(str)                  svc_out_check(str)
        #define OUT_BIN(data, len)          (len=(len>G_async_res_data_size?G_async_res_data_size:len), memcpy(p_content, data, len), p_content += len)
    #else   /* OUTCHECKREALLOC */
        #define OUTSS(str)                  svc_out_check_realloc(str)
        #define OUT_BIN(data, len)          svc_out_check_realloc_bin(data, len)
    #endif

#else   /* SILGY_APP */

    #define HOUT(str)                   (conn[ci].p_header = stpcpy(conn[ci].p_header, str))

    #ifdef OUTFAST
        #define OUTSS(str)                  (conn[ci].p_content = stpcpy(conn[ci].p_content, str))
        #define OUT_BIN(data, len)          (len=(len>OUT_BUFSIZE-OUT_HEADER_BUFSIZE?OUT_BUFSIZE-OUT_HEADER_BUFSIZE:len), memcpy(conn[ci].p_content, data, len), conn[ci].p_content += len)
    #else
        #ifdef OUTCHECK
            #define OUTSS(str)                  eng_out_check(ci, str)
            #define OUT_BIN(data, len)          (len=(len>OUT_BUFSIZE-OUT_HEADER_BUFSIZE?OUT_BUFSIZE-OUT_HEADER_BUFSIZE:len), memcpy(conn[ci].p_content, data, len), conn[ci].p_content += len)
        #else   /* OUTCHECKREALLOC */
            #define OUTSS(str)                  eng_out_check_realloc(ci, str)
            #define OUT_BIN(data, len)          eng_out_check_realloc_bin(ci, data, len)
        #endif
    #endif  /* OUTFAST */

#endif  /* SILGY_SVC */

#ifdef _MSC_VER /* Microsoft compiler */
    #define OUT(...)                        (sprintf(G_tmp, EXPAND_VA(__VA_ARGS__)), OUTSS(G_tmp))
#else   /* GCC */
    #define OUTM(str, ...)                  (sprintf(G_tmp, str, __VA_ARGS__), OUTSS(G_tmp))   /* OUT with multiple args */
    #define CHOOSE_OUT(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, NAME, ...) NAME          /* single or multiple? */
    #define OUT(...)                        CHOOSE_OUT(__VA_ARGS__, OUTM, OUTM, OUTM, OUTM, OUTM, OUTM, OUTM, OUTM, OUTM, OUTM, OUTM, OUTM, OUTM, OUTSS)(__VA_ARGS__)
#endif  /* _MSC_VER */


/* convenient & fast string building */

#define OUTP_BEGIN(buf)                 char *p4outp=buf

#define OUTPS(str)                      (p4outp = stpcpy(p4outp, str))

#ifdef _MSC_VER /* Microsoft compiler */
    #define OUTP(...)                        (sprintf(G_tmp, EXPAND_VA(__VA_ARGS__)), OUTPS(G_tmp))
#else   /* GCC */
    #define OUTPM(str, ...)                  (sprintf(G_tmp, str, __VA_ARGS__), OUTPS(G_tmp))   /* OUTP with multiple args */
    #define CHOOSE_OUTP(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, NAME, ...) NAME          /* single or multiple? */
    #define OUTP(...)                        CHOOSE_OUTP(__VA_ARGS__, OUTPM, OUTPM, OUTPM, OUTPM, OUTPM, OUTPM, OUTPM, OUTPM, OUTPM, OUTPM, OUTPM, OUTPM, OUTPM, OUTPS)(__VA_ARGS__)
#endif  /* _MSC_VER */

#define OUTP_END                        *p4outp = EOS



/* HTTP header -- resets respbuf! */
#define PRINT_HTTP_STATUS(val)          (sprintf(G_tmp, "HTTP/1.1 %d %s\r\n", val, get_http_descr(val)), HOUT(G_tmp))

/* date */
#define PRINT_HTTP_DATE                 (sprintf(G_tmp, "Date: %s\r\n", M_resp_date), HOUT(G_tmp))

/* cache control */
#define PRINT_HTTP_CACHE_PUBLIC         HOUT("Cache-Control: public, max-age=31536000\r\n")
#define PRINT_HTTP_NO_CACHE             HOUT("Cache-Control: private, must-revalidate, no-store, no-cache, max-age=0\r\n")
#define PRINT_HTTP_EXPIRES_STATICS      (sprintf(G_tmp, "Expires: %s\r\n", M_expires_stat), HOUT(G_tmp))
#define PRINT_HTTP_EXPIRES_GENERATED    (sprintf(G_tmp, "Expires: %s\r\n", M_expires_gen), HOUT(G_tmp))
#define PRINT_HTTP_LAST_MODIFIED(str)   (sprintf(G_tmp, "Last-Modified: %s\r\n", str), HOUT(G_tmp))

/* connection */
#define PRINT_HTTP_CONNECTION(ci)       (sprintf(G_tmp, "Connection: %s\r\n", conn[ci].keep_alive?"keep-alive":"close"), HOUT(G_tmp))

/* vary */
#define PRINT_HTTP_VARY_DYN             HOUT("Vary: Accept-Encoding, User-Agent\r\n")
#define PRINT_HTTP_VARY_STAT            HOUT("Vary: Accept-Encoding\r\n")
#define PRINT_HTTP_VARY_UIR             HOUT("Vary: Upgrade-Insecure-Requests\r\n")

/* content language */
#define PRINT_HTTP_LANGUAGE             HOUT("Content-Language: en-us\r\n")

/* content length */
#define PRINT_HTTP_CONTENT_LEN(len)     (sprintf(G_tmp, "Content-Length: %u\r\n", len), HOUT(G_tmp))

/* content encoding */
#define PRINT_HTTP_CONTENT_ENCODING_DEFLATE HOUT("Content-Encoding: deflate\r\n")

/* Security ------------------------------------------------------------------ */

/* HSTS */
#ifndef HSTS_MAX_AGE
#define HSTS_MAX_AGE                    31536000    /* a year */
#endif
#ifdef HSTS_INCLUDE_SUBDOMAINS
#define PRINT_HTTP_HSTS                 (sprintf(G_tmp, "Strict-Transport-Security: max-age=%d; includeSubDomains\r\n", HSTS_MAX_AGE), HOUT(G_tmp))
#else
#define PRINT_HTTP_HSTS                 (sprintf(G_tmp, "Strict-Transport-Security: max-age=%d\r\n", HSTS_MAX_AGE), HOUT(G_tmp))
#endif

#ifdef HTTPS
#ifndef NO_HSTS
#if HSTS_MAX_AGE > 0
#define HSTS_ON
#endif
#endif
#endif

/* cookie */
#ifdef HSTS_ON
#define PRINT_HTTP_COOKIE_A(ci)         (sprintf(G_tmp, "Set-Cookie: as=%s; %sHttpOnly\r\n", conn[ci].cookie_out_a, G_test?"":"Secure; "), HOUT(G_tmp))
#define PRINT_HTTP_COOKIE_L(ci)         (sprintf(G_tmp, "Set-Cookie: ls=%s; %sHttpOnly\r\n", conn[ci].cookie_out_l, G_test?"":"Secure; "), HOUT(G_tmp))
#define PRINT_HTTP_COOKIE_A_EXP(ci)     (sprintf(G_tmp, "Set-Cookie: as=%s; Expires=%s; %sHttpOnly\r\n", conn[ci].cookie_out_a, conn[ci].cookie_out_a_exp, G_test?"":"Secure; "), HOUT(G_tmp))
#define PRINT_HTTP_COOKIE_L_EXP(ci)     (sprintf(G_tmp, "Set-Cookie: ls=%s; Expires=%s; %sHttpOnly\r\n", conn[ci].cookie_out_l, conn[ci].cookie_out_l_exp, G_test?"":"Secure; "), HOUT(G_tmp))
#else
#define PRINT_HTTP_COOKIE_A(ci)         (sprintf(G_tmp, "Set-Cookie: as=%s; HttpOnly\r\n", conn[ci].cookie_out_a), HOUT(G_tmp))
#define PRINT_HTTP_COOKIE_L(ci)         (sprintf(G_tmp, "Set-Cookie: ls=%s; HttpOnly\r\n", conn[ci].cookie_out_l), HOUT(G_tmp))
#define PRINT_HTTP_COOKIE_A_EXP(ci)     (sprintf(G_tmp, "Set-Cookie: as=%s; Expires=%s; HttpOnly\r\n", conn[ci].cookie_out_a, conn[ci].cookie_out_a_exp), HOUT(G_tmp))
#define PRINT_HTTP_COOKIE_L_EXP(ci)     (sprintf(G_tmp, "Set-Cookie: ls=%s; Expires=%s; HttpOnly\r\n", conn[ci].cookie_out_l, conn[ci].cookie_out_l_exp), HOUT(G_tmp))
#endif

/* framing */
#define PRINT_HTTP_SAMEORIGIN           HOUT("X-Frame-Options: SAMEORIGIN\r\n")

/* content type guessing */
#define PRINT_HTTP_NOSNIFF              HOUT("X-Content-Type-Options: nosniff\r\n")

/* identity */
#define PRINT_HTTP_SERVER               HOUT("Server: Silgy\r\n")

/* must be last! */
#define PRINT_HTTP_END_OF_HEADER        HOUT("\r\n")


#define OUT_HEADER_BUFSIZE              4096            /* response header buffer length */


#ifndef CUST_HDR_LEN
#define CUST_HDR_LEN                    255
#endif

#ifndef MAX_HOSTS                                       /* M_hosts size */
#define MAX_HOSTS                       10
#endif

#ifndef MAX_PAYLOAD_SIZE                                /* max incoming POST data length (16 MB) */
#define MAX_PAYLOAD_SIZE                16777216
#endif

#define MAX_LOG_STR_LEN                 4095            /* max log string length */
#define MAX_METHOD_LEN                  7               /* method length */
#define MAX_URI_LEN                     2047            /* max request URI length */
#define MAX_LABEL_LEN                   255             /* max request label length */
#define MAX_VALUE_LEN                   255             /* max request value length */
#define MAX_RESOURCE_LEN                127             /* max resource's name length -- as a first part of URI */
#define MAX_RESOURCES                   10000           /* for M_auth_levels */

/* mainly memory usage */

#ifdef MEM_TINY
#define IN_BUFSIZE                      4096            /* incoming request buffer length (4 kB) */
#define OUT_BUFSIZE                     65536           /* initial HTTP response buffer length (64 kB) */
#define MAX_CONNECTIONS                 10              /* max TCP connections */
#define MAX_SESSIONS                    5               /* max user sessions */
#elif defined MEM_MEDIUM
#define IN_BUFSIZE                      8192            /* incoming request buffer length (8 kB) */
#define OUT_BUFSIZE                     262144          /* initial HTTP response buffer length (256 kB) */
#define MAX_CONNECTIONS                 200             /* max TCP connections (2 per user session) */
#define MAX_SESSIONS                    100             /* max user sessions */
#elif defined MEM_LARGE
#define IN_BUFSIZE                      8192            /* incoming request buffer length (8 kB) */
#define OUT_BUFSIZE                     262144          /* initial HTTP response buffer length (256 kB) */
#define MAX_CONNECTIONS                 1000            /* max TCP connections */
#define MAX_SESSIONS                    500             /* max user sessions */
#elif defined MEM_XLARGE
#define IN_BUFSIZE                      8192            /* incoming request buffer length (8 kB) */
#define OUT_BUFSIZE                     262144          /* initial HTTP response buffer length (256 kB) */
#define MAX_CONNECTIONS                 5000            /* max TCP connections */
#define MAX_SESSIONS                    2500            /* max user sessions */
#elif defined MEM_XXLARGE
#define IN_BUFSIZE                      8192            /* incoming request buffer length (8 kB) */
#define OUT_BUFSIZE                     262144          /* initial HTTP response buffer length (256 kB) */
#define MAX_CONNECTIONS                 10000           /* max TCP connections */
#define MAX_SESSIONS                    5000            /* max user sessions */
#elif defined MEM_XXXLARGE
#define IN_BUFSIZE                      8192            /* incoming request buffer length (8 kB) */
#define OUT_BUFSIZE                     262144          /* initial HTTP response buffer length (256 kB) */
#define MAX_CONNECTIONS                 20000           /* max TCP connections */
#define MAX_SESSIONS                    10000           /* max user sessions */
#elif defined MEM_XXXXLARGE
#define IN_BUFSIZE                      8192            /* incoming request buffer length (8 kB) */
#define OUT_BUFSIZE                     262144          /* initial HTTP response buffer length (256 kB) */
#define MAX_CONNECTIONS                 50000           /* max TCP connections */
#define MAX_SESSIONS                    25000           /* max user sessions */
#else   /* MEM_SMALL -- default */
#define IN_BUFSIZE                      8192            /* incoming request buffer length (8 kB) */
#define OUT_BUFSIZE                     131072          /* initial HTTP response buffer length (128 kB) */
#define MAX_CONNECTIONS                 20              /* max TCP connections */
#define MAX_SESSIONS                    10              /* max user sessions */
#endif

#ifndef TMP_BUFSIZE                                     /* temporary string buffer size */
#define TMP_BUFSIZE                     OUT_BUFSIZE
#endif

#ifdef SILGY_SVC
#undef MAX_CONNECTIONS
#define MAX_CONNECTIONS                 1               /* conn, uses & auses still as arrays to keep the same macros */
#undef MAX_SESSIONS
#define MAX_SESSIONS                    1
#endif  /* SILGY_SVC */

#ifdef FD_MON_SELECT
#if MAX_CONNECTIONS > FD_SETSIZE-2
#undef MAX_CONNECTIONS
#define MAX_CONNECTIONS FD_SETSIZE-2
#endif
#endif  /* FD_MON_SELECT */

#define CLOSING_SESSION_CI              MAX_CONNECTIONS

#ifndef CONN_TIMEOUT
#define CONN_TIMEOUT                    180             /* idle connection timeout in seconds */
#endif

#ifndef USES_TIMEOUT
#define USES_TIMEOUT                    600             /* anonymous user session timeout in seconds */
#endif

#define NOT_CONNECTED                   -1

#define CONN_STATE_DISCONNECTED         '0'
#define CONN_STATE_ACCEPTING            'a'
#define CONN_STATE_CONNECTED            '1'
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

#define STR(str)                        lib_get_string(ci, str)

#ifndef MAX_BLACKLIST
#define MAX_BLACKLIST                   10000
#endif
#ifndef MAX_WHITELIST
#define MAX_WHITELIST                   10000
#endif

#ifndef APP_WEBSITE
#define APP_WEBSITE                     "Silgy Web Application"
#endif
#ifndef APP_DOMAIN
#define APP_DOMAIN                      ""
#endif
#ifndef APP_VERSION
#define APP_VERSION                     "1.0"
#endif
#ifndef APP_LOGIN_URI
#define APP_LOGIN_URI                   "/login"
#endif


/* compression settings */

#ifndef COMPRESS_TRESHOLD
#define COMPRESS_TRESHOLD               500
#endif

#ifndef COMPRESS_LEVEL
#define COMPRESS_LEVEL                  Z_BEST_SPEED
#endif

#define SHOULD_BE_COMPRESSED(len, type) (len > COMPRESS_TRESHOLD && (type==RES_HTML || type==RES_TEXT || type==RES_JSON || type==RES_CSS || type==RES_JS || type==RES_SVG || type==RES_EXE || type==RES_BMP))


#ifdef APP_SESID_LEN
#define SESID_LEN                       APP_SESID_LEN
#else
#define SESID_LEN                       15      /* ~ 89 bits of entropy */
#endif

#ifndef CSRFT_LEN
#define CSRFT_LEN                       7
#endif

#define LANG_LEN                        5


/* response caching */

#ifndef EXPIRES_STATICS
#define EXPIRES_STATICS                 90      /* days */
#endif

#ifndef EXPIRES_GENERATED
#define EXPIRES_GENERATED               30      /* days */
#endif


/* authorization levels */

#define AUTH_LEVEL_NONE                 0       /* no session */
#define AUTH_LEVEL_ANONYMOUS            1       /* anonymous session */
#define AUTH_LEVEL_LOGGED               2       /* logged in session with lowest authorization level */
#define AUTH_LEVEL_LOGGEDIN             2
#define AUTH_LEVEL_USER                 10
#define AUTH_LEVEL_CUSTOMER             20
#define AUTH_LEVEL_STAFF                30
#define AUTH_LEVEL_MODERATOR            40
#define AUTH_LEVEL_ADMIN                50
#define AUTH_LEVEL_ROOT                 100
#define AUTH_LEVEL_NOBODY               125     /* for resources' whitelisting */

#ifndef DEF_RES_AUTH_LEVEL
#define DEF_RES_AUTH_LEVEL              AUTH_LEVEL_NONE     /* default resource authorization level */
#endif


/* errors */

/* 0 always means OK */
#define ERR_INVALID_REQUEST             1
#define ERR_UNAUTHORIZED                2
#define ERR_FORBIDDEN                   3
#define ERR_NOT_FOUND                   4
#define ERR_METHOD                      5
#define ERR_INT_SERVER_ERROR            6
#define ERR_SERVER_TOOBUSY              7
#define ERR_FILE_TOO_BIG                8
#define ERR_REDIRECTION                 9
#define ERR_ASYNC_NO_SUCH_SERVICE       10
#define ERR_ASYNC_TIMEOUT               11
#define ERR_REMOTE_CALL                 12
#define ERR_REMOTE_CALL_STATUS          13
#define ERR_REMOTE_CALL_DATA            14
#define ERR_CSRFT                       15
#define ERR_RECORD_NOT_FOUND            16
/* ------------------------------------- */
#define ERR_MAX_ENGINE_ERROR            99
/* ------------------------------------- */


#define MSG_CAT_OK                      "OK"
#define MSG_CAT_ERROR                   "err"
#define MSG_CAT_WARNING                 "war"
#define MSG_CAT_MESSAGE                 "msg"


/* statics */

#define NOT_STATIC                      -1
#ifdef APP_MAX_STATICS                                          /* max static resources */
#define MAX_STATICS                     APP_MAX_STATICS
#else
#define MAX_STATICS                     1000
#endif

#define STATIC_PATH_LEN                 1024

#define STATIC_SOURCE_INTERNAL          0
#define STATIC_SOURCE_RES               1
#define STATIC_SOURCE_RESMIN            2
#define STATIC_SOURCE_SNIPPET           3

#ifndef MAX_SNIPPETS
#define MAX_SNIPPETS                    1000
#endif


/* asynchronous calls */

#define ASYNC_STATE_FREE                '0'
#define ASYNC_STATE_SENT                '1'
#define ASYNC_STATE_RECEIVED            '2'
#define ASYNC_STATE_TIMEOUTED           '3'


#ifndef ASYNC_MQ_MAXMSG
#define ASYNC_MQ_MAXMSG                 10                      /* max messages in a message queue */
#endif

#ifndef MAX_ASYNC_REQS
#define MAX_ASYNC_REQS                  MAX_SESSIONS            /* max simultaneous async requests */
#endif

#ifndef ASYNC_REQ_MSG_SIZE
#define ASYNC_REQ_MSG_SIZE              16384                   /* request message size */
#endif

#ifndef ASYNC_RES_MSG_SIZE
#define ASYNC_RES_MSG_SIZE              16384                   /* response message size */
#endif

#define ASYNC_REQ_QUEUE                 "/silgy_req"            /* request queue name */
#define ASYNC_RES_QUEUE                 "/silgy_res"            /* response queue name */
#define ASYNC_DEF_TIMEOUT               60                      /* in seconds */
#define ASYNC_MAX_TIMEOUT               1800                    /* in seconds ==> 30 minutes */

#define ASYNC_PAYLOAD_MSG               0
#define ASYNC_PAYLOAD_SHM               1

#define ASYNC_SHM_SIZE                  MAX_PAYLOAD_SIZE

/* these are flags */
#define ASYNC_CHUNK_FIRST               0x040000
#define ASYNC_CHUNK_LAST                0x080000
#define ASYNC_CHUNK_IS_FIRST(n)         ((n & ASYNC_CHUNK_FIRST) == ASYNC_CHUNK_FIRST)
#define ASYNC_CHUNK_IS_LAST(n)          ((n & ASYNC_CHUNK_LAST) == ASYNC_CHUNK_LAST)

#ifdef SILGY_SVC
#define SVC(svc)                        (0==strcmp(G_service, svc))
#define ASYNC_ERR_CODE                  G_error_code
#else
#define SVC(svc)                        (0==strcmp(conn[ci].service, svc))
#define ASYNC_ERR_CODE                  conn[ci].async_err_code
#endif  /* SILGY_SVC */

#ifdef ASYNC_USE_APP_CONTINUE   /* the old way (temporarily) */
#define CALL_ASYNC(svc, data)           eng_async_req(ci, svc, data, TRUE, G_ASYNCDefTimeout, 0)
#define CALL_ASYNC_TM(svc, data, tmout) eng_async_req(ci, svc, data, TRUE, tmout, 0)
#define CALL_ASYNC_NR(svc, data)        eng_async_req(ci, svc, data, FALSE, 0, 0)
#else
#define CALL_ASYNC(svc)                 eng_async_req(ci, svc, NULL, TRUE, G_ASYNCDefTimeout, 0)
#define CALL_ASYNC_TM(svc, tmout)       eng_async_req(ci, svc, NULL, TRUE, tmout, 0)
#define CALL_ASYNC_NR(svc)              eng_async_req(ci, svc, NULL, FALSE, 0, 0)
#endif  /* ASYNC_USE_APP_CONTINUE */

#define CALL_ASYNC_BIN(svc, data, size) eng_async_req(ci, svc, data, TRUE, G_ASYNCDefTimeout, size)


/* resource / content types */

/* incoming */

#define CONTENT_TYPE_URLENCODED         'U'
#define CONTENT_TYPE_JSON               'J'
#define CONTENT_TYPE_MULTIPART          'M'
#define CONTENT_TYPE_OCTET_STREAM       'O'

/* outgoing */

#define CONTENT_TYPE_UNSET              '-'
#define CONTENT_TYPE_USER               '+'
#define RES_TEXT                        'T'
#define RES_HTML                        'H'
#define RES_CSS                         'C'
#define RES_JS                          'S'
#define RES_GIF                         'G'
#define RES_JPG                         'J'
#define RES_ICO                         'I'
#define RES_PNG                         'P'
#define RES_BMP                         'B'
#define RES_SVG                         'V'
#define RES_JSON                        'O'
#define RES_PDF                         'A'
#define RES_AMPEG                       'M'
#define RES_EXE                         'X'
#define RES_ZIP                         'Z'


#define URI(uri)                        eng_is_uri(ci, uri)
#define REQ(res)                        (0==strcmp(conn[ci].resource, res))
#define REQ0(res)                       (0==strcmp(conn[ci].resource, res))
#define REQ1(res)                       (0==strcmp(conn[ci].req1, res))
#define REQ2(res)                       (0==strcmp(conn[ci].req2, res))
#define REQ3(res)                       (0==strcmp(conn[ci].req3, res))
#define ID                              conn[ci].id
#define US                              uses[conn[ci].usi]
#define AUS                             auses[conn[ci].usi]
#define HOST(str)                       (0==strcmp(conn[ci].host_normalized, upper(str)))
#define REQ_GET_HEADER(header)          eng_get_header(ci, header)

#define REQ_DATA                        conn[ci].in_data

#define REST_HEADER_PASS(header)        eng_rest_header_pass(ci, header)


/* response macros */

#define RES_STATUS(val)                 lib_set_res_status(ci, val)
#define RES_CONTENT_TYPE(str)           lib_set_res_content_type(ci, str)
#define RES_LOCATION(str, ...)          lib_set_res_location(ci, str, ##__VA_ARGS__)
#define RES_REDIRECT(str, ...)          RES_LOCATION(str, ##__VA_ARGS__)
#define RES_DONT_CACHE                  conn[ci].dont_cache=TRUE
#define RES_KEEP_CONTENT                conn[ci].keep_content=TRUE
#define RES_CONTENT_DISPOSITION(str, ...) lib_set_res_content_disposition(ci, str, ##__VA_ARGS__)

#define REDIRECT_TO_LANDING             sprintf(conn[ci].location, "%s://%s", PROTOCOL, conn[ci].host)

#define APPEND_CSS(name, first)         lib_append_css(ci, name, first)
#define APPEND_SCRIPT(name, first)      lib_append_script(ci, name, first)



#define SVC_NAME_LEN                    63      /* async service name length */


//#define UA_IE                           (0==strcmp(conn[ci].uagent, "Mozilla/5.0 (Windows NT 6.1; WOW64; Trident/7.0; rv:11.0) like Gecko") || 0==strcmp(conn[ci].uagent, "Mozilla/5.0 (Windows NT 10.0; WOW64; Trident/7.0; rv:11.0) like Gecko"))
#define UA_IE                           (0==strncmp(conn[ci].uagent, "Mozilla/5.0 (Windows NT ", 24) && strstr(conn[ci].uagent, "; WOW64; Trident/7.0; rv:11.0) like Gecko"))


/* Date-Time */

//#define DT_NULL                         "2000-01-01 00:00:00"
#define DT_NULL                         "0000-00-00 00:00:00"
#define DT_NOW                          G_dt
#define DT_TODAY                        silgy_today()
#define DT_NOW_LOCAL                    time_epoch2db(silgy_ua_time(ci))
#define DT_TODAY_LOCAL                  silgy_ua_today(ci)
#define DT_IS_NULL(dt)                  (0==strcmp(dt, DT_NULL))



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


/* user session */

typedef struct {
    /* id */
    char    sesid[SESID_LEN+1];
    /* connection data */
    char    ip[INET_ADDRSTRLEN];
    char    uagent[MAX_VALUE_LEN+1];
    char    referer[MAX_VALUE_LEN+1];
    char    lang[LANG_LEN+1];
    bool    logged;
    /* users table record */
    int     uid;
    char    login[LOGIN_LEN+1];
    char    email[EMAIL_LEN+1];
    char    name[UNAME_LEN+1];
    char    phone[PHONE_LEN+1];
    char    about[ABOUT_LEN+1];
    int     group_id;
    char    auth_level;
    /* time zone info */
    short   tz_offset;
    bool    tz_set;
    /* CSRF token */
    char    csrft[CSRFT_LEN+1];
    /* internal */
    time_t  last_activity;
} usession_t;


#define CUSTOMER                        (US.auth_level==AUTH_LEVEL_CUSTOMER)
#define STAFF                           (US.auth_level==AUTH_LEVEL_STAFF)
#define MODERATOR                       (US.auth_level==AUTH_LEVEL_MODERATOR)
#define ADMIN                           (US.auth_level==AUTH_LEVEL_ADMIN)
#define ROOT                            (US.auth_level==AUTH_LEVEL_ROOT)

#define LOGGED                          US.logged
#define UID                             US.uid


#define CSRFT_REFRESH                   silgy_random(US.csrft, CSRFT_LEN)
#define CSRFT_OUT_INPUT                 OUT("<input type=\"hidden\" name=\"csrft\" value=\"%s\">", US.csrft)
#define OUT_CSRFT                       CSRFT_OUT_INPUT
#define CSRFT_OK                        lib_csrft_ok(ci)



/* counters */

typedef struct {
    unsigned req;            /* all parsed requests */
    unsigned req_dsk;        /* all requests with desktop UA */
    unsigned req_mob;        /* all requests with mobile UA */
    unsigned req_bot;        /* all requests with HTTP header indicating well-known search-engine bots */
    unsigned visits;         /* all visits to domain (Host=APP_DOMAIN) landing page (no action/resource), excl. bots that got 200 */
    unsigned visits_dsk;     /* like visits -- desktop only */
    unsigned visits_mob;     /* like visits -- mobile only */
    unsigned blocked;        /* attempts from blocked IP */
    double   elapsed;        /* sum of elapsed time of all requests for calculating average */
    double   average;        /* average request elapsed */
} counters_t;


/* asynchorous processing */

/* request */

typedef struct {
    unsigned call_id;
    int      ai;
    int      ci;
    char     service[SVC_NAME_LEN+1];
    /* pass some request details over */
    bool     secure;
    char     ip[INET_ADDRSTRLEN];
    char     method[MAX_METHOD_LEN+1];
    bool     post;
    char     payload_location;
    char     uri[MAX_URI_LEN+1];
    char     resource[MAX_RESOURCE_LEN+1];
    char     req1[MAX_RESOURCE_LEN+1];
    char     req2[MAX_RESOURCE_LEN+1];
    char     req3[MAX_RESOURCE_LEN+1];
    char     id[MAX_RESOURCE_LEN+1];
    char     uagent[MAX_VALUE_LEN+1];
    bool     mobile;
    char     referer[MAX_VALUE_LEN+1];
    unsigned clen;
    char     in_cookie[MAX_VALUE_LEN+1];
    char     host[MAX_VALUE_LEN+1];
    char     website[WEBSITE_LEN+1];
    char     lang[LANG_LEN+1];
    char     in_ctype;
    char     boundary[MAX_VALUE_LEN+1];
    char     response;
    int      status;
    char     cust_headers[CUST_HDR_LEN+1];
    int      cust_headers_len;
    char     ctype;
    char     ctypestr[CONTENT_TYPE_LEN+1];
    char     cdisp[CONTENT_DISP_LEN+1];
    char     cookie_out_a[SESID_LEN+1];
    char     cookie_out_a_exp[32];
    char     cookie_out_l[SESID_LEN+1];
    char     cookie_out_l_exp[32];
    char     location[MAX_URI_LEN+1];
    int      usi;
    bool     bot;
    bool     dont_cache;
    bool     keep_content;
    usession_t uses;
#ifndef ASYNC_EXCLUDE_AUSES
    ausession_t auses;
#endif
    counters_t cnts_today;
    counters_t cnts_yesterday;
    counters_t cnts_day_before;
    int      days_up;
    int      open_conn;
    int      open_conn_hwm;
    int      sessions;
    int      sessions_hwm;
    char     last_modified[32];
    int      blacklist_cnt;
} async_req_hdr_t;

typedef struct {
    async_req_hdr_t hdr;
    char            data[ASYNC_REQ_MSG_SIZE-sizeof(async_req_hdr_t)];
} async_req_t;


/* async requests stored on the silgy_app's side */

typedef struct {
    int      ci;
    char     state;
    time_t   sent;
    int      timeout;
} areq_t;


/* response -- the first chunk */

typedef struct {
    int      err_code;
    int      status;
    char     cust_headers[CUST_HDR_LEN+1];
    int      cust_headers_len;
    char     ctype;
    char     ctypestr[CONTENT_TYPE_LEN+1];
    char     cdisp[CONTENT_DISP_LEN+1];
    char     cookie_out_a[SESID_LEN+1];
    char     cookie_out_a_exp[32];
    char     cookie_out_l[SESID_LEN+1];
    char     cookie_out_l_exp[32];
    char     location[MAX_URI_LEN+1];
    bool     dont_cache;
    bool     keep_content;
    int      rest_status;
    unsigned rest_req;
    double   rest_elapsed;
    usession_t uses;
#ifndef ASYNC_EXCLUDE_AUSES
    ausession_t auses;
#endif
    int      invalidate_uid;
    int      invalidate_ci;
} async_res_hdr_t;

typedef struct {
    int             ai;
    int             chunk;
    int             ci;
    int             len;
    async_res_hdr_t hdr;
    char            data[ASYNC_RES_MSG_SIZE-sizeof(async_res_hdr_t)-sizeof(int)*4];
} async_res_t;


/* response -- the second type for the chunks > 1 */

typedef struct {
    int      ai;
    int      chunk;
    int      ci;
    int      len;
    char     data[ASYNC_RES_MSG_SIZE-sizeof(int)*4];
} async_res_data_t;



/* connection */

#ifdef SILGY_SVC
typedef struct {                            /* request details for silgy_svc */
    bool     secure;
    char     ip[INET_ADDRSTRLEN];
    char     method[MAX_METHOD_LEN+1];
    bool     post;
    char     uri[MAX_URI_LEN+1];
    char     resource[MAX_RESOURCE_LEN+1];
    char     req1[MAX_RESOURCE_LEN+1];
    char     req2[MAX_RESOURCE_LEN+1];
    char     req3[MAX_RESOURCE_LEN+1];
    char     id[MAX_RESOURCE_LEN+1];
    char     uagent[MAX_VALUE_LEN+1];
    bool     mobile;
    char     referer[MAX_VALUE_LEN+1];
    unsigned clen;
    char     in_cookie[MAX_VALUE_LEN+1];
    unsigned in_data_allocated;
    char     host[MAX_VALUE_LEN+1];
    char     website[WEBSITE_LEN+1];
    char     lang[LANG_LEN+1];
    char     in_ctype;
    char     boundary[MAX_VALUE_LEN+1];
    char     *in_data;
    int      status;
    char     cust_headers[CUST_HDR_LEN+1];
    int      cust_headers_len;
    char     ctype;
    char     ctypestr[CONTENT_TYPE_LEN+1];
    char     cdisp[CONTENT_DISP_LEN+1];
    char     cookie_out_a[SESID_LEN+1];
    char     cookie_out_a_exp[32];
    char     cookie_out_l[SESID_LEN+1];
    char     cookie_out_l_exp[32];
    char     location[MAX_URI_LEN+1];
    int      usi;
    bool     bot;
    bool     dont_cache;
    bool     keep_content;
} conn_t;
#else   /* SILGY_APP */
typedef struct {
    /* what comes in */
#ifdef _WIN32   /* Windows */
    SOCKET   fd;                             /* file descriptor */
#else
    int      fd;                             /* file descriptor */
#endif  /* _WIN32 */
    bool     secure;                         /* https? */
    char     ip[INET_ADDRSTRLEN];            /* client IP */
    char     pip[INET_ADDRSTRLEN];           /* proxy IP */
    char     in[IN_BUFSIZE];                 /* the whole incoming request */
    char     method[MAX_METHOD_LEN+1];       /* HTTP method */
    unsigned was_read;                       /* request bytes read so far */
    bool     upgrade2https;                  /* Upgrade-Insecure-Requests = 1 */
    /* parsed HTTP request starts here */
    bool     head_only;                      /* request method = HEAD */
    bool     post;                           /* request method = POST */
    char     uri[MAX_URI_LEN+1];             /* requested URI string */
    char     resource[MAX_RESOURCE_LEN+1];   /* from URI (REQ0) */
    char     req1[MAX_RESOURCE_LEN+1];       /* from URI -- level 1 */
    char     req2[MAX_RESOURCE_LEN+1];       /* from URI -- level 2 */
    char     req3[MAX_RESOURCE_LEN+1];       /* from URI -- level 3 */
    char     id[MAX_RESOURCE_LEN+1];         /* from URI -- last part */
    char     proto[16];                      /* HTTP request version */
    char     uagent[MAX_VALUE_LEN+1];        /* user agent string */
    bool     mobile;
    bool     keep_alive;
    char     referer[MAX_VALUE_LEN+1];
    unsigned clen;                           /* incoming & outgoing content length */
    char     in_cookie[MAX_VALUE_LEN+1];
    char     cookie_in_a[SESID_LEN+1];       /* anonymous */
    char     cookie_in_l[SESID_LEN+1];       /* logged in */
    char     host[MAX_VALUE_LEN+1];
    char     host_normalized[MAX_VALUE_LEN+1];
    char     website[WEBSITE_LEN+1];
    char     lang[LANG_LEN+1];
    time_t   if_mod_since;
    char     in_ctypestr[MAX_VALUE_LEN+1];   /* content type as an original string */
    char     in_ctype;                       /* content type */
    char     boundary[MAX_VALUE_LEN+1];      /* for POST multipart/form-data type */
    char     authorization[MAX_VALUE_LEN+1]; /* Authorization */
    bool     accept_deflate;
    int      host_id;
    /* POST data */
    char     *in_data;                       /* POST data */
    /* what goes out */
    unsigned out_hlen;                       /* outgoing header length */
    unsigned out_len;                        /* outgoing length (all) */
    char     *out_start;
#ifdef OUTCHECKREALLOC
    char     *out_data_alloc;                /* allocated space for rendered content */
#else
    char     out_data_alloc[OUT_BUFSIZE];
#endif
    unsigned out_data_allocated;             /* number of allocated bytes */
    char     *out_data;                      /* pointer to the data to send */
    int      status;                         /* HTTP status */
    char     cust_headers[CUST_HDR_LEN+1];
    int      cust_headers_len;
    unsigned data_sent;                      /* how many content bytes have been sent */
    char     ctype;                          /* content type */
    char     ctypestr[CONTENT_TYPE_LEN+1];   /* user (custom) content type */
    char     cdisp[CONTENT_DISP_LEN+1];      /* content disposition */
    time_t   modified;
    char     cookie_out_a[SESID_LEN+1];
    char     cookie_out_a_exp[32];           /* cookie expires */
    char     cookie_out_l[SESID_LEN+1];
    char     cookie_out_l_exp[32];           /* cookie expires */
    char     location[MAX_URI_LEN+1];        /* redirection */
    /* internal stuff */
    unsigned req;                            /* request count */
    struct timespec proc_start;             /* start processing time */
    double   elapsed;                        /* processing time in ms */
    char     conn_state;                     /* connection state (STATE_XXX) */
    char     *p_header;                      /* current header pointer */
    char     *p_content;                     /* current content pointer */
#ifdef HTTPS
    SSL      *ssl;
#endif
    int      ssl_err;
    char     required_auth_level;            /* required authorization level */
    int      usi;                            /* user session index */
    int      static_res;                     /* static resource index in M_stat */
    time_t   last_activity;
    bool     bot;
    bool     expect100;
    bool     dont_cache;
    bool     keep_content;                   /* don't reset already rendered content on error */
#ifdef FD_MON_POLL
    int      pi;                             /* pollfds array index */
#endif
#ifdef ASYNC
    char     service[SVC_NAME_LEN+1];
    int      async_err_code;
//    int      ai;                             /* async responses array index */
#endif
} conn_t;
#endif  /* SILGY_SVC */


/* static resources */

typedef struct {
    char     host[MAX_VALUE_LEN+1];
    char     name[STATIC_PATH_LEN];
    char     type;
    char     *data;
    unsigned len;
    char     *data_deflated;
    unsigned len_deflated;
    time_t   modified;
    char     source;
} stat_res_t;


/* admin info */

typedef struct {
    char sql[1024];
    char th[256];
    char type[32];
} admin_info_t;


/* counters formatted */

typedef struct {
    char req[64];
    char req_dsk[64];
    char req_mob[64];
    char req_bot[64];
    char visits[64];
    char visits_dsk[64];
    char visits_mob[64];
    char blocked[64];
    char average[64];
} counters_fmt_t;



#include <silgy_lib.h>

#ifdef USERS
#include <silgy_usr.h>
#endif



#ifdef __cplusplus
extern "C" {
#endif

/* read from the config file */

extern int          G_logLevel;
extern int          G_logToStdout;
extern int          G_logCombined;
extern int          G_httpPort;
extern int          G_httpsPort;
extern char         G_cipherList[1024];
extern char         G_certFile[256];
extern char         G_certChainFile[256];
extern char         G_keyFile[256];
extern char         G_dbHost[128];
extern int          G_dbPort;
extern char         G_dbName[128];
extern char         G_dbUser[128];
extern char         G_dbPassword[128];
extern int          G_usersRequireAccountActivation;
extern char         G_blockedIPList[256];
extern char         G_whiteList[256];
extern int          G_ASYNCId;
extern int          G_ASYNCDefTimeout;
extern int          G_RESTTimeout;
extern int          G_test;

/* end of config params */

extern int          G_pid;                      /* pid */
extern char         G_appdir[256];              /* application root dir */
extern int          G_days_up;                  /* web server's days up */
extern conn_t       conn[MAX_CONNECTIONS+1];    /* HTTP connections & requests -- by far the most important structure around */
extern int          G_open_conn;                /* number of open connections */
extern int          G_open_conn_hwm;            /* highest number of open connections (high water mark) */
extern char         G_tmp[TMP_BUFSIZE];         /* temporary string buffer */
extern usession_t   uses[MAX_SESSIONS+1];       /* engine user sessions -- they start from 1 */
extern ausession_t  auses[MAX_SESSIONS+1];      /* app user sessions, using the same index (usi) */
extern int          G_sessions;                 /* number of active user sessions */
extern int          G_sessions_hwm;             /* highest number of active user sessions (high water mark) */
extern time_t       G_now;                      /* current time */
extern struct tm    *G_ptm;                     /* human readable current time */
extern char         G_last_modified[32];        /* response header field with server's start time */
extern bool         G_initialized;

/* messages */
extern message_t    G_messages[MAX_MESSAGES];
extern int          G_next_msg;
extern lang_t       G_msg_lang[MAX_LANGUAGES];
extern int          G_next_msg_lang;

/* strings */
extern string_t     G_strings[MAX_STRINGS];
extern int          G_next_str;
extern lang_t       G_str_lang[MAX_LANGUAGES];
extern int          G_next_str_lang;

/* snippets */
extern stat_res_t   G_snippets[MAX_SNIPPETS];
extern int          G_snippets_cnt;

#ifdef HTTPS
extern bool         G_ssl_lib_initialized;
#endif

#ifdef DBMYSQL
extern MYSQL        *G_dbconn;                  /* database connection */
#endif

/* asynchorous processing */
#ifndef _WIN32
extern char         G_req_queue_name[256];
extern char         G_res_queue_name[256];
extern mqd_t        G_queue_req;                /* request queue */
extern mqd_t        G_queue_res;                /* response queue */
#endif  /* _WIN32 */
extern int          G_async_req_data_size;      /* how many bytes are left for data */
extern int          G_async_res_data_size;      /* how many bytes are left for data */

extern char         G_dt[20];                   /* datetime for database or log (YYYY-MM-DD hh:mm:ss) */
extern bool         G_index_present;            /* index.html present in res? */

#ifdef SILGY_SVC
extern async_req_t  req;
extern async_res_t  res;
#ifdef OUTCHECKREALLOC
extern char         *out_data;
#endif
extern char         *p_content;
extern char         G_service[SVC_NAME_LEN+1];
extern int          G_error_code;
extern int          G_usi;
#endif  /* SILGY_SVC */

extern char         G_blacklist[MAX_BLACKLIST+1][INET_ADDRSTRLEN];
extern int          G_blacklist_cnt;            /* G_blacklist length */

extern char         G_whitelist[MAX_WHITELIST+1][INET_ADDRSTRLEN];
extern int          G_whitelist_cnt;            /* G_whitelist length */
/* counters */
extern counters_t   G_cnts_today;               /* today's counters */
extern counters_t   G_cnts_yesterday;           /* yesterday's counters */
extern counters_t   G_cnts_day_before;          /* day before's counters */
/* REST */
extern int          G_rest_status;              /* last REST call response status */
extern unsigned     G_rest_req;                 /* REST calls counter */
extern double       G_rest_elapsed;             /* REST calls elapsed for calculating average */
extern double       G_rest_average;             /* REST calls average elapsed */
extern char         G_rest_content_type[MAX_VALUE_LEN+1];
extern int          G_rest_res_len;
extern int          G_new_user_id;
extern int          G_qs_len;


#ifdef __cplusplus
}   /* extern "C" */
#endif




/* prototypes */

#ifdef __cplusplus
extern "C" {
#endif

    /* public engine functions */

    void silgy_set_auth_level(const char *path, char level);
    int  eng_uses_start(int ci, const char *sesid);
    void eng_uses_downgrade_by_uid(int uid, int ci);
    void eng_async_req(int ci, const char *service, const char *data, char response, int timeout, int size);
    void silgy_add_to_static_res(const char *name, const char *src);
    void eng_block_ip(const char *value, bool autoblocked);
    bool eng_is_uri(int ci, const char *uri);
    bool silgy_set_host_res(const char *host, const char *res, const char *resmin);
    void eng_out_check(int ci, const char *str);
    void eng_out_check_realloc(int ci, const char *str);
    void eng_out_check_realloc_bin(int ci, const char *data, int len);
    char *eng_get_header(int ci, const char *header);
    void eng_rest_header_pass(int ci, const char *header);

    /* public app functions */

#ifdef SILGY_SVC
    void svc_out_check(const char *str);
    void svc_out_check_realloc(const char *str);
    void svc_out_check_realloc_bin(const char *data, int len);
    bool silgy_svc_init(void);
    void silgy_svc_main(int ci);
    void silgy_svc_done(void);
#ifdef APP_ERROR_PAGE
    void silgy_app_error_page(int ci, int code);
#endif
#ifdef APP_ACTIVATION_EMAIL
    int silgy_app_user_activation_email(int ci, int uid, const char *email, const char *linkkey);
#endif
#else   /* not SILGY_SVC */
    bool silgy_app_init(int argc, char *argv[]);
    void silgy_app_done(void);
    void silgy_app_main(int ci);
    bool silgy_app_session_init(int ci);
    void silgy_app_session_done(int ci);
#ifdef ASYNC
#ifdef ASYNC_USE_APP_CONTINUE   /* depreciated */
    void silgy_app_continue(int ci, const char *data);
#endif
#endif
#ifdef APP_ERROR_PAGE
    void silgy_app_error_page(int ci, int code);
#endif
#ifdef EVERY_SECOND
    void app_every_second(void);
#endif
#ifdef APP_ACTIVATION_EMAIL
    int silgy_app_user_activation_email(int ci, int uid, const char *email, const char *linkkey);
#endif
#endif  /* SILGY_SVC */

#ifdef USERS
    bool silgy_app_user_login(int ci);
    void silgy_app_user_logout(int ci);
#endif

#ifdef __cplusplus
}   /* extern "C" */
#endif



#endif  /* SILGY_H */
