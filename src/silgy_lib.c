/* --------------------------------------------------------------------------
   Silgy Web App Engine
   Jurek Muszynski
   silgy.com
   Some parts are Public Domain from other authors, see below
-----------------------------------------------------------------------------
   General purpose library
-------------------------------------------------------------------------- */

#ifdef ICONV
#include <iconv.h>
#endif

#include "silgy.h"


/* globals */

int         G_logLevel=3;               /* log level -- 'info' by default */
char        G_appdir[256]=".";          /* application root dir */
int         G_RESTTimeout=CALL_REST_DEFAULT_TIMEOUT;
int         G_test=0;                   /* test run */
int         G_pid=0;                    /* pid */
time_t      G_now=0;                    /* current time (GMT) */
struct tm   *G_ptm={0};                 /* human readable current time */
char        G_dt[20]="";                /* datetime for database or log (YYYY-MM-DD hh:mm:ss) */
char        G_tmp[TMP_BUFSIZE];         /* temporary string buffer */
char        *G_shm_segptr=NULL;         /* SHM pointer */


/* locals */

static char *M_conf=NULL;               /* config file content */
static FILE *M_log_fd=(FILE*)STDOUT_FILENO; /* log file handle */

static char M_df=0;                     /* date format */
static char M_tsep=' ';                 /* thousand separator */
static char M_dsep='.';                 /* decimal separator */

static int  M_shmid;                    /* SHM id */

static rest_header_t M_rest_headers[REST_MAX_HEADERS];
static int M_rest_headers_cnt=0;
#ifdef _WIN32   /* Windows */
static SOCKET M_rest_sock;
#else
static int M_rest_sock;
#endif  /* _WIN32 */
static int M_rest_connection;
#ifdef HTTPS
static SSL_CTX *M_ssl_ctx=NULL;
static SSL *M_rest_ssl=NULL;
#else
static void *M_rest_ssl=NULL;    /* dummy */
#endif /* HTTPS */
#ifdef AUTO_INIT_EXPERIMENT
static void *M_jsons[JSON_MAX_JSONS];   /* array of pointers for auto-init */
static int M_jsons_cnt=0;
#endif /* AUTO_INIT_EXPERIMENT */
static JSON M_json_pool[JSON_POOL_SIZE*JSON_MAX_LEVELS];    /* for lib_json_from_string */
static int M_json_pool_cnt[JSON_MAX_LEVELS]={0};

static void minify_1(char *dest, const char *src);
static int minify_2(char *dest, const char *src);
static void get_byteorder32(void);
static void get_byteorder64(void);


/* --------------------------------------------------------------------------
   Get the last part of path
-------------------------------------------------------------------------- */
void lib_get_exec_name(char *dst, const char *path)
{
    const char *p=path;
    const char *pd=NULL;

    while ( *p )
    {
#ifdef _WIN32
        if ( *p == '\\' )
#else
        if ( *p == '/' )
#endif
        {
            if ( *(p+1) )   /* not EOS */
                pd = p+1;
        }
        ++p;
    }

    if ( pd )
        strcpy(dst, pd);
    else
        strcpy(dst, path);

//    DBG("exec name [%s]", dst);
}


/* --------------------------------------------------------------------------
   Update G_now, G_ptm and G_dt
-------------------------------------------------------------------------- */
void lib_update_time_globals()
{
    G_now = time(NULL);
    G_ptm = gmtime(&G_now);
    sprintf(G_dt, "%d-%02d-%02d %02d:%02d:%02d", G_ptm->tm_year+1900, G_ptm->tm_mon+1, G_ptm->tm_mday, G_ptm->tm_hour, G_ptm->tm_min, G_ptm->tm_sec);
}


#ifdef HTTPS

#include <openssl/err.h>

/* --------------------------------------------------------------------------
   Log SSL error
-------------------------------------------------------------------------- */
static void log_ssl() 
{ 
    char buf[256]; 
    u_long err; 

    while ( (err=ERR_get_error()) != 0 )
    { 
        ERR_error_string_n(err, buf, sizeof(buf)); 
        ERR(buf); 
    } 
} 
#endif /* HTTPS */

/* --------------------------------------------------------------------------
   Init SSL for a client
-------------------------------------------------------------------------- */
static bool init_ssl_client()
{
#ifdef HTTPS

#ifndef __linux__
#ifndef _WIN32
    /* AIX */
    SSL_METHOD  *method;
#else
    const SSL_METHOD    *method;
#endif
#else
    const SSL_METHOD    *method;
#endif

    DBG("init_ssl (silgy_lib)");

    method = SSLv23_client_method();    /* negotiate the highest protocol version supported by both the server and the client */

    M_ssl_ctx = SSL_CTX_new(method);    /* create new context from method */

    if ( M_ssl_ctx == NULL )
    {
        ERR("SSL_CTX_new failed");
        return FALSE;
    }

    const long flags = SSL_OP_ALL | SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3;
    SSL_CTX_set_options(M_ssl_ctx, flags);

    /* temporarily ignore server cert errors */

    WAR("Ignoring remote server cert errors for REST calls");
    SSL_CTX_set_verify(M_ssl_ctx, SSL_VERIFY_NONE, NULL);

#endif /* HTTPS */
    return TRUE;
}


/* --------------------------------------------------------------------------
   Set socket as non-blocking
-------------------------------------------------------------------------- */
void lib_setnonblocking(int sock)
{
#ifdef _WIN32   /* Windows */

    u_long mode = 1;  // 1 to enable non-blocking socket
    ioctlsocket(sock, FIONBIO, &mode);

#else   /* Linux / UNIX */

    int opts;

    opts = fcntl(sock, F_GETFL);

    if ( opts < 0 )
    {
        ERR("fcntl(F_GETFL) failed");
        return;
    }

    opts = (opts | O_NONBLOCK);

    if ( fcntl(sock, F_SETFL, opts) < 0 )
    {
        ERR("fcntl(F_SETFL) failed");
        return;
    }
#endif
}


/* --------------------------------------------------------------------------
   REST call -- reset request headers
-------------------------------------------------------------------------- */
void lib_rest_headers_reset()
{
    M_rest_headers_cnt = 0;
}


/* --------------------------------------------------------------------------
   REST call -- set request header value
-------------------------------------------------------------------------- */
void lib_rest_header_set(const char *key, const char *value)
{
    int i;

    for ( i=0; i<M_rest_headers_cnt; ++i )
    {
        if ( M_rest_headers[i].key[0]==EOS )
        {
            strncpy(M_rest_headers[M_rest_headers_cnt].key, key, 63);
            M_rest_headers[M_rest_headers_cnt].key[63] = EOS;
            strncpy(M_rest_headers[i].value, value, 255);
            M_rest_headers[i].value[255] = EOS;
            return;
        }
        else if ( 0==strcmp(M_rest_headers[i].key, key) )
        {
            strncpy(M_rest_headers[i].value, value, 255);
            M_rest_headers[i].value[255] = EOS;
            return;
        }
    }

    if ( M_rest_headers_cnt >= REST_MAX_HEADERS ) return;

    strncpy(M_rest_headers[M_rest_headers_cnt].key, key, 63);
    M_rest_headers[M_rest_headers_cnt].key[63] = EOS;
    strncpy(M_rest_headers[M_rest_headers_cnt].value, value, 255);
    M_rest_headers[M_rest_headers_cnt].value[255] = EOS;
    ++M_rest_headers_cnt;
}


/* --------------------------------------------------------------------------
   REST call -- unset request header value
-------------------------------------------------------------------------- */
void lib_rest_header_unset(const char *key)
{
    int i;

    for ( i=0; i<M_rest_headers_cnt; ++i )
    {
        if ( 0==strcmp(M_rest_headers[i].key, key) )
        {
            M_rest_headers[i].key[0] = EOS;
            M_rest_headers[i].value[0] = EOS;
            return;
        }
    }
}


/* --------------------------------------------------------------------------
   REST call / close connection
-------------------------------------------------------------------------- */
static void close_conn(int sock)
{
#ifdef _WIN32   /* Windows */
    closesocket(sock);
#else
    close(sock);
#endif  /* _WIN32 */
}


/* --------------------------------------------------------------------------
   REST call / parse URL
-------------------------------------------------------------------------- */
static bool rest_parse_url(const char *url, char *host, char *port, char *uri, bool *secure)
{
    int len = strlen(url);

    if ( len < 1 )
    {
        ERR("url too short (1)");
        return FALSE;
    }

//    if ( url[len-1] == '/' ) endingslash = TRUE;

    if ( len > 6 && url[0]=='h' && url[1]=='t' && url[2]=='t' && url[3]=='p' && url[4]==':' )
    {
        url += 7;
        len -= 7;
        if ( len < 1 )
        {
            ERR("url too short (2)");
            return FALSE;
        }
    }
    else if ( len > 7 && url[0]=='h' && url[1]=='t' && url[2]=='t' && url[3]=='p' && url[4]=='s' && url[5]==':' )
    {
#ifdef HTTPS
        *secure = TRUE;
        
        url += 8;
        len -= 8;
        if ( len < 1 )
        {
            ERR("url too short (2)");
            return FALSE;
        }

        if ( !M_ssl_ctx && !init_ssl_client() )   /* first time */
        {
            ERR("init_ssl failed");
            return FALSE;
        }
#else
        ERR("HTTPS is not enabled");
        return FALSE;
#endif /* HTTPS */
    }

#ifdef DUMP
    DBG("url [%s]", url);
#endif

    char *colon, *slash;

    colon = strchr((char*)url, ':');
    slash = strchr((char*)url, '/');

    if ( colon )    /* port specified */
    {
        strncpy(host, url, colon-url);
        host[colon-url] = EOS;

        if ( slash )
        {
            strncpy(port, colon+1, slash-colon-1);
            port[slash-colon-1] = EOS;
            strcpy(uri, slash+1);
        }
        else    /* only host name & port */
        {
            strcpy(port, colon+1);
            uri[0] = EOS;
        }
    }
    else    /* no port specified */
    {
        if ( slash )
        {
            strncpy(host, url, slash-url);
            host[slash-url] = EOS;
            strcpy(uri, slash+1);
        }
        else    /* only host name */
        {
            strcpy(host, url);
            uri[0] = EOS;
        }
#ifdef HTTPS
        if ( *secure )
            strcpy(port, "443");
        else
#endif /* HTTPS */
        strcpy(port, "80");
    }
#ifdef DUMP
    DBG("secure=%d", *secure);
    DBG("host [%s]", host);
    DBG("port [%s]", port);
    DBG(" uri [%s]", uri);
#endif
    return TRUE;
}


/* --------------------------------------------------------------------------
   REST call / render request
-------------------------------------------------------------------------- */
static int rest_render_req(char *buffer, const char *method, const char *host, const char *uri, const void *req, bool json, bool keep)
{
    char *p=buffer;     /* stpcpy is faster than strcat */

    /* header */

    p = stpcpy(p, method);
    p = stpcpy(p, " /");
    p = stpcpy(p, uri);
    p = stpcpy(p, " HTTP/1.1\r\n");
    p = stpcpy(p, "Host: ");
    p = stpcpy(p, host);
    p = stpcpy(p, "\r\n");

    if ( 0 != strcmp(method, "GET") && req )
    {
        if ( json )     /* JSON -> string conversion */
        {
            p = stpcpy(p, "Content-Type: application/json\r\n");
            strcpy(G_tmp, lib_json_to_string((JSON*)req));
        }
        char tmp[64];
        sprintf(tmp, "Content-Length: %d\r\n", strlen(json?G_tmp:(char*)req));
        p = stpcpy(p, tmp);
    }

    int i;

    for ( i=0; i<M_rest_headers_cnt; ++i )
    {
        if ( M_rest_headers[i].key[0] )
        {
            p = stpcpy(p, M_rest_headers[i].key);
            p = stpcpy(p, ": ");
            p = stpcpy(p, M_rest_headers[i].value);
            p = stpcpy(p, "\r\n");
        }
    }

    if ( keep )
        p = stpcpy(p, "Connection: keep-alive\r\n");
    else
        p = stpcpy(p, "Connection: close\r\n");
#ifndef NO_IDENTITY
    p = stpcpy(p, "User-Agent: Silgy\r\n");
#endif
    p = stpcpy(p, "\r\n");

    /* body */

    if ( 0 != strcmp(method, "GET") && req )
        p = stpcpy(p, json?G_tmp:(char*)req);

    *p = EOS;

    return p - buffer;
}


/* --------------------------------------------------------------------------
   REST call / connect
-------------------------------------------------------------------------- */
static bool rest_connect(const char *host, const char *port, struct timespec *start, int *timeout_remain, bool secure)
{
static struct {
    char host[256];
    char port[16];
    struct addrinfo addr;
    struct sockaddr ai_addr;
} addresses[REST_ADDRESSES_CACHE_SIZE];
static int addresses_cnt=0, addresses_last=0, i;
    bool addr_cached=FALSE;

    DBG("rest_connect");

    struct addrinfo *result=NULL;

#ifndef REST_CALL_DONT_CACHE_ADDRINFO

    DBG("Trying cache...");

    for ( i=0; i<addresses_cnt; ++i )
    {
//        if ( 0==strcmp(addresses[i].host, host) )
        if ( 0==strcmp(addresses[i].host, host) && 0==strcmp(addresses[i].port, port) )
        {
            DBG("Host %s found in cache (%d)", host, i);
            addr_cached = TRUE;
            result = &addresses[i].addr;
            break;
        }
    }

#endif /* REST_CALL_DONT_CACHE_ADDRINFO */

    if ( !addr_cached )
    {
        DBG("getaddrinfo...");   /* TODO: change to asynchronous, i.e. getaddrinfo_a */

        struct addrinfo hints;
        int s;

        memset(&hints, 0, sizeof(struct addrinfo));
        hints.ai_socktype = SOCK_STREAM;

        if ( (s=getaddrinfo(host, port, &hints, &result)) != 0 )
        {
            ERR("getaddrinfo: %s", gai_strerror(s));
            return FALSE;
        }

#ifdef DUMP
        DBG("elapsed after getaddrinfo: %.3lf ms", lib_elapsed(start));
#endif
        *timeout_remain = G_RESTTimeout - lib_elapsed(start);
        if ( *timeout_remain < 1 ) *timeout_remain = 1;

        /* getaddrinfo() returns a list of address structures.
           Try each address until we successfully connect */
    }

    DBG("Trying to connect...");

    struct addrinfo *rp;

    for ( rp=result; rp!=NULL; rp=rp->ai_next )
    {
        M_rest_sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (M_rest_sock == -1) continue;
#ifdef DUMP
        DBG("socket succeeded");
        DBG("elapsed after socket: %.3lf ms", lib_elapsed(start));
#endif
        *timeout_remain = G_RESTTimeout - lib_elapsed(start);
        if ( *timeout_remain < 1 ) *timeout_remain = 1;

        /* Windows timeout option is a s**t -- go for non-blocking I/O */
        lib_setnonblocking(M_rest_sock);

        /* plain socket connection --------------------------------------- */

        if ( (M_rest_connection=connect(M_rest_sock, rp->ai_addr, rp->ai_addrlen)) != -1 )
        {
            break;  /* immediate success */
        }
        else if ( lib_finish_with_timeout(M_rest_sock, CONNECT, NULL, 0, timeout_remain, NULL, 0) == 0 )
        {
            break;  /* success within timeout */
        }

        close_conn(M_rest_sock);     /* no cigar */
    }

    if ( rp == NULL )     /* No address succeeded */
    {
        ERR("Could not connect");
        if ( result ) freeaddrinfo(result);
        close_conn(M_rest_sock);
        return FALSE;
    }

    /* -------------------------------------------------------------------------- */

    if ( !addr_cached )   /* add to cache */
    {
#ifndef REST_CALL_DONT_CACHE_ADDRINFO
        strcpy(addresses[addresses_last].host, host);
        strcpy(addresses[addresses_last].port, port);
        memcpy(&addresses[addresses_last].addr, rp, sizeof(struct addrinfo));
        /* addrinfo contains pointers -- mind the shallow copy! */
        memcpy(&addresses[addresses_last].ai_addr, rp->ai_addr, sizeof(struct sockaddr));
        addresses[addresses_last].addr.ai_addr = &addresses[addresses_last].ai_addr;
        addresses[addresses_last].addr.ai_next = NULL;

        DBG("Host cached (%d)", addresses_last);

        if ( addresses_cnt < REST_ADDRESSES_CACHE_SIZE-1 )   /* first round */
        {
            ++addresses_cnt;
            ++addresses_last;
        }
        else    /* cache full -- reuse it from start */
        {
            if ( addresses_last < REST_ADDRESSES_CACHE_SIZE-1 )
                ++addresses_last;
            else
                addresses_last = 0;
        }

#endif /* REST_CALL_DONT_CACHE_ADDRINFO */

        freeaddrinfo(result);
    }

#ifdef DUMP
    DBG("elapsed after plain connect: %.3lf ms", lib_elapsed(start));
#endif

    DBG("Connected");

    /* -------------------------------------------------------------------------- */

#ifdef HTTPS
    if ( secure )
    {
        DBG("Trying SSL_new...");

        M_rest_ssl = SSL_new(M_ssl_ctx);

        if ( !M_rest_ssl )
        {
            ERR("SSL_new failed");
            close_conn(M_rest_sock);
            return FALSE;
        }

        DBG("Trying SSL_set_fd...");

        int ret = SSL_set_fd(M_rest_ssl, M_rest_sock);

        if ( ret <= 0 )
        {
            ERR("SSL_set_fd failed, ret = %d", ret);
            close_conn(M_rest_sock);
            SSL_free(M_rest_ssl);
            return FALSE;
        }

        DBG("Trying SSL_connect...");

        ret = SSL_connect(M_rest_ssl);
#ifdef DUMP
        DBG("ret = %d", ret);    /* 1 = success */
#endif
        if ( ret == 1 )
        {
            DBG("SSL_connect immediate success");
        }
        else if ( lib_finish_with_timeout(M_rest_sock, CONNECT, NULL, ret, timeout_remain, M_rest_ssl, 0) > 0 )
        {
            DBG("SSL_connect successful");
        }
        else
        {
            ERR("SSL_connect failed");
            close_conn(M_rest_sock);
            SSL_free(M_rest_ssl);
            return FALSE;
        }

#ifdef DUMP
        DBG("elapsed after SSL connect: %.3lf ms", lib_elapsed(start));
#endif

//        cert = SSL_get_peer_certificate(M_rest_ssl);
    }
#endif /* HTTPS */

    return TRUE;
}


/* --------------------------------------------------------------------------
   REST call / disconnect
-------------------------------------------------------------------------- */
static void rest_disconnect()
{
    DBG("rest_disconnect");

    close(M_rest_connection);
    close_conn(M_rest_sock);
#ifdef HTTPS
    if ( M_rest_ssl )
    {
        SSL_free(M_rest_ssl);
//        DBG("Should be NULL: M_rest_ssl = %d", M_rest_ssl);
        M_rest_ssl = NULL;
    }
#endif
}


/* --------------------------------------------------------------------------
   REST call / get response content length
-------------------------------------------------------------------------- */
static int rest_res_content_length(const char *buffer, int len)
{
    const char *p;

    if ( (p=strstr(buffer, "\nContent-Length: ")) == NULL ) return -1;

    if ( len < (p-buffer) + 18 ) return -1;
    
    char result_str[8];
    char i=0;

    p += 17;

    while ( isdigit(*p) && i<7 )
    {
        result_str[i++] = *p++;
    }

    result_str[i] = EOS;

#ifdef DUMP
    DBG("result_str [%s]", result_str);
#endif

    return atoi(result_str);
}


/* --------------------------------------------------------------------------
   REST call / convert chunked to normal content
   Return number of bytes written to res_content
-------------------------------------------------------------------------- */
static int chunked2content(char *res_content, const char *buffer, int src_len, int res_len)
{
    char *res=res_content;
    char chunk_size_str[8];
    int  chunk_size=1;
    const char *p=buffer;
    int  was_read=0, written=0;

    while ( chunk_size > 0 )    /* chunk by chunk */
    {
        /* get the chunk size */

        int i=0;

        while ( *p!='\r' && i<6 && i<src_len )
            chunk_size_str[i++] = *p++;

        chunk_size_str[i] = EOS;
#ifdef DUMP
        DBG("chunk_size_str [%s]", chunk_size_str);
#endif
        sscanf(chunk_size_str, "%x", &chunk_size);
        DBG("chunk_size = %d", chunk_size);

        was_read += i;

        /* --------------------------------------------------------------- */

        if ( chunk_size == 0 )    /* last one */
        {
            DBG("Last chunk");
            break;
        }
        else if ( chunk_size < 0 )
        {
            ERR("chunk_size < 0");
            break;
        }
        else if ( chunk_size > res_len-written )
        {
            ERR("chunk_size > res_len-written");
            break;
        }

        /* --------------------------------------------------------------- */
        /* skip "\r\n" */

        p += 2;
        was_read += 2;

        /* --------------------------------------------------------------- */

        if ( was_read >= src_len || chunk_size > src_len-was_read )
        {
            ERR("Unexpected end of buffer");
            break;
        }

        /* --------------------------------------------------------------- */
        /* copy chunk to destination */

        res = stpncpy(res, p, chunk_size);
        written += chunk_size;

        p += chunk_size;
        was_read += chunk_size;

        /* --------------------------------------------------------------- */

        while ( *p != '\n' && was_read<src_len-1 )
        {
//            DBG("Skip '%c'", *p);
            ++p;
        }

        ++p;    /* skip '\n' */
        ++was_read;
    }

    return written;
}


/* --------------------------------------------------------------------------
   REST call
-------------------------------------------------------------------------- */
bool lib_rest_req(const void *req, void *res, const char *method, const char *url, bool json, bool keep)
{
    char    host[256];
    char    port[8];
    bool    secure=FALSE;
static char prev_host[256];
static char prev_port[8];
static bool prev_secure=FALSE;
    char    uri[MAX_URI_LEN+1];
static bool connected=FALSE;
static time_t connected_time=0;
    char    res_header[REST_RES_HEADER_LEN+1];
static char buffer[JSON_BUFSIZE];
    long    bytes;
    char    *body;
    int     content_read=0, buffer_read=0;
    int     len, i, j;
    int     timeout_remain = G_RESTTimeout;
#ifdef HTTPS
    int     ssl_err;
#endif /* HTTPS */

    DBG("lib_rest_req [%s] [%s]", method, url);

    /* -------------------------------------------------------------------------- */

    if ( !rest_parse_url(url, host, port, uri, &secure) ) return FALSE;

    len = rest_render_req(buffer, method, host, uri, req, json, keep);

#ifdef DUMP
    DBG("------------------------------------------------------------");
    DBG("lib_rest_req buffer [%s]", buffer);
    DBG("------------------------------------------------------------");
#endif /* DUMP */

    struct timespec start;
    clock_gettime(MONOTONIC_CLOCK_NAME, &start);

    /* -------------------------------------------------------------------------- */

    if ( connected
            && (secure!=prev_secure || 0 != strcmp(host, prev_host) || 0 != strcmp(port, prev_port) || G_now-connected_time > 60) )
    {
        /* reconnect anyway */
        DBG("different host, port or old connection, reconnecting");
        rest_disconnect();
        connected = FALSE;
    }

    bool was_connected = connected;

    /* connect if necessary ----------------------------------------------------- */

    if ( !connected && !rest_connect(host, port, &start, &timeout_remain, secure) ) return FALSE;

    /* -------------------------------------------------------------------------- */

    DBG("Sending request...");

    bool after_reconnect=0;

    while ( timeout_remain > 1 )
    {
#ifdef HTTPS
        if ( secure )
            bytes = SSL_write(M_rest_ssl, buffer, len);
        else
#endif /* HTTPS */
            bytes = send(M_rest_sock, buffer, len, 0);    /* try in one go */

        if ( !secure && bytes <= 0 )
        {
            if ( !was_connected || after_reconnect )
            {
                ERR("Send (after fresh connect) failed");
                rest_disconnect();
                connected = FALSE;
                return FALSE;
            }

            DBG("Disconnected? Trying to reconnect...");
            rest_disconnect();
            if ( !rest_connect(host, port, &start, &timeout_remain, secure) ) return FALSE;
            after_reconnect = 1;
        }
        else if ( secure && bytes == -1 )
        {
            bytes = lib_finish_with_timeout(M_rest_sock, WRITE, buffer, len, &timeout_remain, secure?M_rest_ssl:NULL, 0);

            if ( bytes == -1 )
            {
                if ( !was_connected || after_reconnect )
                {
                    ERR("Send (after fresh connect) failed");
                    rest_disconnect();
                    connected = FALSE;
                    return FALSE;
                }

                DBG("Disconnected? Trying to reconnect...");
                rest_disconnect();
                if ( !rest_connect(host, port, &start, &timeout_remain, secure) ) return FALSE;
                after_reconnect = 1;
            }
        }
        else    /* bytes > 0 ==> OK */
        {
            break;
        }
    }

#ifdef DUMP
    DBG("Sent %ld bytes", bytes);
#endif

    if ( bytes < 15 )
    {
        ERR("send failed, errno = %d (%s)", errno, strerror(errno));
        rest_disconnect();
        connected = FALSE;
        return FALSE;
    }

#ifdef DUMP
    DBG("elapsed after request: %.3lf ms", lib_elapsed(&start));
#endif

    /* -------------------------------------------------------------------------- */

    DBG("Reading response...");

#ifdef HTTPS
    if ( secure )
        bytes = SSL_read(M_rest_ssl, res_header, REST_RES_HEADER_LEN);
    else
#endif /* HTTPS */
        bytes = recv(M_rest_sock, res_header, REST_RES_HEADER_LEN, 0);

    if ( bytes == -1 )
    {
        bytes = lib_finish_with_timeout(M_rest_sock, READ, res_header, REST_RES_HEADER_LEN, &timeout_remain, secure?M_rest_ssl:NULL, 0);

        if ( bytes <= 0 )
        {
            ERR("recv failed, errno = %d (%s)", errno, strerror(errno));
            rest_disconnect();
            connected = FALSE;
            return FALSE;
        }
    }

    DBG("Read %ld bytes", bytes);

#ifdef DUMP
    DBG("elapsed after first response read: %.3lf ms", lib_elapsed(&start));
#endif

    /* -------------------------------------------------------------------------- */
    /* parse the response                                                         */
    /* we assume that at least response header arrived at once                    */

    /* HTTP/1.1 200 OK <== 15 chars */

    char status[4];

    if ( bytes > 14 && 0==strncmp(res_header, "HTTP/1.", 7) )
    {
        res_header[bytes] = EOS;
#ifdef DUMP
        DBG("Got %d bytes of response [%s]", bytes, res_header);
#else
        DBG("Got %d bytes of response", bytes);
#endif /* DUMP */
        strncpy(status, res_header+9, 3);
        status[3] = EOS;
        INF("Response status: %s", status);
    }
    else
    {
        ERR("No or incomplete response");
#ifdef DUMP
        if ( bytes >= 0 )
        {
            res_header[bytes] = EOS;
            DBG("Got %d bytes of response [%s]", bytes, res_header);
        }
#endif
        rest_disconnect();
        connected = FALSE;
        return FALSE;
    }

    /* ------------------------------------------------------------------- */
    /* at this point we've got something that seems to be a HTTP header,
       possibly with content */

    /* ------------------------------------------------------------------- */
    /* find the expected Content-Length                                    */

    int content_length = rest_res_content_length(res_header, bytes);

    if ( content_length > JSON_BUFSIZE-1 )
    {
        ERR("Response content is too big");
        rest_disconnect();
        connected = FALSE;
        return FALSE;
    }

    /* ------------------------------------------------------------------- */
    /*
       There are 4 options now:

       1. Normal content with explicit Content-Length (content_length > 0)
       2. No content gracefully (content_length = 0)
       3. Chunked content (content_length = -1 and Transfer-Encoding says 'chunked')
       4. Error -- that is, no Content-Length header and no Transfer-Encoding

    */

#define TRANSFER_MODE_NORMAL     '1'
#define TRANSFER_MODE_NO_CONTENT '2'
#define TRANSFER_MODE_CHUNKED    '3'
#define TRANSFER_MODE_ERROR      '4'

    char res_content[JSON_BUFSIZE];
    char mode;

    if ( content_length > 0 )     /* Content-Length present in response */
    {
        DBG("TRANSFER_MODE_NORMAL");
        mode = TRANSFER_MODE_NORMAL;
    }
    else if ( content_length == 0 )
    {
        DBG("TRANSFER_MODE_NO_CONTENT");
        mode = TRANSFER_MODE_NO_CONTENT;
    }
    else    /* content_length == -1 */
    {
        if ( strstr(res_header, "\nTransfer-Encoding: chunked") != NULL )
        {
            DBG("TRANSFER_MODE_CHUNKED");
            mode = TRANSFER_MODE_CHUNKED;
        }
        else
        {
            WAR("TRANSFER_MODE_ERROR");
            mode = TRANSFER_MODE_ERROR;
        }
    }

    /* ------------------------------------------------------------------- */
    /* some content may have already been read                             */

    body = strstr(res_header, "\r\n\r\n");

    if ( body )
    {
        body += 4;

        int was_read = bytes - (body-res_header);

        if ( was_read > 0 )
        {
            if ( mode == TRANSFER_MODE_NORMAL )   /* not chunked goes directly to res_content */
            {
                content_read = was_read;
                strncpy(res_content, body, content_read);
            }
            else if ( mode == TRANSFER_MODE_CHUNKED )   /* chunked goes to buffer before res_content */
            {
                buffer_read = was_read;
                strncpy(buffer, body, buffer_read);
            }
        }
    }

    /* ------------------------------------------------------------------- */
    /* read content                                                        */

    if ( mode == TRANSFER_MODE_NORMAL )
    {
        while ( content_read < content_length && timeout_remain > 1 )   /* read whatever we can within timeout */
        {
#ifdef DUMP
            DBG("trying again (content-length)");
#endif

#ifdef HTTPS
            if ( secure )
                bytes = SSL_read(M_rest_ssl, res_content+content_read, JSON_BUFSIZE-content_read-1);
            else
#endif /* HTTPS */
                bytes = recv(M_rest_sock, res_content+content_read, JSON_BUFSIZE-content_read-1, 0);

            if ( bytes == -1 )
                bytes = lib_finish_with_timeout(M_rest_sock, READ, res_content+content_read, JSON_BUFSIZE-content_read-1, &timeout_remain, secure?M_rest_ssl:NULL, 0);

            if ( bytes > 0 )
                content_read += bytes;
        }

        if ( bytes < 1 )
        {
            DBG("timeouted?");
            rest_disconnect();
            connected = FALSE;
            return FALSE;
        }        
    }
    else if ( mode == TRANSFER_MODE_CHUNKED )
    {
        /* for single-threaded process, I can't see better option than to read everything
           into a buffer and then parse and copy chunks into final res_content */

        /* there's no guarantee that every read = one chunk, so just read whatever comes in, until it does */

        while ( bytes > 0 && timeout_remain > 1 )   /* read whatever we can within timeout */
        {
#ifdef DUMP
            DBG("trying again (chunked)");
#endif

#ifdef HTTPS
            if ( secure )
                bytes = SSL_read(M_rest_ssl, buffer+buffer_read, JSON_BUFSIZE-buffer_read-1);
            else
#endif /* HTTPS */
                bytes = recv(M_rest_sock, buffer+buffer_read, JSON_BUFSIZE-buffer_read-1, 0);

            if ( bytes == -1 )
                bytes = lib_finish_with_timeout(M_rest_sock, READ, buffer+buffer_read, JSON_BUFSIZE-buffer_read-1, &timeout_remain, secure?M_rest_ssl:NULL, 0);

            if ( bytes > 0 )
                buffer_read += bytes;
        }

        if ( buffer_read < 5 )
        {
            ERR("buffer_read is only %d, this can't be valid chunked content", buffer_read);
            rest_disconnect();
            connected = FALSE;
            return FALSE;
        }

//        buffer[buffer_read] = EOS;

        content_read = chunked2content(res_content, buffer, buffer_read, JSON_BUFSIZE);
    }

    /* ------------------------------------------------------------------- */

    res_content[content_read] = EOS;
#ifdef DUMP
    DBG("Read %d bytes of content [%s]", content_read, res_content);
#else
    DBG("Read %d bytes of content", content_read);
#endif

    /* ------------------------------------------------------------------- */

    if ( !keep || strstr(res_header, "\nConnection: close") != NULL )
    {
        DBG("Closing connection");
        rest_disconnect();
        connected = FALSE;
    }
    else    /* keep the connection open */
    {
#ifdef HTTPS
        prev_secure = secure;
#endif
        strcpy(prev_host, host);
        strcpy(prev_port, port);
        connected = TRUE;
        connected_time = G_now;
    }

#ifdef DUMP
    DBG("elapsed after second response read: %.3lf ms", lib_elapsed(&start));
#endif

    /* -------------------------------------------------------------------------- */
    /* we expect JSON response in body                                            */

    if ( len && res )
    {
        if ( json )
            lib_json_from_string((JSON*)res, res_content, content_read, 0);
        else
            strcpy((char*)res, res_content);
    }

    return TRUE;
}


/* --------------------------------------------------------------------------
   Finish socket operation with timeout
-------------------------------------------------------------------------- */
int lib_finish_with_timeout(int sock, char readwrite, char *buffer, int len, int *msec, void *ssl, int level)
{
    int             sockerr;
    struct timeval  timeout;
    fd_set          readfds;
    fd_set          writefds;
    int             socks=0;

#ifdef DUMP
    if ( readwrite==READ )
        DBG("lib_finish_with_timeout READ");
    else if ( readwrite==WRITE )
        DBG("lib_finish_with_timeout WRITE");
    else
        DBG("lib_finish_with_timeout CONNECT");
#endif

    if ( level > 10 )
    {
        ERR("lib_finish_with_timeout -- too many levels");
        return -1;
    }

    /* get the error code ------------------------------------------------ */
    /* note: during SSL operations it will be 0                            */

    if ( !ssl )
    {
#ifdef _WIN32   /* Windows */

        sockerr = WSAGetLastError();

        if ( sockerr != WSAEWOULDBLOCK )
        {
            wchar_t *s = NULL;
            FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, sockerr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&s, 0, NULL);
            ERR("%d (%S)", sockerr, s);
            LocalFree(s);
            return -1;
        }

#else   /* Linux */

        sockerr = errno;

        if ( sockerr != EWOULDBLOCK && sockerr != EINPROGRESS )
        {
            ERR("sockerr = %d (%s)", sockerr, strerror(sockerr));
            return -1;
        }

#endif  /* _WIN32 */
    }

    /* set up timeout for select ----------------------------------------- */

    if ( *msec < 1000 )
    {
        timeout.tv_sec = 0;
        timeout.tv_usec = *msec*1000;
    }
    else    /* 1000 ms or more */
    {
        timeout.tv_sec = *msec/1000;
        timeout.tv_usec = (*msec-((int)(*msec/1000)*1000))*1000;
    }

    /* update remaining timeout ------------------------------------------ */

    struct timespec start;
    clock_gettime(MONOTONIC_CLOCK_NAME, &start);

    /* set up fd-s and wait ---------------------------------------------- */

#ifdef DUMP
    DBG("Waiting on select()...");
#endif

    if ( readwrite == READ )
    {
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);
        socks = select(sock+1, &readfds, NULL, NULL, &timeout);
    }
    else    /* WRITE or CONNECT */
    {
        FD_ZERO(&writefds);
        FD_SET(sock, &writefds);
        socks = select(sock+1, NULL, &writefds, NULL, &timeout);
    }

    *msec -= lib_elapsed(&start);
    if ( *msec < 1 ) *msec = 1;
#ifdef DUMP
    DBG("msec reduced to %d ms", *msec);
#endif

    /* process select result --------------------------------------------- */

    if ( socks < 0 )
    {
        ERR("select failed, errno = %d (%s)", errno, strerror(errno));
        return -1;
    }
    else if ( socks == 0 )
    {
        WAR("lib_finish_with_timeout timeouted (was waiting for %.2f ms)", lib_elapsed(&start));
        return -1;
    }
    else    /* socket is ready for I/O */
    {
#ifdef DUMP
        DBG("lib_finish_with_timeout socks > 0");
#endif
#ifdef HTTPS
        int bytes, ssl_err;
        char ec[128]="";
#endif

        if ( readwrite == READ )
        {
#ifdef HTTPS
            if ( ssl )
            {
                if ( buffer )
                    bytes = SSL_read((SSL*)ssl, buffer, len);
                else    /* connect */
                    bytes = SSL_connect((SSL*)ssl);

                if ( bytes > 0 )
                {
                    return bytes;
                }
                else
                {
                    ssl_err = SSL_get_error((SSL*)ssl, bytes);
#ifdef DUMP
                    if ( ssl_err == SSL_ERROR_SYSCALL )
                        sprintf(ec, ", errno = %d (%s)", sockerr, strerror(sockerr));

                    DBG("bytes = %d, ssl_err = %d%s", bytes, ssl_err, ec);
#endif /* DUMP */
                    if ( ssl_err==SSL_ERROR_WANT_READ )
                        return lib_finish_with_timeout(sock, READ, buffer, len, msec, ssl, level+1);
                    else if ( ssl_err==SSL_ERROR_WANT_WRITE )
                        return lib_finish_with_timeout(sock, WRITE, buffer, len, msec, ssl, level+1);
                    else
                    {
                        DBG("SSL_read error %d", ssl_err);
                        return -1;
                    }
                }
            }
            else
#endif /* HTTPS */
                return recv(sock, buffer, len, 0);
        }
        else if ( readwrite == WRITE )
        {
#ifdef HTTPS
            if ( ssl )
            {
                if ( buffer )
                    bytes = SSL_write((SSL*)ssl, buffer, len);
                else    /* connect */
                    bytes = SSL_connect((SSL*)ssl);

                if ( bytes > 0 )
                {
                    return bytes;
                }
                else
                {
                    ssl_err = SSL_get_error((SSL*)ssl, bytes);
#ifdef DUMP
                    if ( ssl_err == SSL_ERROR_SYSCALL )
                        sprintf(ec, ", errno = %d (%s)", sockerr, strerror(sockerr));

                    DBG("bytes = %d, ssl_err = %d%s", bytes, ssl_err, ec);
#endif /* DUMP */
                    if ( ssl_err==SSL_ERROR_WANT_WRITE )
                        return lib_finish_with_timeout(sock, WRITE, buffer, len, msec, ssl, level+1);
                    else if ( ssl_err==SSL_ERROR_WANT_READ )
                        return lib_finish_with_timeout(sock, READ, buffer, len, msec, ssl, level+1);
                    else
                    {
                        DBG("SSL_write error %d", ssl_err);
                        return -1;
                    }
                }
            }
            else
#endif /* HTTPS */
                return send(sock, buffer, len, 0);
        }
        else   /* CONNECT */
        {
#ifdef HTTPS
            if ( ssl )
            {
                ssl_err = SSL_get_error((SSL*)ssl, len);
#ifdef DUMP
                if ( ssl_err == SSL_ERROR_SYSCALL )
                    sprintf(ec, ", errno = %d (%s)", sockerr, strerror(sockerr));

                DBG("error = %d, ssl_err = %d%s", len, ssl_err, ec);
#endif /* DUMP */
                if ( ssl_err==SSL_ERROR_WANT_WRITE )
                    return lib_finish_with_timeout(sock, WRITE, NULL, 0, msec, ssl, level+1);
                else if ( ssl_err==SSL_ERROR_WANT_READ )
                    return lib_finish_with_timeout(sock, READ, NULL, 0, msec, ssl, level+1);
                else
                {
                    DBG("SSL_connect error %d", ssl_err);
                    return -1;
                }
            }
            else
#endif /* HTTPS */
            return 0;
        }
    }
}


/* --------------------------------------------------------------------------
   Set G_appdir
-------------------------------------------------------------------------- */
void lib_get_app_dir()
{
    char *appdir=NULL;

    if ( NULL != (appdir=getenv("SILGYDIR")) )
    {
        strcpy(G_appdir, appdir);
        int len = strlen(G_appdir);
        if ( G_appdir[len-1] == '/' ) G_appdir[len-1] = EOS;
    }
    else
    {
        G_appdir[0] = EOS;   /* not defined */
    }
}


/* --------------------------------------------------------------------------
   Calculate elapsed time
-------------------------------------------------------------------------- */
double lib_elapsed(struct timespec *start)
{
struct timespec end;
    double      elapsed;
    clock_gettime(MONOTONIC_CLOCK_NAME, &end);
    elapsed = (end.tv_sec - start->tv_sec) * 1000.0;
    elapsed += (end.tv_nsec - start->tv_nsec) / 1000000.0;
    return elapsed;
}


/* --------------------------------------------------------------------------
   Log the memory footprint
-------------------------------------------------------------------------- */
void lib_log_memory()
{
    long        mem_used;
    char        mem_used_kb[32];
    char        mem_used_mb[32];
    char        mem_used_gb[32];

    mem_used = lib_get_memory();

    amt(mem_used_kb, mem_used);
    amtd(mem_used_mb, (double)mem_used/1024);
    amtd(mem_used_gb, (double)mem_used/1024/1024);

    ALWAYS("Memory: %s kB (%s MB / %s GB)", mem_used_kb, mem_used_mb, mem_used_gb);
}


/* --------------------------------------------------------------------------
   For lib_memory
-------------------------------------------------------------------------- */
static int mem_parse_line(const char* line)
{
    long    ret=0;
    int     i=0;
    char    strret[64];
    const char* p=line;

    while (!isdigit(*p)) ++p;       /* skip non-digits */

    while (isdigit(*p)) strret[i++] = *p++;

    strret[i] = EOS;

/*  DBG("mem_parse_line: line [%s]", line);
    DBG("mem_parse_line: strret [%s]", strret);*/

    ret = atol(strret);

    return ret;
}


/* --------------------------------------------------------------------------
   Return currently used memory (high water mark) by current process in kB
-------------------------------------------------------------------------- */
long lib_get_memory()
{
    long result=0;

#ifdef __linux__

    char line[128];
    FILE* file = fopen("/proc/self/status", "r");

    if ( !file )
    {
        ERR("fopen(\"/proc/self/status\" failed, errno = %d (%s)", errno, strerror(errno));
        return result;
    }

    while ( fgets(line, 128, file) != NULL )
    {
        if ( strncmp(line, "VmHWM:", 6) == 0 )
        {
            result = mem_parse_line(line);
            break;
        }
    }

    fclose(file);

#else   /* not Linux */

#ifdef _WIN32   /* Windows */

    PROCESS_MEMORY_COUNTERS_EX pmc;
    GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc));
    result = pmc.PrivateUsage / 1024;

#else   /* UNIX */

struct rusage usage;

    getrusage(RUSAGE_SELF, &usage);
    result = usage.ru_maxrss;

#endif  /* _WIN32 */

#endif  /* __linux__ */

    return result;
}


/* --------------------------------------------------------------------------
   Filter everything but basic letters and digits off
---------------------------------------------------------------------------*/
char *lib_filter_strict(const char *src)
{
static char dst[1024];
    int     i=0, j=0;

    while ( src[i] && j<1023 )
    {
        if ( (src[i] >= 65 && src[i] <= 90)
                || (src[i] >= 97 && src[i] <= 122)
                || isdigit(src[i]) )
            dst[j++] = src[i];
        else if ( src[i] == ' ' )
            dst[j++] = '_';

        ++i;
    }

    dst[j] = EOS;

    return dst;
}


/* --------------------------------------------------------------------------
   Add spaces to make string to have len length
-------------------------------------------------------------------------- */
char *lib_add_spaces(const char *src, int len)
{
static char ret[1024];
    int     src_len;
    int     spaces;
    int     i;

    src_len = strlen(src);

    spaces = len - src_len;

    if ( spaces < 0 ) spaces = 0;

    strcpy(ret, src);

    for ( i=src_len; i<len; ++i )
        ret[i] = ' ';

    ret[i] = EOS;

    return ret;
}


/* --------------------------------------------------------------------------
   Add leading spaces to make string to have len length
-------------------------------------------------------------------------- */
char *lib_add_lspaces(const char *src, int len)
{
static char ret[1024];
    int     src_len;
    int     spaces;
    int     i;

    src_len = strlen(src);

    spaces = len - src_len;

    if ( spaces < 0 ) spaces = 0;

    for ( i=0; i<spaces; ++i )
        ret[i] = ' ';

    strcpy(ret+spaces, src);

    return ret;
}


/* --------------------------------------------------------------------------
  determine resource type by its extension
-------------------------------------------------------------------------- */
char get_res_type(const char *fname)
{
    char    *ext=NULL;
    char    uext[8]="";

//  DBG("name: [%s]", fname);

    if ( (ext=(char*)strrchr(fname, '.')) == NULL )     /* no dot */
        return RES_TEXT;

    if ( ext-fname == strlen(fname)-1 )             /* dot is the last char */
        return RES_TEXT;

    ++ext;

    if ( strlen(ext) > 4 )                          /* extension too long */
        return RES_TEXT;

//  DBG("ext: [%s]", ext);

    strcpy(uext, upper(ext));

    if ( 0==strcmp(uext, "HTML") || 0==strcmp(uext, "HTM") )
        return RES_HTML;
    else if ( 0==strcmp(uext, "CSS") )
        return RES_CSS;
    else if ( 0==strcmp(uext, "JS") )
        return RES_JS;
    else if ( 0==strcmp(uext, "PDF") )
        return RES_PDF;
    else if ( 0==strcmp(uext, "GIF") )
        return RES_GIF;
    else if ( 0==strcmp(uext, "JPG") )
        return RES_JPG;
    else if ( 0==strcmp(uext, "ICO") )
        return RES_ICO;
    else if ( 0==strcmp(uext, "PNG") )
        return RES_PNG;
    else if ( 0==strcmp(uext, "BMP") )
        return RES_BMP;
    else if ( 0==strcmp(uext, "MP3") )
        return RES_AMPEG;
    else if ( 0==strcmp(uext, "EXE") )
        return RES_EXE;
    else if ( 0==strcmp(uext, "ZIP") )
        return RES_ZIP;

}


/* --------------------------------------------------------------------------
  convert URI (YYYY-MM-DD) date to tm struct
-------------------------------------------------------------------------- */
void date_str2rec(const char *str, date_t *rec)
{
    int     len;
    int     i;
    int     j=0;
    char    part='Y';
    char    strtmp[8];

    len = strlen(str);

    /* empty or invalid date => return today */

    if ( len != 10 || str[4] != '-' || str[7] != '-' )
    {
        DBG("date_str2rec: empty or invalid date in URI, returning today");
        rec->year = G_ptm->tm_year+1900;
        rec->month = G_ptm->tm_mon+1;
        rec->day = G_ptm->tm_mday;
        return;
    }

    for (i=0; i<len; ++i)
    {
        if ( str[i] != '-' )
            strtmp[j++] = str[i];
        else    /* end of part */
        {
            strtmp[j] = EOS;
            if ( part == 'Y' )  /* year */
            {
                rec->year = atoi(strtmp);
                part = 'M';
            }
            else if ( part == 'M' ) /* month */
            {
                rec->month = atoi(strtmp);
                part = 'D';
            }
            j = 0;
        }
    }

    /* day */

    strtmp[j] = EOS;
    rec->day = atoi(strtmp);
}


/* --------------------------------------------------------------------------
   Convert date_t date to YYYY-MM-DD string
-------------------------------------------------------------------------- */
void date_rec2str(char *str, date_t *rec)
{
    sprintf(str, "%d-%02d-%02d", rec->year, rec->month, rec->day);
}


/* --------------------------------------------------------------------------
   Convert HTTP time to epoch
   Tue, 18 Oct 2016 13:13:03 GMT
   Thu, 24 Nov 2016 21:19:40 GMT
-------------------------------------------------------------------------- */
time_t time_http2epoch(const char *str)
{
    time_t  epoch;
    char    tmp[8];
struct tm   tm;
//  char    *temp;  // temporarily

    // temporarily
//  DBG("time_http2epoch in:  [%s]", str);

    if ( strlen(str) != 29 )
        return 0;

    /* day */

    strncpy(tmp, str+5, 2);
    tmp[2] = EOS;
    tm.tm_mday = atoi(tmp);

    /* month */

    strncpy(tmp, str+8, 3);
    tmp[3] = EOS;
    if ( 0==strcmp(tmp, "Feb") )
        tm.tm_mon = 1;
    else if ( 0==strcmp(tmp, "Mar") )
        tm.tm_mon = 2;
    else if ( 0==strcmp(tmp, "Apr") )
        tm.tm_mon = 3;
    else if ( 0==strcmp(tmp, "May") )
        tm.tm_mon = 4;
    else if ( 0==strcmp(tmp, "Jun") )
        tm.tm_mon = 5;
    else if ( 0==strcmp(tmp, "Jul") )
        tm.tm_mon = 6;
    else if ( 0==strcmp(tmp, "Aug") )
        tm.tm_mon = 7;
    else if ( 0==strcmp(tmp, "Sep") )
        tm.tm_mon = 8;
    else if ( 0==strcmp(tmp, "Oct") )
        tm.tm_mon = 9;
    else if ( 0==strcmp(tmp, "Nov") )
        tm.tm_mon = 10;
    else if ( 0==strcmp(tmp, "Dec") )
        tm.tm_mon = 11;
    else    /* January */
        tm.tm_mon = 0;

    /* year */

    strncpy(tmp, str+12, 4);
    tmp[4] = EOS;
    tm.tm_year = atoi(tmp) - 1900;

    /* hour */

    strncpy(tmp, str+17, 2);
    tmp[2] = EOS;
    tm.tm_hour = atoi(tmp);

    /* minute */

    strncpy(tmp, str+20, 2);
    tmp[2] = EOS;
    tm.tm_min = atoi(tmp);

    /* second */

    strncpy(tmp, str+23, 2);
    tmp[2] = EOS;
    tm.tm_sec = atoi(tmp);

//  DBG("%d-%02d-%02d %02d:%02d:%02d", tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

#ifdef __linux__
    epoch = timegm(&tm);
#else
    epoch = mktime(&tm);
#endif

    // temporarily
//  char *temp = time_epoch2http(epoch);
//  DBG("time_http2epoch out: [%s]", temp);

    return epoch;
}


/* --------------------------------------------------------------------------
   Convert db time to epoch
   2016-12-25 12:15:00
-------------------------------------------------------------------------- */
time_t time_db2epoch(const char *str)
{
    time_t  epoch;
    char    tmp[8];
struct tm   tm;

    if ( strlen(str) != 19 )
        return 0;

    /* year */

    strncpy(tmp, str, 4);
    tmp[4] = EOS;
    tm.tm_year = atoi(tmp) - 1900;

    /* month */

    strncpy(tmp, str+5, 2);
    tmp[2] = EOS;
    tm.tm_mon = atoi(tmp) - 1;

    /* day */

    strncpy(tmp, str+8, 2);
    tmp[2] = EOS;
    tm.tm_mday = atoi(tmp);

    /* hour */

    strncpy(tmp, str+11, 2);
    tmp[2] = EOS;
    tm.tm_hour = atoi(tmp);

    /* minute */

    strncpy(tmp, str+14, 2);
    tmp[2] = EOS;
    tm.tm_min = atoi(tmp);

    /* second */

    strncpy(tmp, str+17, 2);
    tmp[2] = EOS;
    tm.tm_sec = atoi(tmp);

#ifdef __linux__
    epoch = timegm(&tm);
#else
    epoch = mktime(&tm);
#endif

    return epoch;
}


/* --------------------------------------------------------------------------
   Convert epoch to HTTP time
-------------------------------------------------------------------------- */
char *time_epoch2http(time_t epoch)
{
static char str[32];
struct tm   *ptm;

    ptm = gmtime(&epoch);
#ifdef _WIN32   /* Windows */
    strftime(str, 32, "%a, %d %b %Y %H:%M:%S GMT", ptm);
#else
    strftime(str, 32, "%a, %d %b %Y %T GMT", ptm);
#endif  /* _WIN32 */
//  DBG("time_epoch2http: [%s]", str);
    return str;
}


/* --------------------------------------------------------------------------
   Set decimal & thousand separator
---------------------------------------------------------------------------*/
void lib_set_datetime_formats(const char *lang)
{
    char ulang[8];

    DBG("lib_set_datetime_formats, lang [%s]", lang);

    strcpy(ulang, upper(lang));

    // date format

    if ( 0==strcmp(ulang, "EN-US") )
        M_df = 1;
    else if ( 0==strcmp(ulang, "EN-GB") || 0==strcmp(ulang, "EN-AU") || 0==strcmp(ulang, "FR-FR") || 0==strcmp(ulang, "EN-IE") || 0==strcmp(ulang, "ES-ES") || 0==strcmp(ulang, "IT-IT") || 0==strcmp(ulang, "PT-PT") || 0==strcmp(ulang, "PT-BR") || 0==strcmp(ulang, "ES-AR") )
        M_df = 2;
    else if ( 0==strcmp(ulang, "PL-PL") || 0==strcmp(ulang, "RU-RU") || 0==strcmp(ulang, "DE-CH") || 0==strcmp(ulang, "FR-CH") )
        M_df = 3;
    else
        M_df = 0;

    // amount format

    if ( 0==strcmp(ulang, "EN-US") || 0==strcmp(ulang, "EN-GB") || 0==strcmp(ulang, "EN-AU") || 0==strcmp(ulang, "TH-TH") )
    {
        M_tsep = ',';
        M_dsep = '.';
    }
    else if ( 0==strcmp(ulang, "PL-PL") || 0==strcmp(ulang, "IT-IT") || 0==strcmp(ulang, "NB-NO") || 0==strcmp(ulang, "ES-ES") )
    {
        M_tsep = '.';
        M_dsep = ',';
    }
    else
    {
        M_tsep = ' ';
        M_dsep = ',';
    }
}


/* --------------------------------------------------------------------------
   Format amount
---------------------------------------------------------------------------*/
void amt(char *stramt, long in_amt)
{
    char    in_stramt[64];
    int     len;
    int     i, j=0;
    bool    minus=FALSE;

    sprintf(in_stramt, "%ld", in_amt);

    if ( in_stramt[0] == '-' )  /* change to proper UTF-8 minus sign */
    {
        strcpy(stramt, "− ");
        j = 4;
        minus = TRUE;
    }

    len = strlen(in_stramt);

/*  DBG("----- len = %d", len); */

    for ( i=(j?1:0); i<len; ++i, ++j )
    {
        if ( ((!minus && i) || (minus && i>1)) && !((len-i)%3) )
            stramt[j++] = M_tsep;
        stramt[j] = in_stramt[i];
    }

    stramt[j] = EOS;
}


/* --------------------------------------------------------------------------
   Format double amount
---------------------------------------------------------------------------*/
void amtd(char *stramt, double in_amt)
{
    char    in_stramt[64];
    int     len;
    int     i, j=0;
    bool    minus=FALSE;

    sprintf(in_stramt, "%0.2lf", in_amt);

    if ( in_stramt[0] == '-' )  /* change to proper UTF-8 minus sign */
    {
        strcpy(stramt, "− ");
        j = 4;
        minus = TRUE;
    }

    len = strlen(in_stramt);

    for ( i=(j?1:0); i<len; ++i, ++j )
    {
        if ( in_stramt[i]=='.' && M_dsep!='.' )
        {
            stramt[j] = M_dsep;
            continue;
        }
        else if ( ((!minus && i) || (minus && i>1)) && !((len-i)%3) && len-i > 3 && in_stramt[i] != ' ' && in_stramt[i-1] != ' ' && in_stramt[i-1] != '-' )
        {
            stramt[j++] = M_tsep;   /* extra character */
        }
        stramt[j] = in_stramt[i];
    }

    stramt[j] = EOS;
}


/* --------------------------------------------------------------------------
   Format amount -- special version
---------------------------------------------------------------------------*/
void lib_amt(char *stramt, long in_amt)
{
    char    in_stramt[64];
    int     len;
    int     i, j=0;
    bool    minus=FALSE;

    sprintf(in_stramt, "%ld", in_amt);

    if ( in_stramt[0] == '-' )
    {
        strcpy(stramt, "- ");
        j = 2;
        minus = TRUE;
    }

    len = strlen(in_stramt);

    for ( i=(j?1:0); i<len; ++i, ++j )
    {
        if ( ((!minus && i) || (minus && i>1)) && !((len-i)%3) )
            stramt[j++] = M_tsep;
        stramt[j] = in_stramt[i];
    }

    stramt[j] = EOS;
}


/* --------------------------------------------------------------------------
   Format double amount -- special version
---------------------------------------------------------------------------*/
void lib_amtd(char *stramt, double in_amt)
{
    char    in_stramt[64];
    int     len;
    int     i, j=0;
    bool    minus=FALSE;

    sprintf(in_stramt, "%0.2lf", in_amt);

    if ( in_stramt[0] == '-' )
    {
        strcpy(stramt, "- ");
        j = 2;
        minus = TRUE;
    }

    len = strlen(in_stramt);

    for ( i=(j?1:0); i<len; ++i, ++j )
    {
        if ( in_stramt[i]=='.' && M_dsep!='.' )
        {
            stramt[j] = M_dsep;
            continue;
        }
        else if ( ((!minus && i) || (minus && i>1)) && !((len-i)%3) && len-i > 3 && in_stramt[i] != ' ' && in_stramt[i-1] != ' ' && in_stramt[i-1] != '-' )
        {
            stramt[j++] = M_tsep;   /* extra character */
        }
        stramt[j] = in_stramt[i];
    }

    stramt[j] = EOS;
}


/* --------------------------------------------------------------------------
   Format double amount string to string
---------------------------------------------------------------------------*/
void samts(char *stramt, const char *in_amt)
{
    double  d;

    sscanf(in_amt, "%lf", &d);
    amtd(stramt, d);
}


/* --------------------------------------------------------------------------
   Convert string replacing first comma to dot
---------------------------------------------------------------------------*/
void lib_normalize_float(char *str)
{
    char *comma = strchr(str, ',');
    if ( comma ) *comma = '.';
}


/* --------------------------------------------------------------------------
   Format time (add separators between parts)
---------------------------------------------------------------------------*/
void ftm(char *strtm, long in_tm)
{
    char    in_strtm[16];
    int     i, j=0;
const char  sep=':';

    sprintf(in_strtm, "%06ld", in_tm);

    for ( i=0; i<6; ++i, ++j )
    {
        if ( i == 2 || i == 4 )
            strtm[j++] = sep;
        strtm[j] = in_strtm[i];
    }

    strtm[j] = EOS;
}


/* --------------------------------------------------------------------------
   Format date depending on M_df
---------------------------------------------------------------------------*/
char *fmt_date(short year, short month, short day)
{
static char date[16];

    if ( M_df == 1 )
        sprintf(date, "%02d/%02d/%d", month, day, year);
    else if ( M_df == 2 )
        sprintf(date, "%02d/%02d/%d", day, month, year);
    else if ( M_df == 3 )
        sprintf(date, "%02d.%02d.%d", day, month, year);
    else    /* M_df == 0 */
        sprintf(date, "%d-%02d-%02d", year, month, day);

    return date;
}


/* --------------------------------------------------------------------------
   SQL-escape string
-------------------------------------------------------------------------- */
char *silgy_sql_esc(const char *str)
{
static char dst[MAX_LONG_URI_VAL_LEN+1];
    int     i=0, j=0;

    while ( str[i] )
    {
        if ( j > MAX_LONG_URI_VAL_LEN-3 )
            break;
        else if ( str[i] == '\'' )
        {
            dst[j++] = '\\';
            dst[j++] = '\'';
        }
        else if ( str[i] == '"' )
        {
            dst[j++] = '\\';
            dst[j++] = '"';
        }
        else if ( str[i] == '\\' )
        {
            dst[j++] = '\\';
            dst[j++] = '\\';
        }
        else
            dst[j++] = str[i];
        ++i;
    }

    dst[j] = EOS;

    return dst;
}


/* --------------------------------------------------------------------------
   HTML-escape string
-------------------------------------------------------------------------- */
char *silgy_html_esc(const char *str)
{
static char dst[MAX_LONG_URI_VAL_LEN+1];
    int     i=0, j=0;

    while ( str[i] )
    {
        if ( j > MAX_LONG_URI_VAL_LEN-7 )
            break;
        else if ( str[i] == '\'' )
        {
            dst[j++] = '&';
            dst[j++] = 'a';
            dst[j++] = 'p';
            dst[j++] = 'o';
            dst[j++] = 's';
            dst[j++] = ';';
        }
        else if ( str[i] == '\\' )
        {
            dst[j++] = '\\';
            dst[j++] = '\\';
        }
        else if ( str[i] == '"' )
        {
            dst[j++] = '&';
            dst[j++] = 'q';
            dst[j++] = 'u';
            dst[j++] = 'o';
            dst[j++] = 't';
            dst[j++] = ';';
        }
        else if ( str[i] == '<' )
        {
            dst[j++] = '&';
            dst[j++] = 'l';
            dst[j++] = 't';
            dst[j++] = ';';
        }
        else if ( str[i] == '>' )
        {
            dst[j++] = '&';
            dst[j++] = 'g';
            dst[j++] = 't';
            dst[j++] = ';';
        }
        else if ( str[i] == '&' )
        {
            dst[j++] = '&';
            dst[j++] = 'a';
            dst[j++] = 'm';
            dst[j++] = 'p';
            dst[j++] = ';';
        }
        else if ( str[i] == '\n' )
        {
            dst[j++] = '<';
            dst[j++] = 'b';
            dst[j++] = 'r';
            dst[j++] = '>';
        }
        else if ( str[i] != '\r' )
            dst[j++] = str[i];
        ++i;
    }

    dst[j] = EOS;

    return dst;
}


/* --------------------------------------------------------------------------
   ex unsan_noparse
   HTML un-escape string
-------------------------------------------------------------------------- */
char *silgy_html_unesc(const char *str)
{
static char dst[MAX_LONG_URI_VAL_LEN+1];
    int     i=0, j=0;

    while ( str[i] )
    {
        if ( j > MAX_LONG_URI_VAL_LEN-1 )
            break;
        else if ( i > 4
                    && str[i-5]=='&'
                    && str[i-4]=='a'
                    && str[i-3]=='p'
                    && str[i-2]=='o'
                    && str[i-1]=='s'
                    && str[i]==';' )
        {
            j -= 5;
            dst[j++] = '\'';
        }
        else if ( i > 1
                    && str[i-1]=='\\'
                    && str[i]=='\\' )
        {
            j -= 1;
            dst[j++] = '\\';
        }
        else if ( i > 4
                    && str[i-5]=='&'
                    && str[i-4]=='q'
                    && str[i-3]=='u'
                    && str[i-2]=='o'
                    && str[i-1]=='t'
                    && str[i]==';' )
        {
            j -= 5;
            dst[j++] = '"';
        }
        else if ( i > 2
                    && str[i-3]=='&'
                    && str[i-2]=='l'
                    && str[i-1]=='t'
                    && str[i]==';' )
        {
            j -= 3;
            dst[j++] = '<';
        }
        else if ( i > 2
                    && str[i-3]=='&'
                    && str[i-2]=='g'
                    && str[i-1]=='t'
                    && str[i]==';' )
        {
            j -= 3;
            dst[j++] = '>';
        }
        else if ( i > 3
                    && str[i-4]=='&'
                    && str[i-3]=='a'
                    && str[i-2]=='m'
                    && str[i-1]=='p'
                    && str[i]==';' )
        {
            j -= 4;
            dst[j++] = '&';
        }
        else if ( i > 2
                    && str[i-3]=='<'
                    && str[i-2]=='b'
                    && str[i-1]=='r'
                    && str[i]=='>' )
        {
            j -= 3;
            dst[j++] = '\n';
        }
        else
            dst[j++] = str[i];

        ++i;
    }

    dst[j] = EOS;

    return dst;
}


/* --------------------------------------------------------------------------
   Primitive URI encoding
---------------------------------------------------------------------------*/
char *uri_encode(const char *str)
{
static char uri_encode[1024];
    int     i;

    for ( i=0; str[i] && i<1023; ++i )
    {
        if ( str[i] == ' ' )
            uri_encode[i] = '+';
        else
            uri_encode[i] = str[i];
    }

    uri_encode[i] = EOS;

    return uri_encode;
}


/* --------------------------------------------------------------------------
   Convert string to upper
---------------------------------------------------------------------------*/
char *upper(const char *str)
{
static char upper[1024];
    int     i;

    for ( i=0; str[i] && i<1023; ++i )
    {
        if ( str[i] >= 97 && str[i] <= 122 )
            upper[i] = str[i] - 32;
        else
            upper[i] = str[i];
    }

    upper[i] = EOS;

    return upper;
}


/* --------------------------------------------------------------------------
   Strip trailing spaces from string
-------------------------------------------------------------------------- */
char *stp_right(char *str)
{
    char *p;

    for ( p = str + strlen(str) - 1;
          p >= str && (*p == ' ' || *p == '\t');
          p-- )
          *p = 0;

    return str;
}


/* --------------------------------------------------------------------------
   Return TRUE if digits only
---------------------------------------------------------------------------*/
bool strdigits(const char *src)
{
    int i;

    for ( i=0; src[i]; ++i )
    {
        if ( !isdigit(src[i]) )
            return FALSE;
    }

    return TRUE;
}


/* --------------------------------------------------------------------------
   Copy string without spaces and tabs
---------------------------------------------------------------------------*/
char *nospaces(char *dst, const char *src)
{
    const char  *p=src;
    int     i=0;

    while ( *p )
    {
        if ( *p != ' ' && *p != '\t' )
            dst[i++] = *p;
        ++p;
    }

    dst[i] = EOS;

    return dst;
}


/* --------------------------------------------------------------------------
   Generate random string
-------------------------------------------------------------------------- */
void silgy_random(char *dest, int len)
{
const char  *chars="abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
static unsigned long req=0;
    int     i;

    srand((G_now-1520000000)+G_pid+req);

    ++req;

    for ( i=0; i<len; ++i )
        dest[i] = chars[rand() % 62];

    dest[i] = EOS;
}


/* --------------------------------------------------------------------------
  sleep for n miliseconds
  n must be less than 1 second (< 1000)!
-------------------------------------------------------------------------- */
void msleep(int msec)
{
    struct timeval tv;

    if ( msec < 1000 )
    {
        tv.tv_sec = 0;
        tv.tv_usec = msec*1000;
    }
    else    /* 1000 ms or more */
    {
        tv.tv_sec = msec/1000;
        tv.tv_usec = (msec-((int)(msec/1000)*1000))*1000;
    }

/*    DBG("-----------------------");
    DBG("msec = %d", msec);
    DBG("tv.tv_sec = %d", tv.tv_sec);
    DBG("tv.tv_usec = %d", tv.tv_usec); */

    select(0, NULL, NULL, NULL, &tv);
}


#ifdef AUTO_INIT_EXPERIMENT
/* --------------------------------------------------------------------------
   Implicitly init JSON buffer
-------------------------------------------------------------------------- */
static void json_auto_init(JSON *json)
{
    int     i;
    bool    initialized=0;

    for ( i=0; i<M_jsons_cnt; ++i )
    {
        if ( M_jsons[i] == json )
        {
            initialized = 1;
            break;
        }
    }

    if ( !initialized )     /* recognize it by the address */
    {
        if ( M_jsons_cnt >= JSON_MAX_JSONS )
            M_jsons_cnt = 0;

        M_jsons[M_jsons_cnt] = json;
        ++M_jsons_cnt;
        lib_json_reset(json);
    }
}
#endif /* AUTO_INIT_EXPERIMENT */


/* --------------------------------------------------------------------------
   Reset JSON object
-------------------------------------------------------------------------- */
void lib_json_reset(JSON *json)
{
#ifdef DUMP
    DBG("lib_json_reset");
#endif
    json->cnt = 0;
    json->array = 0;
}


/* --------------------------------------------------------------------------
   Get index from JSON buffer
-------------------------------------------------------------------------- */
static int json_get_i(JSON *json, const char *name)
{
    int     i;

    for ( i=0; i<json->cnt; ++i )
        if ( 0==strcmp(json->rec[i].name, name) )
            return i;

    return -1;
}


/* --------------------------------------------------------------------------
   Convert Silgy JSON format to JSON string
   Reentrant version
-------------------------------------------------------------------------- */
static void json_to_string(char *dst, JSON *json, bool array)
{
    char    *p=dst;
    int     i;

    p = stpcpy(p, array?"[":"{");

    for ( i=0; i<json->cnt; ++i )
    {
        /* key */

        if ( !array )
        {
            p = stpcpy(p, "\"");
            p = stpcpy(p, json->rec[i].name);
            p = stpcpy(p, "\":");
        }

        /* value */

        if ( json->rec[i].type == JSON_STRING )
        {
            p = stpcpy(p, "\"");
            p = stpcpy(p, json->rec[i].value);
            p = stpcpy(p, "\"");
        }
        else if ( json->rec[i].type==JSON_INTEGER || json->rec[i].type==JSON_FLOAT || json->rec[i].type==JSON_BOOL )
        {
            p = stpcpy(p, json->rec[i].value);
        }
        else if ( json->rec[i].type == JSON_RECORD )
        {
            char tmp[32784];
            json_to_string(tmp, (JSON*)atol(json->rec[i].value), FALSE);
            p = stpcpy(p, tmp);
        }
        else if ( json->rec[i].type == JSON_ARRAY )
        {
            char tmp[32784];
            json_to_string(tmp, (JSON*)atol(json->rec[i].value), TRUE);
            p = stpcpy(p, tmp);
        }

        if ( i < json->cnt-1 )
            p = stpcpy(p, ",");
    }

    p = stpcpy(p, array?"]":"}");

    *p = EOS;
}


/* --------------------------------------------------------------------------
   Add indent
-------------------------------------------------------------------------- */
static char *json_indent(int level)
{
#define JSON_PRETTY_INDENT "    "

static char dst[256];
    int     i;

    dst[0] = EOS;

    for ( i=0; i<level; ++i )
        strcat(dst, JSON_PRETTY_INDENT);

    return dst;
}


/* --------------------------------------------------------------------------
   Convert Silgy JSON format to JSON string
   Reentrant version
-------------------------------------------------------------------------- */
static void json_to_string_pretty(char *dst, JSON *json, bool array, int level)
{
    char    *p=dst;
    int     i;

    p = stpcpy(p, array?"[\n":"{\n");

    for ( i=0; i<json->cnt; ++i )
    {
        p = stpcpy(p, json_indent(level));

        /* key */

        if ( !array )
        {
            p = stpcpy(p, "\"");
            p = stpcpy(p, json->rec[i].name);
            p = stpcpy(p, "\": ");
        }

        /* value */

        if ( json->rec[i].type == JSON_STRING )
        {
            p = stpcpy(p, "\"");
            p = stpcpy(p, json->rec[i].value);
            p = stpcpy(p, "\"");
        }
        else if ( json->rec[i].type==JSON_INTEGER || json->rec[i].type==JSON_FLOAT || json->rec[i].type==JSON_BOOL )
        {
            p = stpcpy(p, json->rec[i].value);
        }
        else if ( json->rec[i].type == JSON_RECORD )
        {
            if ( !array || i > 0 )
            {
                p = stpcpy(p, "\n");
                p = stpcpy(p, json_indent(level));
            }
            char tmp[32784];
            json_to_string_pretty(tmp, (JSON*)atol(json->rec[i].value), FALSE, level+1);
            p = stpcpy(p, tmp);
        }
        else if ( json->rec[i].type == JSON_ARRAY )
        {
            if ( !array || i > 0 )
            {
                p = stpcpy(p, "\n");
                p = stpcpy(p, json_indent(level));
            }
            char tmp[32784];
            json_to_string_pretty(tmp, (JSON*)atol(json->rec[i].value), TRUE, level+1);
            p = stpcpy(p, tmp);
        }

        if ( i < json->cnt-1 )
            p = stpcpy(p, ",");

        p = stpcpy(p, "\n");
    }

    p = stpcpy(p, json_indent(level-1));
    p = stpcpy(p, array?"]":"}");

    *p = EOS;
}


/* --------------------------------------------------------------------------
   Convert Silgy JSON format to JSON string
-------------------------------------------------------------------------- */
char *lib_json_to_string(JSON *json)
{
static char dst[JSON_BUFSIZE];

    json_to_string(dst, json, FALSE);

    return dst;
}


/* --------------------------------------------------------------------------
   Convert Silgy JSON format to JSON string
-------------------------------------------------------------------------- */
char *lib_json_to_string_pretty(JSON *json)
{
static char dst[JSON_BUFSIZE];

    json_to_string_pretty(dst, json, FALSE, 1);

    return dst;
}


/* --------------------------------------------------------------------------
   Find matching closing bracket in JSON string
-------------------------------------------------------------------------- */
static char *get_json_closing_bracket(const char *src)
{
    int     i=1, subs=0;
    bool    in_quotes=0;

#ifdef DUMP
//    DBG("get_json_closing_bracket [%s]", src);
#endif /* DUMP */

    while ( src[i] )
    {
        if ( src[i]=='"' )
        {
            if ( in_quotes )
                in_quotes = 0;
            else
                in_quotes = 1;
        }
        else if ( src[i]=='{' && !in_quotes )
        {
            ++subs;
        }
        else if ( src[i]=='}' && !in_quotes )
        {
            if ( subs<1 )
                return (char*)src+i;
            else
                subs--;
        }

        ++i;
    }

    return NULL;
}


/* --------------------------------------------------------------------------
   Find matching closing square bracket in JSON string
-------------------------------------------------------------------------- */
static char *get_json_closing_square_bracket(const char *src)
{
    int     i=1, subs=0;
    bool    in_quotes=0;

#ifdef DUMP
//    DBG("get_json_closing_square_bracket [%s]", src);
#endif /* DUMP */

    while ( src[i] )
    {
        if ( src[i]=='"' )
        {
            if ( in_quotes )
                in_quotes = 0;
            else
                in_quotes = 1;
        }
        else if ( src[i]=='[' && !in_quotes )
        {
            ++subs;
        }
        else if ( src[i]==']' && !in_quotes )
        {
            if ( subs<1 )
                return (char*)src+i;
            else
                subs--;
        }

        ++i;
    }

    return NULL;
}


/* --------------------------------------------------------------------------
   Convert JSON string to Silgy JSON format
-------------------------------------------------------------------------- */
void lib_json_from_string(JSON *json, const char *src, int len, int level)
{
    int     i=0, j=0;
    char    key[32];
    char    value[256];
    int     index;
    char    now_key=0, now_value=0, inside_array=0, type;

    if ( len == 0 ) len = strlen(src);

    if ( level == 0 )
    {
        lib_json_reset(json);

        while ( i<len && src[i] != '{' ) ++i;   /* skip junk if there's any */

        if ( src[i] != '{' )    /* no opening bracket */
        {
            ERR("JSON syntax error -- no opening curly bracket");
            return;
        }

        ++i;    /* skip '{' */
    }
    else if ( src[i]=='{' )     /* record */
    {
        ++i;    /* skip '{' */
    }
    else if ( src[i]=='[' )     /* array */
    {
        inside_array = 1;
        ++i;    /* skip '[' */
        index = -1;
    }

#ifdef DUMP
    char tmp[32784];
    strncpy(tmp, src+i, len-i);
    tmp[len-i] = EOS;
    DBG("lib_json_from_string level %d [%s]", level, tmp);
    if ( inside_array ) DBG("inside_array");
#endif /* DUMP */

    for ( i; i<len; ++i )
    {
        if ( !now_key && !now_value )
        {
            while ( i<len && (src[i]==' ' || src[i]=='\t' || src[i]=='\r' || src[i]=='\n') ) ++i;

            if ( !inside_array && src[i]=='"' )
            {
                now_key = 1;
                j = 0;
                ++i;    /* skip '"' */
            }
        }

        if ( (now_key && src[i]=='"') || (inside_array && !now_value && (index==-1 || src[i]==',')) )      /* end of key */
        {
#ifdef DUMP
            if ( now_key && src[i]=='"' )
                DBG("second if because of now_key");
            else
                DBG("second if because of inside_array");
#endif /* DUMP */
            if ( inside_array )
            {
                if ( src[i]==',' ) ++i;    /* skip ',' */

                ++index;
#ifdef DUMP
                DBG("inside_array, starting new value, index = %d", index);
#endif
            }
            else
            {
                key[j] = EOS;
#ifdef DUMP
                DBG("key [%s]", key);
#endif
                now_key = 0;

                ++i;    /* skip '"' */

                while ( i<len && src[i]!=':' ) ++i;

                if ( src[i] != ':' )
                {
                    ERR("JSON syntax error -- no colon after name");
                    return;
                }

                ++i;    /* skip ':' */
            }

            while ( i<len && (src[i]==' ' || src[i]=='\t' || src[i]=='\r' || src[i]=='\n') ) ++i;

            if ( i==len )
            {
                ERR("JSON syntax error -- expected value");
                return;
            }

            /* value starts here --------------------------------------------------- */

            if ( src[i]=='"' )    /* JSON_STRING */
            {
#ifdef DUMP
                DBG("JSON_STRING");
#endif
                type = JSON_STRING;

                now_value = 1;
                j = 0;
            }
            else if ( src[i]=='{' )     /* JSON_RECORD */
            {
#ifdef DUMP
                DBG("JSON_RECORD");
#endif
                type = JSON_RECORD;

                if ( level < JSON_MAX_LEVELS-1 )
                {
                    if ( M_json_pool_cnt[level] >= JSON_POOL_SIZE ) M_json_pool_cnt[level] = 0;   /* overwrite previous ones */

                    lib_json_reset(&M_json_pool[JSON_POOL_SIZE*level+M_json_pool_cnt[level]]);

                    /* save the pointer first as a parent record */
                    if ( inside_array )
                        lib_json_add_record(json, NULL, &M_json_pool[JSON_POOL_SIZE*level+M_json_pool_cnt[level]], FALSE, index);
                    else
                        lib_json_add_record(json, key, &M_json_pool[JSON_POOL_SIZE*level+M_json_pool_cnt[level]], FALSE, -1);
                    /* fill in the destination (children) */
                    char *closing;
                    if ( (closing=get_json_closing_bracket(src+i)) )
                    {
//                        DBG("closing [%s], len=%d", closing, closing-(src+i));
                        lib_json_from_string(&M_json_pool[JSON_POOL_SIZE*level+M_json_pool_cnt[level]], src+i, closing-(src+i)+1, level+1);
                        ++M_json_pool_cnt[level];
                        i += closing-(src+i);
//                        DBG("after closing record bracket [%s]", src+i);
                    }
                    else    /* syntax error */
                    {
                        ERR("No closing bracket in JSON record");
                        break;
                    }
                }
            }
            else if ( src[i]=='[' )     /* JSON_ARRAY */
            {
#ifdef DUMP
                DBG("JSON_ARRAY");
#endif
                type = JSON_ARRAY;

                if ( level < JSON_MAX_LEVELS-1 )
                {
                    if ( M_json_pool_cnt[level] >= JSON_POOL_SIZE ) M_json_pool_cnt[level] = 0;   /* overwrite previous ones */

                    lib_json_reset(&M_json_pool[JSON_POOL_SIZE*level+M_json_pool_cnt[level]]);

                    /* save the pointer first as a parent record */
                    if ( inside_array )
                        lib_json_add_record(json, NULL, &M_json_pool[JSON_POOL_SIZE*level+M_json_pool_cnt[level]], TRUE, index);
                    else
                        lib_json_add_record(json, key, &M_json_pool[JSON_POOL_SIZE*level+M_json_pool_cnt[level]], TRUE, -1);
                    /* fill in the destination (children) */
                    char *closing;
                    if ( (closing=get_json_closing_square_bracket(src+i)) )
                    {
//                        DBG("closing [%s], len=%d", closing, closing-(src+i));
                        lib_json_from_string(&M_json_pool[JSON_POOL_SIZE*level+M_json_pool_cnt[level]], src+i, closing-(src+i)+1, level+1);
                        ++M_json_pool_cnt[level];
                        i += closing-(src+i);
//                        DBG("after closing array bracket [%s]", src+i);
                    }
                    else    /* syntax error */
                    {
                        ERR("No closing square bracket in JSON array");
                        break;
                    }
                }
            }
            else    /* number */
            {
#ifdef DUMP
                DBG("JSON_INTEGER || JSON_FLOAT || JSON_BOOL");
#endif
                i--;

                now_value = 1;
                j = 0;
            }
        }
        else if ( now_value && ((type==JSON_STRING && src[i]=='"' && src[i-1]!='\\') || src[i]==',' || src[i]=='}' || src[i]==']' || src[i]=='\r' || src[i]=='\n') )     /* end of value */
        {
            value[j] = EOS;
#ifdef DUMP
            DBG("value [%s]", value);
#endif
            if ( type==JSON_STRING ) ++i;   /* skip '"' */

            if ( inside_array )
            {
                if ( type==JSON_STRING )
                    lib_json_add(json, NULL, value, 0, 0, JSON_STRING, index);
                else if ( value[0]=='t' )
                    lib_json_add(json, NULL, NULL, 1, 0, JSON_BOOL, index);
                else if ( value[0]=='f' )
                    lib_json_add(json, NULL, NULL, 0, 0, JSON_BOOL, index);
                else if ( strchr(value, '.') )
                    lib_json_add(json, NULL, NULL, 0, atof(value), JSON_FLOAT, index);
                else
                    lib_json_add(json, NULL, NULL, atol(value), 0, JSON_INTEGER, index);
            }
            else
            {
                if ( type==JSON_STRING )
                    lib_json_add(json, key, value, 0, 0, JSON_STRING, -1);
                else if ( value[0]=='t' )
                    lib_json_add(json, key, NULL, 1, 0, JSON_BOOL, -1);
                else if ( value[0]=='f' )
                    lib_json_add(json, key, NULL, 0, 0, JSON_BOOL, -1);
                else if ( strchr(value, '.') )
                    lib_json_add(json, key, NULL, 0, atof(value), JSON_FLOAT, -1);
                else
                    lib_json_add(json, key, NULL, atol(value), 0, JSON_INTEGER, -1);
            }

            now_value = 0;

            if ( src[i]==',' ) i--;     /* we need it to recognize the next array element */
        }
        else if ( now_key )
        {
            if ( j < 30 )
                key[j++] = src[i];
        }
        else if ( now_value )
        {
            if ( j < 254 )
                value[j++] = src[i];
        }

        if ( src[i-2]=='}' && !now_value && level==0 )
            break;
    }
}


/* --------------------------------------------------------------------------
   Log JSON buffer
-------------------------------------------------------------------------- */
void lib_json_log_dbg(JSON *json, const char *name)
{
    int     i;
    char    type[32];

    DBG("-------------------------------------------------------------------------------");

    if ( name )
        DBG("%s:", name);
    else
        DBG("JSON record:");

    for ( i=0; i<json->cnt; ++i )
    {
        if ( json->rec[i].type == JSON_STRING )
            strcpy(type, "JSON_STRING");
        else if ( json->rec[i].type == JSON_INTEGER )
            strcpy(type, "JSON_INTEGER");
        else if ( json->rec[i].type == JSON_FLOAT )
            strcpy(type, "JSON_FLOAT");
        else if ( json->rec[i].type == JSON_BOOL )
            strcpy(type, "JSON_BOOL");
        else if ( json->rec[i].type == JSON_RECORD )
            strcpy(type, "JSON_RECORD");
        else if ( json->rec[i].type == JSON_ARRAY )
            strcpy(type, "JSON_ARRAY");
        else
        {
            sprintf(type, "Unknown type! (%d)", json->rec[i].type);
            break;
        }

        DBG("%d %s [%s] %s", i, json->array?"":json->rec[i].name, json->rec[i].value, type);
    }

    DBG("-------------------------------------------------------------------------------");
}


/* --------------------------------------------------------------------------
   Log JSON buffer
-------------------------------------------------------------------------- */
void lib_json_log_inf(JSON *json, const char *name)
{
    int     i;
    char    type[32];

    INF("-------------------------------------------------------------------------------");

    if ( name )
        INF("%s:", name);
    else
        INF("JSON record:");

    for ( i=0; i<json->cnt; ++i )
    {
        if ( json->rec[i].type == JSON_STRING )
            strcpy(type, "JSON_STRING");
        else if ( json->rec[i].type == JSON_INTEGER )
            strcpy(type, "JSON_INTEGER");
        else if ( json->rec[i].type == JSON_FLOAT )
            strcpy(type, "JSON_FLOAT");
        else if ( json->rec[i].type == JSON_BOOL )
            strcpy(type, "JSON_BOOL");
        else if ( json->rec[i].type == JSON_RECORD )
            strcpy(type, "JSON_RECORD");
        else if ( json->rec[i].type == JSON_ARRAY )
            strcpy(type, "JSON_ARRAY");
        else
        {
            sprintf(type, "Unknown type! (%d)", json->rec[i].type);
            break;
        }

        INF("%d %s [%s] %s", i, json->array?"":json->rec[i].name, json->rec[i].value, type);
    }

    INF("-------------------------------------------------------------------------------");
}


/* --------------------------------------------------------------------------
   Add/set value to a JSON buffer
-------------------------------------------------------------------------- */
bool lib_json_add(JSON *json, const char *name, const char *str_value, long int_value, double flo_value, char type, int i)
{
#ifdef AUTO_INIT_EXPERIMENT
    json_auto_init(json);
#endif

    if ( name )
    {
        i = json_get_i(json, name);

        if ( i==-1 )    /* not present -- append new */
        {
            if ( json->cnt >= JSON_MAX_ELEMS ) return FALSE;
            i = json->cnt;
            ++json->cnt;
            strncpy(json->rec[i].name, name, 31);
            json->rec[i].name[31] = EOS;
            json->array = FALSE;
        }
    }
    else    /* array */
    {
        if ( i >= JSON_MAX_ELEMS-1 ) return FALSE;
        json->array = TRUE;
        if ( json->cnt < i+1 ) json->cnt = i + 1;
    }

    if ( type == JSON_STRING )
    {
        strncpy(json->rec[i].value, str_value, 255);
        json->rec[i].value[255] = EOS;
    }
    else if ( type == JSON_BOOL )
    {
        if ( int_value )
            strcpy(json->rec[i].value, "true");
        else
            strcpy(json->rec[i].value, "false");
    }
    else if ( type == JSON_INTEGER )
    {
        sprintf(json->rec[i].value, "%ld", int_value);
    }
    else    /* float */
    {
        snprintf(json->rec[i].value, 256, "%f", flo_value);
    }

    json->rec[i].type = type;

    return TRUE;
}


/* --------------------------------------------------------------------------
   Insert value into JSON buffer
-------------------------------------------------------------------------- */
bool lib_json_add_record(JSON *json, const char *name, JSON *json_sub, bool is_array, int i)
{
    DBG("lib_json_add_record (%s)", is_array?"ARRAY":"RECORD");

#ifdef AUTO_INIT_EXPERIMENT
    json_auto_init(json);
#endif

    if ( name )
    {
#ifdef DUMP
        DBG("name [%s]", name);
#endif
        i = json_get_i(json, name);

        if ( i==-1 )    /* not present -- append new */
        {
            if ( json->cnt >= JSON_MAX_ELEMS ) return FALSE;
            i = json->cnt;
            ++json->cnt;
            strncpy(json->rec[i].name, name, 31);
            json->rec[i].name[31] = EOS;
            json->array = FALSE;
        }
    }
    else    /* array */
    {
#ifdef DUMP
        DBG("index = %d", i);
#endif
        if ( i >= JSON_MAX_ELEMS-1 ) return FALSE;
        json->array = TRUE;
        if ( json->cnt < i+1 ) json->cnt = i + 1;
    }

    /* store sub-record address as a text in value */

    sprintf(json->rec[i].value, "%ld", json_sub);

    json->rec[i].type = is_array?JSON_ARRAY:JSON_RECORD;

    return TRUE;
}


/* --------------------------------------------------------------------------
   Get value from JSON buffer
-------------------------------------------------------------------------- */
char *lib_json_get_str(JSON *json, const char *name, int i)
{
static char dst[256];

    if ( !name )    /* array elem */
    {
        if ( i >= json->cnt )
        {
            ERR("lib_json_get_str index (%d) out of bound (max = %d)", i, json->cnt-1);
            dst[0] = EOS;
            return dst;
        }

        if ( json->rec[i].type==JSON_STRING || json->rec[i].type==JSON_INTEGER || json->rec[i].type==JSON_FLOAT || json->rec[i].type==JSON_BOOL )
        {
            strcpy(dst, json->rec[i].value);
            return dst;
        }
        else    /* types don't match */
        {
            dst[0] = EOS;
            return dst;   /* types don't match or couldn't convert */
        }
    }
    
    for ( i=0; i<json->cnt; ++i )
    {
        if ( 0==strcmp(json->rec[i].name, name) )
        {
            if ( json->rec[i].type==JSON_STRING || json->rec[i].type==JSON_INTEGER || json->rec[i].type==JSON_FLOAT || json->rec[i].type==JSON_BOOL )
            {
                strcpy(dst, json->rec[i].value);
                return dst;
            }

            dst[0] = EOS;
            return dst;   /* types don't match or couldn't convert */
        }
    }

    dst[0] = EOS;
    return dst;   /* no such field */
}


/* --------------------------------------------------------------------------
   Get value from JSON buffer
-------------------------------------------------------------------------- */
long lib_json_get_int(JSON *json, const char *name, int i)
{
    if ( !name )    /* array elem */
    {
        if ( i >= json->cnt )
        {
            ERR("lib_json_get_int index (%d) out of bound (max = %d)", i, json->cnt-1);
            return 0;
        }

        if ( json->rec[i].type == JSON_INTEGER )
            return atol(json->rec[i].value);
        else    /* types don't match */
            return 0;
    }

    for ( i=0; i<json->cnt; ++i )
    {
        if ( 0==strcmp(json->rec[i].name, name) )
        {
            if ( json->rec[i].type == JSON_INTEGER )
            {
                return atol(json->rec[i].value);
            }

            return 0;   /* types don't match or couldn't convert */
        }
    }

    return 0;   /* no such field */
}


/* --------------------------------------------------------------------------
   Get value from JSON buffer
-------------------------------------------------------------------------- */
double lib_json_get_float(JSON *json, const char *name, int i)
{
    if ( !name )    /* array elem */
    {
        if ( i >= json->cnt )
        {
            ERR("lib_json_get_float index (%d) out of bound (max = %d)", i, json->cnt-1);
            return 0;
        }

        if ( json->rec[i].type == JSON_FLOAT )
            return atof(json->rec[i].value);
        else    /* types don't match */
            return 0;
    }

    for ( i=0; i<json->cnt; ++i )
    {
        if ( 0==strcmp(json->rec[i].name, name) )
        {
            if ( json->rec[i].type == JSON_FLOAT )
            {
                return atof(json->rec[i].value);
            }

            return 0;   /* types don't match or couldn't convert */
        }
    }

    return 0;   /* no such field */
}


/* --------------------------------------------------------------------------
   Get value from JSON buffer
-------------------------------------------------------------------------- */
bool lib_json_get_bool(JSON *json, const char *name, int i)
{
    if ( !name )    /* array elem */
    {
        if ( i >= json->cnt )
        {
            ERR("lib_json_get_bool index (%d) out of bound (max = %d)", i, json->cnt-1);
            return FALSE;
        }

        if ( json->rec[i].type == JSON_BOOL )
        {
            if ( json->rec[i].value[0] == 't' )
                return TRUE;
            else
                return FALSE;
        }
        else if ( json->rec[i].type == JSON_STRING )
        {
            if ( 0==strcmp(json->rec[i].value, "true") )
                return TRUE;
            else
                return FALSE;
        }
        else    /* types don't match */
        {
            return FALSE;
        }
    }

    for ( i=0; i<json->cnt; ++i )
    {
        if ( 0==strcmp(json->rec[i].name, name) )
        {
            if ( json->rec[i].type == JSON_BOOL )
            {
                if ( json->rec[i].value[0] == 't' )
                    return TRUE;
                else
                    return FALSE;
            }
            else if ( json->rec[i].type == JSON_STRING )
            {
                if ( 0==strcmp(json->rec[i].value, "true") )
                    return TRUE;
                else
                    return FALSE;
            }

            return FALSE;   /* types don't match or couldn't convert */
        }
    }

    return FALSE;   /* no such field */
}


/* --------------------------------------------------------------------------
   Get (copy) value from JSON buffer
   How to change it to returning pointer without confusing beginners?
   It would be better performing without copying all the fields
-------------------------------------------------------------------------- */
bool lib_json_get_record(JSON *json, const char *name, JSON *json_sub, int i)
{
    DBG("lib_json_get_record by %s", name?"name":"index");

    if ( !name )    /* array elem */
    {
        if ( i >= json->cnt )
        {
            ERR("lib_json_get_record index (%d) out of bound (max = %d)", i, json->cnt-1);
            return FALSE;
        }
#ifdef DUMP
        DBG("index = %d", i);
#endif
        if ( json->rec[i].type == JSON_RECORD || json->rec[i].type == JSON_ARRAY )
        {
            memcpy(json_sub, (JSON*)atol(json->rec[i].value), sizeof(JSON));
            return TRUE;
        }
        else
        {
            return FALSE;   /* types don't match or couldn't convert */
        }
    }

#ifdef DUMP
    DBG("name [%s]", name);
#endif

    for ( i=0; i<json->cnt; ++i )
    {
        if ( 0==strcmp(json->rec[i].name, name) )
        {
//            DBG("lib_json_get_record, found [%s]", name);
            if ( json->rec[i].type == JSON_RECORD || json->rec[i].type == JSON_ARRAY )
            {
                memcpy(json_sub, (JSON*)atol(json->rec[i].value), sizeof(JSON));
                return TRUE;
            }

//            DBG("lib_json_get_record, types of [%s] don't match", name);
            return FALSE;   /* types don't match or couldn't convert */
        }
    }

//    DBG("lib_json_get_record, [%s] not found", name);
    return FALSE;   /* no such field */
}


/* --------------------------------------------------------------------------
   Check system's endianness
-------------------------------------------------------------------------- */
void get_byteorder()
{
    if ( sizeof(long) == 4 )
        get_byteorder32();
    else
        get_byteorder64();
}


/* --------------------------------------------------------------------------
   Check system's endianness
-------------------------------------------------------------------------- */
static void get_byteorder32()
{
    union {
        long l;
        char c[4];
    } test;

    DBG("Checking 32-bit endianness...");

    memset(&test, 0, sizeof(test));

    test.l = 1;

    if ( test.c[3] && !test.c[2] && !test.c[1] && !test.c[0] )
    {
        INF("This is 32-bit Big Endian");
        return;
    }

    if ( !test.c[3] && !test.c[2] && !test.c[1] && test.c[0] )
    {
        INF("This is 32-bit Little Endian");
        return;
    }

    DBG("Unknown Endianness!");
}


/* --------------------------------------------------------------------------
   Check system's endianness
-------------------------------------------------------------------------- */
static void get_byteorder64()
{
    union {
        long l;
        char c[8];
    } test;

    DBG("Checking 64-bit endianness...");

    memset(&test, 0, sizeof(test));

    test.l = 1;

    if ( test.c[7] && !test.c[3] && !test.c[2] && !test.c[1] && !test.c[0] )
    {
        INF("This is 64-bit Big Endian");
        return;
    }

    if ( !test.c[7] && !test.c[3] && !test.c[2] && !test.c[1] && test.c[0] )
    {
        INF("This is 64-bit Little Endian");
        return;
    }

    DBG("Unknown Endianness!");
}


/* --------------------------------------------------------------------------
   Convert database datetime to epoch time
-------------------------------------------------------------------------- */
time_t db2epoch(const char *str)
{

    int     i;
    int     j=0;
    char    part='Y';
    char    strtmp[8];
struct tm   t={0};

/*  DBG("db2epoch: str: [%s]", str); */

    for ( i=0; str[i]; ++i )
    {
        if ( isdigit(str[i]) )
            strtmp[j++] = str[i];
        else    /* end of part */
        {
            strtmp[j] = EOS;
            if ( part == 'Y' )  /* year */
            {
                t.tm_year = atoi(strtmp) - 1900;
                part = 'M';
            }
            else if ( part == 'M' ) /* month */
            {
                t.tm_mon = atoi(strtmp) - 1;
                part = 'D';
            }
            else if ( part == 'D' ) /* day */
            {
                t.tm_mday = atoi(strtmp);
                part = 'H';
            }
            else if ( part == 'H' ) /* hour */
            {
                t.tm_hour = atoi(strtmp);
                part = 'm';
            }
            else if ( part == 'm' ) /* minutes */
            {
                t.tm_min = atoi(strtmp);
                part = 's';
            }
            j = 0;
        }
    }

    /* seconds */

    strtmp[j] = EOS;
    t.tm_sec = atoi(strtmp);

    return mktime(&t);
}


/* --------------------------------------------------------------------------
   Send an email
-------------------------------------------------------------------------- */
bool sendemail(int ci, const char *to, const char *subject, const char *message)
{
#ifndef _WIN32
    char    sender[256];
    char    *colon;
    char    comm[256];

#ifndef ASYNC_SERVICE   /* web server mode */

    sprintf(sender, "%s <noreply@%s>", conn[ci].website, conn[ci].host);

    /* happens when using non-standard port */

    if ( G_test && (colon=strchr(sender, ':')) )
    {
        *colon = '>';
        *(++colon) = EOS;
        DBG("sender truncated to [%s]", sender);
    }
#else
    sprintf(sender, "%s <noreply@%s>", APP_WEBSITE, APP_DOMAIN);
#endif

    sprintf(comm, "/usr/lib/sendmail -t -f \"%s\"", sender);

    FILE *mailpipe = popen(comm, "w");

    if ( mailpipe == NULL )
    {
        ERR("Failed to invoke sendmail");
        return FALSE;
    }
    else
    {
        DBG("Sending email to: [%s], subject: [%s]", to, subject);
        fprintf(mailpipe, "To: %s\n", to);
        fprintf(mailpipe, "From: %s\n", sender);
        fprintf(mailpipe, "Subject: %s\n\n", subject);
        fwrite(message, 1, strlen(message), mailpipe);
        fwrite("\n.\n", 1, 3, mailpipe);
        pclose(mailpipe);
    }

#endif  /* _WIN32 */

    return TRUE;
}


/* --------------------------------------------------------------------------
   Minify CSS/JS -- new version
   remove all white spaces and new lines unless in quotes
   also remove // style comments
   add a space after some keywords
   return new length
-------------------------------------------------------------------------- */
int silgy_minify(char *dest, const char *src)
{
static char temp[4194304];

    minify_1(temp, src);
    return minify_2(dest, temp);
}


/* --------------------------------------------------------------------------
   First pass -- only remove comments
-------------------------------------------------------------------------- */
static void minify_1(char *dest, const char *src)
{
    int     len;
    int     i;
    int     j=0;
    bool    opensq=FALSE;       /* single quote */
    bool    opendq=FALSE;       /* double quote */
    bool    openco=FALSE;       /* comment */
    bool    opensc=FALSE;       /* star comment */

    len = strlen(src);

    for ( i=0; i<len; ++i )
    {
        if ( !openco && !opensc && !opensq && src[i]=='"' && (i==0 || (i>0 && src[i-1]!='\\')) )
        {
            if ( !opendq )
                opendq = TRUE;
            else
                opendq = FALSE;
        }
        else if ( !openco && !opensc && !opendq && src[i]=='\'' )
        {
            if ( !opensq )
                opensq = TRUE;
            else
                opensq = FALSE;
        }
        else if ( !opensq && !opendq && !openco && !opensc && src[i]=='/' && src[i+1] == '/' )
        {
            openco = TRUE;
        }
        else if ( !opensq && !opendq && !openco && !opensc && src[i]=='/' && src[i+1] == '*' )
        {
            opensc = TRUE;
        }
        else if ( openco && src[i]=='\n' )
        {
            openco = FALSE;
        }
        else if ( opensc && src[i]=='*' && src[i+1]=='/' )
        {
            opensc = FALSE;
            i += 2;
        }

        if ( !openco && !opensc )       /* unless it's a comment ... */
            dest[j++] = src[i];
    }

    dest[j] = EOS;
}


/* --------------------------------------------------------------------------
   return new length
-------------------------------------------------------------------------- */
static int minify_2(char *dest, const char *src)
{
    int     len;
    int     i;
    int     j=0;
    bool    opensq=FALSE;       /* single quote */
    bool    opendq=FALSE;       /* double quote */
    bool    openbr=FALSE;       /* curly braces */
    bool    openwo=FALSE;       /* word */
    bool    opencc=FALSE;       /* colon */
    bool    skip_ws=FALSE;      /* skip white spaces */
    char    word[256]="";
    int     wi=0;               /* word index */

    len = strlen(src);

    for ( i=0; i<len; ++i )
    {
        if ( !opensq && src[i]=='"' && (i==0 || (i>0 && src[i-1]!='\\')) )
        {
            if ( !opendq )
                opendq = TRUE;
            else
                opendq = FALSE;
        }
        else if ( !opendq && src[i]=='\'' )
        {
            if ( !opensq )
                opensq = TRUE;
            else
                opensq = FALSE;
        }
        else if ( !opensq && !opendq && !openbr && src[i]=='{' )
        {
            openbr = TRUE;
            openwo = FALSE;
            wi = 0;
            skip_ws = TRUE;
        }
        else if ( !opensq && !opendq && openbr && src[i]=='}' )
        {
            openbr = FALSE;
            openwo = FALSE;
            wi = 0;
            skip_ws = TRUE;
        }
        else if ( !opensq && !opendq && openbr && !opencc && src[i]==':' )
        {
            opencc = TRUE;
            openwo = FALSE;
            wi = 0;
            skip_ws = TRUE;
        }
        else if ( !opensq && !opendq && opencc && src[i]==';' )
        {
            opencc = FALSE;
            openwo = FALSE;
            wi = 0;
            skip_ws = TRUE;
        }
        else if ( !opensq && !opendq && !opencc && !openwo && (isalpha(src[i]) || src[i]=='|' || src[i]=='&') ) /* word is starting */
        {
            openwo = TRUE;
        }
        else if ( !opensq && !opendq && openwo && !isalnum(src[i]) && src[i]!='_' && src[i]!='|' && src[i]!='&' )   /* end of word */
        {
            word[wi] = EOS;
            if ( 0==strcmp(word, "var")
                    || (0==strcmp(word, "function") && src[i]!='(')
                    || (0==strcmp(word, "else") && src[i]!='{')
                    || 0==strcmp(word, "new")
                    || (0==strcmp(word, "return") && src[i]!=';')
                    || 0==strcmp(word, "||")
                    || 0==strcmp(word, "&&") )
                dest[j++] = ' ';
            openwo = FALSE;
            wi = 0;
            skip_ws = TRUE;
        }

        if ( opensq || opendq
                || src[i+1] == '|' || src[i+1] == '&'
                || (src[i] != ' ' && src[i] != '\t' && src[i] != '\n' && src[i] != '\r')
                || opencc )
            dest[j++] = src[i];

        if ( openwo )
            word[wi++] = src[i];

        if ( skip_ws )
        {
            while ( src[i+1] && (src[i+1]==' ' || src[i+1]=='\t' || src[i+1]=='\n' || src[i+1]=='\r') ) ++i;
            skip_ws = FALSE;
        }
    }

    dest[j] = EOS;

    return j;
}


/* --------------------------------------------------------------------------
  increment date by 'days' days. Return day of week as well.
  Format: YYYY-MM-DD
-------------------------------------------------------------------------- */
void date_inc(char *str, int days, int *dow)
{
    char    full[32];
    time_t  told, tnew;

    sprintf(full, "%s 00:00:00", str);

    told = db2epoch(full);

    tnew = told + 3600*24*days;

    G_ptm = gmtime(&tnew);
    sprintf(str, "%d-%02d-%02d", G_ptm->tm_year+1900, G_ptm->tm_mon+1, G_ptm->tm_mday);
    *dow = G_ptm->tm_wday;

    G_ptm = gmtime(&G_now); /* set it back */

}


/* --------------------------------------------------------------------------
  compare the dates
  Format: YYYY-MM-DD
-------------------------------------------------------------------------- */
int date_cmp(const char *str1, const char *str2)
{
    char    full[32];
    time_t  t1, t2;

    sprintf(full, "%s 00:00:00", str1);
    t1 = db2epoch(full);

    sprintf(full, "%s 00:00:00", str2);
    t2 = db2epoch(full);

    return t1 - t2;
}


/* --------------------------------------------------------------------------
   Read the config file
-------------------------------------------------------------------------- */
bool lib_read_conf(const char *file)
{
    FILE    *h_file=NULL;

    /* open the conf file */

    if ( NULL == (h_file=fopen(file, "r")) )
    {
//        printf("Error opening %s, using defaults.\n", file);
        return FALSE;
    }

    /* read content into M_conf for silgy_read_param */

    fseek(h_file, 0, SEEK_END);     /* determine the file size */
    long size = ftell(h_file);
    rewind(h_file);

    if ( (M_conf=(char*)malloc(size+1)) == NULL )
    {
        printf("ERROR: Couldn't get %ld bytes for M_conf\n", size+1);
        fclose(h_file);
        return FALSE;
    }

    fread(M_conf, size, 1, h_file);
    *(M_conf+size) = EOS;

    fclose(h_file);

    return TRUE;
}


#ifdef OLD_CODE
bool lib_read_conf(const char *file)
{
    FILE    *h_file=NULL;
    int     c=0;
    int     i=0;
    char    now_label=1;
    char    now_value=0;
    char    now_comment=0;
    char    label[64]="";
    char    value[256]="";

    /* open the conf file */

    if ( NULL == (h_file=fopen(file, "r")) )
    {
//        printf("Error opening %s, using defaults.\n", file);
        return FALSE;
    }

    /* read content into M_conf for silgy_read_param */

    fseek(h_file, 0, SEEK_END);     /* determine the file size */
    long size = ftell(h_file);
    rewind(h_file);
    if ( (M_conf=(char*)malloc(size+1)) == NULL )
    {
        printf("ERROR: Couldn't get %ld bytes for M_conf\n", size+1);
        fclose(h_file);
        return FALSE;
    }
    fread(M_conf, size, 1, h_file);
    *(M_conf+size) = EOS;
    rewind(h_file);

    /* parse the conf file */

    while ( EOF != (c=fgetc(h_file)) )
    {
        if ( c == '\r' ) continue;

        if ( !now_value && (c == ' ' || c == '\t') ) continue;  /* omit whitespaces */

        if ( c == '\n' )    /* end of value or end of comment or empty line */
        {
            if ( now_value )    /* end of value */
            {
                value[i] = EOS;
#ifndef ASYNC_SERVICE
                eng_set_param(label, value);
//                app_set_param(label, value);
#endif
            }
            now_label = 1;
            now_value = 0;
            now_comment = 0;
            i = 0;
        }
        else if ( now_comment )
        {
            continue;
        }
        else if ( c == '=' )    /* end of label */
        {
            now_label = 0;
            now_value = 1;
            label[i] = EOS;
            i = 0;
        }
        else if ( c == '#' )    /* possible end of value */
        {
            if ( now_value )    /* end of value */
            {
                value[i] = EOS;
#ifndef ASYNC_SERVICE
                eng_set_param(label, value);
//                app_set_param(label, value);
#endif
            }
            now_label = 0;
            now_value = 0;
            now_comment = 1;
            i = 0;
        }
        else if ( now_label )   /* label */
        {
            label[i] = c;
            ++i;
        }
        else if ( now_value )   /* value */
        {
            value[i] = c;
            ++i;
        }
    }

    if ( now_value )    /* end of value */
    {
        value[i] = EOS;
#ifndef ASYNC_SERVICE
        eng_set_param(label, value);
//        app_set_param(label, value);
#endif
    }

    if ( NULL != h_file )
        fclose(h_file);

    return TRUE;
}
#endif /* OLD_CODE */


/* --------------------------------------------------------------------------
   Get param from config file
---------------------------------------------------------------------------*/
bool silgy_read_param_str(const char *param, char *dest)
{
    char *p;

//    DBG("silgy_read_param [%s]", param);

    if ( !M_conf )
    {
        ERR("No config file or not read yet");
        return FALSE;
    }

    if ( (p=strstr(M_conf, param)) == NULL )
    {
//        if ( dest ) dest[0] = EOS;
        return FALSE;
    }

    /* string present but is it label or value? */

    if ( p > M_conf && *(p-1) != '\n' )
    {
        /* looks like it's a value or it's commented out */
//        if ( dest ) dest[0] = EOS;
        return FALSE;
    }


    /* param present ----------------------------------- */

    if ( !dest ) return TRUE;   /* it's only a presence check */


    /* copy value to dest ------------------------------ */

    p += strlen(param);

    while ( *p=='=' || *p==' ' || *p=='\t' )
        ++p;

    int i=0;

    while ( *p != '\r' && *p != '\n' && *p != '#' && *p != EOS )
        dest[i++] = *p++;

    dest[i] = EOS;

    return TRUE;
}


/* --------------------------------------------------------------------------
   Get integer param from config file
---------------------------------------------------------------------------*/
bool silgy_read_param_int(const char *param, int *dest)
{
    char tmp[256];

    if ( silgy_read_param_str(param, tmp) )
    {
        if ( dest ) *dest = atoi(tmp);
        return TRUE;
    }

    return FALSE;
}


/* --------------------------------------------------------------------------
   Create a pid file
-------------------------------------------------------------------------- */
char *lib_create_pid_file(const char *name)
{
static char pidfilename[256];
    FILE    *fpid=NULL;
    char    command[256];

    G_pid = getpid();

    sprintf(pidfilename, "%s.pid", name);

    /* check if the pid file already exists */

    if ( access(pidfilename, F_OK) != -1 )
    {
        WAR("PID file already exists");
        INF("Killing the old process...");
#ifdef _WIN32   /* Windows */
        /* open the pid file and read process id */
        if ( NULL == (fpid=fopen(pidfilename, "r")) )
        {
            ERR("Couldn't open pid file for reading");
            return NULL;
        }
        fseek(fpid, 0, SEEK_END);     /* determine the file size */
        int fsize = ftell(fpid);
        if ( fsize < 1 || fsize > 30 )
        {
            fclose(fpid);
            ERR("Something's wrong with the pid file size (%d bytes)", fsize);
            return NULL;
        }
        rewind(fpid);
        char oldpid[32];
        fread(oldpid, fsize, 1, fpid);
        fclose(fpid);
        oldpid[fsize] = EOS;
        DBG("oldpid [%s]", oldpid);

        msleep(100);

        sprintf(command, "taskkill /pid %s", oldpid);
#else
        sprintf(command, "kill `cat %s`", pidfilename);
#endif  /* _WIN32 */
//        system(command);

//        msleep(100);

        INF("Removing pid file...");
#ifdef _WIN32   /* Windows */
        sprintf(command, "del %s", pidfilename);
#else
        sprintf(command, "rm %s", pidfilename);
#endif
        system(command);

        msleep(100);
    }

    /* create a pid file */

    if ( NULL == (fpid=fopen(pidfilename, "w")) )
    {
        INF("Tried to create [%s]", pidfilename);
        ERR("Failed to create pid file, errno = %d (%s)", errno, strerror(errno));
        return NULL;
    }

    /* write pid to pid file */

    if ( fprintf(fpid, "%d", G_pid) < 1 )
    {
        ERR("Couldn't write to pid file, errno = %d (%s)", errno, strerror(errno));
        return NULL;
    }

    fclose(fpid);

    return pidfilename;
}


/* --------------------------------------------------------------------------
   Attach to shared memory segment
-------------------------------------------------------------------------- */
bool lib_shm_create(long bytes)
{
#ifndef _WIN32
    key_t key;

    /* Create unique key via call to ftok() */
    key = ftok(".", 'S');

    /* Open the shared memory segment - create if necessary */
    if ( (M_shmid=shmget(key, bytes, IPC_CREAT|IPC_EXCL|0666)) == -1 )
    {
        printf("Shared memory segment exists - opening as client\n");

        /* Segment probably already exists - try as a client */
        if ( (M_shmid=shmget(key, bytes, 0)) == -1 )
        {
            perror("shmget");
            return FALSE;
        }
    }
    else
    {
        printf("Creating new shared memory segment\n");
    }

    /* Attach (map) the shared memory segment into the current process */
    if ( (G_shm_segptr=(char*)shmat(M_shmid, 0, 0)) == (char*)-1 )
    {
        perror("shmat");
        return FALSE;
    }
#endif
    return TRUE;
}


/* --------------------------------------------------------------------------
   Delete shared memory segment
-------------------------------------------------------------------------- */
void lib_shm_delete(long bytes)
{
#ifndef _WIN32
    if ( lib_shm_create(bytes) )
    {
        shmctl(M_shmid, IPC_RMID, 0);
        printf("Shared memory segment marked for deletion\n");
    }
#endif
}


/* --------------------------------------------------------------------------
   Start a log
-------------------------------------------------------------------------- */
bool log_start(const char *prefix, bool test)
{
    char    fprefix[64]="";     /* formatted prefix */
    char    fname[512];         /* file name */
    char    ffname[512];        /* full file name */
    
    if ( G_logLevel < 1 ) return TRUE;
    
    if ( M_log_fd != NULL && M_log_fd != (FILE*)STDOUT_FILENO ) return TRUE;  /* already started */

    if ( prefix && prefix[0] )
        sprintf(fprefix, "%s_", prefix);

    sprintf(fname, "%s%d%02d%02d_%02d%02d", fprefix, G_ptm->tm_year+1900, G_ptm->tm_mon+1, G_ptm->tm_mday, G_ptm->tm_hour, G_ptm->tm_min);

    if ( test )
        sprintf(ffname, "%s_t.log", fname);
    else
        sprintf(ffname, "%s.log", fname);

    /* first try in SILGYDIR --------------------------------------------- */

    if ( G_appdir[0] )
    {
        char fffname[512];       /* full file name with path */
        sprintf(fffname, "%s/logs/%s", G_appdir, ffname);
        if ( NULL == (M_log_fd=fopen(fffname, "a")) )
        {
            if ( NULL == (M_log_fd=fopen(ffname, "a")) )  /* try current dir */
            {
                printf("ERROR: Couldn't open log file.\n");
                return FALSE;
            }
        }
    }
    else    /* no SILGYDIR -- try current dir */
    {
        if ( NULL == (M_log_fd=fopen(ffname, "a")) )
        {
            printf("ERROR: Couldn't open log file.\n");
            return FALSE;
        }
    }

    if ( fprintf(M_log_fd, "-------------------------------------------------------------------------------------------------\n") < 0 )
    {
        perror("fprintf");
        return FALSE;
    }

    ALWAYS(" %s  Starting %s's log. Server version: %s, app version: %s", G_dt, APP_WEBSITE, WEB_SERVER_VERSION, APP_VERSION);

    fprintf(M_log_fd, "-------------------------------------------------------------------------------------------------\n\n");

    return TRUE;
}


/* --------------------------------------------------------------------------
   Write to log with date/time
-------------------------------------------------------------------------- */
void log_write_time(int level, const char *message, ...)
{
    va_list     plist;
static char     buffer[MAX_LOG_STR_LEN+1+64];   /* don't use stack */

    if ( level > G_logLevel ) return;

    /* output timestamp */

    fprintf(M_log_fd, "[%s] ", G_dt);

    if ( LOG_ERR == level )
        fprintf(M_log_fd, "ERROR: ");
    else if ( LOG_WAR == level )
        fprintf(M_log_fd, "WARNING: ");

    /* compile message with arguments into buffer */

    va_start(plist, message);
    vsprintf(buffer, message, plist);
    va_end(plist);

    /* write to log file */

    fprintf(M_log_fd, "%s\n", buffer);

#ifdef DUMP
    fflush(M_log_fd);
#else
    if ( G_logLevel >= LOG_DBG ) fflush(M_log_fd);
#endif
}


/* --------------------------------------------------------------------------
   Write to log
-------------------------------------------------------------------------- */
void log_write(int level, const char *message, ...)
{
    va_list     plist;
static char     buffer[MAX_LOG_STR_LEN+1+64];   /* don't use stack */

    if ( level > G_logLevel ) return;

    if ( LOG_ERR == level )
        fprintf(M_log_fd, "ERROR: ");
    else if ( LOG_WAR == level )
        fprintf(M_log_fd, "WARNING: ");

    /* compile message with arguments into buffer */

    va_start(plist, message);
    vsprintf(buffer, message, plist);
    va_end(plist);

    /* write to log file */

    fprintf(M_log_fd, "%s\n", buffer);

#ifdef DUMP
    fflush(M_log_fd);
#else
    if ( G_logLevel >= LOG_DBG ) fflush(M_log_fd);
#endif
}


/* --------------------------------------------------------------------------
   Write looong string to a log or --
   its first (MAX_LOG_STR_LEN-50) part if it's longer
-------------------------------------------------------------------------- */
void log_long(const char *str, long len, const char *desc)
{
static char log_buffer[MAX_LOG_STR_LEN+1];

    if ( len < MAX_LOG_STR_LEN-50 )
        DBG("%s:\n\n[%s]\n", desc, str);
    else
    {
        strncpy(log_buffer, str, MAX_LOG_STR_LEN-50);
        strcpy(log_buffer+MAX_LOG_STR_LEN-50, " (...)");
        DBG("%s:\n\n[%s]\n", desc, log_buffer);
    }
}


/* --------------------------------------------------------------------------
   Flush log
-------------------------------------------------------------------------- */
void log_flush()
{
    if ( M_log_fd != NULL )
        fflush(M_log_fd);
}


/* --------------------------------------------------------------------------
   Close log
-------------------------------------------------------------------------- */
void log_finish()
{
    ALWAYS("Closing log");

    if ( M_log_fd != NULL && M_log_fd != (FILE*)STDOUT_FILENO )
    {
        fclose(M_log_fd);
        M_log_fd = (FILE*)STDOUT_FILENO;
    }
}


#ifdef ICONV
/* --------------------------------------------------------------------------
   Convert string
-------------------------------------------------------------------------- */
char *lib_convert(char *src, const char *cp_from, const char *cp_to)
{
static char dst[1024];

    iconv_t cd = iconv_open(cp_to, cp_from);

    if (cd == (iconv_t) -1)
    {
        strcpy(dst, "iconv_open failed");
        return dst;
    }

    char *in_buf = src;
    size_t in_left = strlen(src);

    char *out_buf = &dst[0];
    size_t out_left = 1023;

    do
    {
        if (iconv(cd, &in_buf, &in_left, &out_buf, &out_left) == (size_t) -1)
        {
            strcpy(dst, "iconv failed");
            return dst;
        }
    } while (in_left > 0 && out_left > 0);

    *out_buf = 0;

    iconv_close(cd);

    return dst;
}
#endif /* ICONV */





/* ================================================================================================ */
/* Base64                                                                                           */
/* ================================================================================================ */
/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */
/* ====================================================================
 * Copyright (c) 1995-1999 The Apache Group.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the Apache Group
 *    for use in the Apache HTTP server project (http://www.apache.org/)."
 *
 * 4. The names "Apache Server" and "Apache Group" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    apache@apache.org.
 *
 * 5. Products derived from this software may not be called "Apache"
 *    nor may "Apache" appear in their names without prior written
 *    permission of the Apache Group.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the Apache Group
 *    for use in the Apache HTTP server project (http://www.apache.org/)."
 *
 * THIS SOFTWARE IS PROVIDED BY THE APACHE GROUP ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE APACHE GROUP OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Group and was originally based
 * on public domain software written at the National Center for
 * Supercomputing Applications, University of Illinois, Urbana-Champaign.
 * For more information on the Apache Group and the Apache HTTP server
 * project, please see <http://www.apache.org/>.
 *
 */

/* Base64 encoder/decoder. Originally Apache file ap_base64.c
 */

/* aaaack but it's fast and const should make it shared text page. */
static const unsigned char pr2six[256] =
{
    /* ASCII table */
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64, 64, 64, 63,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 64, 64, 64,
    64,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64, 64,
    64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64
};

int Base64decode_len(const char *bufcoded)
{
    int nbytesdecoded;
    register const unsigned char *bufin;
    register int nprbytes;

    bufin = (const unsigned char *) bufcoded;
    while (pr2six[*(bufin++)] <= 63);

    nprbytes = (bufin - (const unsigned char *) bufcoded) - 1;
    nbytesdecoded = ((nprbytes + 3) / 4) * 3;

    return nbytesdecoded + 1;
}

int Base64decode(char *bufplain, const char *bufcoded)
{
    int nbytesdecoded;
    register const unsigned char *bufin;
    register unsigned char *bufout;
    register int nprbytes;

    bufin = (const unsigned char *) bufcoded;
    while (pr2six[*(bufin++)] <= 63);
    nprbytes = (bufin - (const unsigned char *) bufcoded) - 1;
    nbytesdecoded = ((nprbytes + 3) / 4) * 3;

    bufout = (unsigned char *) bufplain;
    bufin = (const unsigned char *) bufcoded;

    while (nprbytes > 4) {
    *(bufout++) =
        (unsigned char) (pr2six[*bufin] << 2 | pr2six[bufin[1]] >> 4);
    *(bufout++) =
        (unsigned char) (pr2six[bufin[1]] << 4 | pr2six[bufin[2]] >> 2);
    *(bufout++) =
        (unsigned char) (pr2six[bufin[2]] << 6 | pr2six[bufin[3]]);
    bufin += 4;
    nprbytes -= 4;
    }

    /* Note: (nprbytes == 1) would be an error, so just ingore that case */
    if (nprbytes > 1) {
    *(bufout++) =
        (unsigned char) (pr2six[*bufin] << 2 | pr2six[bufin[1]] >> 4);
    }
    if (nprbytes > 2) {
    *(bufout++) =
        (unsigned char) (pr2six[bufin[1]] << 4 | pr2six[bufin[2]] >> 2);
    }
    if (nprbytes > 3) {
    *(bufout++) =
        (unsigned char) (pr2six[bufin[2]] << 6 | pr2six[bufin[3]]);
    }

    *(bufout++) = '\0';
    nbytesdecoded -= (4 - nprbytes) & 3;
    return nbytesdecoded;
}

static const char basis_64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int Base64encode_len(int len)
{
    return ((len + 2) / 3 * 4) + 1;
}

int Base64encode(char *encoded, const char *string, int len)
{
    int i;
    char *p;

    p = encoded;
    for (i = 0; i < len - 2; i += 3) {
    *p++ = basis_64[(string[i] >> 2) & 0x3F];
    *p++ = basis_64[((string[i] & 0x3) << 4) |
                    ((int) (string[i + 1] & 0xF0) >> 4)];
    *p++ = basis_64[((string[i + 1] & 0xF) << 2) |
                    ((int) (string[i + 2] & 0xC0) >> 6)];
    *p++ = basis_64[string[i + 2] & 0x3F];
    }
    if (i < len) {
    *p++ = basis_64[(string[i] >> 2) & 0x3F];
    if (i == (len - 1)) {
        *p++ = basis_64[((string[i] & 0x3) << 4)];
        *p++ = '=';
    }
    else {
        *p++ = basis_64[((string[i] & 0x3) << 4) |
                        ((int) (string[i + 1] & 0xF0) >> 4)];
        *p++ = basis_64[((string[i + 1] & 0xF) << 2)];
    }
    *p++ = '=';
    }

    *p++ = '\0';
    return p - encoded;
}



/* ================================================================================================ */
/* SHA1                                                                                             */
/* ================================================================================================ */
/*
SHA-1 in C
By Steve Reid <sreid@sea-to-sky.net>
100% Public Domain

-----------------
Modified 7/98
By James H. Brown <jbrown@burgoyne.com>
Still 100% Public Domain

Corrected a problem which generated improper hash values on 16 bit machines
Routine SHA1Update changed from
    void SHA1Update(SHA1_CTX* context, unsigned char* data, unsigned int
len)
to
    void SHA1Update(SHA1_CTX* context, unsigned char* data, unsigned
long len)

The 'len' parameter was declared an int which works fine on 32 bit machines.
However, on 16 bit machines an int is too small for the shifts being done
against
it.  This caused the hash function to generate incorrect values if len was
greater than 8191 (8K - 1) due to the 'len << 3' on line 3 of SHA1Update().

Since the file IO in main() reads 16K at a time, any file 8K or larger would
be guaranteed to generate the wrong hash (e.g. Test Vector #3, a million
"a"s).

I also changed the declaration of variables i & j in SHA1Update to
unsigned long from unsigned int for the same reason.

These changes should make no difference to any 32 bit implementations since
an
int and a long are the same size in those environments.

--
I also corrected a few compiler warnings generated by Borland C.
1. Added #include <process.h> for exit() prototype
2. Removed unused variable 'j' in SHA1Final
3. Changed exit(0) to return(0) at end of main.

ALL changes I made can be located by searching for comments containing 'JHB'
-----------------
Modified 8/98
By Steve Reid <sreid@sea-to-sky.net>
Still 100% public domain

1- Removed #include <process.h> and used return() instead of exit()
2- Fixed overwriting of finalcount in SHA1Final() (discovered by Chris Hall)
3- Changed email address from steve@edmweb.com to sreid@sea-to-sky.net

-----------------
Modified 4/01
By Saul Kravitz <Saul.Kravitz@celera.com>
Still 100% PD
Modified to run on Compaq Alpha hardware.

-----------------
Modified 07/2002
By Ralph Giles <giles@ghostscript.com>
Still 100% public domain
modified for use with stdint types, autoconf
code cleanup, removed attribution comments
switched SHA1Final() argument order for consistency
use SHA1_ prefix for public api
move public api to sha1.h
*/

/*
Test Vectors (from FIPS PUB 180-1)
"abc"
  A9993E36 4706816A BA3E2571 7850C26C 9CD0D89D
"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"
  84983E44 1C3BD26E BAAE4AA1 F95129E5 E54670F1
A million repetitions of "a"
  34AA973C D4C4DAA4 F61EEB2B DBAD2731 6534016F
*/

/*#define WORDS_BIGENDIAN        on AIX only! */

#define SHA1HANDSOFF

static void SHA1_Transform2(uint32_t state[5], const uint8_t buffer[64]);

#define rol(value, bits) (((value) << (bits)) | ((value) >> (32 - (bits))))

/* blk0() and blk() perform the initial expand. */
/* I got the idea of expanding during the round function from SSLeay */
/* FIXME: can we do this in an endian-proof way? */
#ifdef WORDS_BIGENDIAN
#define blk0(i) block->l[i]
#else
#define blk0(i) (block->l[i] = (rol(block->l[i],24)&0xFF00FF00) \
    |(rol(block->l[i],8)&0x00FF00FF))
#endif
#define blk(i) (block->l[i&15] = rol(block->l[(i+13)&15]^block->l[(i+8)&15] \
    ^block->l[(i+2)&15]^block->l[i&15],1))

/* (R0+R1), R2, R3, R4 are the different operations used in libSHA1 */
#define R0(v,w,x,y,z,i) z+=((w&(x^y))^y)+blk0(i)+0x5A827999+rol(v,5);w=rol(w,30);
#define R1(v,w,x,y,z,i) z+=((w&(x^y))^y)+blk(i)+0x5A827999+rol(v,5);w=rol(w,30);
#define R2(v,w,x,y,z,i) z+=(w^x^y)+blk(i)+0x6ED9EBA1+rol(v,5);w=rol(w,30);
#define R3(v,w,x,y,z,i) z+=(((w|x)&y)|(w&x))+blk(i)+0x8F1BBCDC+rol(v,5);w=rol(w,30);
#define R4(v,w,x,y,z,i) z+=(w^x^y)+blk(i)+0xCA62C1D6+rol(v,5);w=rol(w,30);


static void libSHA1_Init(SHA1_CTX* context);
static void libSHA1_Update(SHA1_CTX* context, const uint8_t* data, const size_t len);
static void libSHA1_Final(SHA1_CTX* context, uint8_t digest[SHA1_DIGEST_SIZE]);



/* Hash a single 512-bit block. This is the core of the algorithm. */
static void SHA1_Transform2(uint32_t state[5], const uint8_t buffer[64])
{
    uint32_t a, b, c, d, e;
    typedef union {
        uint8_t c[64];
        uint32_t l[16];
    } CHAR64LONG16;
    CHAR64LONG16* block;

#ifdef SHA1HANDSOFF
    static uint8_t workspace[64];
    block = (CHAR64LONG16*)workspace;
    memcpy(block, buffer, 64);
#else
    block = (CHAR64LONG16*)buffer;
#endif

    /* Copy context->state[] to working vars */
    a = state[0];
    b = state[1];
    c = state[2];
    d = state[3];
    e = state[4];

    /* 4 rounds of 20 operations each. Loop unrolled. */
    R0(a,b,c,d,e, 0); R0(e,a,b,c,d, 1); R0(d,e,a,b,c, 2); R0(c,d,e,a,b, 3);
    R0(b,c,d,e,a, 4); R0(a,b,c,d,e, 5); R0(e,a,b,c,d, 6); R0(d,e,a,b,c, 7);
    R0(c,d,e,a,b, 8); R0(b,c,d,e,a, 9); R0(a,b,c,d,e,10); R0(e,a,b,c,d,11);
    R0(d,e,a,b,c,12); R0(c,d,e,a,b,13); R0(b,c,d,e,a,14); R0(a,b,c,d,e,15);
    R1(e,a,b,c,d,16); R1(d,e,a,b,c,17); R1(c,d,e,a,b,18); R1(b,c,d,e,a,19);
    R2(a,b,c,d,e,20); R2(e,a,b,c,d,21); R2(d,e,a,b,c,22); R2(c,d,e,a,b,23);
    R2(b,c,d,e,a,24); R2(a,b,c,d,e,25); R2(e,a,b,c,d,26); R2(d,e,a,b,c,27);
    R2(c,d,e,a,b,28); R2(b,c,d,e,a,29); R2(a,b,c,d,e,30); R2(e,a,b,c,d,31);
    R2(d,e,a,b,c,32); R2(c,d,e,a,b,33); R2(b,c,d,e,a,34); R2(a,b,c,d,e,35);
    R2(e,a,b,c,d,36); R2(d,e,a,b,c,37); R2(c,d,e,a,b,38); R2(b,c,d,e,a,39);
    R3(a,b,c,d,e,40); R3(e,a,b,c,d,41); R3(d,e,a,b,c,42); R3(c,d,e,a,b,43);
    R3(b,c,d,e,a,44); R3(a,b,c,d,e,45); R3(e,a,b,c,d,46); R3(d,e,a,b,c,47);
    R3(c,d,e,a,b,48); R3(b,c,d,e,a,49); R3(a,b,c,d,e,50); R3(e,a,b,c,d,51);
    R3(d,e,a,b,c,52); R3(c,d,e,a,b,53); R3(b,c,d,e,a,54); R3(a,b,c,d,e,55);
    R3(e,a,b,c,d,56); R3(d,e,a,b,c,57); R3(c,d,e,a,b,58); R3(b,c,d,e,a,59);
    R4(a,b,c,d,e,60); R4(e,a,b,c,d,61); R4(d,e,a,b,c,62); R4(c,d,e,a,b,63);
    R4(b,c,d,e,a,64); R4(a,b,c,d,e,65); R4(e,a,b,c,d,66); R4(d,e,a,b,c,67);
    R4(c,d,e,a,b,68); R4(b,c,d,e,a,69); R4(a,b,c,d,e,70); R4(e,a,b,c,d,71);
    R4(d,e,a,b,c,72); R4(c,d,e,a,b,73); R4(b,c,d,e,a,74); R4(a,b,c,d,e,75);
    R4(e,a,b,c,d,76); R4(d,e,a,b,c,77); R4(c,d,e,a,b,78); R4(b,c,d,e,a,79);

    /* Add the working vars back into context.state[] */
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;

    /* Wipe variables */
    a = b = c = d = e = 0;
}


/* SHA1Init - Initialize new context */
static void libSHA1_Init(SHA1_CTX* context)
{
    /* libSHA1 initialization constants */
    context->state[0] = 0x67452301;
    context->state[1] = 0xEFCDAB89;
    context->state[2] = 0x98BADCFE;
    context->state[3] = 0x10325476;
    context->state[4] = 0xC3D2E1F0;
    context->count[0] = context->count[1] = 0;
}


/* Run your data through this. */
static void libSHA1_Update(SHA1_CTX* context, const uint8_t* data, const size_t len)
{
    size_t i, j;

    j = (context->count[0] >> 3) & 63;
    if ((context->count[0] += len << 3) < (len << 3)) context->count[1]++;
    context->count[1] += (len >> 29);
    if ((j + len) > 63) {
        memcpy(&context->buffer[j], data, (i = 64-j));
        SHA1_Transform2(context->state, context->buffer);
        for ( ; i + 63 < len; i += 64) {
            SHA1_Transform2(context->state, data + i);
        }
        j = 0;
    }
    else i = 0;
    memcpy(&context->buffer[j], &data[i], len - i);
}


/* Add padding and return the message digest. */
static void libSHA1_Final(SHA1_CTX* context, uint8_t digest[SHA1_DIGEST_SIZE])
{
    uint32_t i;
    uint8_t  finalcount[8];

    for (i = 0; i < 8; i++) {
        finalcount[i] = (unsigned char)((context->count[(i >= 4 ? 0 : 1)]
         >> ((3-(i & 3)) * 8) ) & 255);  /* Endian independent */
    }
    libSHA1_Update(context, (uint8_t *)"\200", 1);
    while ((context->count[0] & 504) != 448) {
        libSHA1_Update(context, (uint8_t *)"\0", 1);
    }
    libSHA1_Update(context, finalcount, 8);  /* Should cause a SHA1_Transform() */
    for (i = 0; i < SHA1_DIGEST_SIZE; i++) {
        digest[i] = (uint8_t)
         ((context->state[i>>2] >> ((3-(i & 3)) * 8) ) & 255);
    }

    /* Wipe variables */
    i = 0;
    memset(context->buffer, 0, 64);
    memset(context->state, 0, 20);
    memset(context->count, 0, 8);
    memset(finalcount, 0, 8);   /* SWR */

#ifdef SHA1HANDSOFF  /* make SHA1Transform overwrite its own static vars */
    SHA1_Transform2(context->state, context->buffer);
#endif
}


void libSHA1(unsigned char *ptr, unsigned int size, unsigned char *outbuf)
{
  SHA1_CTX ctx;

  libSHA1_Init(&ctx);
  libSHA1_Update(&ctx, ptr, size);
  libSHA1_Final(&ctx, outbuf);
}



void digest_to_hex(const uint8_t digest[SHA1_DIGEST_SIZE], char *output)
{
    int i,j;
    char *c = output;

    for (i = 0; i < SHA1_DIGEST_SIZE/4; i++) {
        for (j = 0; j < 4; j++) {
            sprintf(c,"%02X", digest[i*4+j]);
            c += 2;
        }
        sprintf(c, " ");
        c += 1;
    }
    *(c - 1) = '\0';
}




#ifdef _WIN32   /* Windows */
/* --------------------------------------------------------------------------
   Windows port of getpid
-------------------------------------------------------------------------- */
int getpid()
{
    return GetCurrentProcessId();
}


/* --------------------------------------------------------------------------
   Windows port of clock_gettime
   https://stackoverflow.com/questions/5404277/porting-clock-gettime-to-windows
-------------------------------------------------------------------------- */
#define exp7           10000000     // 1E+7
#define exp9         1000000000     // 1E+9
#define w2ux 116444736000000000     // 1 Jan 1601 to 1 Jan 1970

static void unix_time(struct timespec *spec)
{
    __int64 wintime;
    GetSystemTimeAsFileTime((FILETIME*)&wintime); 
    wintime -= w2ux;
    spec->tv_sec = wintime / exp7;                 
    spec->tv_nsec = wintime % exp7 * 100;
}

int clock_gettime(int dummy, struct timespec *spec)
{
   static  struct timespec startspec;
   static double ticks2nano;
   static __int64 startticks, tps=0;
   __int64 tmp, curticks;

   QueryPerformanceFrequency((LARGE_INTEGER*)&tmp); // some strange system can possibly change freq?

   if ( tps != tmp )
   {
       tps = tmp; // init ONCE
       QueryPerformanceCounter((LARGE_INTEGER*)&startticks);
       unix_time(&startspec);
       ticks2nano = (double)exp9 / tps;
   }

   QueryPerformanceCounter((LARGE_INTEGER*)&curticks);
   curticks -= startticks;
   spec->tv_sec = startspec.tv_sec + (curticks / tps);
   spec->tv_nsec = startspec.tv_nsec + (double)(curticks % tps) * ticks2nano;
   if ( !(spec->tv_nsec < exp9) )
   {
       ++spec->tv_sec;
       spec->tv_nsec -= exp9;
   }

   return 0;
}


/* --------------------------------------------------------------------------
   Windows port of stpcpy
-------------------------------------------------------------------------- */
char *stpcpy(char *dest, const char *src)
{
    register char *d=dest;
    register const char *s=src;

    do
        *d++ = *s;
    while (*s++ != '\0');

    return d - 1;
}


/* --------------------------------------------------------------------------
   Windows port of stpcpy
-------------------------------------------------------------------------- */
char *stpncpy(char *dest, const char *src, unsigned long len)
{
    register char *d=dest;
    register const char *s=src;
    int count=0;

    do
        *d++ = *s;
    while (*s++ != '\0' && ++count<len);

    return d - 1;
}
#endif  /* _WIN32 */


#ifndef strnstr
/* --------------------------------------------------------------------------
   Windows port of strnstr
-------------------------------------------------------------------------- */
char *strnstr(const char *haystack, const char *needle, size_t len)
{
    int i;
    size_t needle_len;

    if ( 0 == (needle_len = strnlen(needle, len)) )
        return (char*)haystack;

    for ( i=0; i<=(int)(len-needle_len); ++i )
    {
        if ( (haystack[0] == needle[0]) && (0 == strncmp(haystack, needle, needle_len)) )
            return (char*)haystack;

        ++haystack;
    }

    return NULL;
}
#endif
