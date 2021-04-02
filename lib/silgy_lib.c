/* --------------------------------------------------------------------------
   Silgy Web App Engine
   Jurek Muszynski
   silgy.com
   Some parts are Public Domain from other authors, see below
-----------------------------------------------------------------------------
   General purpose library
-------------------------------------------------------------------------- */

#include <silgy.h>

#ifdef ICONV
#include <iconv.h>
#include <locale.h>
#endif

#include <limits.h>     /* INT_MAX */

#define RANDOM_NUMBERS 1024*64



/* globals */

int         G_logLevel=3;               /* log level -- 'info' by default */
int         G_logToStdout=0;            /* log to stdout */
int         G_logCombined=0;            /* standard log format */
char        G_appdir[256]="..";         /* application root dir */
int         G_RESTTimeout=CALL_REST_DEFAULT_TIMEOUT;
int         G_test=0;                   /* test run */
int         G_pid=0;                    /* pid */
time_t      G_now=0;                    /* current time (GMT) */
struct tm   *G_ptm={0};                 /* human readable current time */
char        G_dt[20]="";                /* datetime for database or log (YYYY-MM-DD hh:mm:ss) */
char        G_tmp[TMP_BUFSIZE];         /* temporary string buffer */
bool        G_initialized=0;

/* messages */
message_t   G_messages[MAX_MESSAGES]={0};
int         G_next_msg=0;
lang_t      G_msg_lang[MAX_LANGUAGES]={0};
int         G_next_msg_lang=0;

/* strings */
string_t    G_strings[MAX_STRINGS]={0};
int         G_next_str=0;
lang_t      G_str_lang[MAX_LANGUAGES]={0};
int         G_next_str_lang=0;

stat_res_t  G_snippets[MAX_SNIPPETS]={0};
int         G_snippets_cnt=0;

#ifdef HTTPS
bool        G_ssl_lib_initialized=0;
#endif

unsigned    G_rest_req=0;               /* REST calls counter */
double      G_rest_elapsed=0;           /* REST calls elapsed for calculating average */
double      G_rest_average=0;           /* REST calls average elapsed */
int         G_rest_status;              /* last REST call response status */
char        G_rest_content_type[MAX_VALUE_LEN+1];
int         G_rest_res_len=0;
int         G_qs_len=0;


/* locals */

static char *M_conf=NULL;               /* config file content */
static FILE *M_log_fd=NULL;             /* log file handle */

static char M_df=0;                     /* date format */
static char M_tsep=' ';                 /* thousand separator */
static char M_dsep='.';                 /* decimal separator */

static char *M_md_dest;
static char M_md_list_type;

#ifndef _WIN32
static int  M_shmid[MAX_SHM_SEGMENTS]={0}; /* SHM id-s */
#endif

static rest_header_t M_rest_headers[REST_MAX_HEADERS];
static int M_rest_headers_cnt=0;
#ifdef _WIN32   /* Windows */
static SOCKET M_rest_sock;
#else
static int M_rest_sock;
#endif  /* _WIN32 */
#ifdef HTTPS
static SSL_CTX *M_ssl_ctx=NULL;
static SSL *M_rest_ssl=NULL;
//static SSL_SESSION *M_ssl_session=NULL;
#else
static void *M_rest_ssl=NULL;    /* dummy */
#endif  /* HTTPS */
static char M_rest_mode;

static bool M_rest_proxy=FALSE;

static unsigned char M_random_numbers[RANDOM_NUMBERS];
static char M_random_initialized=0;


static void load_err_messages(void);
static void load_strings(void);
static void minify_1(char *dest, const char *src, int len);
static int  minify_2(char *dest, const char *src);
static void get_byteorder32(void);
static void get_byteorder64(void);


/* --------------------------------------------------------------------------
   Library init
-------------------------------------------------------------------------- */
void silgy_lib_init()
{
    int i;

    DBG("silgy_lib_init");

    /* G_pid */

    G_pid = getpid();

    /* G_appdir */

    lib_get_app_dir();

    /* time globals */

    lib_update_time_globals();

    /* log file fd */

    M_log_fd = stdout;

    /* load error messages */

    load_err_messages();

#ifndef SILGY_CLIENT
    /* load strings */
    load_strings();

    for ( i=0; i<MAX_SNIPPETS; ++i )
        strcpy(G_snippets[i].name, "-");
#endif

#ifdef ICONV
    setlocale(LC_ALL, "");
#endif
}


/* --------------------------------------------------------------------------
   Library clean up
-------------------------------------------------------------------------- */
void silgy_lib_done()
{
    int i;

    DBG("silgy_lib_done");

#ifndef _WIN32

    for ( i=0; i<MAX_SHM_SEGMENTS; ++i )
        lib_shm_delete(i);

#endif  /* _WIN32 */

    log_finish();
}


/* --------------------------------------------------------------------------
   Copy 0-terminated UTF-8 string safely
   dst_len is in bytes and excludes terminating 0
   dst_len must be > 0
-------------------------------------------------------------------------- */
void silgy_safe_copy(char *dst, const char *src, size_t dst_len)
{
#ifdef DUMP
    DBG("silgy_safe_copy [%s], dst_len = %d", src, dst_len);
#endif
    strncpy(dst, src, dst_len+1);

    if ( dst[dst_len] == EOS )
    {
#ifdef DUMP
        DBG("not truncated");
#endif
        return;   /* not truncated */
    }

    /* ------------------------- */
    /* string has been truncated */

    if ( !UTF8_ANY(dst[dst_len]) )
    {
        dst[dst_len] = EOS;
#ifdef DUMP
        DBG("truncated string won't break the UTF-8 sequence");
#endif
        return;
    }

    /* ------------------------------- */
    /* string ends with UTF-8 sequence */

    /* cut until beginning of the sequence found */

    while ( !UTF8_START(dst[dst_len]) )
    {
#ifdef DUMP
        DBG("UTF-8 sequence byte (%x)", dst[dst_len]);
#endif
        if ( dst_len == 0 )
        {
            dst[0] = EOS;
            return;
        }

        dst_len--;
    }

    dst[dst_len] = EOS;
}



/* --------------------------------------------------------- */
/* Time zone handling -------------------------------------- */


#ifndef SILGY_CLIENT
/* --------------------------------------------------------------------------
   Set client's time zone offset on the server
-------------------------------------------------------------------------- */
void silgy_set_tz(int ci)
{
    DBG("silgy_set_tz");

    /* Log user's time zone */

    QSVAL tz;

    if ( QS("tz", tz) )
        INF("Time zone: %s", tz);

    /* set it only once */

    US.tz_set = TRUE;

    /* time zone offset */

    int tzo;

    if ( !QSI("tzo", &tzo) ) return;

    DBG("tzo=%d", tzo);

    if ( tzo < -900 || tzo > 900 ) return;

    US.tz_offset = tzo * -1;
}


/* --------------------------------------------------------------------------
   Calculate client's local time
-------------------------------------------------------------------------- */
time_t silgy_ua_time(int ci)
{
    return G_now + US.tz_offset * 60;
}


/* --------------------------------------------------------------------------
   Return client's local today string
-------------------------------------------------------------------------- */
char *silgy_ua_today(int ci)
{
static char today[11];
    strncpy(today, DT_NOW_LOCAL, 10);
    today[10] = EOS;
    return today;
}
#endif  /* SILGY_CLIENT */


/* --------------------------------------------------------------------------
   Return today string
-------------------------------------------------------------------------- */
char *silgy_today()
{
static char today[11];
    strncpy(today, DT_NOW, 10);
    today[10] = EOS;
    return today;
}



/* --------------------------------------------------------- */
/* MD parsing ---------------------------------------------- */

#define MD_TAG_NONE         '0'
#define MD_TAG_B            'b'
#define MD_TAG_I            'i'
#define MD_TAG_U            'u'
#define MD_TAG_CODE         'c'
#define MD_TAG_P            'p'
#define MD_TAG_H1           '1'
#define MD_TAG_H2           '2'
#define MD_TAG_H3           '3'
#define MD_TAG_H4           '4'
#define MD_TAG_LI           'l'
#define MD_TAG_ACC_BR       'B'
#define MD_TAG_EOD          '~'

#define MD_LIST_ORDERED     'O'
#define MD_LIST_UNORDERED   'U'

#define IS_TAG_BLOCK        (tag==MD_TAG_P || tag==MD_TAG_H1 || tag==MD_TAG_H2 || tag==MD_TAG_H3 || tag==MD_TAG_H4 || tag==MD_TAG_LI)


/* --------------------------------------------------------------------------
   Detect MD tag
-------------------------------------------------------------------------- */
static int detect_tag(const char *src, char *tag, bool start, bool newline, bool nested)
{
    int skip=0;

    if ( newline )
    {
        ++src;   /* skip '\n' */
        ++skip;
    }

    if ( start || newline || nested )
    {
        while ( *src=='\r' || *src==' ' || *src=='\t' )
        {
            ++src;
            ++skip;
        }
    }

    if ( *src=='*' )   /* bold, italic or list item */
    {
        if ( *(src+1)=='*' )
        {
            *tag = MD_TAG_B;
            skip += 2;
        }
        else if ( (start || newline) && *(src+1)==' ' )
        {
            *tag = MD_TAG_LI;
            skip += 2;
            M_md_list_type = MD_LIST_UNORDERED;
        }
        else    /* italic */
        {
            *tag = MD_TAG_I;
            skip += 1;
        }
    }
    else if ( *src=='_' )   /* underline */
    {
        if ( start || newline || *(src-1)==' ' )
        {
            *tag = MD_TAG_U;
            skip += 1;
        }
        else
            *tag = MD_TAG_NONE;
    }
    else if ( *src=='`' )   /* monospace */
    {
        *tag = MD_TAG_CODE;
        skip += 1;
    }
    else if ( (start || newline || nested) && isdigit(*src) && *(src+1)=='.' && *(src+2)==' ' )   /* single-digit ordered list */
    {
        *tag = MD_TAG_LI;
        skip += 3;
        M_md_list_type = MD_LIST_ORDERED;
    }
    else if ( (start || newline || nested) && isdigit(*src) && isdigit(*(src+1)) && *(src+2)=='.' && *(src+3)==' ' )   /* double-digit ordered list */
    {
        *tag = MD_TAG_LI;
        skip += 4;
        M_md_list_type = MD_LIST_ORDERED;
    }
    else if ( (start || newline || nested) && *src=='#' )    /* headers */
    {
        if ( *(src+1)=='#' )
        {
            if ( *(src+2)=='#' )
            {
                if ( *(src+3)=='#' )
                {
                    *tag = MD_TAG_H4;
                    skip += 5;
                }
                else
                {
                    *tag = MD_TAG_H3;
                    skip += 4;
                }
            }
            else
            {
                *tag = MD_TAG_H2;
                skip += 3;
            }
        }
        else
        {
            *tag = MD_TAG_H1;
            skip += 2;
        }
    }
    else if ( start || nested || (newline && *src=='\n') )    /* paragraph */
    {
        if ( start )
        {
            *tag = MD_TAG_P;
        }
        else if ( *(src+1) == EOS )
        {
            *tag = MD_TAG_EOD;
        }
        else if ( nested )
        {
            *tag = MD_TAG_P;
        }
        else    /* block tag begins */
        {
            skip += detect_tag(src, tag, false, true, true);
        }
    }
    else if ( *src )
    {
        *tag = MD_TAG_ACC_BR;   /* accidental line break perhaps */
    }
    else    /* end of document */
    {
        *tag = MD_TAG_EOD;
    }

    return skip;
}


/* --------------------------------------------------------------------------
   Open HTML tag
-------------------------------------------------------------------------- */
static int open_tag(char tag)
{
    int written=0;

    if ( tag == MD_TAG_B )
    {
        M_md_dest = stpcpy(M_md_dest, "<b>");
        written = 3;
    }
    else if ( tag == MD_TAG_I )
    {
        M_md_dest = stpcpy(M_md_dest, "<i>");
        written = 3;
    }
    else if ( tag == MD_TAG_U )
    {
        M_md_dest = stpcpy(M_md_dest, "<u>");
        written = 3;
    }
    else if ( tag == MD_TAG_CODE )
    {
        M_md_dest = stpcpy(M_md_dest, "<code>");
        written = 6;
    }
    else if ( tag == MD_TAG_P )
    {
        M_md_dest = stpcpy(M_md_dest, "<p>");
        written = 3;
    }
    else if ( tag == MD_TAG_H1 )
    {
        M_md_dest = stpcpy(M_md_dest, "<h1>");
        written = 4;
    }
    else if ( tag == MD_TAG_H2 )
    {
        M_md_dest = stpcpy(M_md_dest, "<h2>");
        written = 4;
    }
    else if ( tag == MD_TAG_H3 )
    {
        M_md_dest = stpcpy(M_md_dest, "<h3>");
        written = 4;
    }
    else if ( tag == MD_TAG_H4 )
    {
        M_md_dest = stpcpy(M_md_dest, "<h4>");
        written = 4;
    }
    else if ( tag == MD_TAG_LI )
    {
        M_md_dest = stpcpy(M_md_dest, "<li>");
        written = 4;
    }

    return written;
}


/* --------------------------------------------------------------------------
   Close HTML tag
-------------------------------------------------------------------------- */
static int close_tag(const char *src, char tag)
{
    int written=0;

    if ( tag == MD_TAG_B )
    {
        M_md_dest = stpcpy(M_md_dest, "</b>");
        written = 4;
    }
    else if ( tag == MD_TAG_I )
    {
        M_md_dest = stpcpy(M_md_dest, "</i>");
        written = 4;
    }
    else if ( tag == MD_TAG_U )
    {
        M_md_dest = stpcpy(M_md_dest, "</u>");
        written = 4;
    }
    else if ( tag == MD_TAG_CODE )
    {
        M_md_dest = stpcpy(M_md_dest, "</code>");
        written = 7;
    }
    else if ( tag == MD_TAG_P )
    {
        M_md_dest = stpcpy(M_md_dest, "</p>");
        written = 4;
    }
    else if ( tag == MD_TAG_H1 )
    {
        M_md_dest = stpcpy(M_md_dest, "</h1>");
        written = 5;
    }
    else if ( tag == MD_TAG_H2 )
    {
        M_md_dest = stpcpy(M_md_dest, "</h2>");
        written = 5;
    }
    else if ( tag == MD_TAG_H3 )
    {
        M_md_dest = stpcpy(M_md_dest, "</h3>");
        written = 5;
    }
    else if ( tag == MD_TAG_H4 )
    {
        M_md_dest = stpcpy(M_md_dest, "</h4>");
        written = 5;
    }
    else if ( tag == MD_TAG_LI )
    {
        M_md_dest = stpcpy(M_md_dest, "</li>");
        written = 5;
    }

    return written;
}


/* --------------------------------------------------------------------------
   Render simplified markdown to HTML
-------------------------------------------------------------------------- */
char *silgy_render_md(char *dest, const char *src, size_t len)
{
    int  pos=0;    /* source position */
    char tag;
    char tag_b=MD_TAG_NONE;   /* block */
    char tag_i=MD_TAG_NONE;   /* inline */
    int  skip;
    int  written=0;
    bool list=0;
    bool escape=false;

    M_md_dest = dest;

    if ( len < 40 )
    {
        *M_md_dest = EOS;
        return dest;
    }

    skip = detect_tag(src, &tag, true, false, false);

#ifdef DUMP
    DBG("tag %c detected", tag);
#endif

    if ( skip )
    {
        src += skip;
        pos += skip;
    }

    if ( tag == MD_TAG_LI )
    {
#ifdef DUMP
        DBG("Starting unordered list");
#endif
        M_md_dest = stpcpy(M_md_dest, "<ul>");
        written += 4;
        list = 1;
    }

    if ( IS_TAG_BLOCK )
    {
        tag_b = tag;
        written += open_tag(tag_b);
    }
    else    /* inline */
    {
        tag_b = MD_TAG_P;   /* there should always be a block */
        written += open_tag(tag_b);

        tag_i = tag;
        written += open_tag(tag_i);
    }

    const char *prev1, *prev2;

    while ( *src && written < len-18 )   /* worst case: </code></li></ul> */
    {
#ifdef DUMP
        DBG("%c", *src);
#endif
        if ( pos > 0 )
            prev1 = src - 1;

        if ( pos > 1 )
            prev2 = src - 2;

        if ( *src == '\\' && !escape )
        {
            escape = true;
        }
        else if ( (*src=='*' || *src=='_' || *src=='`') && !escape )   /* inline tags */
        {
            if ( tag_i==MD_TAG_B || tag_i==MD_TAG_I || tag_i==MD_TAG_U || tag_i==MD_TAG_CODE )
            {
#ifdef DUMP
                DBG("Closing inline tag %c", tag_i);
#endif
                written += close_tag(src, tag_i);

                if ( tag_i==MD_TAG_B )    /* double-char code */
                {
                    ++src;
                    ++pos;
                }

                tag_i = MD_TAG_NONE;
            }
            else if ( !pos || *(src-1)=='\r' || *(src-1)=='\n' || *(src-1)==' ' || *(src-1)=='(' )    /* opening tag */
            {
                skip = detect_tag(src, &tag, false, false, false);
#ifdef DUMP
                DBG("tag %c detected", tag);
#endif
                if ( skip )
                {
                    src += skip;
                    pos += skip;
                }

                if ( IS_TAG_BLOCK )
                {
                    tag_b = tag;
                    written += open_tag(tag_b);
                }
                else    /* inline */
                {
                    tag_i = tag;
                    written += open_tag(tag_i);
                }

                if ( tag != MD_TAG_NONE && pos )
                {
                    src--;
                    pos--;
                }
            }
            else    /* copy character to dest */
            {
                *M_md_dest++ = *src;
                ++written;
                escape = false;
            }
        }
        else if ( pos && *src=='-' && *prev1=='-' )   /* convert -- to ndash */
        {
            M_md_dest = stpcpy(--M_md_dest, "–");
            written += 3;
        }
        else if ( *src=='\n' && pos>1 && *prev1==' ' && *prev2==' ' )   /* convert    to <br> */
        {
            M_md_dest -= 2;
            M_md_dest = stpcpy(M_md_dest, "<br>");
            written += 2;
        }
        else if ( *src=='\n' )   /* block tags */
        {
            skip = detect_tag(src, &tag, false, true, false);
#ifdef DUMP
            DBG("tag %c detected", tag);
#endif
            if ( skip )
            {
                src += skip;
                pos += skip;
            }

            if ( tag != MD_TAG_NONE && tag != MD_TAG_ACC_BR && tag_b != MD_TAG_NONE )
            {
#ifdef DUMP
                DBG("Closing block tag %c", tag_b);
#endif
                written += close_tag(src, tag_b);
                tag_b = MD_TAG_NONE;
            }

            if ( tag == MD_TAG_LI )
            {
                if ( !list )   /* start a list */
                {
#ifdef DUMP
                    DBG("Starting %sordered list", M_md_list_type==MD_LIST_ORDERED?"":"un");
#endif
                    if ( M_md_list_type == MD_LIST_ORDERED )
                        M_md_dest = stpcpy(M_md_dest, "<ol>");
                    else
                        M_md_dest = stpcpy(M_md_dest, "<ul>");

                    list = 1;
                    written += 4;
                }
            }
            else if ( tag == MD_TAG_ACC_BR )   /* accidental line break */
            {
                M_md_dest = stpcpy(M_md_dest, " ");
                ++written;
            }
            else if ( list )   /* close the list */
            {
#ifdef DUMP
                DBG("Closing %sordered list", M_md_list_type==MD_LIST_ORDERED?"":"un");
#endif
                if ( M_md_list_type == MD_LIST_ORDERED )
                    M_md_dest = stpcpy(M_md_dest, "</ol>");
                else
                    M_md_dest = stpcpy(M_md_dest, "</ul>");

                list = 0;
                written += 5;
            }

            if ( IS_TAG_BLOCK )
            {
                tag_b = tag;
                written += open_tag(tag_b);
            }
            else if ( tag != MD_TAG_NONE && tag != MD_TAG_ACC_BR && tag != MD_TAG_EOD )   /* inline */
            {
                tag_b = MD_TAG_P;   /* always open block tag */
                written += open_tag(tag_b);

                tag_i = tag;    /* open inline tag */
                written += open_tag(tag_i);
            }

            if ( pos )
            {
                src--;
                pos--;
            }
        }
        else    /* copy character to dest */
        {
            *M_md_dest++ = *src;
            ++written;
            escape = false;
        }

        ++src;
        ++pos;
    }

    if ( tag_b != MD_TAG_NONE )
    {
#ifdef DUMP
        DBG("Closing block tag %c", tag_b);
#endif
        written += close_tag(src, tag_b);

        if ( list )    /* close a list */
        {
#ifdef DUMP
            DBG("Closing %sordered list", M_md_list_type==MD_LIST_ORDERED?"":"un");
#endif
            if ( M_md_list_type == MD_LIST_ORDERED )
                M_md_dest = stpcpy(M_md_dest, "</ol>");
            else
                M_md_dest = stpcpy(M_md_dest, "</ul>");

            written += 5;
        }
    }

    *M_md_dest = EOS;

#ifdef DUMP
    log_long(dest, written, "silgy_render_md result");
#endif

    return dest;
}


/* --------------------------------------------------------------------------
   Encode string for JSON
-------------------------------------------------------------------------- */
char *silgy_json_enc(const char *src)
{
static char dst[JSON_BUFSIZE];
    int cnt=0;

    while ( *src && cnt < JSON_BUFSIZE-3 )
    {
        if ( *src=='\t' )
        {
            dst[cnt++] = '\\';
            dst[cnt++] = 't';
        }
        else if ( *src=='\n' )
        {
            dst[cnt++] = '\\';
            dst[cnt++] = 'n';
        }
        else if ( *src=='\r' )
        {
            /* ignore */
        }
        else
        {
            dst[cnt++] = *src;
        }

        ++src;
    }

    dst[cnt] = EOS;

    return dst;
}


/* --------------------------------------------------------------------------
   Verify CSRF token
-------------------------------------------------------------------------- */
bool lib_csrft_ok(int ci)
{
#ifndef SILGY_CLIENT

    QSVAL csrft;

    if ( !QS("csrft", csrft) ) return FALSE;

    if ( 0 != strcmp(csrft, US.csrft) ) return FALSE;

#endif  /* SILGY_CLIENT */

    return TRUE;
}


/* --------------------------------------------------------------------------
   Load error messages
-------------------------------------------------------------------------- */
static void load_err_messages()
{
    DBG("load_err_messages");

    silgy_add_message(OK,                        "EN-US", "OK");
    silgy_add_message(ERR_INVALID_REQUEST,       "EN-US", "Invalid HTTP request");
    silgy_add_message(ERR_UNAUTHORIZED,          "EN-US", "Unauthorized");
    silgy_add_message(ERR_FORBIDDEN,             "EN-US", "Forbidden");
    silgy_add_message(ERR_NOT_FOUND,             "EN-US", "Page not found");
    silgy_add_message(ERR_METHOD,                "EN-US", "Method not allowed");
    silgy_add_message(ERR_INT_SERVER_ERROR,      "EN-US", "Apologies, this is our fault. Please try again later.");
    silgy_add_message(ERR_SERVER_TOOBUSY,        "EN-US", "Apologies, we are experiencing very high demand right now, please try again in a few minutes.");
    silgy_add_message(ERR_FILE_TOO_BIG,          "EN-US", "File too big");
    silgy_add_message(ERR_REDIRECTION,           "EN-US", "Redirection required");
    silgy_add_message(ERR_ASYNC_NO_SUCH_SERVICE, "EN-US", "No such service");
    silgy_add_message(ERR_ASYNC_TIMEOUT,         "EN-US", "Asynchronous service timeout");
    silgy_add_message(ERR_REMOTE_CALL,           "EN-US", "Couldn't call the remote service");
    silgy_add_message(ERR_REMOTE_CALL_STATUS,    "EN-US", "Remote service call returned unsuccessful status");
    silgy_add_message(ERR_REMOTE_CALL_DATA,      "EN-US", "Data returned from the remote service is invalid");
    silgy_add_message(ERR_CSRFT,                 "EN-US", "Your previous session has expired. Please refresh this page before trying again.");
    silgy_add_message(ERR_RECORD_NOT_FOUND,      "EN-US", "Record not found");
}


/* --------------------------------------------------------------------------
   Add error message
-------------------------------------------------------------------------- */
void silgy_add_message(int code, const char *lang, const char *message, ...)
{
    if ( G_next_msg >= MAX_MESSAGES )
    {
        ERR("MAX_MESSAGES (%d) has been reached", MAX_MESSAGES);
        return;
    }

    va_list plist;
    char buffer[MAX_MSG_LEN+1];

    /* compile message with arguments into buffer */

    va_start(plist, message);
    vsprintf(buffer, message, plist);
    va_end(plist);

    G_messages[G_next_msg].code = code;
    if ( lang )
        strcpy(G_messages[G_next_msg].lang, upper(lang));
    strcpy(G_messages[G_next_msg].message, buffer);

    ++G_next_msg;

    /* in case message was added after init */

    if ( G_initialized )
        sort_messages();
}


/* --------------------------------------------------------------------------
   Comparing function for messages
---------------------------------------------------------------------------*/
int compare_messages(const void *a, const void *b)
{
    const message_t *p1 = (message_t*)a;
    const message_t *p2 = (message_t*)b;

    int res = strcmp(p1->lang, p2->lang);

    if ( res > 0 )
        return 1;
    else if ( res < 0 )
        return -1;

    /* same language then */

    if ( p1->code < p2->code )
        return -1;
    else if ( p1->code > p2->code )
        return 1;
    else
        return 0;
}


/* --------------------------------------------------------------------------
   Sort and index messages by languages
-------------------------------------------------------------------------- */
void sort_messages()
{
    qsort(&G_messages, G_next_msg, sizeof(message_t), compare_messages);

    int i;

    for ( i=0; i<G_next_msg; ++i )
    {
        if ( 0 != strcmp(G_messages[i].lang, G_msg_lang[G_next_msg_lang].lang) )
        {
            if ( G_next_msg_lang ) G_msg_lang[G_next_msg_lang-1].next_lang_index = i;

            strcpy(G_msg_lang[G_next_msg_lang].lang, G_messages[i].lang);
            G_msg_lang[G_next_msg_lang].first_index = i;
            ++G_next_msg_lang;
        }
    }

    G_msg_lang[G_next_msg_lang-1].next_lang_index = G_next_msg;
}


/* --------------------------------------------------------------------------
   Get error description for user in STRINGS_LANG
-------------------------------------------------------------------------- */
static char *lib_get_message_fallback(int code)
{
    int l, m;

    /* try in STRINGS_LANG */

    for ( l=0; l<G_next_msg_lang; ++l )   /* jump to the right language */
    {
        if ( 0==strcmp(G_msg_lang[l].lang, STRINGS_LANG) )
        {
            for ( m=G_msg_lang[l].first_index; m<G_msg_lang[l].next_lang_index; ++m )
                if ( G_messages[m].code == code )
                    return G_messages[m].message;
        }
    }

    /* try in any language */

    for ( m=0; m<G_next_msg; ++m )
        if ( G_messages[m].code == code )
            return G_messages[m].message;

    /* not found */

static char unknown[128];
    sprintf(unknown, "Unknown code: %d", code);
    return unknown;
}


/* --------------------------------------------------------------------------
   Get message category
-------------------------------------------------------------------------- */
static char *get_msg_cat(int code)
{
static char cat[64];

    if ( code == OK )
    {
        strcpy(cat, MSG_CAT_OK);
    }
    else if ( code < ERR_MAX_ENGINE_ERROR )
    {
        strcpy(cat, MSG_CAT_ERROR);
    }
#ifdef USERS
    else if ( code < ERR_MAX_USR_LOGIN_ERROR )
    {
        strcpy(cat, MSG_CAT_USR_LOGIN);
    }
    else if ( code < ERR_MAX_USR_EMAIL_ERROR )
    {
        strcpy(cat, MSG_CAT_USR_EMAIL);
    }
    else if ( code < ERR_MAX_USR_PASSWORD_ERROR )
    {
        strcpy(cat, MSG_CAT_USR_PASSWORD);
    }
    else if ( code < ERR_MAX_USR_REPEAT_PASSWORD_ERROR )
    {
        strcpy(cat, MSG_CAT_USR_REPEAT_PASSWORD);
    }
    else if ( code < ERR_MAX_USR_OLD_PASSWORD_ERROR )
    {
        strcpy(cat, MSG_CAT_USR_OLD_PASSWORD);
    }
    else if ( code < ERR_MAX_USR_ERROR )
    {
        strcpy(cat, MSG_CAT_ERROR);
    }
    else if ( code < WAR_MAX_USR_WARNING )
    {
        strcpy(cat, MSG_CAT_WARNING);
    }
    else if ( code < MSG_MAX_USR_MESSAGE )
    {
        strcpy(cat, MSG_CAT_MESSAGE);
    }
#endif  /* USERS */
    else    /* app error */
    {
        strcpy(cat, MSG_CAT_ERROR);
    }

    return cat;
}


/* --------------------------------------------------------------------------
   Message category test
   Only 3 main categories are recognized:
   error (red), warning (yellow) and message (green)
-------------------------------------------------------------------------- */
bool silgy_is_msg_main_cat(int code, const char *arg_cat)
{
    char cat[64];

    strcpy(cat, get_msg_cat(code));

    if ( 0==strcmp(cat, arg_cat) )
        return TRUE;

#ifdef USERS
    if ( 0==strcmp(arg_cat, MSG_CAT_ERROR) &&
            (0==strcmp(cat, MSG_CAT_USR_LOGIN) || 0==strcmp(cat, MSG_CAT_USR_EMAIL) || 0==strcmp(cat, MSG_CAT_USR_PASSWORD) || 0==strcmp(cat, MSG_CAT_USR_REPEAT_PASSWORD) || 0==strcmp(cat, MSG_CAT_USR_OLD_PASSWORD)) )
        return TRUE;
#endif

    return FALSE;
}


/* --------------------------------------------------------------------------
   Get error description for user
   Pick the user session language if possible
   TODO: binary search
-------------------------------------------------------------------------- */
char *lib_get_message(int ci, int code)
{
#ifndef SILGY_CLIENT

    if ( 0==strcmp(US.lang, STRINGS_LANG) )   /* no need to translate */
        return lib_get_message_fallback(code);

    if ( !US.lang[0] )   /* unknown client language */
        return lib_get_message_fallback(code);

    int l, m;

    for ( l=0; l<G_next_msg_lang; ++l )   /* jump to the right language */
    {
        if ( 0==strcmp(G_msg_lang[l].lang, US.lang) )
        {
            for ( m=G_msg_lang[l].first_index; m<G_msg_lang[l].next_lang_index; ++m )
                if ( G_messages[m].code == code )
                    return G_messages[m].message;
        }
    }

    /* if not found, ignore country code */

    for ( l=0; l<G_next_msg_lang; ++l )
    {
        if ( 0==strncmp(G_msg_lang[l].lang, US.lang, 2) )
        {
            for ( m=G_msg_lang[l].first_index; m<G_msg_lang[l].next_lang_index; ++m )
                if ( G_messages[m].code == code )
                    return G_messages[m].message;
        }
    }

    /* fallback */

    return lib_get_message_fallback(code);

#else   /* SILGY_CLIENT */

static char dummy[16];

    return dummy;

#endif  /* SILGY_CLIENT */
}


/* --------------------------------------------------------------------------
   Parse and set strings from data
-------------------------------------------------------------------------- */
static void parse_and_set_strings(const char *lang, const char *data)
{
    DBG("parse_and_set_strings, lang [%s]", lang);

    const char *p=data;
    int i, j=0;
    char string_orig[MAX_STR_LEN+1];
    char string_in_lang[MAX_STR_LEN+1];
    bool now_key=1, now_val=0, now_com=0;

    if ( G_next_str_lang >= MAX_LANGUAGES )
    {
        ERR("MAX_LANGUAGES (%d) has been reached", MAX_LANGUAGES);
        return;
    }

    strcpy(G_str_lang[G_next_str_lang].lang, upper(lang));
    G_str_lang[G_next_str_lang].first_index = G_next_str;

    while ( *p )
    {
        if ( *p=='#' )   /* comment */
        {
            now_key = 0;
            now_com = 1;

            if ( now_val )
            {
                now_val = 0;
                string_in_lang[j] = EOS;
                silgy_add_string(lang, string_orig, string_in_lang);
            }
        }
        else if ( now_key && *p==STRINGS_SEP )   /* separator */
        {
            now_key = 0;
            now_val = 1;
            string_orig[j] = EOS;
            j = 0;
        }
        else if ( *p=='\r' || *p=='\n' )
        {
            if ( now_val )
            {
                now_val = 0;
                string_in_lang[j] = EOS;
                silgy_add_string(lang, string_orig, string_in_lang);
            }
            else if ( now_com )
            {
                now_com = 0;
            }

            j = 0;

            now_key = 1;
        }
        else if ( now_key && *p!='\n' )
        {
            string_orig[j++] = *p;
        }
        else if ( now_val )
        {
            string_in_lang[j++] = *p;
        }

        ++p;
    }

    if ( now_val )
    {
        string_in_lang[j] = EOS;
        silgy_add_string(lang, string_orig, string_in_lang);
    }

    G_str_lang[G_next_str_lang].next_lang_index = G_next_str;
    ++G_next_str_lang;
}


/* --------------------------------------------------------------------------
   Load strings
-------------------------------------------------------------------------- */
static void load_strings()
{
    int     i, len;
    char    bindir[STATIC_PATH_LEN];        /* full path to bin */
    char    namewpath[STATIC_PATH_LEN];     /* full path including file name */
    DIR     *dir;
    struct dirent *dirent;
    FILE    *fd;
    char    *data=NULL;
    char    lang[8];

    DBG("load_strings");

    if ( G_appdir[0] == EOS ) return;

    sprintf(bindir, "%s/bin", G_appdir);

    if ( (dir=opendir(bindir)) == NULL )
    {
        DBG("Couldn't open directory [%s]", bindir);
        return;
    }

    while ( (dirent=readdir(dir)) )
    {
        if ( 0 != strncmp(dirent->d_name, "strings.", 8) )
            continue;

        sprintf(namewpath, "%s/%s", bindir, dirent->d_name);

        DBG("namewpath [%s]", namewpath);

#ifdef _WIN32   /* Windows */
        if ( NULL == (fd=fopen(namewpath, "rb")) )
#else
        if ( NULL == (fd=fopen(namewpath, "r")) )
#endif  /* _WIN32 */
            ERR("Couldn't open %s", namewpath);
        else
        {
            fseek(fd, 0, SEEK_END);     /* determine the file size */
            len = ftell(fd);
            rewind(fd);

            if ( NULL == (data=(char*)malloc(len+1)) )
            {
                ERR("Couldn't allocate %d bytes for %s", len, dirent->d_name);
                fclose(fd);
                closedir(dir);
                return;
            }

            fread(data, len, 1, fd);
            fclose(fd);
            *(data+len) = EOS;

            parse_and_set_strings(get_file_ext(dirent->d_name), data);

            free(data);
            data = NULL;
        }
    }

    closedir(dir);
}


/* --------------------------------------------------------------------------
   Add string
-------------------------------------------------------------------------- */
void silgy_add_string(const char *lang, const char *str, const char *str_lang)
{
    if ( G_next_str >= MAX_STRINGS )
    {
        ERR("MAX_STRINGS (%d) has been reached", MAX_STRINGS);
        return;
    }

    strcpy(G_strings[G_next_str].lang, upper(lang));
//    strcpy(G_strings[G_next_str].string_orig, str);
    strcpy(G_strings[G_next_str].string_upper, upper(str));
    strcpy(G_strings[G_next_str].string_in_lang, str_lang);

    ++G_next_str;
}


/* --------------------------------------------------------------------------
   Get a string
   Pick the user session language if possible
   If not, return given string
   TODO: binary search
-------------------------------------------------------------------------- */
const char *lib_get_string(int ci, const char *str)
{
#ifndef SILGY_CLIENT

    if ( 0==strcmp(US.lang, STRINGS_LANG) )   /* no need to translate */
        return str;

    if ( !US.lang[0] )   /* unknown client language */
        return str;

    char str_upper[MAX_STR_LEN+1];

    strcpy(str_upper, upper(str));

    int l, s;

    for ( l=0; l<G_next_str_lang; ++l )   /* jump to the right language */
    {
        if ( 0==strcmp(G_str_lang[l].lang, US.lang) )
        {
            for ( s=G_str_lang[l].first_index; s<G_str_lang[l].next_lang_index; ++s )
                if ( 0==strcmp(G_strings[s].string_upper, str_upper) )
                    return G_strings[s].string_in_lang;

            /* language found but not this string */
            return str;
        }
    }

    /* if not found, ignore country code */

    for ( l=0; l<G_next_str_lang; ++l )
    {
        if ( 0==strncmp(G_str_lang[l].lang, US.lang, 2) )
        {
            for ( s=G_str_lang[l].first_index; s<G_str_lang[l].next_lang_index; ++s )
                if ( 0==strcmp(G_strings[s].string_upper, str_upper) )
                    return G_strings[s].string_in_lang;
        }
    }

    /* fallback */

    return str;

#else   /* SILGY_CLIENT */

static char dummy[16];

    return dummy;

#endif  /* SILGY_CLIENT */
}


/* --------------------------------------------------------------------------
   URI encoding
---------------------------------------------------------------------------*/
char *urlencode(const char *src)
{
static char     dest[4096];
    int         i, j=0;
    const char  hex[]="0123456789ABCDEF";

    for ( i=0; src[i] && j<4092; ++i )
    {
        if ( (48 <= src[i] && src[i] <= 57) || (65 <= src[i] && src[i] <= 90) || (97 <= src[i] && src[i] <= 122)
                || src[i]=='-' || src[i]=='_' || src[i]=='.' || src[i]=='~' )
        {
            dest[j++] = src[i];
        }
        else
        {
            dest[j++] = '%';
            dest[j++] = hex[(unsigned char)(src[i]) >> 4];
            dest[j++] = hex[(unsigned char)(src[i]) & 15];
        }
    }

    dest[j] = EOS;

    return dest;
}


/* --------------------------------------------------------------------------
   Open database connection
-------------------------------------------------------------------------- */
bool lib_open_db()
{
#ifdef DBMYSQL
    if ( !G_dbName[0] )
    {
        ERR("dbName parameter is required in silgy.conf");
        return FALSE;
    }

    if ( NULL == (G_dbconn=mysql_init(NULL)) )
    {
        ERR("%u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
        return FALSE;
    }

#ifdef DBMYSQLRECONNECT
    my_bool reconnect=1;
    mysql_options(G_dbconn, MYSQL_OPT_RECONNECT, &reconnect);
#endif

//    unsigned long max_packet=33554432;  /* 32 MB */
//    mysql_options(G_dbconn, MYSQL_OPT_MAX_ALLOWED_PACKET, &max_packet);

    if ( NULL == mysql_real_connect(G_dbconn, G_dbHost[0]?G_dbHost:NULL, G_dbUser, G_dbPassword, G_dbName, G_dbPort, NULL, 0) )
    {
        ERR("%u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
        return FALSE;
    }
#endif
    return TRUE;
}


/* --------------------------------------------------------------------------
   Close database connection
-------------------------------------------------------------------------- */
void lib_close_db()
{
#ifdef DBMYSQL
    if ( G_dbconn )
        mysql_close(G_dbconn);
#endif
}


/* --------------------------------------------------------------------------
   Return TRUE if file exists and it's readable
-------------------------------------------------------------------------- */
bool lib_file_exists(const char *fname)
{
    FILE *f=NULL;

    if ( NULL != (f=fopen(fname, "r")) )
    {
        fclose(f);
        return TRUE;
    }

    return FALSE;
}


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


/* --------------------------------------------------------------------------
   Find first free slot in G_snippets
-------------------------------------------------------------------------- */
static int first_free_snippet()
{
    int i=0;

    for ( i=0; i<MAX_SNIPPETS; ++i )
    {
        if ( G_snippets[i].name[0]=='-' || G_snippets[i].name[0]==EOS )
        {
            if ( i > G_snippets_cnt ) G_snippets_cnt = i;
            return i;
        }
    }

    ERR("MAX_SNIPPETS reached (%d)! You can set/increase MAX_SNIPPETS in silgy_app.h.", MAX_SNIPPETS);

    return -1;   /* nothing's free, we ran out of snippets! */
}


/* --------------------------------------------------------------------------
   Read snippets from disk
-------------------------------------------------------------------------- */
bool read_snippets(bool first_scan, const char *path)
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
    struct stat fstat;
    char    mod_time[32];

#ifndef _WIN32
    if ( G_appdir[0] == EOS ) return TRUE;
#endif

    if ( first_scan && !path ) DBG("");

#ifdef DUMP
    if ( first_scan )
    {
        if ( !path ) DBG_LINE_LONG;
        DBG("read_snippets");
    }
#endif

#ifdef _WIN32   /* be more forgiving */

    if ( G_appdir[0] )
    {
        sprintf(resdir, "%s/snippets", G_appdir);
    }
    else    /* no SILGYDIR */
    {
        sprintf(resdir, "../snippets");
    }

#else   /* Linux -- don't fool around */

    sprintf(resdir, "%s/snippets", G_appdir);

#endif  /* _WIN32 */

#ifdef DUMP
    if ( first_scan )
        DBG("resdir [%s]", resdir);
#endif

    if ( !path )   /* highest level */
    {
        strcpy(ressubdir, resdir);
    }
    else    /* recursive call */
    {
        sprintf(ressubdir, "%s/%s", resdir, path);
    }

#ifdef DUMP
    if ( first_scan )
        DBG("ressubdir [%s]", ressubdir);
#endif

    if ( (dir=opendir(ressubdir)) == NULL )
    {
        if ( first_scan )
            DBG("Couldn't open directory [%s]", ressubdir);
        return TRUE;    /* don't panic, just no snippets will be used */
    }

    /* ------------------------------------------------------------------- */
    /* check removed files */

    if ( !first_scan && !path )   /* on the highest level only */
    {
#ifdef DUMP
//        DBG("Checking removed files...");
#endif
        for ( i=0; i<=G_snippets_cnt; ++i )
        {
            if ( G_snippets[i].name[0]==EOS ) continue;   /* already removed */
#ifdef DUMP
//            DBG("Checking %s...", G_snippets[i].name);
#endif
            char fullpath[STATIC_PATH_LEN];
            sprintf(fullpath, "%s/%s", resdir, G_snippets[i].name);

            if ( !lib_file_exists(fullpath) )
            {
                INF("Removing %s from snippets", G_snippets[i].name);

                G_snippets[i].name[0] = EOS;

                free(G_snippets[i].data);
                G_snippets[i].data = NULL;
                G_snippets[i].len = 0;
            }
        }
    }

    /* ------------------------------------------------------------------- */
#ifdef DUMP
//    DBG("Reading %sfiles", first_scan?"":"new ");
#endif
    /* read the files into the memory */

    while ( (dirent=readdir(dir)) )
    {
        if ( dirent->d_name[0] == '.' )   /* skip ".", ".." and hidden files */
            continue;

        /* ------------------------------------------------------------------- */
        /* resource name */

        if ( !path )
            strcpy(resname, dirent->d_name);
        else
            sprintf(resname, "%s/%s", path, dirent->d_name);

#ifdef DUMP
        if ( first_scan )
            DBG("resname [%s]", resname);
#endif

        /* ------------------------------------------------------------------- */
        /* additional file info */

        sprintf(namewpath, "%s/%s", resdir, resname);

#ifdef DUMP
        if ( first_scan )
            DBG("namewpath [%s]", namewpath);
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
            if ( first_scan )
                DBG("Reading subdirectory [%s]...", dirent->d_name);
#endif
            read_snippets(first_scan, resname);
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
            bool exists_not_changed = FALSE;

            for ( i=0; i<=G_snippets_cnt; ++i )
            {
                if ( G_snippets[i].name[0]==EOS ) continue;   /* removed */

                /* ------------------------------------------------------------------- */

                if ( 0==strcmp(G_snippets[i].name, resname) )
                {
#ifdef DUMP
//                    DBG("%s already read", resname);
#endif
                    if ( G_snippets[i].modified == fstat.st_mtime )
                    {
#ifdef DUMP
//                        DBG("Not modified");
#endif
                        exists_not_changed = TRUE;
                    }
                    else
                    {
                        INF("%s has been modified", resname);
                        reread = TRUE;
                    }

                    break;
                }
            }

            if ( exists_not_changed ) continue;   /* not modified */
        }

        /* find the first unused slot in G_snippets array */

        if ( !reread )
        {
            i = first_free_snippet();
            /* file name */
            strcpy(G_snippets[i].name, resname);
        }

        /* last modified */

        G_snippets[i].modified = fstat.st_mtime;

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
            G_snippets[i].len = ftell(fd);
            rewind(fd);

            /* allocate the final destination */

            if ( reread )
            {
                free(G_snippets[i].data);
                G_snippets[i].data = NULL;
            }

            G_snippets[i].data = (char*)malloc(G_snippets[i].len+1);

            if ( NULL == G_snippets[i].data )
            {
                ERR("Couldn't allocate %u bytes for %s", G_snippets[i].len+1, G_snippets[i].name);
                fclose(fd);
                closedir(dir);
                return FALSE;
            }

            fread(G_snippets[i].data, G_snippets[i].len, 1, fd);

            fclose(fd);

            /* log file info ----------------------------------- */

            if ( G_logLevel > LOG_INF )
            {
                G_ptm = gmtime(&G_snippets[i].modified);
                sprintf(mod_time, "%d-%02d-%02d %02d:%02d:%02d", G_ptm->tm_year+1900, G_ptm->tm_mon+1, G_ptm->tm_mday, G_ptm->tm_hour, G_ptm->tm_min, G_ptm->tm_sec);
                G_ptm = gmtime(&G_now);     /* set it back */
                DBG("%s %s\t\t%u bytes", lib_add_spaces(G_snippets[i].name, 28), mod_time, G_snippets[i].len);
            }
        }
    }

    closedir(dir);

    if ( first_scan && !path ) DBG("");

    return TRUE;
}


/* --------------------------------------------------------------------------
   Get snippet
-------------------------------------------------------------------------- */
char *silgy_get_snippet(const char *name)
{
#ifndef SILGY_CLIENT
    int i;

    for ( i=0; G_snippets[i].name[0] != '-'; ++i )
    {
        if ( 0==strcmp(G_snippets[i].name, name) )
            return G_snippets[i].data;
    }
#endif  /* SILGY_CLIENT */
    return NULL;
}


/* --------------------------------------------------------------------------
   Get snippet length
-------------------------------------------------------------------------- */
unsigned silgy_get_snippet_len(const char *name)
{
#ifndef SILGY_CLIENT
    int i;

    for ( i=0; G_snippets[i].name[0] != '-'; ++i )
    {
        if ( 0==strcmp(G_snippets[i].name, name) )
            return G_snippets[i].len;
    }
#endif  /* SILGY_CLIENT */
    return 0;
}


/* --------------------------------------------------------------------------
   OUT snippet
-------------------------------------------------------------------------- */
void lib_out_snippet(int ci, const char *name)
{
#ifndef SILGY_CLIENT
    int i;

    for ( i=0; G_snippets[i].name[0] != '-'; ++i )
    {
        if ( 0==strcmp(G_snippets[i].name, name) )
        {
            OUT_BIN(G_snippets[i].data, G_snippets[i].len);
            break;
        }
    }
#endif  /* SILGY_CLIENT */
}


/* --------------------------------------------------------------------------
   OUT markdown snippet
-------------------------------------------------------------------------- */
void lib_out_snippet_md(int ci, const char *name)
{
#ifndef SILGY_CLIENT
    int i;

    for ( i=0; G_snippets[i].name[0] != '-'; ++i )
    {
        if ( 0==strcmp(G_snippets[i].name, name) )
        {
            silgy_render_md(G_tmp, G_snippets[i].data, TMP_BUFSIZE-1);
            OUT(G_tmp);
            break;
        }
    }
#endif  /* SILGY_CLIENT */
}



#ifdef HTTPS
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
#endif  /* HTTPS */


/* --------------------------------------------------------------------------
   Init SSL for a client
-------------------------------------------------------------------------- */
static bool init_ssl_client()
{
#ifdef HTTPS
    const SSL_METHOD *method;

    DBG("init_ssl (silgy_lib)");

    if ( !G_ssl_lib_initialized )
    {
        DBG("Initializing SSL_lib...");

        /* libssl init */
        SSL_library_init();
        SSL_load_error_strings();

        /* libcrypto init */
        OpenSSL_add_all_algorithms();
        ERR_load_crypto_strings();

        G_ssl_lib_initialized = TRUE;
    }

    method = SSLv23_client_method();    /* negotiate the highest protocol version supported by both the server and the client */

    M_ssl_ctx = SSL_CTX_new(method);    /* create new context from method */

    if ( M_ssl_ctx == NULL )
    {
        ERR("SSL_CTX_new failed");
        return FALSE;
    }

//    const long flags = SSL_OP_ALL | SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_COMPRESSION;
    const long flags = SSL_OP_ALL | SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3;
    SSL_CTX_set_options(M_ssl_ctx, flags);

    /* temporarily ignore server cert errors */

    WAR("Ignoring remote server cert errors for REST calls");
//    SSL_CTX_set_verify(M_ssl_ctx, SSL_VERIFY_NONE, NULL);

#endif  /* HTTPS */
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


#ifndef SILGY_CLIENT
/* --------------------------------------------------------------------------
   Output standard HTML header
-------------------------------------------------------------------------- */
void lib_out_html_header(int ci)
{
    OUT("<!DOCTYPE html>");
    OUT("<html>");
    OUT("<head>");
    OUT("<title>%s</title>", APP_WEBSITE);
#ifdef APP_DESCRIPTION
    OUT("<meta name=\"description\" content=\"%s\">", APP_DESCRIPTION);
#endif
#ifdef APP_KEYWORDS
    OUT("<meta name=\"keywords\" content=\"%s\">", APP_KEYWORDS);
#endif
    if ( REQ_MOB )  // if mobile request
        OUT("<meta name=\"viewport\" content=\"width=device-width\">");
    OUT("</head>");
    OUT("<body>");
}


/* --------------------------------------------------------------------------
   Output standard HTML footer
-------------------------------------------------------------------------- */
void lib_out_html_footer(int ci)
{
    OUT("</body>");
    OUT("</html>");
}


/* --------------------------------------------------------------------------
   Add CSS link to HTML head
-------------------------------------------------------------------------- */
void lib_append_css(int ci, const char *fname, bool first)
{
    if ( first )
    {
        DBG("first = TRUE; Defining ldlink()");
        OUT("function ldlink(n){var f=document.createElement('link');f.setAttribute(\"rel\",\"stylesheet\");f.setAttribute(\"type\",\"text/css\");f.setAttribute(\"href\",n);document.getElementsByTagName(\"head\")[0].appendChild(f);}");
    }
    OUT("ldlink('%s');", fname);
}


/* --------------------------------------------------------------------------
   Add script to HTML head
-------------------------------------------------------------------------- */
void lib_append_script(int ci, const char *fname, bool first)
{
    if ( first )
    {
        DBG("first = TRUE; Defining ldscript()");
        OUT("function ldscript(n){var f=document.createElement('script');f.setAttribute(\"type\",\"text/javascript\");f.setAttribute(\"src\",n);document.getElementsByTagName(\"head\")[0].appendChild(f);}");
    }
    OUT("ldscript('%s');", fname);
}
#endif  /* SILGY_CLIENT */


/* --------------------------------------------------------------------------
   URI-decode character
-------------------------------------------------------------------------- */
static int xctod(int c)
{
    if ( isdigit(c) )
        return c - '0';
    else if ( isupper(c) )
        return c - 'A' + 10;
    else if ( islower(c) )
        return c - 'a' + 10;
    else
        return 0;
}


/* --------------------------------------------------------------------------
   URI-decode src
-------------------------------------------------------------------------- */
char *uri_decode(char *src, int srclen, char *dest, int maxlen)
{
    char *endp=src+srclen;
    char *srcp;
    char *destp=dest;
    int  written=0;

    for ( srcp=src; srcp<endp; ++srcp )
    {
        if ( *srcp == '+' )
            *destp++ = ' ';
        else if ( *srcp == '%' )
        {
            *destp++ = 16 * xctod(*(srcp+1)) + xctod(*(srcp+2));
            srcp += 2;
        }
        else
            *destp++ = *srcp;

        ++written;

        if ( written == maxlen )
        {
            WAR("URI val truncated");
            break;
        }
    }

    *destp = EOS;

    G_qs_len = written;

    return dest;
}


#ifndef SILGY_CLIENT
/* --------------------------------------------------------------------------
   Get incoming request data. TRUE if found.
-------------------------------------------------------------------------- */
bool get_qs_param(int ci, const char *fieldname, char *retbuf, int maxlen, char esc_type)
{
static char interbuf[65536];

    if ( conn[ci].in_ctype == CONTENT_TYPE_URLENCODED )
    {
static char rawbuf[196608];    /* URL-encoded can have up to 3 times bytes count */

        if ( !get_qs_param_raw(ci, fieldname, rawbuf, maxlen*3-1) )
        {
            if ( retbuf ) retbuf[0] = EOS;
            return FALSE;
        }

        if ( retbuf )
            uri_decode(rawbuf, G_qs_len, interbuf, maxlen);
    }
    else    /* usually JSON or multipart */
    {
        if ( !get_qs_param_raw(ci, fieldname, interbuf, maxlen) )
        {
            if ( retbuf ) retbuf[0] = EOS;
            return FALSE;
        }
    }

    /* now we have URI-decoded string in interbuf */

    if ( retbuf )
    {
        if ( esc_type == ESC_HTML )
            sanitize_html(retbuf, interbuf, maxlen);
        else if ( esc_type == ESC_SQL )
            sanitize_sql(retbuf, interbuf, maxlen);
        else
        {
            strncpy(retbuf, interbuf, maxlen);
            retbuf[maxlen] = EOS;
        }
    }

    return TRUE;
}


/* --------------------------------------------------------------------------
   Get the incoming param if Content-Type == JSON
-------------------------------------------------------------------------- */
static bool get_qs_param_json(int ci, const char *fieldname, char *retbuf, int maxlen)
{
static int prev_ci=-1;
static unsigned prev_req;
static JSON req={0};

    /* parse JSON only once per request */

    if ( ci != prev_ci || G_cnts_today.req != prev_req )
    {
        if ( !REQ_DATA )
            return FALSE;

        if ( !JSON_FROM_STRING(req, REQ_DATA) )
            return FALSE;

        prev_ci = ci;
        prev_req = G_cnts_today.req;
    }

    if ( !JSON_PRESENT(req, fieldname) )
        return FALSE;

    strncpy(retbuf, JSON_GET_STR(req, fieldname), maxlen);
    retbuf[maxlen] = EOS;

    return TRUE;
}


/* --------------------------------------------------------------------------
   Get text value from multipart-form-data
-------------------------------------------------------------------------- */
static bool get_qs_param_multipart_txt(int ci, const char *fieldname, char *retbuf, int maxlen)
{
    char     *p;
    unsigned len;

    p = get_qs_param_multipart(ci, fieldname, &len, NULL);

    if ( !p ) return FALSE;

//    if ( len > MAX_URI_VAL_LEN ) return FALSE;

#ifdef DUMP
    DBG("len = %d", len);
#endif

    if ( len > maxlen )
    {
        len = maxlen;
#ifdef DUMP
        DBG("len reduced to %d", len);
#endif
    }

    strncpy(retbuf, p, len);
    retbuf[len] = EOS;

    return TRUE;
}


/* --------------------------------------------------------------------------
   Get the query string value. Return TRUE if found.
-------------------------------------------------------------------------- */
bool get_qs_param_raw(int ci, const char *fieldname, char *retbuf, int maxlen)
{
    char *qs, *end;

    G_qs_len = 0;

#ifdef DUMP
    DBG("get_qs_param_raw: fieldname [%s]", fieldname);
#endif

    if ( conn[ci].post )
    {
        if ( conn[ci].in_ctype == CONTENT_TYPE_JSON )
        {
            return get_qs_param_json(ci, fieldname, retbuf, maxlen);
        }
        else if ( conn[ci].in_ctype == CONTENT_TYPE_MULTIPART )
        {
            return get_qs_param_multipart_txt(ci, fieldname, retbuf, maxlen);
        }
        else if ( conn[ci].in_ctype != CONTENT_TYPE_URLENCODED && conn[ci].in_ctype != CONTENT_TYPE_UNSET )
        {
            WAR("Invalid Content-Type");
            if ( retbuf ) retbuf[0] = EOS;
            return FALSE;
        }
        qs = conn[ci].in_data;
        end = qs + conn[ci].clen;
    }
    else    /* GET */
    {
        qs = strchr(conn[ci].uri, '?');
    }

    if ( qs == NULL )
    {
        if ( retbuf ) retbuf[0] = EOS;
        return FALSE;
    }

    if ( !conn[ci].post )   /* GET */
    {
        ++qs;      /* skip the question mark */
        end = qs + (strlen(conn[ci].uri) - (qs-conn[ci].uri));
#ifdef DUMP
        DBG("get_qs_param_raw: qs len = %d", strlen(conn[ci].uri) - (qs-conn[ci].uri));
#endif
    }

    int fnamelen = strlen(fieldname);

    if ( fnamelen < 1 )
    {
        if ( retbuf ) retbuf[0] = EOS;
        return FALSE;
    }

    char *val = qs;

    bool found=FALSE;

    while ( val < end )
    {
        val = strstr(val, fieldname);

        if ( val == NULL )
        {
            if ( retbuf ) retbuf[0] = EOS;
            return FALSE;
        }

        if ( val != qs && *(val-1) != '&' )
        {
            ++val;
            continue;
        }

        val += fnamelen;

        if ( *val == '=' )   /* found */
        {
            found = TRUE;
            break;
        }
    }

    if ( !found )
    {
        if ( retbuf ) retbuf[0] = EOS;
        return FALSE;
    }

    ++val;  /* skip '=' */

    /* copy the value */

    int i=0;

    while ( *val && *val != '&' && i<maxlen )
        retbuf[i++] = *val++;

    retbuf[i] = EOS;

#ifdef DUMP
    log_long(retbuf, i, "get_qs_param_raw: retbuf");
#endif

    G_qs_len = i;

    return TRUE;
}


/* --------------------------------------------------------------------------
   multipart-form-data receipt
   Return length or -1 if error
   If retfname is not NULL then assume binary data and it must be the last
   data element
-------------------------------------------------------------------------- */
char *get_qs_param_multipart(int ci, const char *fieldname, unsigned *retlen, char *retfname)
{
    unsigned blen;           /* boundary length */
    char     *cp;            /* current pointer */
    char     *p;             /* tmp pointer */
    unsigned b;              /* tmp bytes count */
    char     fn[MAX_LABEL_LEN+1];    /* field name */
    char     *end;
    unsigned len;

    /* Couple of checks to make sure it's properly formatted multipart content */

    if ( conn[ci].in_ctype != CONTENT_TYPE_MULTIPART )
    {
        WAR("This is not multipart/form-data");
        return NULL;
    }

    if ( !conn[ci].in_data )
        return NULL;

    if ( conn[ci].clen < 10 )
    {
        WAR("Content length seems to be too small for multipart (%u)", conn[ci].clen);
        return NULL;
    }

    cp = conn[ci].in_data;

    if ( !conn[ci].boundary[0] )    /* find first end of line -- that would be end of boundary */
    {
        if ( NULL == (p=strchr(cp, '\n')) )
        {
            WAR("Request syntax error");
            return NULL;
        }

        b = p - cp - 2;     /* skip -- */

        if ( b < 2 )
        {
            WAR("Boundary appears to be too short (%u)", b);
            return NULL;
        }
        else if ( b > 255 )
        {
            WAR("Boundary appears to be too long (%u)", b);
            return NULL;
        }

        strncpy(conn[ci].boundary, cp+2, b);
        if ( conn[ci].boundary[b-1] == '\r' )
            conn[ci].boundary[b-1] = EOS;
        else
            conn[ci].boundary[b] = EOS;
    }

    blen = strlen(conn[ci].boundary);

    if ( conn[ci].in_data[conn[ci].clen-4] != '-' || conn[ci].in_data[conn[ci].clen-3] != '-' )
    {
        WAR("Content doesn't end with '--'");
        return NULL;
    }

    while (TRUE)    /* find the right section */
    {
        if ( NULL == (p=strstr(cp, conn[ci].boundary)) )
        {
            WAR("No (next) boundary found");
            return NULL;
        }

        b = p - cp + blen;
        cp += b;

        if ( NULL == (p=strstr(cp, "Content-Disposition: form-data;")) )
        {
            WAR("No Content-Disposition label");
            return NULL;
        }

        b = p - cp + 30;
        cp += b;

        if ( NULL == (p=strstr(cp, "name=\"")) )
        {
            WAR("No field name");
            return NULL;
        }

        b = p - cp + 6;
        cp += b;

//      DBG("field name starts from: [%s]", cp);

        if ( NULL == (p=strchr(cp, '"')) )
        {
            WAR("No field name closing quote");
            return NULL;
        }

        b = p - cp;

        if ( b > MAX_LABEL_LEN )
        {
            WAR("Field name too long (%u)", b);
            return NULL;
        }

        strncpy(fn, cp, b);
        fn[b] = EOS;

//      DBG("fn: [%s]", fn);

        if ( 0==strcmp(fn, fieldname) )     /* found */
            break;

        cp += b;
    }

    /* find a file name */

    if ( retfname )
    {
        if ( NULL == (p=strstr(cp, "filename=\"")) )
        {
            WAR("No file name");
            return NULL;
        }

        b = p - cp + 10;
        cp += b;

    //  DBG("file name starts from: [%s]", cp);

        if ( NULL == (p=strchr(cp, '"')) )
        {
            WAR("No file name closing quote");
            return NULL;
        }

        b = p - cp;

        if ( b > MAX_URI_VAL_LEN )
        {
            WAR("File name too long (%u)", b);
            return NULL;
        }

        strncpy(fn, cp, b);
        fn[b] = EOS;        /* fn now contains file name */

        cp += b;
    }

    /* now look for the section header end where the actual data begins */

    if ( NULL == (p=strstr(cp, "\r\n\r\n")) )
    {
        WAR("No section header end");
        return NULL;
    }

    b = p - cp + 4;
    cp += b;        /* cp now points to the actual data */

    /* find out data length */

    if ( !retfname )    /* text */
    {
        if ( NULL == (end=strstr(cp, conn[ci].boundary)) )
        {
            WAR("No closing boundary found");
            return NULL;
        }

        len = end - cp - 4;     /* minus CRLF-- */
    }
    else    /* potentially binary content -- calculate rather than use strstr */
    {
        len = conn[ci].clen - (cp - conn[ci].in_data) - blen - 8;  /* fast version */
                                                                /* Note that the file content must come as last! */
    }

    if ( len < 0 )
    {
        WAR("Ooops, something went terribly wrong! Data length = %u", len);
        return NULL;
    }

    /* everything looks good so far */

    *retlen = len;

    if ( retfname )
        strcpy(retfname, fn);

    return cp;
}


/* --------------------------------------------------------------------------
   Get integer value from the query string
-------------------------------------------------------------------------- */
bool lib_qsi(int ci, const char *fieldname, int *retbuf)
{
    QSVAL s;

    if ( get_qs_param_raw(ci, fieldname, s, MAX_URI_VAL_LEN) )
    {
        if ( retbuf )
            *retbuf = atoi(s);

        return TRUE;
    }

    return FALSE;
}


/* --------------------------------------------------------------------------
   Get unsigned value from the query string
-------------------------------------------------------------------------- */
bool lib_qsu(int ci, const char *fieldname, unsigned *retbuf)
{
    QSVAL s;

    if ( get_qs_param_raw(ci, fieldname, s, MAX_URI_VAL_LEN) )
    {
        if ( retbuf )
            *retbuf = (unsigned)strtoul(s, NULL, 10);

        return TRUE;
    }

    return FALSE;
}


/* --------------------------------------------------------------------------
   Get float value from the query string
-------------------------------------------------------------------------- */
bool lib_qsf(int ci, const char *fieldname, float *retbuf)
{
    QSVAL s;

    if ( get_qs_param_raw(ci, fieldname, s, MAX_URI_VAL_LEN) )
    {
        if ( retbuf )
            sscanf(s, "%f", retbuf);

        return TRUE;
    }

    return FALSE;
}


/* --------------------------------------------------------------------------
   Get double value from the query string
-------------------------------------------------------------------------- */
bool lib_qsd(int ci, const char *fieldname, double *retbuf)
{
    QSVAL s;

    if ( get_qs_param_raw(ci, fieldname, s, MAX_URI_VAL_LEN) )
    {
        if ( retbuf )
            sscanf(s, "%lf", retbuf);

        return TRUE;
    }

    return FALSE;
}


/* --------------------------------------------------------------------------
   Get bool value from the query string
-------------------------------------------------------------------------- */
bool lib_qsb(int ci, const char *fieldname, bool *retbuf)
{
    QSVAL s;

    if ( get_qs_param_raw(ci, fieldname, s, MAX_URI_VAL_LEN) )
    {
        if ( retbuf )
        {
            if ( s[0] == 't'
                    || s[0] == 'T'
                    || s[0] == '1'
                    || 0==strcmp(s, "on") )
                *retbuf = true;
            else
                *retbuf = false;
        }

        return TRUE;
    }

    return FALSE;
}


/* --------------------------------------------------------------------------
   Set response status
-------------------------------------------------------------------------- */
void lib_set_res_status(int ci, int status)
{
    conn[ci].status = status;
}


/* --------------------------------------------------------------------------
   Set custom header
-------------------------------------------------------------------------- */
bool lib_res_header(int ci, const char *hdr, const char *val)
{
    int hlen = strlen(hdr);
    int vlen = strlen(val);
    int all = hlen + vlen + 4;

    if ( all > CUST_HDR_LEN - conn[ci].cust_headers_len )
    {
        WAR("Couldn't add %s to custom headers: no space", hdr);
        return FALSE;
    }

    strcat(conn[ci].cust_headers, hdr);
    strcat(conn[ci].cust_headers, ": ");
    strcat(conn[ci].cust_headers, val);
    strcat(conn[ci].cust_headers, "\r\n");

    conn[ci].cust_headers_len += all;

    return TRUE;
}


/* --------------------------------------------------------------------------
   Get request cookie
-------------------------------------------------------------------------- */
bool lib_get_cookie(int ci, const char *key, char *value)
{
    char nkey[128];
    char *v;

    sprintf(nkey, "%s=", key);

    v = strstr(conn[ci].in_cookie, nkey);

    if ( !v ) return FALSE;

    /* key present */

    if ( value )
    {
        int j=0;

        while ( *v!='=' ) ++v;

        ++v;    /* skip '=' */

        while ( *v && *v!=';' )
            value[j++] = *(v++);

        value[j] = EOS;
    }

    return TRUE;
}


/* --------------------------------------------------------------------------
   Set cookie
-------------------------------------------------------------------------- */
bool lib_set_cookie(int ci, const char *key, const char *value, int days)
{
    char v[CUST_HDR_LEN+1];

    if ( days )
        sprintf(v, "%s=%s; Expires=%s;", key, value, time_epoch2http(G_now + 3600*24*days));
    else    /* current session only */
        sprintf(v, "%s=%s", key, value);

    return lib_res_header(ci, "Set-Cookie", v);
}


/* --------------------------------------------------------------------------
   Set response content type
   Mirrored print_content_type
-------------------------------------------------------------------------- */
void lib_set_res_content_type(int ci, const char *str)
{
    if ( 0==strcmp(str, "text/html; charset=utf-8") )
        conn[ci].ctype = RES_HTML;
    else if ( 0==strcmp(str, "text/plain") )
        conn[ci].ctype = RES_TEXT;
    else if ( 0==strcmp(str, "text/css") )
        conn[ci].ctype = RES_CSS;
    else if ( 0==strcmp(str, "application/javascript") )
        conn[ci].ctype = RES_JS;
    else if ( 0==strcmp(str, "image/gif") )
        conn[ci].ctype = RES_GIF;
    else if ( 0==strcmp(str, "image/jpeg") )
        conn[ci].ctype = RES_JPG;
    else if ( 0==strcmp(str, "image/x-icon") )
        conn[ci].ctype = RES_ICO;
    else if ( 0==strcmp(str, "image/png") )
        conn[ci].ctype = RES_PNG;
    else if ( 0==strcmp(str, "image/bmp") )
        conn[ci].ctype = RES_BMP;
    else if ( 0==strcmp(str, "image/svg+xml") )
        conn[ci].ctype = RES_SVG;
    else if ( 0==strcmp(str, "application/json") )
        conn[ci].ctype = RES_JSON;
    else if ( 0==strcmp(str, "application/pdf") )
        conn[ci].ctype = RES_PDF;
    else if ( 0==strcmp(str, "audio/mpeg") )
        conn[ci].ctype = RES_AMPEG;
    else if ( 0==strcmp(str, "application/x-msdownload") )
        conn[ci].ctype = RES_EXE;
    else if ( 0==strcmp(str, "application/zip") )
        conn[ci].ctype = RES_ZIP;
    else    /* custom */
    {
        if ( 0==strncmp(str, "text/html", 9) )
            conn[ci].ctype = RES_HTML;
        else if ( 0==strncmp(str, "text/plain", 10) )
            conn[ci].ctype = RES_TEXT;
        else if ( !str[0] )
            conn[ci].ctype = CONTENT_TYPE_UNSET;
        else
            conn[ci].ctype = CONTENT_TYPE_USER;

        COPY(conn[ci].ctypestr, str, CONTENT_TYPE_LEN);
    }
}


/* --------------------------------------------------------------------------
   Set location
-------------------------------------------------------------------------- */
void lib_set_res_location(int ci, const char *str, ...)
{
    va_list plist;

    va_start(plist, str);
    vsprintf(conn[ci].location, str, plist);
    va_end(plist);
}


/* --------------------------------------------------------------------------
   Set response content disposition
-------------------------------------------------------------------------- */
void lib_set_res_content_disposition(int ci, const char *str, ...)
{
    va_list plist;

    va_start(plist, str);
    vsprintf(conn[ci].cdisp, str, plist);
    va_end(plist);
}


/* --------------------------------------------------------------------------
   Send message description as plain, pipe-delimited text as follows:
   <category>|<description>
-------------------------------------------------------------------------- */
void lib_send_msg_description(int ci, int code)
{
    char cat[64];
    char msg[1024];

    strcpy(cat, get_msg_cat(code));
    strcpy(msg, silgy_message(code));

#ifdef MSG_FORMAT_JSON
    OUT("{\"code\":%d,\"category\":\"%s\",\"message\":\"%s\"}", code, cat, msg);
    conn[ci].ctype = RES_JSON;
#else
    OUT("%d|%s|%s", code, cat, msg);
    conn[ci].ctype = RES_TEXT;
#endif

    RES_KEEP_CONTENT;

//    DBG("lib_send_msg_description: [%s]", G_tmp);

    RES_DONT_CACHE;
}


/* --------------------------------------------------------------------------
   Format counters
-------------------------------------------------------------------------- */
static void format_counters(counters_fmt_t *s, counters_t *n)
{
    amt(s->req, n->req);
    amt(s->req_dsk, n->req_dsk);
    amt(s->req_mob, n->req_mob);
    amt(s->req_bot, n->req_bot);
    amt(s->visits, n->visits);
    amt(s->visits_dsk, n->visits_dsk);
    amt(s->visits_mob, n->visits_mob);
    amt(s->blocked, n->blocked);
    amtd(s->average, n->average);
}


/* --------------------------------------------------------------------------
   Users info
-------------------------------------------------------------------------- */
static void users_info(int ci, char activity, int rows, admin_info_t ai[], int ai_cnt)
{
#ifdef DBMYSQL
#ifdef USERS
    char        sql[SQLBUF];
    MYSQL_RES   *result;
    MYSQL_ROW   row;

    char ai_sql[SQLBUF]="";

    if ( ai && ai_cnt )
    {
        int i;
        for ( i=0; i<ai_cnt; ++i )
        {
            strcat(ai_sql, ", (");
            strcat(ai_sql, ai[i].sql);
            strcat(ai_sql, ")");
        }
    }

//    sprintf(sql, "SELECT id, login, email, name, status, created, last_login, visits%s FROM users ORDER BY last_login DESC, created DESC", ai_sql);
    sprintf(sql, "SELECT id, login, email, name, status, created, last_login, visits%s FROM users", ai_sql);

    char activity_desc[64];
    int days;

    if ( activity == AI_USERS_YAU )
    {
        strcpy(activity_desc, "yearly active");
        days = 366;
    }
    else if ( activity == AI_USERS_MAU )
    {
        strcpy(activity_desc, "monthly active");
        days = 31;
    }
    else if ( activity == AI_USERS_DAU )
    {
        strcpy(activity_desc, "daily active");
        days = 2;
    }
    else    /* all */
    {
        strcpy(activity_desc, "all");
        days = 0;
    }
        
    char tmp[256];

    if ( days==366 || days==31 )
    {
        sprintf(tmp, " WHERE status=%d AND visits>0 AND DATEDIFF('%s', last_login)<%d", USER_STATUS_ACTIVE, DT_NOW, days);
        strcat(sql, tmp);
    }
    else if ( days==2 )   /* last 24 hours */
    {
        sprintf(tmp, " WHERE status=%d AND visits>0 AND TIME_TO_SEC(TIMEDIFF('%s', last_login))<86401", USER_STATUS_ACTIVE, DT_NOW);
        strcat(sql, tmp);
    }

    strcat(sql, " ORDER BY last_login DESC, created DESC");

    DBG("sql: %s", sql);

    mysql_query(G_dbconn, sql);

    result = mysql_store_result(G_dbconn);

    if ( !result )
    {
        ERR("%u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
        OUT("<p>Error %u: %s</p>", mysql_errno(G_dbconn), mysql_error(G_dbconn));
        RES_STATUS(500);
        return;
    }

    int records = mysql_num_rows(result);

    INF("admin_info: %d %s user(s)", records, activity_desc);

    int last_to_show = records<rows?records:rows;

    char formatted1[64];
    char formatted2[64];

    amt(formatted1, records);
    amt(formatted2, last_to_show);
    OUT("<p>%s %s users, showing %s of last seen</p>", formatted1, activity_desc, formatted2);

    OUT("<table cellpadding=4 border=1 style=\"margin-bottom:3em;\">");

    char ai_th[1024]="";

    if ( ai && ai_cnt )
    {
        int i;
        for ( i=0; i<ai_cnt; ++i )
        {
            strcat(ai_th, "<th>");
            strcat(ai_th, ai[i].th);
            strcat(ai_th, "</th>");
        }
    }

    OUT("<tr>");

    if ( REQ_DSK )
        OUT("<th>id</th><th>login</th><th>email</th><th>name</th><th>created</th><th>last_login</th><th>visits</th>%s", ai_th);
    else
        OUT("<th>id</th><th>login</th><th>email</th><th>last_login</th><th>visits</th>%s", ai_th);

    OUT("</tr>");

//    int     id;                     /* row[0] */
//    char    login[LOGIN_LEN+1];     /* row[1] */
//    char    email[EMAIL_LEN+1];     /* row[2] */
//    char    name[UNAME_LEN+1];      /* row[3] */
//    char    status;                 /* row[4] */
//    char    created[32];            /* row[5] */
//    char    last_login[32];         /* row[6] */
//    int     visits;                 /* row[7] */

    char fmt0[64];  /* id */
    char fmt7[64];  /* visits */

    int  i;
    char trstyle[16];

    char ai_td[4096]="";
    double ai_double;
    char ai_fmt[64];

    for ( i=0; i<last_to_show; ++i )
    {
        row = mysql_fetch_row(result);

        amt(fmt0, atoi(row[0]));    /* id */
        amt(fmt7, atoi(row[7]));    /* visits */

        if ( atoi(row[4]) != USER_STATUS_ACTIVE )
            strcpy(trstyle, " class=g");
        else
            trstyle[0] = EOS;

        OUT("<tr%s>", trstyle);

        if ( ai && ai_cnt )
        {
            ai_td[0] = EOS;

            int j;
            for ( j=0; j<ai_cnt; ++j )
            {
                if ( 0==strcmp(ai[j].type, "int") )
                {
                    strcat(ai_td, "<td class=r>");
                    amt(ai_fmt, atoi(row[j+8]));
                    strcat(ai_td, ai_fmt);
                }
                else if ( 0==strcmp(ai[j].type, "long") )
                {
                    strcat(ai_td, "<td class=r>");
                    amt(ai_fmt, atol(row[j+8]));
                    strcat(ai_td, ai_fmt);
                }
                else if ( 0==strcmp(ai[j].type, "float") || 0==strcmp(ai[j].type, "double") )
                {
                    strcat(ai_td, "<td class=r>");
                    sscanf(row[j+8], "%f", &ai_double);
                    amtd(ai_fmt, ai_double);
                    strcat(ai_td, ai_fmt);
                }
                else    /* string */
                {
                    strcat(ai_td, "<td>");
                    strcat(ai_td, row[j+8]);
                }

                strcat(ai_td, "</td>");
            }
        }

        if ( REQ_DSK )
            OUT("<td class=r>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td class=r>%s</td>%s", fmt0, row[1], row[2], row[3], row[5], row[6], fmt7, ai_td);
        else
            OUT("<td class=r>%s</td><td>%s</td><td>%s</td><td>%s</td><td class=r>%s</td>%s", fmt0, row[1], row[2], row[6], fmt7, ai_td);

        OUT("</tr>");
    }

    OUT("</table>");

    mysql_free_result(result);

#endif  /* USERS */
#endif  /* DBMYSQL */
}


/* --------------------------------------------------------------------------
   Admin dashboard
-------------------------------------------------------------------------- */
void silgy_admin_info(int ci, int users, admin_info_t ai[], int ai_cnt, bool header_n_footer)
{
#ifdef USERS
    if ( !LOGGED || uses[conn[ci].usi].auth_level < AUTH_LEVEL_ADMIN )
    {
        ERR("silgy_admin_info: user authorization level < %d", AUTH_LEVEL_ADMIN);
        RES_STATUS(404);
        return;
    }
#endif  /* USERS */

    /* ------------------------------------------------------------------- */

    INF("admin_info: --------------------");
//    INF("admin_info: 2019-11-08 10:56:19"
    INF("admin_info: %s", DT_NOW);

    if ( header_n_footer )
    {
        OUT_HTML_HEADER;

        OUT("<style>");
        OUT("body{font-family:monospace;font-size:10pt;}");
        OUT(".r{text-align:right;}");
        OUT(".g{color:grey;}");
        OUT("</style>");
    }
    else
    {
        OUT("<style>");
        OUT(".r{text-align:right;}");
        OUT(".g{color:grey;}");
        OUT("</style>");
    }

    OUT("<h1>Admin Info</h1>");

    OUT("<h2>Server</h2>");

    /* ------------------------------------------------------------------- */
    /* Server info */

    char formatted[64];

    amt(formatted, G_days_up);
    OUT("<p>Server started on %s (%s day(s) up) Silgy %s</p>", G_last_modified, formatted, WEB_SERVER_VERSION);

    OUT("<p>App version: %s</p>", APP_VERSION);

    /* ------------------------------------------------------------------- */
    /* Memory */

    OUT("<h2>Memory</h2>");

    int  mem_used;
    char mem_used_kb[64];
    char mem_used_mb[64];
    char mem_used_gb[64];

    mem_used = lib_get_memory();

    amt(mem_used_kb, mem_used);
    amtd(mem_used_mb, (double)mem_used/1024);
    amtd(mem_used_gb, (double)mem_used/1024/1024);

    OUT("<p>HWM: %s kB (%s MB / %s GB)</p>", mem_used_kb, mem_used_mb, mem_used_gb);

    OUT("<h2>Counters</h2>");

    OUT("<p>%d open connection(s), HWM: %d</p>", G_open_conn, G_open_conn_hwm);

    OUT("<p>%d user session(s), HWM: %d</p>", G_sessions, G_sessions_hwm);

    /* ------------------------------------------------------------------- */
    /* Counters */

    counters_fmt_t t;       /* today */
    counters_fmt_t y;       /* yesterday */
    counters_fmt_t b;       /* the day before */

    format_counters(&t, &G_cnts_today);
    format_counters(&y, &G_cnts_yesterday);
    if ( REQ_DSK )
        format_counters(&b, &G_cnts_day_before);

    OUT("<table cellpadding=4 border=1>");

    if ( REQ_DSK )  /* desktop -- 3 days' stats */
    {
        OUT("<tr><th>counter</th><th colspan=3>the day before</th><th colspan=3>yesterday</th><th colspan=3>today</th></tr>");
        OUT("<tr><td rowspan=2>all traffic (parsed requests)</td><td>all</td><td>dsk</td><td>mob</td><td>all</td><td>dsk</td><td>mob</td><td>all</td><td>dsk</td><td>mob</td></tr>");
        OUT("<tr><td class=r>%s</td><td class=r>%s</td><td class=r>%s</td><td class=r>%s</td><td class=r>%s</td><td class=r>%s</td><td class=r>%s</td><td class=r>%s</td><td class=r>%s</td></tr>", b.req, b.req_dsk, b.req_mob, y.req, y.req_dsk, y.req_mob, t.req, t.req_dsk, t.req_mob);
        OUT("<tr><td>bots</td><td colspan=3 class=r>%s</td><td colspan=3 class=r>%s</td><td colspan=3 class=r>%s</td></tr>", b.req_bot, y.req_bot, t.req_bot);
        OUT("<tr><td rowspan=2>visits</td><td>all</td><td>dsk</td><td>mob</td><td>all</td><td>dsk</td><td>mob</td><td>all</td><td>dsk</td><td>mob</td></tr>");
        OUT("<tr><td class=r><b>%s</b></td><td class=r>%s</td><td class=r>%s</td><td class=r><b>%s</b></td><td class=r>%s</td><td class=r>%s</td><td class=r><b>%s</b></td><td class=r>%s</td><td class=r>%s</td></tr>", b.visits, b.visits_dsk, b.visits_mob, y.visits, y.visits_dsk, y.visits_mob, t.visits, t.visits_dsk, t.visits_mob);
        OUT("<tr><td>attempts blocked</td><td colspan=3 class=r>%s</td><td colspan=3 class=r>%s</td><td colspan=3 class=r>%s</td></tr>", b.blocked, y.blocked, t.blocked);
        OUT("<tr><td>average</td><td colspan=3 class=r>%s ms</td><td colspan=3 class=r>%s ms</td><td colspan=3 class=r>%s ms</td></tr>", b.average, y.average, t.average);
    }
    else    /* mobile -- 2 days' stats */
    {
        OUT("<tr><th>counter</th><th colspan=3>yesterday</th><th colspan=3>today</th></tr>");
        OUT("<tr><td rowspan=2>all traffic (parsed requests)</td><td>all</td><td>dsk</td><td>mob</td><td>all</td><td>dsk</td><td>mob</td></tr>");
        OUT("<tr><td class=r>%s</td><td class=r>%s</td><td class=r>%s</td><td class=r>%s</td><td class=r>%s</td><td class=r>%s</td></tr>", y.req, y.req_dsk, y.req_mob, t.req, t.req_dsk, t.req_mob);
        OUT("<tr><td>bots</td><td colspan=3 class=r>%s</td><td colspan=3 class=r>%s</td></tr>", y.req_bot, t.req_bot);
        OUT("<tr><td rowspan=2>visits</td><td>all</td><td>dsk</td><td>mob</td><td>all</td><td>dsk</td><td>mob</td></tr>");
        OUT("<tr><td class=r><b>%s</b></td><td class=r>%s</td><td class=r>%s</td><td class=r><b>%s</b></td><td class=r>%s</td><td class=r>%s</td></tr>", y.visits, y.visits_dsk, y.visits_mob, t.visits, t.visits_dsk, t.visits_mob);
        OUT("<tr><td>attempts blocked</td><td colspan=3 class=r>%s</td><td colspan=3 class=r>%s</td></tr>", y.blocked, t.blocked);
        OUT("<tr><td>average</td><td colspan=3 class=r>%s ms</td><td colspan=3 class=r>%s ms</td></tr>", y.average, t.average);
    }

    OUT("</table>");

    /* ------------------------------------------------------------------- */
    /* IP blacklist */

    if ( G_blacklist_cnt )
    {
        OUT("<h2>Blacklist</h2>");

        amt(formatted, G_blacklist_cnt);
        OUT("<p>%s blacklisted IPs</p>", formatted);
//        OUT("<p><form action=\"add2blocked\" method=\"post\"><input type=\"text\" name=\"ip\"> <input type=\"submit\" onClick=\"wait();\" value=\"Block\"></form></p>");
    }

    /* ------------------------------------------------------------------- */
    /* Logs */

//    OUT("<p><a href=\"logs\">Logs</a></p>");

    /* ------------------------------------------------------------------- */
    /* Users */
#ifdef USERS
    if ( users > 0 )
    {
        OUT("<h2>Users</h2>");
        users_info(ci, AI_USERS_ALL, users, ai, ai_cnt);
        users_info(ci, AI_USERS_YAU, users, ai, ai_cnt);
        users_info(ci, AI_USERS_MAU, users, ai, ai_cnt);
        users_info(ci, AI_USERS_DAU, users, ai, ai_cnt);
    }
#endif

    if ( header_n_footer )
        OUT_HTML_FOOTER;

    RES_DONT_CACHE;
}
#endif  /* SILGY_CLIENT */


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
            strncpy(M_rest_headers[M_rest_headers_cnt].key, key, REST_HEADER_KEY_LEN);
            M_rest_headers[M_rest_headers_cnt].key[REST_HEADER_KEY_LEN] = EOS;
            strncpy(M_rest_headers[i].value, value, REST_HEADER_VAL_LEN);
            M_rest_headers[i].value[REST_HEADER_VAL_LEN] = EOS;
            return;
        }
        else if ( 0==strcmp(M_rest_headers[i].key, key) )
        {
            strncpy(M_rest_headers[i].value, value, REST_HEADER_VAL_LEN);
            M_rest_headers[i].value[REST_HEADER_VAL_LEN] = EOS;
            return;
        }
    }

    if ( M_rest_headers_cnt >= REST_MAX_HEADERS ) return;

    strncpy(M_rest_headers[M_rest_headers_cnt].key, key, REST_HEADER_KEY_LEN);
    M_rest_headers[M_rest_headers_cnt].key[REST_HEADER_KEY_LEN] = EOS;
    strncpy(M_rest_headers[M_rest_headers_cnt].value, value, REST_HEADER_VAL_LEN);
    M_rest_headers[M_rest_headers_cnt].value[REST_HEADER_VAL_LEN] = EOS;
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
#endif  /* HTTPS */
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
#endif  /* HTTPS */
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
   REST call / return true if header is already present
-------------------------------------------------------------------------- */
static bool rest_header_present(const char *key)
{
    int i;
    char uheader[MAX_LABEL_LEN+1];

    strcpy(uheader, upper(key));

    for ( i=0; i<M_rest_headers_cnt; ++i )
    {
        if ( 0==strcmp(upper(M_rest_headers[i].key), uheader) )
        {
            return TRUE;
        }
    }

    return FALSE;
}


/* --------------------------------------------------------------------------
   REST call / set proxy
-------------------------------------------------------------------------- */
void lib_rest_proxy(bool value)
{
    M_rest_proxy = value;
}


/* --------------------------------------------------------------------------
   REST call / render request
-------------------------------------------------------------------------- */
static int rest_render_req(char *buffer, const char *method, const char *host, const char *uri, const void *req, bool json, bool keep)
{
    char *p=buffer;     /* stpcpy is faster than strcat */

    /* header */

    p = stpcpy(p, method);

    if ( M_rest_proxy )
        p = stpcpy(p, " ");
    else
        p = stpcpy(p, " /");

    p = stpcpy(p, uri);
    p = stpcpy(p, " HTTP/1.1\r\n");
    p = stpcpy(p, "Host: ");
    p = stpcpy(p, host);
    p = stpcpy(p, "\r\n");

    char jtmp[JSON_BUFSIZE];

    if ( 0 != strcmp(method, "GET") && req )
    {
        if ( json )     /* JSON -> string conversion */
        {
            if ( !rest_header_present("Content-Type") )
                p = stpcpy(p, "Content-Type: application/json; charset=utf-8\r\n");

            strcpy(jtmp, lib_json_to_string((JSON*)req));
        }
        else
        {
            if ( !rest_header_present("Content-Type") )
                p = stpcpy(p, "Content-Type: application/x-www-form-urlencoded; charset=utf-8\r\n");
        }
        char tmp[64];
        sprintf(tmp, "Content-Length: %d\r\n", strlen(json?jtmp:(char*)req));
        p = stpcpy(p, tmp);
    }

    if ( json && !rest_header_present("Accept") )
        p = stpcpy(p, "Accept: application/json\r\n");

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
    if ( !rest_header_present("User-Agent") )
#ifdef SILGY_AS_BOT
        p = stpcpy(p, "User-Agent: Silgy Bot\r\n");
#else
        p = stpcpy(p, "User-Agent: Silgy\r\n");
#endif
#endif  /* NO_IDENTITY */

    /* end of headers */

    p = stpcpy(p, "\r\n");

    /* body */

    if ( 0 != strcmp(method, "GET") && req )
        p = stpcpy(p, json?jtmp:(char*)req);

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
static int addresses_cnt=0, addresses_last=0;
    int  i;
    bool addr_cached=FALSE;

    DBG("rest_connect [%s:%s]", host, port);

    struct addrinfo *result=NULL;

#ifndef REST_CALL_DONT_CACHE_ADDRINFO

    DBG("Trying cache...");

#ifdef DUMP
    DBG("addresses_cnt=%d", addresses_cnt);
#endif

    for ( i=0; i<addresses_cnt; ++i )
    {
        if ( 0==strcmp(addresses[i].host, host) && 0==strcmp(addresses[i].port, port) )
        {
            DBG("Host [%s:%s] found in cache (%d)", host, port, i);
            addr_cached = TRUE;
            result = &addresses[i].addr;
            break;
        }
    }

#endif  /* REST_CALL_DONT_CACHE_ADDRINFO */

    if ( !addr_cached )
    {
#ifndef REST_CALL_DONT_CACHE_ADDRINFO
        DBG("Host [%s:%s] not found in cache", host, port);
#endif
        DBG("getaddrinfo...");   /* TODO: change to asynchronous, i.e. getaddrinfo_a */

        struct addrinfo hints;
        int s;

        memset(&hints, 0, sizeof(struct addrinfo));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

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
#ifdef DUMP
        DBG("Trying socket...");
#endif
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

        int timeout_tmp = G_RESTTimeout/5;

        /* plain socket connection --------------------------------------- */

        if ( connect(M_rest_sock, rp->ai_addr, rp->ai_addrlen) != -1 )
        {
            break;  /* immediate success */
        }
        else if ( lib_finish_with_timeout(M_rest_sock, CONNECT, WRITE, NULL, 0, &timeout_tmp, NULL, 0) == 0 )
        {
            break;  /* success within timeout */
        }

        close_conn(M_rest_sock);   /* no cigar */
    }

    if ( rp == NULL )   /* no address succeeded */
    {
        ERR("Could not connect");
        if ( result && !addr_cached ) freeaddrinfo(result);
        return FALSE;
    }

    /* -------------------------------------------------------------------------- */

    *timeout_remain = G_RESTTimeout - lib_elapsed(start);
    if ( *timeout_remain < 1 ) *timeout_remain = 1;

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

        /* get the remote address */
        char remote_addr[INET_ADDRSTRLEN]="";
        struct sockaddr_in *remote_addr_struct = (struct sockaddr_in*)rp->ai_addr;
#ifdef _WIN32   /* Windows */
        strcpy(remote_addr, inet_ntoa(remote_addr_struct->sin_addr));
#else
        inet_ntop(AF_INET, &(remote_addr_struct->sin_addr), remote_addr, INET_ADDRSTRLEN);
#endif
        INF("Connected to %s", remote_addr);

        DBG("Host [%s:%s] added to cache (%d)", host, port, addresses_last);

        if ( addresses_cnt < REST_ADDRESSES_CACHE_SIZE-1 )   /* first round */
        {
            ++addresses_cnt;    /* cache usage */
            ++addresses_last;   /* next to use index */
        }
        else    /* cache full -- reuse it from start */
        {
            if ( addresses_last < REST_ADDRESSES_CACHE_SIZE-1 )
                ++addresses_last;
            else
                addresses_last = 0;
        }

#endif  /* REST_CALL_DONT_CACHE_ADDRINFO */

        freeaddrinfo(result);
    }

    DBG("Connected");

#ifdef DUMP
    DBG("elapsed after plain connect: %.3lf ms", lib_elapsed(start));
#endif

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
            M_rest_ssl = NULL;
            return FALSE;
        }

        DBG("Trying SSL_set_tlsext_host_name...");

        ret = SSL_set_tlsext_host_name(M_rest_ssl, host);

        if ( ret <= 0 )
        {
            ERR("SSL_set_tlsext_host_name failed, ret = %d", ret);
            close_conn(M_rest_sock);
            SSL_free(M_rest_ssl);
            M_rest_ssl = NULL;
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
        else if ( lib_finish_with_timeout(M_rest_sock, CONNECT, CONNECT, NULL, ret, timeout_remain, M_rest_ssl, 0) > 0 )
        {
            DBG("SSL_connect successful");
        }
        else
        {
            ERR("SSL_connect failed");
            close_conn(M_rest_sock);
            SSL_free(M_rest_ssl);
            M_rest_ssl = NULL;
            return FALSE;
        }

#ifdef DUMP
        DBG("elapsed after SSL connect: %.3lf ms", lib_elapsed(start));
#endif
//        M_ssl_session = SSL_get_session(M_rest_ssl);

        X509 *server_cert;
        server_cert = SSL_get_peer_certificate(M_rest_ssl);
        if ( server_cert )
        {
            DBG("Got server certificate");
			X509_NAME *certname;
			certname = X509_NAME_new();
			certname = X509_get_subject_name(server_cert);
			DBG("server_cert [%s]", X509_NAME_oneline(certname, NULL, 0));
            X509_free(server_cert);
        }
        else
            WAR("Couldn't get server certificate");
    }
#endif  /* HTTPS */

    return TRUE;
}


/* --------------------------------------------------------------------------
   REST call / disconnect
-------------------------------------------------------------------------- */
static void rest_disconnect(int ssl_ret)
{
    DBG("rest_disconnect");

#ifdef HTTPS
    if ( M_rest_ssl )
    {
        bool shutdown=TRUE;

        if ( ssl_ret < 0 )
        {
            int ssl_err = SSL_get_error(M_rest_ssl, ssl_ret);

            if ( ssl_err==SSL_ERROR_SYSCALL || ssl_err==SSL_ERROR_SSL )
                shutdown = FALSE;
        }

        if ( shutdown )
        {
            int timeout_tmp = G_RESTTimeout/5;

            int ret = SSL_shutdown(M_rest_ssl);

            if ( ret == 1 )
            {
                DBG("SSL_shutdown immediate success");
            }
            else if ( ret == 0 )
            {
                DBG("First SSL_shutdown looks fine, trying to complete the bidirectional shutdown handshake...");

                ret = SSL_shutdown(M_rest_ssl);

                if ( ret == 1 )
                    DBG("SSL_shutdown success");
                else if ( lib_finish_with_timeout(M_rest_sock, SHUTDOWN, SHUTDOWN, NULL, ret, &timeout_tmp, M_rest_ssl, 0) > 0 )
                    DBG("SSL_shutdown successful");
                else
                    ERR("SSL_shutdown failed");
            }
            else if ( lib_finish_with_timeout(M_rest_sock, SHUTDOWN, SHUTDOWN, NULL, ret, &timeout_tmp, M_rest_ssl, 0) > 0 )
            {
                DBG("SSL_shutdown successful");
            }
            else
            {
                ERR("SSL_shutdown failed");
            }
        }

        SSL_free(M_rest_ssl);

/*        if ( M_ssl_session )
            SSL_SESSION_free(M_ssl_session); */

        M_rest_ssl = NULL;
    }
#endif  /* HTTPS */

    close_conn(M_rest_sock);
}


/* --------------------------------------------------------------------------
   REST call / convert chunked to normal content
   Return number of bytes written to res_content
-------------------------------------------------------------------------- */
static int chunked2content(char *res_content, const char *buffer, int src_len, int res_len)
{
    char *res=res_content;
    char chunk_size_str[8];
    int  chunk_size=src_len;
    const char *p=buffer;
    int  was_read=0, written=0;

    while ( chunk_size > 0 )    /* chunk by chunk */
    {
        /* get the chunk size */

        int i=0;

        while ( *p!='\r' && *p!='\n' && i<6 && i<src_len )
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
            WAR("chunk_size < 0");
            break;
        }
        else if ( chunk_size > res_len-written )
        {
            WAR("chunk_size > res_len-written");
            break;
        }

        /* --------------------------------------------------------------- */
        /* skip "\r\n" */

//        p += 2;
#ifdef DUMP
        DBG("skip %d (should be 13)", *p);
#endif
        ++p;    /* skip '\r' */
#ifdef DUMP
        DBG("skip %d (should be 10)", *p);
#endif
        ++p;    /* skip '\n' */

        was_read += 2;

        /* once again may be needed */

        if ( *p == '\r' )
        {
#ifdef DUMP
            DBG("skip 13");
#endif
            ++p;    /* skip '\r' */
            ++was_read;
        }

        if ( *p == '\n' )
        {
#ifdef DUMP
            DBG("skip 10");
#endif
            ++p;    /* skip '\n' */
            ++was_read;
        }

#ifdef DUMP
        DBG("was_read = %d", was_read);
#endif

        /* --------------------------------------------------------------- */

        if ( was_read >= src_len || chunk_size > src_len-was_read )
        {
            WAR("Unexpected end of buffer");
            break;
        }

        /* --------------------------------------------------------------- */
        /* copy chunk to destination */

        /* stpncpy() returns a pointer to the terminating null byte in dest, or,
           if dest is not null-terminated, dest+n. */

        DBG("Appending %d bytes to the content buffer", chunk_size);

#ifdef DUMP
        DBG("p starts with '%c'", *p);
#endif
        res = stpncpy(res, p, chunk_size);
        written += chunk_size;

#ifdef DUMP
        DBG("written = %d", written);
#endif

        p += chunk_size;
        was_read += chunk_size;

#ifdef DUMP
//        DBG("p starts with '%c'", *p);
#endif
        /* --------------------------------------------------------------- */

        while ( *p != '\n' && was_read<src_len-1 )
        {
#ifdef DUMP
            DBG("skip %d (expected 13)", *p);
#endif
            ++p;
            ++was_read;
        }

#ifdef DUMP
        DBG("skip %d (should be 10)", *p);
#endif
        ++p;    /* skip '\n' */

        ++was_read;
    }

    return written;
}


/* --------------------------------------------------------------------------
   REST call / get response content length
-------------------------------------------------------------------------- */
static int rest_res_content_length(const char *u_res_header, int len)
{
    const char *p;

    if ( (p=strstr(u_res_header, "\nCONTENT-LENGTH: ")) == NULL )
        return -1;

    if ( len < (p-u_res_header) + 18 ) return -1;

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



#define TRANSFER_MODE_NORMAL     '1'
#define TRANSFER_MODE_NO_CONTENT '2'
#define TRANSFER_MODE_CHUNKED    '3'
#define TRANSFER_MODE_ERROR      '4'


/* --------------------------------------------------------------------------
   REST call / parse response
-------------------------------------------------------------------------- */
bool lib_rest_res_parse(char *res_header, int bytes)
{
    /* HTTP/1.1 200 OK <== 15 chars */

    char status[4];

    if ( bytes < 14 || 0 != strncmp(res_header, "HTTP/1.", 7) )
    {
        return FALSE;
    }

    res_header[bytes] = EOS;
#ifdef DUMP
    DBG("");
    DBG("Got %d bytes of response [%s]", bytes, res_header);
#else
    DBG("Got %d bytes of response", bytes);
#endif  /* DUMP */

    /* Status */

    strncpy(status, res_header+9, 3);
    status[3] = EOS;
    G_rest_status = atoi(status);
    INF("REST response status: %s", status);

    char u_res_header[REST_RES_HEADER_LEN+1];   /* uppercase */
    strcpy(u_res_header, upper(res_header));

    /* Content-Type */

    const char *p;

    if ( (p=strstr(u_res_header, "\nCONTENT-TYPE: ")) == NULL )
    {
        G_rest_content_type[0] = EOS;
    }
    else if ( bytes < (p-res_header) + 16 )
    {
        G_rest_content_type[0] = EOS;
    }
    else
    {
        char i=0;

        p += 15;

        while ( *p != '\r' && *p != '\n' && *p && i<255 )
        {
            G_rest_content_type[i++] = *p++;
        }

        G_rest_content_type[i] = EOS;
        DBG("REST content type [%s]", G_rest_content_type);
    }

    /* content length */

    G_rest_res_len = rest_res_content_length(u_res_header, bytes);

    if ( G_rest_res_len > JSON_BUFSIZE-1 )
    {
        WAR("Response content is too big (%d)", G_rest_res_len);
        return FALSE;
    }

    if ( G_rest_res_len > 0 )     /* Content-Length present in response */
    {
        DBG("TRANSFER_MODE_NORMAL");
        M_rest_mode = TRANSFER_MODE_NORMAL;
    }
    else if ( G_rest_res_len == 0 )
    {
        DBG("TRANSFER_MODE_NO_CONTENT");
        M_rest_mode = TRANSFER_MODE_NO_CONTENT;
    }
    else    /* content length == -1 */
    {
        if ( strstr(u_res_header, "\nTRANSFER-ENCODING: CHUNKED") != NULL )
        {
            DBG("TRANSFER_MODE_CHUNKED");
            M_rest_mode = TRANSFER_MODE_CHUNKED;
        }
        else
        {
            WAR("TRANSFER_MODE_ERROR");
            M_rest_mode = TRANSFER_MODE_ERROR;
            return FALSE;
        }
    }

    return TRUE;
}


/* --------------------------------------------------------------------------
   REST call
-------------------------------------------------------------------------- */
bool lib_rest_req(const void *req, void *res, const char *method, const char *url, bool json, bool keep)
{
    char     host[256];
    char     port[8];
    bool     secure=FALSE;
static char  prev_host[256];
static char  prev_port[8];
static bool  prev_secure=FALSE;
    char     uri[MAX_URI_LEN+1];
static bool  connected=FALSE;
static time_t connected_time=0;
    char     res_header[REST_RES_HEADER_LEN+1];
static char  buffer[JSON_BUFSIZE];
    int      bytes;
    char     *body;
    unsigned content_read=0, buffer_read=0;
    unsigned len, i, j;
    int      timeout_remain = G_RESTTimeout;
#ifdef HTTPS
    int      ssl_err;
#endif  /* HTTPS */

    DBG("lib_rest_req [%s] [%s]", method, url);

    /* -------------------------------------------------------------------------- */

    if ( !rest_parse_url(url, host, port, uri, &secure) ) return FALSE;

    if ( M_rest_proxy )
        strcpy(uri, url);

    len = rest_render_req(buffer, method, host, uri, req, json, keep);

#ifdef DUMP
    DBG("------------------------------------------------------------");
    DBG("lib_rest_req buffer [%s]", buffer);
    DBG("------------------------------------------------------------");
#endif  /* DUMP */

    struct timespec start;
#ifdef _WIN32
    clock_gettime_win(&start);
#else
    clock_gettime(MONOTONIC_CLOCK_NAME, &start);
#endif

    /* -------------------------------------------------------------------------- */

    if ( connected
            && (secure!=prev_secure || 0 != strcmp(host, prev_host) || 0 != strcmp(port, prev_port) || G_now-connected_time > CONN_TIMEOUT) )
    {
        /* reconnect anyway */
        DBG("different host, port or old connection, reconnecting");
        rest_disconnect(0);
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
        {
/*            char first_char[2];
            first_char[0] = buffer[0];
            first_char[1] = EOS;

            bytes = SSL_write(M_rest_ssl, first_char, 1);

            if ( bytes > 0 )
                bytes = SSL_write(M_rest_ssl, buffer+1, len-1) + bytes; */

            bytes = SSL_write(M_rest_ssl, buffer, len);
        }
        else
#endif  /* HTTPS */
            bytes = send(M_rest_sock, buffer, len, 0);    /* try in one go */

        if ( !secure && bytes <= 0 )
        {
            if ( !was_connected || after_reconnect )
            {
                ERR("Send (after fresh connect) failed");
                rest_disconnect(0);
                connected = FALSE;
                return FALSE;
            }

            DBG("Disconnected? Trying to reconnect...");
            rest_disconnect(0);
            if ( !rest_connect(host, port, &start, &timeout_remain, secure) ) return FALSE;
            after_reconnect = 1;
        }
        else if ( secure && bytes == -1 )
        {
            bytes = lib_finish_with_timeout(M_rest_sock, WRITE, WRITE, buffer, len, &timeout_remain, secure?M_rest_ssl:NULL, 0);

            if ( bytes == -1 )
            {
                if ( !was_connected || after_reconnect )
                {
                    ERR("Send (after fresh connect) failed");
                    rest_disconnect(-1);
                    connected = FALSE;
                    return FALSE;
                }

                DBG("Disconnected? Trying to reconnect...");
                rest_disconnect(-1);
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
    DBG("Sent %d bytes", bytes);
#endif

    if ( bytes < 15 )
    {
        ERR("send failed, errno = %d (%s)", errno, strerror(errno));
        rest_disconnect(bytes);
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
#endif  /* HTTPS */
        bytes = recv(M_rest_sock, res_header, REST_RES_HEADER_LEN, 0);

    if ( bytes == -1 )
    {
        bytes = lib_finish_with_timeout(M_rest_sock, READ, READ, res_header, REST_RES_HEADER_LEN, &timeout_remain, secure?M_rest_ssl:NULL, 0);

        if ( bytes <= 0 )
        {
            ERR("recv failed, errno = %d (%s)", errno, strerror(errno));
            rest_disconnect(bytes);
            connected = FALSE;
            return FALSE;
        }
    }

    DBG("Read %d bytes", bytes);

#ifdef DUMP
    DBG("elapsed after first response read: %.3lf ms", lib_elapsed(&start));
#endif

    /* -------------------------------------------------------------------------- */
    /* parse the response                                                         */
    /* we assume that at least response header arrived at once                    */

    if ( !lib_rest_res_parse(res_header, bytes) )
    {
        ERR("No or invalid response");
#ifdef DUMP
        if ( bytes >= 0 )
        {
            res_header[bytes] = EOS;
            DBG("Got %d bytes of response [%s]", bytes, res_header);
        }
#endif
        G_rest_status = 500;
        rest_disconnect(bytes);
        connected = FALSE;
        return FALSE;
    }

    /* ------------------------------------------------------------------- */
    /* at this point we've got something that seems to be a HTTP header,
       possibly with content */

static char res_content[JSON_BUFSIZE];

    /* ------------------------------------------------------------------- */
    /* some content may have already been read                             */

    body = strstr(res_header, "\r\n\r\n");

    if ( body )
    {
        body += 4;

        int was_read = bytes - (body-res_header);

        if ( was_read > 0 )
        {
            if ( M_rest_mode == TRANSFER_MODE_NORMAL )   /* not chunked goes directly to res_content */
            {
                content_read = was_read;
                strncpy(res_content, body, content_read);
            }
            else if ( M_rest_mode == TRANSFER_MODE_CHUNKED )   /* chunked goes to buffer before res_content */
            {
                buffer_read = was_read;
                strncpy(buffer, body, buffer_read);
            }
        }
    }

    /* ------------------------------------------------------------------- */
    /* read content                                                        */

    if ( M_rest_mode == TRANSFER_MODE_NORMAL )
    {
        while ( content_read < G_rest_res_len && timeout_remain > 1 )   /* read whatever we can within timeout */
        {
#ifdef DUMP
            DBG("trying again (content-length)");
#endif

#ifdef HTTPS
            if ( secure )
                bytes = SSL_read(M_rest_ssl, res_content+content_read, JSON_BUFSIZE-content_read-1);
            else
#endif  /* HTTPS */
                bytes = recv(M_rest_sock, res_content+content_read, JSON_BUFSIZE-content_read-1, 0);

            if ( bytes == -1 )
                bytes = lib_finish_with_timeout(M_rest_sock, READ, READ, res_content+content_read, JSON_BUFSIZE-content_read-1, &timeout_remain, secure?M_rest_ssl:NULL, 0);

            if ( bytes > 0 )
                content_read += bytes;
        }

        if ( bytes < 1 )
        {
            DBG("timeouted?");
            rest_disconnect(bytes);
            connected = FALSE;
            return FALSE;
        }
    }
    else if ( M_rest_mode == TRANSFER_MODE_CHUNKED )
    {
        /* for single-threaded process, I can't see better option than to read everything
           into a buffer and then parse and copy chunks into final res_content */

        /* there's no guarantee that every read = one chunk, so just read whatever comes in, until it does */

        while ( bytes > 0 && timeout_remain > 1 )   /* read whatever we can within timeout */
        {
#ifdef DUMP
            DBG("Has the last chunk been read?");
            DBG("buffer_read = %d", buffer_read);
            if ( buffer_read > 5 )
            {
                int ii;
                for ( ii=buffer_read-6; ii<buffer_read; ++ii )
                {
                    if ( buffer[ii] == '\r' )
                        DBG("buffer[%d] '\\r'", ii);
                    else if ( buffer[ii] == '\n' )
                        DBG("buffer[%d] '\\n'", ii);
                    else
                        DBG("buffer[%d] '%c'", ii, buffer[ii]);
                }
            }
#endif
            if ( buffer_read>5 && buffer[buffer_read-6]=='\n' && buffer[buffer_read-5]=='0' && buffer[buffer_read-4]=='\r' && buffer[buffer_read-3]=='\n' && buffer[buffer_read-2]=='\r' && buffer[buffer_read-1]=='\n' )
            {
                DBG("Last chunk detected (with \\r\\n\\r\\n)");
                break;
            }
            else if ( buffer_read>3 && buffer[buffer_read-4]=='\n' && buffer[buffer_read-3]=='0' && buffer[buffer_read-2]=='\r' && buffer[buffer_read-1]=='\n' )
            {
                DBG("Last chunk detected (with \\r\\n)");
                break;
            }

#ifdef DUMP
            DBG("trying again (chunked)");
#endif

#ifdef HTTPS
            if ( secure )
                bytes = SSL_read(M_rest_ssl, buffer+buffer_read, JSON_BUFSIZE-buffer_read-1);
            else
#endif  /* HTTPS */
                bytes = recv(M_rest_sock, buffer+buffer_read, JSON_BUFSIZE-buffer_read-1, 0);

            if ( bytes == -1 )
                bytes = lib_finish_with_timeout(M_rest_sock, READ, READ, buffer+buffer_read, JSON_BUFSIZE-buffer_read-1, &timeout_remain, secure?M_rest_ssl:NULL, 0);

            if ( bytes > 0 )
                buffer_read += bytes;
        }

        if ( buffer_read < 5 )
        {
            ERR("buffer_read is only %d, this can't be valid chunked content", buffer_read);
            rest_disconnect(bytes);
            connected = FALSE;
            return FALSE;
        }

        content_read = chunked2content(res_content, buffer, buffer_read, JSON_BUFSIZE);
    }

    /* ------------------------------------------------------------------- */

    res_content[content_read] = EOS;

    DBG("Read %d bytes of content", content_read);

    G_rest_res_len = content_read;

#ifdef DUMP
    log_long(res_content, content_read, "Content");
#endif

    /* ------------------------------------------------------------------- */

    if ( !keep || strstr(res_header, "\nConnection: close") != NULL || strstr(res_header, "\nConnection: Close") != NULL )
    {
        DBG("Closing connection");
        rest_disconnect(bytes);
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

    /* -------------------------------------------------------------------------- */
    /* stats                                                                      */

    ++G_rest_req;

    double elapsed = lib_elapsed(&start);

    DBG("REST call finished in %.3lf ms", elapsed);

    G_rest_elapsed += elapsed;

    G_rest_average = G_rest_elapsed / G_rest_req;

    return TRUE;
}


/* --------------------------------------------------------------------------
   Finish socket operation with timeout
-------------------------------------------------------------------------- */
int lib_finish_with_timeout(int sock, char oper, char readwrite, char *buffer, int len, int *msec, void *ssl, int level)
{
    int             sockerr;
    struct timeval  timeout;
    fd_set          readfds;
    fd_set          writefds;
    int             socks=0;
#ifdef HTTPS
    int             bytes;
    int             ssl_err;
#endif

#ifdef DUMP
    if ( oper == READ )
        DBG("lib_finish_with_timeout READ, level=%d", level);
    else if ( oper == WRITE )
        DBG("lib_finish_with_timeout WRITE, level=%d", level);
    else if ( oper == CONNECT )
        DBG("lib_finish_with_timeout CONNECT, level=%d", level);
    else if ( oper == SHUTDOWN )
        DBG("lib_finish_with_timeout SHUTDOWN, level=%d", level);
    else
        ERR("lib_finish_with_timeout -- unknown operation: %d", oper);
#endif

    if ( level > 20 )   /* just in case */
    {
        ERR("lib_finish_with_timeout -- too many levels");
        return -1;
    }

    /* get the error code ------------------------------------------------ */
    /* note: during SSL operations it will be 0                            */

#ifdef _WIN32   /* Windows */
    sockerr = WSAGetLastError();
#else
    sockerr = errno;
#endif

    if ( !ssl )
    {
#ifdef _WIN32   /* Windows */

//        sockerr = WSAGetLastError();

        if ( sockerr != WSAEWOULDBLOCK )
        {
            wchar_t *s = NULL;
            FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_MAX_WIDTH_MASK, NULL, sockerr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&s, 0, NULL);
            ERR("%d (%S)", sockerr, s);
            LocalFree(s);
            return -1;
        }

#else   /* Linux */

//        sockerr = errno;

        if ( sockerr != EWOULDBLOCK && sockerr != EINPROGRESS )
        {
            ERR("errno = %d (%s)", sockerr, strerror(sockerr));
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
#ifdef _WIN32
    clock_gettime_win(&start);
#else
    clock_gettime(MONOTONIC_CLOCK_NAME, &start);
#endif

    /* set up fd-s and wait ---------------------------------------------- */

    if ( readwrite == READ )
    {
#ifdef DUMP
        DBG("READ, waiting on select()...");
#endif
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);
        socks = select(sock+1, &readfds, NULL, NULL, &timeout);
    }
    else if ( readwrite == WRITE )
    {
#ifdef DUMP
        DBG("WRITE, waiting on select()...");
#endif
        FD_ZERO(&writefds);
        FD_SET(sock, &writefds);
        socks = select(sock+1, NULL, &writefds, NULL, &timeout);
    }
    else if ( readwrite == CONNECT || readwrite == SHUTDOWN )   /* SSL only! */
    {
#ifdef HTTPS
#ifdef DUMP
        DBG("SSL_connect or SSL_shutdown, determining the next step...");
#endif
        ssl_err = SSL_get_error((SSL*)ssl, len);

#ifdef DUMP
        DBG("len = %d", len);
        log_ssl_error(ssl_err);
#endif  /* DUMP */

        if ( ssl_err==SSL_ERROR_WANT_READ )
            return lib_finish_with_timeout(sock, oper, READ, buffer, len, msec, ssl, level+1);
        else if ( ssl_err==SSL_ERROR_WANT_WRITE )
            return lib_finish_with_timeout(sock, oper, WRITE, buffer, len, msec, ssl, level+1);
        else
        {
            DBG("SSL_connect or SSL_shutdown error %d", ssl_err);
            return -1;
        }
#endif  /* HTTPS */
    }
    else
    {
        ERR("lib_finish_with_timeout -- invalid readwrite (%d) for this operation (%d)", readwrite, oper);
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
        if ( readwrite == READ )
        {
#ifdef HTTPS
            if ( ssl )
            {
                if ( buffer )
                    bytes = SSL_read((SSL*)ssl, buffer, len);
                else if ( oper == CONNECT )
                    bytes = SSL_connect((SSL*)ssl);
                else    /* SHUTDOWN */
                    bytes = SSL_shutdown((SSL*)ssl);

                if ( bytes > 0 )
                {
                    return bytes;
                }
                else
                {
                    ssl_err = SSL_get_error((SSL*)ssl, bytes);

#ifdef DUMP
                    DBG("bytes = %d", bytes);
                    log_ssl_error(ssl_err);
#endif  /* DUMP */

                    if ( ssl_err==SSL_ERROR_WANT_READ )
                        return lib_finish_with_timeout(sock, oper, READ, buffer, len, msec, ssl, level+1);
                    else if ( ssl_err==SSL_ERROR_WANT_WRITE )
                        return lib_finish_with_timeout(sock, oper, WRITE, buffer, len, msec, ssl, level+1);
                    else
                    {
                        DBG("SSL_read error %d", ssl_err);
                        return -1;
                    }
                }
            }
            else
#endif  /* HTTPS */
                return recv(sock, buffer, len, 0);
        }
        else if ( readwrite == WRITE )
        {
#ifdef HTTPS
            if ( ssl )
            {
                if ( buffer )
                    bytes = SSL_write((SSL*)ssl, buffer, len);
                else if ( oper == CONNECT )
                    bytes = SSL_connect((SSL*)ssl);
                else    /* SHUTDOWN */
                    bytes = SSL_shutdown((SSL*)ssl);

                if ( bytes > 0 )
                {
                    return bytes;
                }
                else
                {
                    ssl_err = SSL_get_error((SSL*)ssl, bytes);

#ifdef DUMP
                    DBG("bytes = %d", bytes);
                    log_ssl_error(ssl_err);
#endif  /* DUMP */

                    if ( ssl_err==SSL_ERROR_WANT_WRITE )
                        return lib_finish_with_timeout(sock, oper, WRITE, buffer, len, msec, ssl, level+1);
                    else if ( ssl_err==SSL_ERROR_WANT_READ )
                        return lib_finish_with_timeout(sock, oper, READ, buffer, len, msec, ssl, level+1);
                    else
                    {
                        DBG("SSL_write error %d", ssl_err);
                        return -1;
                    }
                }
            }
            else
#endif  /* HTTPS */
                return send(sock, buffer, len, 0);
        }
        else
        {
            ERR("lib_finish_with_timeout -- should have never reached this!");
            return -1;
        }
    }
}


/* --------------------------------------------------------------------------
   Log SSL error

   From openssl.h:

#define SSL_ERROR_NONE        (0)
#define SSL_ERROR_SSL         (1)
#define SSL_ERROR_WANT_READ   (2)
#define SSL_ERROR_WANT_WRITE  (3)
#define SSL_ERROR_SYSCALL     (5)
#define SSL_ERROR_ZERO_RETURN (6)
-------------------------------------------------------------------------- */
void log_ssl_error(int ssl_err)
{
#ifdef HTTPS

    if ( ssl_err != SSL_ERROR_SYSCALL ) return;

#ifdef _WIN32   /* Windows */

    int sockerr = WSAGetLastError();

    wchar_t *s = NULL;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_MAX_WIDTH_MASK, NULL, sockerr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&s, 0, NULL);
    DBG("ssl_err=SSL_ERROR_SYSCALL, errno=%d (%S)", sockerr, s);
    LocalFree(s);

#else   /* Linux */

    int sockerr = errno;

    DBG("ssl_err=SSL_ERROR_SYSCALL, errno=%d (%s)", sockerr, strerror(sockerr));

#endif  /* _WIN32 */

#endif  /* HTTPS */
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
#ifdef _WIN32
    clock_gettime_win(&end);
#else
    clock_gettime(MONOTONIC_CLOCK_NAME, &end);
#endif
    elapsed = (end.tv_sec - start->tv_sec) * 1000.0;
    elapsed += (end.tv_nsec - start->tv_nsec) / 1000000.0;
    return elapsed;
}


/* --------------------------------------------------------------------------
   Log the memory footprint
-------------------------------------------------------------------------- */
void lib_log_memory()
{
    int         mem_used;
    char        mem_used_kb[32];
    char        mem_used_mb[32];
    char        mem_used_gb[32];

    mem_used = lib_get_memory();

    amt(mem_used_kb, mem_used);
    amtd(mem_used_mb, (double)mem_used/1024);
    amtd(mem_used_gb, (double)mem_used/1024/1024);

    ALWAYS_LINE;
    ALWAYS("Memory: %s kB (%s MB / %s GB)", mem_used_kb, mem_used_mb, mem_used_gb);
    ALWAYS_LINE;
}


/* --------------------------------------------------------------------------
   For lib_memory
-------------------------------------------------------------------------- */
static int mem_parse_line(const char* line)
{
    int     ret=0;
    int     i=0;
    char    strret[64];
    const char* p=line;

    while (!isdigit(*p)) ++p;       /* skip non-digits */

    while (isdigit(*p)) strret[i++] = *p++;

    strret[i] = EOS;

/*  DBG("mem_parse_line: line [%s]", line);
    DBG("mem_parse_line: strret [%s]", strret);*/

    ret = atoi(strret);

    return ret;
}


/* --------------------------------------------------------------------------
   Return currently used memory (high water mark) by current process in kB
-------------------------------------------------------------------------- */
int lib_get_memory()
{
    int result=0;

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
char *silgy_filter_strict(const char *src)
{
static char dst[4096];
    int     i=0, j=0;

    while ( src[i] && j<4095 )
    {
        if ( (src[i] >= 65 && src[i] <= 90)
                || (src[i] >= 97 && src[i] <= 122)
                || isdigit(src[i]) )
            dst[j++] = src[i];
        else if ( src[i] == ' ' || src[i] == '\t' || src[i] == '\n' )
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
static char ret[4096];
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
static char ret[4096];
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
   Get the file extension
-------------------------------------------------------------------------- */
char *get_file_ext(const char *fname)
{
static char ext[256];
    char *pext=NULL;

#ifdef DUMP
    DBG("name: [%s]", fname);
#endif

    if ( (pext=(char*)strrchr(fname, '.')) == NULL )     /* no dot */
    {
        ext[0] = EOS;
        return ext;
    }

    if ( pext-fname == strlen(fname)-1 )        /* dot is the last char */
    {
        ext[0] = EOS;
        return ext;
    }

    ++pext;

    strcpy(ext, pext);

#ifdef DUMP
    DBG("ext: [%s]", ext);
#endif

    return ext;
}


/* --------------------------------------------------------------------------
   Determine resource type by its extension
-------------------------------------------------------------------------- */
char get_res_type(const char *fname)
{
    char *ext=NULL;
    char uext[8]="";

#ifdef DUMP
//  DBG("name: [%s]", fname);
#endif

    if ( (ext=(char*)strrchr(fname, '.')) == NULL )     /* no dot */
        return RES_TEXT;

    if ( ext-fname == strlen(fname)-1 )             /* dot is the last char */
        return RES_TEXT;

    ++ext;

    if ( strlen(ext) > 4 )                          /* extension too long */
        return RES_TEXT;

#ifdef DUMP
//  DBG("ext: [%s]", ext);
#endif

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
    else if ( 0==strcmp(uext, "SVG") )
        return RES_SVG;
    else if ( 0==strcmp(uext, "MP3") )
        return RES_AMPEG;
    else if ( 0==strcmp(uext, "EXE") )
        return RES_EXE;
    else if ( 0==strcmp(uext, "ZIP") )
        return RES_ZIP;

    return RES_TEXT;
}


/* --------------------------------------------------------------------------
   Convert URI (YYYY-MM-DD) date to tm struct
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

    for ( i=0; i<len; ++i )
    {
        if ( str[i] != '-' )
        {
//            DBG("str[i] = %c", str[i]);
            strtmp[j++] = str[i];
        }
        else    /* end of part */
        {
            strtmp[j] = EOS;

            if ( part == 'Y' )  /* year */
            {
                rec->year = atoi(strtmp);
                part = 'M';
            }
            else if ( part == 'M' )  /* month */
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
   Is year leap?
-------------------------------------------------------------------------- */
static bool leap(short year)
{
    year += 1900;
    
    if ( year % 4 == 0 && ((year % 100) != 0 || (year % 400) == 0) )
        return TRUE;
    
    return FALSE;
}


/* --------------------------------------------------------------------------
   Convert database datetime to epoch time
-------------------------------------------------------------------------- */
static time_t win_timegm(struct tm *t)
{
    time_t epoch;

    static int days[2][12]={
        {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
        {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}
    };

    int i;

    for ( i=70; i<t->tm_year; ++i )
        epoch += leap(i)?366:365;

    for ( i=0; i<t->tm_mon; ++i )
        epoch += days[leap(t->tm_year)][i];

    epoch += t->tm_mday - 1;
    epoch *= 24;

    epoch += t->tm_hour;
    epoch *= 60;

    epoch += t->tm_min;
    epoch *= 60;

    epoch += t->tm_sec;

    return epoch;
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
    epoch = win_timegm(&tm);
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
    epoch = win_timegm(&tm);
#endif

    return epoch;
}


/* --------------------------------------------------------------------------
   Convert epoch to HTTP time
-------------------------------------------------------------------------- */
char *time_epoch2http(time_t epoch)
{
static char str[32];

    G_ptm = gmtime(&epoch);
#ifdef _WIN32   /* Windows */
    strftime(str, 32, "%a, %d %b %Y %H:%M:%S GMT", G_ptm);
#else
    strftime(str, 32, "%a, %d %b %Y %T GMT", G_ptm);
#endif  /* _WIN32 */

    G_ptm = gmtime(&G_now);  /* make sure G_ptm is up to date */

//  DBG("time_epoch2http: [%s]", str);

    return str;
}


/* --------------------------------------------------------------------------
   Convert epoch to db time (YYYY-MM-DD HH:mm:ss)
-------------------------------------------------------------------------- */
char *time_epoch2db(time_t epoch)
{
static char str[20];

    G_ptm = gmtime(&epoch);

    strftime(str, 20, "%Y-%m-%d %H:%M:%S", G_ptm);

    G_ptm = gmtime(&G_now);  /* make sure G_ptm is up to date */

//    DBG("time_epoch2db: [%s]", str);

    return str;
}


/* --------------------------------------------------------------------------
   Set decimal & thousand separator
---------------------------------------------------------------------------*/
void lib_set_datetime_formats(const char *lang)
{
#ifdef DUMP
    DBG("lib_set_datetime_formats, lang [%s]", lang);
#endif

    /* date format */

    if ( 0==strcmp(lang, "EN-US") )
        M_df = 1;
    else if ( 0==strcmp(lang, "EN-GB") || 0==strcmp(lang, "EN-AU") || 0==strcmp(lang, "FR-FR") || 0==strcmp(lang, "EN-IE") || 0==strcmp(lang, "ES-ES") || 0==strcmp(lang, "IT-IT") || 0==strcmp(lang, "PT-PT") || 0==strcmp(lang, "PT-BR") || 0==strcmp(lang, "ES-AR") )
        M_df = 2;
    else if ( 0==strcmp(lang, "PL-PL") || 0==strcmp(lang, "RU-RU") || 0==strcmp(lang, "DE-CH") || 0==strcmp(lang, "FR-CH") )
        M_df = 3;
    else
        M_df = 0;

    /* amount format */

    if ( 0==strcmp(lang, "EN-US") || 0==strcmp(lang, "EN-GB") || 0==strcmp(lang, "EN-AU") || 0==strcmp(lang, "TH-TH") )
    {
        M_tsep = ',';
        M_dsep = '.';
    }
    else if ( 0==strcmp(lang, "PL-PL") || 0==strcmp(lang, "IT-IT") || 0==strcmp(lang, "NB-NO") || 0==strcmp(lang, "ES-ES") )
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
char *silgy_amt(double val)
{
static char str[64];

    amtd(str, val);

    return str;
}


/* --------------------------------------------------------------------------
   Format amount
---------------------------------------------------------------------------*/
void amt(char *stramt, long long in_amt)
{
    char    in_stramt[256];
    int     len;
    int     i, j=0;
    bool    minus=FALSE;

    sprintf(in_stramt, "%lld", in_amt);

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

    sanitize_sql(dst, str, MAX_LONG_URI_VAL_LEN);

    return dst;
}


/* --------------------------------------------------------------------------
   HTML-escape string
-------------------------------------------------------------------------- */
char *silgy_html_esc(const char *str)
{
static char dst[MAX_LONG_URI_VAL_LEN+1];

    sanitize_html(dst, str, MAX_LONG_URI_VAL_LEN);

    return dst;
}


/* --------------------------------------------------------------------------
   SQL-escape string respecting destination length (excluding '\0')
-------------------------------------------------------------------------- */
void sanitize_sql_old(char *dest, const char *str, int len)
{
    strncpy(dest, silgy_sql_esc(str), len);
    dest[len] = EOS;

    /* cut off orphaned single backslash */

    int i=len-1;
    int bs=0;
    while ( dest[i]=='\\' && i>-1 )
    {
        ++bs;
        i--;
    }

    if ( bs % 2 )   /* odd number of trailing backslashes -- cut one */
        dest[len-1] = EOS;
}


/* --------------------------------------------------------------------------
   SQL-escape string respecting destination length (excluding '\0')
-------------------------------------------------------------------------- */
void sanitize_sql(char *dst, const char *str, int len)
{
    int i=0, j=0;

    while ( str[i] )
    {
        if ( j > len-3 )
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
}


/* --------------------------------------------------------------------------
   HTML-escape string respecting destination length (excluding '\0')
-------------------------------------------------------------------------- */
void sanitize_html(char *dst, const char *str, int len)
{
    int i=0, j=0;

    while ( str[i] )
    {
        if ( j > len-7 )
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
        else if ( str[i] == '"' )
        {
            dst[j++] = '&';
            dst[j++] = 'q';
            dst[j++] = 'u';
            dst[j++] = 'o';
            dst[j++] = 't';
            dst[j++] = ';';
        }
/*        else if ( str[i] == '\\' )
        {
            dst[j++] = '\\';
            dst[j++] = '\\';
        }*/
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
#ifdef HTML_ESCAPE_NEW_LINES
        else if ( str[i] == '\n' )
        {
            dst[j++] = '<';
            dst[j++] = 'b';
            dst[j++] = 'r';
            dst[j++] = '>';
        }
#endif  /* HTML_ESCAPE_NEW_LINES */
        else if ( str[i] != '\r' )
            dst[j++] = str[i];
        ++i;
    }

    dst[j] = EOS;
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
   Convert string to upper
---------------------------------------------------------------------------*/
char *upper(const char *str)
{
static char upper[4096];
    int     i;

    for ( i=0; str[i] && i<4095; ++i )
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



#ifndef SILGY_CLIENT
/* --------------------------------------------------------------------------
   Return a random 8-bit number from M_random_numbers
-------------------------------------------------------------------------- */
static unsigned char get_random_number()
{
    static int i=0;

    if ( M_random_initialized )
    {
        if ( i >= RANDOM_NUMBERS )  /* refill the pool with fresh numbers */
        {
            init_random_numbers();
            i = 0;
        }
        return M_random_numbers[i++];
    }
    else
    {
        WAR("Using get_random_number() before M_random_initialized");
        return rand() % 256;
    }
}


/* --------------------------------------------------------------------------
   Seed rand()
-------------------------------------------------------------------------- */
static void seed_rand()
{
#define SILGY_SEEDS_MEM 256  /* remaining 8 bits & last seeds to remember */
static char first=1;
/* make sure at least the last SILGY_SEEDS_MEM seeds are unique */
static unsigned int seeds[SILGY_SEEDS_MEM];

    DBG("seed_rand");

    /* generate possibly random, or at least based on some non-deterministic factors, 32-bit integer */

    int time_remainder = (int)G_now % 63 + 1;     /* 6 bits */
    int mem_remainder = lib_get_memory() % 63 + 1;    /* 6 bits */
    int pid_remainder;       /* 6 bits */
    int yesterday_rem;    /* 6 bits */

    if ( first )    /* initial seed */
    {
        pid_remainder = G_pid % 63 + 1;
        yesterday_rem = G_cnts_yesterday.req % 63 + 1;
    }
    else    /* subsequent calls */
    {
        pid_remainder = rand() % 63 + 1;
        yesterday_rem = rand() % 63 + 1;
    }

static int seeded=0;    /* 8 bits */

    unsigned int seed;
static unsigned int prev_seed=0;

    while ( 1 )
    {
        if ( seeded >= SILGY_SEEDS_MEM )
            seeded = 1;
        else
            ++seeded;

        seed = pid_remainder * time_remainder * mem_remainder * yesterday_rem * seeded;

        /* check uniqueness in the history */

        char found=0;
        int i = 0;
        while ( i < SILGY_SEEDS_MEM )
        {
            if ( seeds[i++] == seed )
            {
                found = 1;
                break;
            }
        }

        if ( found )    /* same seed again */
        {
            WAR("seed %u repeated; seeded = %d, i = %d", seed, seeded, i);
        }
        else   /* seed not found ==> OK */
        {
            /* new seed needs to be at least 10000 apart from the previous one */

            if ( !first && abs((long long)(seed-prev_seed)) < 10000 )
            {
                WAR("seed %u too close to the previous one (%u); seeded = %d", seed, prev_seed, seeded);
            }
            else    /* everything OK */
            {
                seeds[seeded-1] = seed;
                break;
            }
        }

        /* stir it up to avoid endless loop */

        pid_remainder = rand() % 63 + 1;
        time_remainder = rand() % 63 + 1;
    }

    char f[256];
    amt(f, seed);
    DBG("seed = %s", f);
    DBG("");

    prev_seed = seed;

    first = 0;

    srand(seed);
}


/* --------------------------------------------------------------------------
   Initialize M_random_numbers array
-------------------------------------------------------------------------- */
void init_random_numbers()
{
    int i;

#ifdef DUMP
    struct timespec start;
#ifdef _WIN32
    clock_gettime_win(&start);
#else
    clock_gettime(MONOTONIC_CLOCK_NAME, &start);
#endif
#endif  /* DUMP */

    DBG("init_random_numbers");

    seed_rand();

#ifdef __linux__
    /* On Linux we have access to a hardware-influenced RNG */

    DBG("Trying /dev/urandom...");

    int urandom_fd = open("/dev/urandom", O_RDONLY);

    if ( urandom_fd )
    {
        read(urandom_fd, &M_random_numbers, RANDOM_NUMBERS);

        close(urandom_fd);

        INF("M_random_numbers obtained from /dev/urandom");
    }
    else
    {
        WAR("Couldn't open /dev/urandom");

        /* fallback to old plain rand(), seeded the best we could */

        for ( i=0; i<RANDOM_NUMBERS; ++i )
            M_random_numbers[i] = rand() % 256;

        INF("M_random_numbers obtained from rand()");
    }

#else   /* Windows */

    for ( i=0; i<RANDOM_NUMBERS; ++i )
        M_random_numbers[i] = rand() % 256;

    INF("M_random_numbers obtained from rand()");

#endif

    INF("");

    M_random_initialized = 1;

#ifdef DUMP
    DBG("--------------------------------------------------------------------------------------------------------------------------------");
    DBG("M_random_numbers distribution visualization");
    DBG("The square below should be filled fairly randomly and uniformly.");
    DBG("If it's not, or you can see any regular patterns across the square, your system may be broken or too old to be deemed secure.");
    DBG("--------------------------------------------------------------------------------------------------------------------------------");

    /* One square takes two columns, so we can have between 0 and 4 dots per square */

#define SQUARE_ROWS             64
#define SQUARE_COLS             SQUARE_ROWS*2
#define SQUARE_IS_EMPTY(x, y)   (dots[y][x*2]==' ' && dots[y][x*2+1]==' ')
#define SQUARE_HAS_ONE(x, y)    (dots[y][x*2]==' ' && dots[y][x*2+1]=='.')
#define SQUARE_HAS_TWO(x, y)    (dots[y][x*2]=='.' && dots[y][x*2+1]=='.')
#define SQUARE_HAS_THREE(x, y)  (dots[y][x*2]=='.' && dots[y][x*2+1]==':')
#define SQUARE_HAS_FOUR(x, y)   (dots[y][x*2]==':' && dots[y][x*2+1]==':')

    char dots[SQUARE_ROWS][SQUARE_COLS+1]={0};
    int j;

    for ( i=0; i<SQUARE_ROWS; ++i )
        for ( j=0; j<SQUARE_COLS; ++j )
            dots[i][j] = ' ';

    /* we only have SQUARE_ROWS^2 squares with 5 possible values in each of them */
    /* let only once in divider in */

    int divider = RANDOM_NUMBERS / (SQUARE_ROWS*SQUARE_ROWS) + 1;

    for ( i=0; i<RANDOM_NUMBERS-1; i+=2 )
    {
        if ( i % divider ) continue;

        int x = M_random_numbers[i] % SQUARE_ROWS;
        int y = M_random_numbers[i+1] % SQUARE_ROWS;

        if ( SQUARE_IS_EMPTY(x, y) )    /* make it one */
            dots[y][x*2+1] = '.';
        else if ( SQUARE_HAS_ONE(x, y) )    /* make it two */
            dots[y][x*2] = '.';
        else if ( SQUARE_HAS_TWO(x, y) )    /* make it three */
            dots[y][x*2+1] = ':';
        else if ( SQUARE_HAS_THREE(x, y) )  /* make it four */
            dots[y][x*2] = ':';
    }

    for ( i=0; i<SQUARE_ROWS; ++i )
        DBG(dots[i]);

    DBG("--------------------------------------------------------------------------------------------------------------------------------");
    DBG("");
    DBG("init_random_numbers took %.3lf ms", lib_elapsed(&start));
    DBG("");
#endif
}


/* --------------------------------------------------------------------------
   Generate random string
   Generates FIPS-compliant random sequences (tested with Burp)
-------------------------------------------------------------------------- */
void silgy_random(char *dest, int len)
{
const char  *chars="abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
static int  since_seed=0;
    int     i;

#ifdef DUMP
    struct timespec start;
#ifdef _WIN32
    clock_gettime_win(&start);
#else
    clock_gettime(MONOTONIC_CLOCK_NAME, &start);
#endif
#endif  /* DUMP */

    if ( since_seed > (G_cnts_today.req % 246 + 10) )  /* seed every now and then */
//    if ( 1 )  /* test */
    {
        seed_rand();
        since_seed = 0;
    }
    else
    {
        ++since_seed;
#ifdef DUMP
        DBG("since_seed = %d", since_seed);
#endif
    }

#ifdef DUMP
    DBG_LINE;
#endif

    int r;

    for ( i=0; i<len; ++i )
    {
        /* source random numbers from two different sets: 'normal' and 'lucky' */

        if ( get_random_number() % 3 == 0 )
        {
#ifdef DUMP
            DBG("i=%d lucky", i);
#endif
            r = get_random_number();
            while ( r > 247 ) r = get_random_number();   /* avoid modulo bias -- 62*4 - 1 */
        }
        else
        {
#ifdef DUMP
            DBG("i=%d normal", i);
#endif
            r = rand() % 256;
            while ( r > 247 ) r = rand() % 256;
        }

        dest[i] = chars[r % 62];
    }

    dest[i] = EOS;

#ifdef DUMP
    DBG_LINE;
#endif

#ifdef DUMP
    DBG("silgy_random took %.3lf ms", lib_elapsed(&start));
#endif
}
#endif  /* SILGY_CLIENT */


/* --------------------------------------------------------------------------
   Sleep for n miliseconds
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
#endif  /* AUTO_INIT_EXPERIMENT */


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
        else if ( json->rec[i].type==JSON_INTEGER || json->rec[i].type==JSON_UNSIGNED || json->rec[i].type==JSON_FLOAT || json->rec[i].type==JSON_DOUBLE || json->rec[i].type==JSON_BOOL )
        {
            p = stpcpy(p, json->rec[i].value);
        }
        else if ( json->rec[i].type == JSON_RECORD )
        {
            intptr_t jp;
            sscanf(json->rec[i].value, "%p", &jp);
            char tmp[JSON_BUFSIZE];
            json_to_string(tmp, (JSON*)jp, FALSE);
            p = stpcpy(p, tmp);
        }
        else if ( json->rec[i].type == JSON_ARRAY )
        {
            intptr_t jp;
            sscanf(json->rec[i].value, "%p", &jp);
            char tmp[JSON_BUFSIZE];
            json_to_string(tmp, (JSON*)jp, TRUE);
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

static char dst[4096];
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
        else if ( json->rec[i].type==JSON_INTEGER || json->rec[i].type==JSON_UNSIGNED || json->rec[i].type==JSON_FLOAT || json->rec[i].type==JSON_DOUBLE || json->rec[i].type==JSON_BOOL )
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
            intptr_t jp;
            sscanf(json->rec[i].value, "%p", &jp);
            char tmp[JSON_BUFSIZE];
            json_to_string_pretty(tmp, (JSON*)jp, FALSE, level+1);
            p = stpcpy(p, tmp);
        }
        else if ( json->rec[i].type == JSON_ARRAY )
        {
            if ( !array || i > 0 )
            {
                p = stpcpy(p, "\n");
                p = stpcpy(p, json_indent(level));
            }
            intptr_t jp;
            sscanf(json->rec[i].value, "%p", &jp);
            char tmp[JSON_BUFSIZE];
            json_to_string_pretty(tmp, (JSON*)jp, TRUE, level+1);
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
    bool    in_quotes=0, escape=0;

#ifdef DUMP
    int len = strlen(src);
    DBG("get_json_closing_bracket: len = %d", len);
//    log_long(src, len, "get_json_closing_bracket");
#endif  /* DUMP */

    while ( src[i] )
    {
        if ( escape )
        {
            escape = 0;
        }
        else if ( src[i]=='\\' && in_quotes )
        {
            escape = 1;
        }
        else if ( src[i]=='"' )
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
                return (char*)(src+i);
            else
                subs--;
        }

        ++i;
    }

#ifdef DUMP
    DBG("get_json_closing_bracket: not found");
#endif

    return NULL;
}


/* --------------------------------------------------------------------------
   Find matching closing square bracket in JSON string
-------------------------------------------------------------------------- */
static char *get_json_closing_square_bracket(const char *src)
{
    int     i=1, subs=0;
    bool    in_quotes=0, escape=0;

#ifdef DUMP
    int len = strlen(src);
    DBG("get_json_closing_square_bracket: len = %d", len);
//    log_long(src, len, "get_json_closing_square_bracket");
#endif  /* DUMP */

    while ( src[i] )
    {
#ifdef DUMP
//        DBG("%c", src[i]);
#endif
        if ( escape )
        {
            escape = 0;
        }
        else if ( src[i]=='\\' && in_quotes )
        {
            escape = 1;
        }
        else if ( src[i]=='"' )
        {
#ifdef DUMP
//            DBG("in_quotes = %d", in_quotes);
#endif
            if ( in_quotes )
                in_quotes = 0;
            else
                in_quotes = 1;
#ifdef DUMP
//            DBG("in_quotes switched to %d", in_quotes);
#endif
        }
        else if ( src[i]=='[' && !in_quotes )
        {
            ++subs;
#ifdef DUMP
//            DBG("subs+1 = %d", subs);
#endif
        }
        else if ( src[i]==']' && !in_quotes )
        {
            if ( subs<1 )
                return (char*)(src+i);
            else
                subs--;
#ifdef DUMP
//            DBG("subs-1 = %d", subs);
#endif
        }

        ++i;
    }

#ifdef DUMP
    DBG("get_json_closing_square_bracket: not found");
#endif

    return NULL;
}


/* --------------------------------------------------------------------------
   Convert JSON string to Silgy JSON format
-------------------------------------------------------------------------- */
bool lib_json_from_string(JSON *json, const char *src, int len, int level)
{
    int     i=0, j=0;
    char    key[JSON_KEY_LEN+1];
    char    value[JSON_VAL_LEN+1];
    int     index;
    char    now_key=0, now_value=0, inside_array=0, type;
    float   flo_value;
    double  dbl_value;

static JSON json_pool[JSON_POOL_SIZE*JSON_MAX_LEVELS];
static int  json_pool_cnt[JSON_MAX_LEVELS]={0};

    if ( len == 0 ) len = strlen(src);

    if ( level == 0 )
    {
        lib_json_reset(json);

        while ( i<len && src[i] != '{' ) ++i;   /* skip junk if there's any */

        if ( src[i] != '{' )    /* no opening bracket */
        {
            WAR("JSON syntax error -- no opening curly bracket");
            return FALSE;
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
static char tmp[JSON_BUFSIZE];
    strncpy(tmp, src+i, len-i);
    tmp[len-i] = EOS;
    char debug[64];
    sprintf(debug, "lib_json_from_string level %d", level);
    log_long(tmp, len, debug);
    if ( inside_array ) DBG("inside_array");
#endif  /* DUMP */

    for ( i; i<len; ++i )
    {
        if ( !now_key && !now_value )
        {
            while ( i<len && (src[i]==' ' || src[i]=='\t' || src[i]=='\r' || src[i]=='\n') ) ++i;

            if ( !inside_array && src[i]=='"' )  /* start of key */
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
#endif  /* DUMP */
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
                    WAR("JSON syntax error -- no colon after name");
                    return FALSE;
                }

                ++i;    /* skip ':' */
            }

            while ( i<len && (src[i]==' ' || src[i]=='\t' || src[i]=='\r' || src[i]=='\n') ) ++i;

            if ( i==len )
            {
                WAR("JSON syntax error -- expected value");
                return FALSE;
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
                    if ( json_pool_cnt[level] >= JSON_POOL_SIZE ) json_pool_cnt[level] = 0;   /* overwrite previous ones */

                    int pool_idx = JSON_POOL_SIZE*level + json_pool_cnt[level];
                    lib_json_reset(&json_pool[pool_idx]);
                    /* save the pointer first as a parent record */
                    if ( inside_array )
                        lib_json_add_record(json, NULL, &json_pool[pool_idx], FALSE, index);
                    else
                        lib_json_add_record(json, key, &json_pool[pool_idx], FALSE, -1);
                    /* fill in the destination (children) */
                    char *closing;
                    if ( (closing=get_json_closing_bracket(src+i)) )
                    {
//                        DBG("closing [%s], len=%d", closing, closing-(src+i));
                        if ( !lib_json_from_string(&json_pool[pool_idx], src+i, closing-(src+i)+1, level+1) )
                            return FALSE;
                        ++json_pool_cnt[level];
                        i += closing-(src+i);
//                        DBG("after closing record bracket [%s]", src+i);
                    }
                    else    /* syntax error */
                    {
                        WAR("No closing bracket in JSON record");
                        return FALSE;
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
                    if ( json_pool_cnt[level] >= JSON_POOL_SIZE ) json_pool_cnt[level] = 0;   /* overwrite previous ones */

                    int pool_idx = JSON_POOL_SIZE*level + json_pool_cnt[level];
                    lib_json_reset(&json_pool[pool_idx]);
                    /* save the pointer first as a parent record */
                    if ( inside_array )
                        lib_json_add_record(json, NULL, &json_pool[pool_idx], TRUE, index);
                    else
                        lib_json_add_record(json, key, &json_pool[pool_idx], TRUE, -1);
                    /* fill in the destination (children) */
                    char *closing;
                    if ( (closing=get_json_closing_square_bracket(src+i)) )
                    {
//                        DBG("closing [%s], len=%d", closing, closing-(src+i));
                        if ( !lib_json_from_string(&json_pool[pool_idx], src+i, closing-(src+i)+1, level+1) )
                            return FALSE;
                        ++json_pool_cnt[level];
                        i += closing-(src+i);
//                        DBG("after closing array bracket [%s]", src+i);
                    }
                    else    /* syntax error */
                    {
                        WAR("No closing square bracket in JSON array");
                        return FALSE;
                    }
                }
            }
            else    /* number */
            {
#ifdef DUMP
                DBG("JSON_INTEGER || JSON_UNSIGNED || JSON_FLOAT || JSON_DOUBLE || JSON_BOOL");
#endif
                type = JSON_INTEGER;    /* we're not sure yet but need to mark it's definitely not STRING */

                i--;

                now_value = 1;
                j = 0;
            }
        }
        else if ( now_value
                    && ((type==JSON_STRING && src[i]=='"' && src[i-1]!='\\')
                            || (type!=JSON_STRING && (src[i]==',' || src[i]=='}' || src[i]==']' || src[i]=='\r' || src[i]=='\n'))) )     /* end of value */
        {
            value[j] = EOS;
#ifdef DUMP
            DBG("value [%s]", value);
#endif
            if ( type==JSON_STRING ) ++i;   /* skip closing '"' */

            /* src[i] should now be at ',' */

            if ( inside_array )
            {
                if ( type==JSON_STRING )
                    lib_json_add(json, NULL, value, 0, 0, 0, 0, JSON_STRING, index);
                else if ( value[0]=='t' )
                    lib_json_add(json, NULL, NULL, 1, 0, 0, 0, JSON_BOOL, index);
                else if ( value[0]=='f' )
                    lib_json_add(json, NULL, NULL, 0, 0, 0, 0, JSON_BOOL, index);
                else if ( strchr(value, '.') )
                {
                    if ( strlen(value) <= JSON_MAX_FLOAT_LEN )
                    {
                        sscanf(value, "%f", &flo_value);
                        lib_json_add(json, NULL, NULL, 0, 0, flo_value, 0, JSON_FLOAT, index);
                    }
                    else    /* double */
                    {
                        sscanf(value, "%lf", &dbl_value);
                        lib_json_add(json, NULL, NULL, 0, 0, 0, dbl_value, JSON_DOUBLE, index);
                    }
                }
                else if ( value[0] == '-' || strtoul(value, NULL, 10) < INT_MAX )
                    lib_json_add(json, NULL, NULL, atoi(value), 0, 0, 0, JSON_INTEGER, index);
                else    /* unsigned */
                    lib_json_add(json, NULL, NULL, 0, (unsigned)strtoul(value, NULL, 10), 0, 0, JSON_UNSIGNED, index);
            }
            else    /* not an array */
            {
                if ( type==JSON_STRING )
                    lib_json_add(json, key, value, 0, 0, 0, 0, JSON_STRING, -1);
                else if ( value[0]=='t' )
                    lib_json_add(json, key, NULL, 1, 0, 0, 0, JSON_BOOL, -1);
                else if ( value[0]=='f' )
                    lib_json_add(json, key, NULL, 0, 0, 0, 0, JSON_BOOL, -1);
                else if ( strchr(value, '.') )
                {
                    if ( strlen(value) <= JSON_MAX_FLOAT_LEN )
                    {
                        sscanf(value, "%f", &flo_value);
                        lib_json_add(json, key, NULL, 0, 0, flo_value, 0, JSON_FLOAT, -1);
                    }
                    else    /* double */
                    {
                        sscanf(value, "%lf", &dbl_value);
                        lib_json_add(json, key, NULL, 0, 0, 0, dbl_value, JSON_DOUBLE, -1);
                    }
                }
                else if ( value[0] == '-' || strtoul(value, NULL, 10) < INT_MAX )
                    lib_json_add(json, key, NULL, atoi(value), 0, 0, 0, JSON_INTEGER, -1);
                else    /* unsigned */
                    lib_json_add(json, key, NULL, 0, (unsigned)strtoul(value, NULL, 10), 0, 0, JSON_UNSIGNED, -1);
            }

            now_value = 0;

            if ( src[i]==',' ) i--;     /* we need it to detect the next array element */
        }
        else if ( now_key )
        {
            if ( j < JSON_KEY_LEN )
                key[j++] = src[i];
        }
        else if ( now_value )
        {
            if ( j < JSON_VAL_LEN )
                value[j++] = src[i];
        }

//        if ( src[i-2]=='}' && !now_value && level==0 )    /* end of JSON */
//            break;
    }

    return TRUE;
}


/* --------------------------------------------------------------------------
   Log JSON buffer
-------------------------------------------------------------------------- */
void lib_json_log_dbg(JSON *json, const char *name)
{
    int     i;
    char    type[32];

    DBG_LINE;

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
        else if ( json->rec[i].type == JSON_UNSIGNED )
            strcpy(type, "JSON_UNSIGNED");
        else if ( json->rec[i].type == JSON_FLOAT )
            strcpy(type, "JSON_FLOAT");
        else if ( json->rec[i].type == JSON_DOUBLE )
            strcpy(type, "JSON_DOUBLE");
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

    DBG_LINE;
}


/* --------------------------------------------------------------------------
   Log JSON buffer
-------------------------------------------------------------------------- */
void lib_json_log_inf(JSON *json, const char *name)
{
    int     i;
    char    type[32];

    INF_LINE;

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
        else if ( json->rec[i].type == JSON_UNSIGNED )
            strcpy(type, "JSON_UNSIGNED");
        else if ( json->rec[i].type == JSON_FLOAT )
            strcpy(type, "JSON_FLOAT");
        else if ( json->rec[i].type == JSON_DOUBLE )
            strcpy(type, "JSON_DOUBLE");
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

    INF_LINE;
}


/* --------------------------------------------------------------------------
   Add/set value to a JSON buffer
-------------------------------------------------------------------------- */
bool lib_json_add(JSON *json, const char *name, const char *str_value, int int_value, unsigned uint_value, float flo_value, double dbl_value, char type, int i)
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
            strncpy(json->rec[i].name, name, JSON_KEY_LEN);
            json->rec[i].name[JSON_KEY_LEN] = EOS;
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
        strncpy(json->rec[i].value, str_value, JSON_VAL_LEN);
        json->rec[i].value[JSON_VAL_LEN] = EOS;
    }
    else if ( type == JSON_INTEGER )
    {
        sprintf(json->rec[i].value, "%d", int_value);
    }
    else if ( type == JSON_UNSIGNED )
    {
        sprintf(json->rec[i].value, "%u", uint_value);
    }
    else if ( type == JSON_FLOAT )
    {
        snprintf(json->rec[i].value, JSON_VAL_LEN, "%f", flo_value);
    }
    else if ( type == JSON_DOUBLE )
    {
        snprintf(json->rec[i].value, JSON_VAL_LEN, "%lf", dbl_value);
    }
    else if ( type == JSON_BOOL )
    {
        if ( int_value )
            strcpy(json->rec[i].value, "true");
        else
            strcpy(json->rec[i].value, "false");
    }
    else
    {
        COPY(json->rec[i].value, "Invalid type!", JSON_VAL_LEN);
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
            strncpy(json->rec[i].name, name, JSON_KEY_LEN);
            json->rec[i].name[JSON_KEY_LEN] = EOS;
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

    sprintf(json->rec[i].value, "%p", json_sub);

    json->rec[i].type = is_array?JSON_ARRAY:JSON_RECORD;

    return TRUE;
}


/* --------------------------------------------------------------------------
   Check value presence in JSON buffer
-------------------------------------------------------------------------- */
bool lib_json_present(JSON *json, const char *name)
{
    int i;

    for ( i=0; i<json->cnt; ++i )
    {
        if ( 0==strcmp(json->rec[i].name, name) )
            return TRUE;
    }

    return FALSE;
}


/* --------------------------------------------------------------------------
   Get value from JSON buffer
-------------------------------------------------------------------------- */
char *lib_json_get_str(JSON *json, const char *name, int i)
{
static char dst[JSON_VAL_LEN+1];

    if ( !name )    /* array elem */
    {
        if ( i >= json->cnt )
        {
            ERR("lib_json_get_str index (%d) out of bound (max = %d)", i, json->cnt-1);
            dst[0] = EOS;
            return dst;
        }

        if ( json->rec[i].type==JSON_STRING || json->rec[i].type==JSON_INTEGER || json->rec[i].type==JSON_UNSIGNED || json->rec[i].type==JSON_FLOAT || json->rec[i].type==JSON_DOUBLE || json->rec[i].type==JSON_BOOL )
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
            if ( json->rec[i].type==JSON_STRING || json->rec[i].type==JSON_INTEGER || json->rec[i].type==JSON_UNSIGNED || json->rec[i].type==JSON_FLOAT || json->rec[i].type==JSON_DOUBLE || json->rec[i].type==JSON_BOOL )
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
int lib_json_get_int(JSON *json, const char *name, int i)
{
    if ( !name )    /* array elem */
    {
        if ( i >= json->cnt )
        {
            ERR("lib_json_get_int index (%d) out of bound (max = %d)", i, json->cnt-1);
            return 0;
        }

        if ( json->rec[i].type == JSON_INTEGER )
            return atoi(json->rec[i].value);
        else if ( json->rec[i].type == JSON_UNSIGNED && strtoul(json->rec[i].value, NULL, 10) < INT_MAX )
            return (int)strtoul(json->rec[i].value, NULL, 10);
        else    /* types don't match */
            return 0;
    }

    for ( i=0; i<json->cnt; ++i )
    {
        if ( 0==strcmp(json->rec[i].name, name) )
        {
            if ( json->rec[i].type == JSON_INTEGER )
                return atoi(json->rec[i].value);
            else if ( json->rec[i].type == JSON_UNSIGNED && strtoul(json->rec[i].value, NULL, 10) < INT_MAX )
                return (int)strtoul(json->rec[i].value, NULL, 10);

            return 0;   /* types don't match or couldn't convert */
        }
    }

    return 0;   /* no such field */
}


/* --------------------------------------------------------------------------
   Get value from JSON buffer
-------------------------------------------------------------------------- */
unsigned lib_json_get_uint(JSON *json, const char *name, int i)
{
    if ( !name )    /* array elem */
    {
        if ( i >= json->cnt )
        {
            ERR("lib_json_get_uint index (%d) out of bound (max = %d)", i, json->cnt-1);
            return 0;
        }

        if ( json->rec[i].type == JSON_UNSIGNED )
        {
            return (unsigned)strtoul(json->rec[i].value, NULL, 10);
        }
        else if ( json->rec[i].type == JSON_INTEGER )
        {
            int tmp = atoi(json->rec[i].value);

            if ( tmp >= 0 )
                return (unsigned)tmp;
            else
                WAR("lib_json_get_uint value < 0");
        }
        else    /* types don't match */
            return 0;
    }

    for ( i=0; i<json->cnt; ++i )
    {
        if ( 0==strcmp(json->rec[i].name, name) )
        {
            if ( json->rec[i].type == JSON_UNSIGNED )
            {
                return (unsigned)strtoul(json->rec[i].value, NULL, 10);
            }
            else if ( json->rec[i].type == JSON_INTEGER )
            {
                int tmp = atoi(json->rec[i].value);

                if ( tmp >= 0 )
                    return (unsigned)tmp;
                else
                    WAR("lib_json_get_uint value < 0");
            }

            return 0;   /* types don't match or couldn't convert */
        }
    }

    return 0;   /* no such field */
}


/* --------------------------------------------------------------------------
   Get value from JSON buffer
-------------------------------------------------------------------------- */
float lib_json_get_float(JSON *json, const char *name, int i)
{
    float flo_value;

    if ( !name )    /* array elem */
    {
        if ( i >= json->cnt )
        {
            ERR("lib_json_get_float index (%d) out of bound (max = %d)", i, json->cnt-1);
            return 0;
        }

        if ( json->rec[i].type == JSON_FLOAT )
        {
            sscanf(json->rec[i].value, "%f", &flo_value);
            return flo_value;
        }
        else    /* types don't match */
            return 0;
    }

    for ( i=0; i<json->cnt; ++i )
    {
        if ( 0==strcmp(json->rec[i].name, name) )
        {
            if ( json->rec[i].type == JSON_FLOAT )
            {
                sscanf(json->rec[i].value, "%f", &flo_value);
                return flo_value;
            }

            return 0;   /* types don't match or couldn't convert */
        }
    }

    return 0;   /* no such field */
}


/* --------------------------------------------------------------------------
   Get value from JSON buffer
-------------------------------------------------------------------------- */
double lib_json_get_double(JSON *json, const char *name, int i)
{
    double dbl_value;

    if ( !name )    /* array elem */
    {
        if ( i >= json->cnt )
        {
            ERR("lib_json_get_double index (%d) out of bound (max = %d)", i, json->cnt-1);
            return 0;
        }

        if ( json->rec[i].type == JSON_FLOAT || json->rec[i].type == JSON_DOUBLE )
        {
            sscanf(json->rec[i].value, "%lf", &dbl_value);
            return dbl_value;
        }
        else    /* types don't match */
            return 0;
    }

    for ( i=0; i<json->cnt; ++i )
    {
        if ( 0==strcmp(json->rec[i].name, name) )
        {
            if ( json->rec[i].type == JSON_FLOAT || json->rec[i].type == JSON_DOUBLE )
            {
                sscanf(json->rec[i].value, "%lf", &dbl_value);
                return dbl_value;
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
            intptr_t jp;
            sscanf(json->rec[i].value, "%p", &jp);
            memcpy(json_sub, (JSON*)jp, sizeof(JSON));
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
                intptr_t jp;
                sscanf(json->rec[i].value, "%p", &jp);
                memcpy(json_sub, (JSON*)jp, sizeof(JSON));
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
static void get_byteorder32()
{
    union {
        intptr_t p;
        char c[4];
    } test;

    DBG("Checking 32-bit endianness...");

    memset(&test, 0, sizeof(test));

    test.p = 1;

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
        intptr_t p;
        char c[8];
    } test;

    DBG("Checking 64-bit endianness...");

    INF("sizeof(long) = %d", sizeof(long));

    memset(&test, 0, sizeof(test));

    test.p = 1;

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
   Check system's endianness
-------------------------------------------------------------------------- */
void get_byteorder()
{
    if ( sizeof(intptr_t) == 4 )
        get_byteorder32();
    else if ( sizeof(intptr_t) == 8 )
        get_byteorder64();
}


/* --------------------------------------------------------------------------
   Convert database datetime to epoch time
-------------------------------------------------------------------------- */
time_t db2epoch(const char *str)
{
    time_t  epoch;
    int     i;
    int     j=0;
    char    part='Y';
    char    tmp[8];
struct tm   t={0};

/*  DBG("db2epoch: str: [%s]", str); */

    for ( i=0; str[i]; ++i )
    {
        if ( isdigit(str[i]) )
        {
            tmp[j++] = str[i];
        }
        else    /* end of part */
        {
            tmp[j] = EOS;

            if ( part == 'Y' )  /* year */
            {
                t.tm_year = atoi(tmp) - 1900;
                part = 'M';
            }
            else if ( part == 'M' )  /* month */
            {
                t.tm_mon = atoi(tmp) - 1;
                part = 'D';
            }
            else if ( part == 'D' )  /* day */
            {
                t.tm_mday = atoi(tmp);
                part = 'H';
            }
            else if ( part == 'H' )  /* hour */
            {
                t.tm_hour = atoi(tmp);
                part = 'm';
            }
            else if ( part == 'm' )  /* minutes */
            {
                t.tm_min = atoi(tmp);
                part = 's';
            }

            j = 0;
        }
    }

    /* seconds */

    tmp[j] = EOS;
    t.tm_sec = atoi(tmp);

#ifdef __linux__
    epoch = timegm(&t);
#else
    epoch = win_timegm(&t);
#endif

    return epoch;
}


/* --------------------------------------------------------------------------
   Send an email
-------------------------------------------------------------------------- */
bool silgy_email(const char *to, const char *subject, const char *message)
{
    DBG("Sending email to [%s], subject [%s]", to, subject);

#ifndef _WIN32
    char    sender[512];
    char    comm[512];

//#ifndef SILGY_SVC   /* web server mode */

//    sprintf(sender, "%s <noreply@%s>", conn[ci].website, conn[ci].host);

    /* happens when using non-standard port */

//    char    *colon;
//    if ( G_test && (colon=strchr(sender, ':')) )
//    {
//        *colon = '>';
//        *(++colon) = EOS;
//        DBG("sender truncated to [%s]", sender);
//    }
//#else
    sprintf(sender, "%s <%s@%s>", APP_WEBSITE, EMAIL_FROM_USER, APP_DOMAIN);
//#endif  /* SILGY_SVC */

    sprintf(comm, "/usr/lib/sendmail -t -f \"%s\"", sender);

    FILE *mailpipe = popen(comm, "w");

    if ( mailpipe == NULL )
    {
        ERR("Failed to invoke sendmail");
        return FALSE;
    }
    else
    {
        fprintf(mailpipe, "From: %s\n", sender);
        fprintf(mailpipe, "To: %s\n", to);
        fprintf(mailpipe, "Subject: %s\n", subject);
        fprintf(mailpipe, "Content-Type: text/plain; charset=\"utf-8\"\n\n");
        fwrite(message, strlen(message), 1, mailpipe);
        fwrite("\n.\n", 3, 1, mailpipe);
        pclose(mailpipe);
    }

    return TRUE;

#else   /* Windows */

    WAR("There's no email service for Windows");
    return TRUE;

#endif  /* _WIN32 */
}


/* --------------------------------------------------------------------------
   Convert string to quoted-printable
-------------------------------------------------------------------------- */
/*void qp(char *dst, const char *asrc)
{
    char *src=(char*)asrc;
    int curr_line_length = 0;
    bool first=TRUE;

    while ( *src )
    {
        if ( curr_line_length > 72 )
        {
            // insert '=' if prev char exists and is not a space
            if ( !first )
            {
                if (*(src-1) != 0x20)
                    *dst++ = '=';
            }
            *dst++ = '\n';
            curr_line_length = 0;
        }

        if ( *src == 0x20 )
        {
            *dst++ = *src;
        }
        else if ( *src >= 33 && *src <= 126 && *src != 61 )
        {
            *dst++ = *src;
            // double escape newline periods
            // http://tools.ietf.org/html/rfc5321#section-4.5.2
            if ( curr_line_length == 0 && *src == 46 )
            {
                *dst++ = '.';
            }
        }
        else
        {
            *dst++ = '=';
            char hex[8];
            sprintf(hex, "%x", (*src >> 4) & 0x0F);
            dst = stpcpy(dst, hex);
            sprintf(hex, "%x", *src & 0x0F);
            dst = stpcpy(dst, hex);
            // 2 more chars bc hex and equals
            curr_line_length += 2;
        }

        ++curr_line_length;
        first = FALSE;
        ++src;
    }

    *dst = EOS;
}*/


/* --------------------------------------------------------------------------
   Send an email with attachement
-------------------------------------------------------------------------- */
bool silgy_email_attach(const char *to, const char *subject, const char *message, const char *att_name, const char *att_data, int att_data_len)
{
    DBG("Sending email to [%s], subject [%s], with attachement [%s]", to, subject, att_name);

#define BOUNDARY "silgybndGq7ehJxt"

#ifndef _WIN32
    char    sender[512];
    char    comm[512];

    sprintf(sender, "%s <%s@%s>", APP_WEBSITE, EMAIL_FROM_USER, APP_DOMAIN);

    sprintf(comm, "/usr/lib/sendmail -t -f \"%s\"", sender);

    FILE *mailpipe = popen(comm, "w");

    if ( mailpipe == NULL )
    {
        ERR("Failed to invoke sendmail");
        return FALSE;
    }
    else
    {
        fprintf(mailpipe, "From: %s\n", sender);
        fprintf(mailpipe, "To: %s\n", to);
        fprintf(mailpipe, "Subject: %s\n", subject);
        fprintf(mailpipe, "Content-Type: multipart/mixed; boundary=%s\n", BOUNDARY);
        fprintf(mailpipe, "\n");

        /* message */

        fprintf(mailpipe, "--%s\n", BOUNDARY);

        fprintf(mailpipe, "Content-Type: text/plain; charset=\"utf-8\"\n");
//        fprintf(mailpipe, "Content-Transfer-Encoding: quoted-printable\n");
        fprintf(mailpipe, "Content-Disposition: inline\n");
        fprintf(mailpipe, "\n");


/*        char *qpm;
        int qpm_len = strlen(message) * 4;

        if ( !(qpm=(char*)malloc(qpm_len)) )
        {
            ERR("Couldn't allocate %d bytes for qpm", qpm_len);
            return FALSE;
        }

        qp(qpm, message);

        DBG("qpm [%s]", qpm); */

        fwrite(message, strlen(message), 1, mailpipe);
//        fwrite(qpm, 1, strlen(qpm), mailpipe);
//        free(qpm);
        fprintf(mailpipe, "\n\n");


        /* attachement */

        fprintf(mailpipe, "--%s\n", BOUNDARY);

        fprintf(mailpipe, "Content-Type: application\n");
        fprintf(mailpipe, "Content-Transfer-Encoding: base64\n");
        fprintf(mailpipe, "Content-Disposition: attachment; filename=\"%s\"\n", att_name);
        fprintf(mailpipe, "\n");

        char *b64data;
        int b64data_len = ((4 * att_data_len / 3) + 3) & ~3;

        DBG("Predicted b64data_len = %d", b64data_len);

        if ( !(b64data=(char*)malloc(b64data_len+16)) )   /* just in case, to be verified */
        {
            ERR("Couldn't allocate %d bytes for b64data", b64data_len+16);
            return FALSE;
        }

        Base64encode(b64data, att_data, att_data_len);
        b64data_len = strlen(b64data);

        DBG("     Real b64data_len = %d", b64data_len);

        fwrite(b64data, b64data_len, 1, mailpipe);

        free(b64data);

        /* finish */

        fprintf(mailpipe, "\n\n--%s--\n", BOUNDARY);

        pclose(mailpipe);
    }

    return TRUE;

#else   /* Windows */

    WAR("There's no email service for Windows");
    return TRUE;

#endif  /* _WIN32 */
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
    char *temp;

    unsigned len = strlen(src);

    if ( !(temp=(char*)malloc(len+1)) )
    {
        ERR("Couldn't allocate %u bytes for silgy_minify", len);
        return 0;
    }

    minify_1(temp, src, len);

    int ret = minify_2(dest, temp);

    free(temp);

    return ret;
}


/* --------------------------------------------------------------------------
   First pass -- only remove comments
-------------------------------------------------------------------------- */
static void minify_1(char *dest, const char *src, int len)
{
    int     i;
    int     j=0;
    bool    opensq=FALSE;       /* single quote */
    bool    opendq=FALSE;       /* double quote */
    bool    openco=FALSE;       /* comment */
    bool    opensc=FALSE;       /* star comment */

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
   Return new length
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
    int     backslashes=0;

    len = strlen(src);

    for ( i=0; i<len; ++i )
    {
        /* 'foo - TRUE */
        /* \'foo - FALSE */
        /* \\'foo - TRUE */

        /* odd number of backslashes invalidates the quote */

        if ( !opensq && src[i]=='"' && backslashes%2==0 )
        {
            if ( !opendq )
                opendq = TRUE;
            else
                opendq = FALSE;
        }
        else if ( !opendq && src[i]=='\'' && backslashes%2==0 )
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
        else if ( !opensq && !opendq && !opencc && !openwo && (isalpha(src[i]) || src[i]=='|' || src[i]=='&') )  /* word is starting */
        {
            openwo = TRUE;
        }
        else if ( !opensq && !opendq && openwo && !isalnum(src[i]) && src[i]!='_' && src[i]!='|' && src[i]!='&' )   /* end of word */
        {
            word[wi] = EOS;
            if ( 0==strcmp(word, "var")
                    || 0==strcmp(word, "let")
                    || (0==strcmp(word, "function") && src[i]!='(')
                    || (0==strcmp(word, "else") && src[i]!='{')
                    || 0==strcmp(word, "new")
                    || 0==strcmp(word, "enum")
                    || 0==strcmp(word, "const")
                    || 0==strcmp(word, "import")
                    || (0==strcmp(word, "return") && src[i]!=';') )
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

        if ( src[i]=='\\' )
            ++backslashes;
        else
            backslashes = 0;

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
   Increment date by 'days' days. Return day of week as well.
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

    G_ptm = gmtime(&G_now);   /* set it back */

}


/* --------------------------------------------------------------------------
   Compare dates
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
    FILE *h_file=NULL;

    /* open the conf file */

#ifdef _WIN32   /* Windows */
    if ( NULL == (h_file=fopen(file, "rb")) )
#else
    if ( NULL == (h_file=fopen(file, "r")) )
#endif
    {
//        printf("Error opening %s, using defaults.\n", file);
        return FALSE;
    }

    /* read content into M_conf for silgy_read_param */

    fseek(h_file, 0, SEEK_END);     /* determine the file size */
    unsigned size = ftell(h_file);
    rewind(h_file);

    if ( (M_conf=(char*)malloc(size+1)) == NULL )
    {
        printf("ERROR: Couldn't get %u bytes for M_conf\n", size+1);
        fclose(h_file);
        return FALSE;
    }

    fread(M_conf, size, 1, h_file);
    *(M_conf+size) = EOS;

    fclose(h_file);

    return TRUE;
}


/* --------------------------------------------------------------------------
   Get param from config file
---------------------------------------------------------------------------*/
bool silgy_read_param_str(const char *param, char *dest)
{
    char *p;
    int  plen = strlen(param);
#ifdef DUMP
    DBG("silgy_read_param_str [%s]", param);
#endif
    if ( !M_conf )
    {
//        ERR("No config file or not read yet");
        return FALSE;
    }

    if ( (p=strstr(M_conf, param)) == NULL )
    {
//        if ( dest ) dest[0] = EOS;
        return FALSE;
    }

    /* string present but is it label or value? */

    bool found=FALSE;

    while ( p )    /* param may be commented out but present later */
    {
        if ( p > M_conf && *(p-1) != '\n' )  /* commented out or within quotes -- try the next occurence */
        {
#ifdef DUMP
            DBG("param commented out or within quotes, continue search...");
#endif
            p = strstr(++p, param);
        }
        else if ( *(p+plen) != '=' && *(p+plen) != ' ' && *(p+plen) != '\t' )
        {
#ifdef DUMP
            DBG("param does not end with '=', space or tab, continue search...");
#endif
            p = strstr(++p, param);
        }
        else
        {
            found = TRUE;
            break;
        }
    }

    if ( !found )
        return FALSE;

    /* param present ----------------------------------- */

    if ( !dest ) return TRUE;   /* it's only a presence check */


    /* copy value to dest ------------------------------ */

    p += plen;

    while ( *p=='=' || *p==' ' || *p=='\t' )
        ++p;

    int i=0;

    while ( *p != '\r' && *p != '\n' && *p != '#' && *p != EOS )
        dest[i++] = *p++;

    dest[i] = EOS;

    DBG("%s [%s]", param, dest);

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
static char pidfilename[512];
    FILE    *fpid=NULL;
    char    command[512];

    G_pid = getpid();

#ifdef _WIN32   /* Windows */
    sprintf(pidfilename, "%s\\bin\\%s.pid", G_appdir, name);
#else
    sprintf(pidfilename, "%s/bin/%s.pid", G_appdir, name);
#endif

    /* check if the pid file already exists */

    if ( access(pidfilename, F_OK) != -1 )
    {
        WAR("PID file already exists");
        INF("Killing the old process...");
#ifdef _WIN32   /* Windows */
        /* open the pid file and read process id */
        if ( NULL == (fpid=fopen(pidfilename, "rb")) )
        {
            ERR("Couldn't open pid file for reading");
            return NULL;
        }
        fseek(fpid, 0, SEEK_END);     /* determine the file size */
        int fsize = ftell(fpid);
        if ( fsize < 1 || fsize > 60 )
        {
            fclose(fpid);
            ERR("Something's wrong with the pid file size (%d bytes)", fsize);
            return NULL;
        }
        rewind(fpid);
        char oldpid[64];
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

#ifdef _WIN32   /* Windows */
    if ( NULL == (fpid=fopen(pidfilename, "wb")) )
#else
    if ( NULL == (fpid=fopen(pidfilename, "w")) )
#endif
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
char *lib_shm_create(unsigned bytes, int index)
{
    char *shm_segptr=NULL;

    if ( index >= MAX_SHM_SEGMENTS )
    {
        ERR("Too many SHM segments, MAX_SHM_SEGMENTS=%d", MAX_SHM_SEGMENTS);
        return NULL;
    }

#ifndef _WIN32

    /* Create unique key via call to ftok() */

    key_t key;
    if ( (key=ftok(G_appdir, '0'+(char)index)) == -1 )
    {
        ERR("ftok, errno = %d (%s)", errno, strerror(errno));
        return NULL;
    }

    /* Open the shared memory segment - create if necessary */

    if ( (M_shmid[index]=shmget(key, bytes, IPC_CREAT|IPC_EXCL|0600)) == -1 )
    {
        INF("Shared memory segment exists - opening as client");

        if ( (M_shmid[index]=shmget(key, bytes, 0)) == -1 )
        {
            ERR("shmget, errno = %d (%s)", errno, strerror(errno));
            return NULL;
        }
    }
    else
    {
        INF("Creating new shared memory segment");
    }

    /* Attach (map) the shared memory segment into the current process */

    if ( (shm_segptr=(char*)shmat(M_shmid[index], 0, 0)) == (char*)-1 )
    {
        ERR("shmat, errno = %d (%s)", errno, strerror(errno));
        return NULL;
    }

#endif  /* _WIN32 */

    return shm_segptr;
}


/* --------------------------------------------------------------------------
   Delete shared memory segment
-------------------------------------------------------------------------- */
void lib_shm_delete(int index)
{
#ifndef _WIN32
    if ( M_shmid[index] )
    {
        shmctl(M_shmid[index], IPC_RMID, 0);
        M_shmid[index] = 0;
        INF("Shared memory segment (index=%d) marked for deletion", index);
    }
#endif  /* _WIN32 */
}


/* --------------------------------------------------------------------------
   Start a log
-------------------------------------------------------------------------- */
bool log_start(const char *prefix, bool test)
{
    char    fprefix[64]="";     /* formatted prefix */
    char    fname[512];         /* file name */
    char    ffname[512];        /* full file name */

    if ( G_logLevel < 1 ) return TRUE;  /* no log */

    if ( G_logToStdout != 1 )   /* log to a file */
    {
        if ( M_log_fd != NULL && M_log_fd != stdout ) return TRUE;  /* already started */

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
    }

    fprintf(M_log_fd, LOG_LINE_LONG_N);

    ALWAYS(" %s  Starting %s's log. Server version: %s, app version: %s", G_dt, APP_WEBSITE, WEB_SERVER_VERSION, APP_VERSION);

    fprintf(M_log_fd, LOG_LINE_LONG_NN);

    return TRUE;
}


/* --------------------------------------------------------------------------
   Write to log with date/time
-------------------------------------------------------------------------- */
void log_write_time(int level, const char *message, ...)
{
    if ( level > G_logLevel ) return;

    /* output timestamp */

    fprintf(M_log_fd, "[%s] ", G_dt+11);

    if ( LOG_ERR == level )
        fprintf(M_log_fd, "ERROR: ");
    else if ( LOG_WAR == level )
        fprintf(M_log_fd, "WARNING: ");

    /* compile message with arguments into buffer */

    va_list plist;
    char buffer[MAX_LOG_STR_LEN+1+64];

    va_start(plist, message);
    vsprintf(buffer, message, plist);
    va_end(plist);

    /* write to the log file */

    fprintf(M_log_fd, "%s\n", buffer);

#ifdef DUMP
    fflush(M_log_fd);
#else
    if ( G_logLevel >= LOG_DBG || level == LOG_ERR ) fflush(M_log_fd);
#endif
}


/* --------------------------------------------------------------------------
   Write to log
-------------------------------------------------------------------------- */
void log_write(int level, const char *message, ...)
{
    if ( level > G_logLevel ) return;

    if ( LOG_ERR == level )
        fprintf(M_log_fd, "ERROR: ");
    else if ( LOG_WAR == level )
        fprintf(M_log_fd, "WARNING: ");

    /* compile message with arguments into buffer */

    va_list plist;
    char buffer[MAX_LOG_STR_LEN+1+64];

    va_start(plist, message);
    vsprintf(buffer, message, plist);
    va_end(plist);

    /* write to the log file */

    fprintf(M_log_fd, "%s\n", buffer);

#ifdef DUMP
    fflush(M_log_fd);
#else
    if ( G_logLevel >= LOG_DBG || level == LOG_ERR ) fflush(M_log_fd);
#endif
}


/* --------------------------------------------------------------------------
   Write looong message to a log or --
   its first (MAX_LOG_STR_LEN-50) part if it's longer
-------------------------------------------------------------------------- */
void log_long(const char *message, int len, const char *desc)
{
    if ( G_logLevel < LOG_DBG ) return;

    char buffer[MAX_LOG_STR_LEN+1];

    if ( len < MAX_LOG_STR_LEN-50 )
    {
        strncpy(buffer, message, len);
        buffer[len] = EOS;
    }
    else
    {
        strncpy(buffer, message, MAX_LOG_STR_LEN-50);
        strcpy(buffer+MAX_LOG_STR_LEN-50, " (...)");
    }

    DBG("%s:\n\n[%s]\n", desc, buffer);
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
    if ( G_logLevel > 0 )
        ALWAYS_T("Closing log");

    if ( M_log_fd != NULL && M_log_fd != stdout )
    {
        fclose(M_log_fd);
        M_log_fd = stdout;
    }
}


#ifdef ICONV
/* --------------------------------------------------------------------------
   Convert string
-------------------------------------------------------------------------- */
char *silgy_convert(const char *src, const char *cp_from, const char *cp_to)
{
static char dst[4096];

    iconv_t cd = iconv_open(cp_to, cp_from);

    if ( cd == (iconv_t)-1 )
    {
        strcpy(dst, "iconv_open failed");
        return dst;
    }

    const char *in_buf = src;
    size_t in_left = strlen(src);

    char *out_buf = &dst[0];
    size_t out_left = 4095;

    do
    {
        if ( iconv(cd, (char**)&in_buf, &in_left, &out_buf, &out_left) == (size_t)-1 )
        {
            strcpy(dst, "iconv failed");
            return dst;
        }
    } while (in_left > 0 && out_left > 0);

    *out_buf = 0;

    iconv_close(cd);

    return dst;
}
#endif  /* ICONV */








/* ================================================================================================ */
/* MD5                                                                                              */
/* ================================================================================================ */
/*
 * This is an OpenSSL-compatible implementation of the RSA Data Security, Inc.
 * MD5 Message-Digest Algorithm (RFC 1321).
 *
 * Homepage:
 * http://openwall.info/wiki/people/solar/software/public-domain-source-code/md5
 *
 * Author:
 * Alexander Peslyak, better known as Solar Designer <solar at openwall.com>
 *
 * This software was written by Alexander Peslyak in 2001.  No copyright is
 * claimed, and the software is hereby placed in the public domain.
 * In case this attempt to disclaim copyright and place the software in the
 * public domain is deemed null and void, then the software is
 * Copyright (c) 2001 Alexander Peslyak and it is hereby released to the
 * general public under the following terms:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted.
 *
 * There's ABSOLUTELY NO WARRANTY, express or implied.
 *
 */
 
#ifdef HAVE_OPENSSL
#include <openssl/md5.h>
#elif !defined(_MD5_H)
#define _MD5_H
 
/* Any 32-bit or wider unsigned integer data type will do */
typedef unsigned int MD5_u32plus;
 
typedef struct {
	MD5_u32plus lo, hi;
	MD5_u32plus a, b, c, d;
	unsigned char buffer[64];
	MD5_u32plus block[16];
} MD5_CTX;
 
extern void MD5_Init(MD5_CTX *ctx);
extern void MD5_Update(MD5_CTX *ctx, const void *data, unsigned long size);
extern void MD5_Final(unsigned char *result, MD5_CTX *ctx);
 
#endif



#ifndef HAVE_OPENSSL
 
#include <string.h>
 
/*
 * The basic MD5 functions.
 *
 * F and G are optimized compared to their RFC 1321 definitions for
 * architectures that lack an AND-NOT instruction, just like in Colin Plumb's
 * implementation.
 */
#define F(x, y, z)			((z) ^ ((x) & ((y) ^ (z))))
#define G(x, y, z)			((y) ^ ((z) & ((x) ^ (y))))
#define H(x, y, z)			(((x) ^ (y)) ^ (z))
#define H2(x, y, z)			((x) ^ ((y) ^ (z)))
#define I(x, y, z)			((y) ^ ((x) | ~(z)))
 
/*
 * The MD5 transformation for all four rounds.
 */
#define STEP(f, a, b, c, d, x, t, s) \
	(a) += f((b), (c), (d)) + (x) + (t); \
	(a) = (((a) << (s)) | (((a) & 0xffffffff) >> (32 - (s)))); \
	(a) += (b);
 
/*
 * SET reads 4 input bytes in little-endian byte order and stores them in a
 * properly aligned word in host byte order.
 *
 * The check for little-endian architectures that tolerate unaligned memory
 * accesses is just an optimization.  Nothing will break if it fails to detect
 * a suitable architecture.
 *
 * Unfortunately, this optimization may be a C strict aliasing rules violation
 * if the caller's data buffer has effective type that cannot be aliased by
 * MD5_u32plus.  In practice, this problem may occur if these MD5 routines are
 * inlined into a calling function, or with future and dangerously advanced
 * link-time optimizations.  For the time being, keeping these MD5 routines in
 * their own translation unit avoids the problem.
 */
#if defined(__i386__) || defined(__x86_64__) || defined(__vax__)
#define SET(n) \
	(*(MD5_u32plus *)&ptr[(n) * 4])
#define GET(n) \
	SET(n)
#else
#define SET(n) \
	(ctx->block[(n)] = \
	(MD5_u32plus)ptr[(n) * 4] | \
	((MD5_u32plus)ptr[(n) * 4 + 1] << 8) | \
	((MD5_u32plus)ptr[(n) * 4 + 2] << 16) | \
	((MD5_u32plus)ptr[(n) * 4 + 3] << 24))
#define GET(n) \
	(ctx->block[(n)])
#endif
 
/*
 * This processes one or more 64-byte data blocks, but does NOT update the bit
 * counters.  There are no alignment requirements.
 */
static const void *body(MD5_CTX *ctx, const void *data, unsigned long size)
{
	const unsigned char *ptr;
	MD5_u32plus a, b, c, d;
	MD5_u32plus saved_a, saved_b, saved_c, saved_d;
 
	ptr = (const unsigned char *)data;
 
	a = ctx->a;
	b = ctx->b;
	c = ctx->c;
	d = ctx->d;
 
	do {
		saved_a = a;
		saved_b = b;
		saved_c = c;
		saved_d = d;
 
/* Round 1 */
		STEP(F, a, b, c, d, SET(0), 0xd76aa478, 7)
		STEP(F, d, a, b, c, SET(1), 0xe8c7b756, 12)
		STEP(F, c, d, a, b, SET(2), 0x242070db, 17)
		STEP(F, b, c, d, a, SET(3), 0xc1bdceee, 22)
		STEP(F, a, b, c, d, SET(4), 0xf57c0faf, 7)
		STEP(F, d, a, b, c, SET(5), 0x4787c62a, 12)
		STEP(F, c, d, a, b, SET(6), 0xa8304613, 17)
		STEP(F, b, c, d, a, SET(7), 0xfd469501, 22)
		STEP(F, a, b, c, d, SET(8), 0x698098d8, 7)
		STEP(F, d, a, b, c, SET(9), 0x8b44f7af, 12)
		STEP(F, c, d, a, b, SET(10), 0xffff5bb1, 17)
		STEP(F, b, c, d, a, SET(11), 0x895cd7be, 22)
		STEP(F, a, b, c, d, SET(12), 0x6b901122, 7)
		STEP(F, d, a, b, c, SET(13), 0xfd987193, 12)
		STEP(F, c, d, a, b, SET(14), 0xa679438e, 17)
		STEP(F, b, c, d, a, SET(15), 0x49b40821, 22)
 
/* Round 2 */
		STEP(G, a, b, c, d, GET(1), 0xf61e2562, 5)
		STEP(G, d, a, b, c, GET(6), 0xc040b340, 9)
		STEP(G, c, d, a, b, GET(11), 0x265e5a51, 14)
		STEP(G, b, c, d, a, GET(0), 0xe9b6c7aa, 20)
		STEP(G, a, b, c, d, GET(5), 0xd62f105d, 5)
		STEP(G, d, a, b, c, GET(10), 0x02441453, 9)
		STEP(G, c, d, a, b, GET(15), 0xd8a1e681, 14)
		STEP(G, b, c, d, a, GET(4), 0xe7d3fbc8, 20)
		STEP(G, a, b, c, d, GET(9), 0x21e1cde6, 5)
		STEP(G, d, a, b, c, GET(14), 0xc33707d6, 9)
		STEP(G, c, d, a, b, GET(3), 0xf4d50d87, 14)
		STEP(G, b, c, d, a, GET(8), 0x455a14ed, 20)
		STEP(G, a, b, c, d, GET(13), 0xa9e3e905, 5)
		STEP(G, d, a, b, c, GET(2), 0xfcefa3f8, 9)
		STEP(G, c, d, a, b, GET(7), 0x676f02d9, 14)
		STEP(G, b, c, d, a, GET(12), 0x8d2a4c8a, 20)
 
/* Round 3 */
		STEP(H, a, b, c, d, GET(5), 0xfffa3942, 4)
		STEP(H2, d, a, b, c, GET(8), 0x8771f681, 11)
		STEP(H, c, d, a, b, GET(11), 0x6d9d6122, 16)
		STEP(H2, b, c, d, a, GET(14), 0xfde5380c, 23)
		STEP(H, a, b, c, d, GET(1), 0xa4beea44, 4)
		STEP(H2, d, a, b, c, GET(4), 0x4bdecfa9, 11)
		STEP(H, c, d, a, b, GET(7), 0xf6bb4b60, 16)
		STEP(H2, b, c, d, a, GET(10), 0xbebfbc70, 23)
		STEP(H, a, b, c, d, GET(13), 0x289b7ec6, 4)
		STEP(H2, d, a, b, c, GET(0), 0xeaa127fa, 11)
		STEP(H, c, d, a, b, GET(3), 0xd4ef3085, 16)
		STEP(H2, b, c, d, a, GET(6), 0x04881d05, 23)
		STEP(H, a, b, c, d, GET(9), 0xd9d4d039, 4)
		STEP(H2, d, a, b, c, GET(12), 0xe6db99e5, 11)
		STEP(H, c, d, a, b, GET(15), 0x1fa27cf8, 16)
		STEP(H2, b, c, d, a, GET(2), 0xc4ac5665, 23)
 
/* Round 4 */
		STEP(I, a, b, c, d, GET(0), 0xf4292244, 6)
		STEP(I, d, a, b, c, GET(7), 0x432aff97, 10)
		STEP(I, c, d, a, b, GET(14), 0xab9423a7, 15)
		STEP(I, b, c, d, a, GET(5), 0xfc93a039, 21)
		STEP(I, a, b, c, d, GET(12), 0x655b59c3, 6)
		STEP(I, d, a, b, c, GET(3), 0x8f0ccc92, 10)
		STEP(I, c, d, a, b, GET(10), 0xffeff47d, 15)
		STEP(I, b, c, d, a, GET(1), 0x85845dd1, 21)
		STEP(I, a, b, c, d, GET(8), 0x6fa87e4f, 6)
		STEP(I, d, a, b, c, GET(15), 0xfe2ce6e0, 10)
		STEP(I, c, d, a, b, GET(6), 0xa3014314, 15)
		STEP(I, b, c, d, a, GET(13), 0x4e0811a1, 21)
		STEP(I, a, b, c, d, GET(4), 0xf7537e82, 6)
		STEP(I, d, a, b, c, GET(11), 0xbd3af235, 10)
		STEP(I, c, d, a, b, GET(2), 0x2ad7d2bb, 15)
		STEP(I, b, c, d, a, GET(9), 0xeb86d391, 21)
 
		a += saved_a;
		b += saved_b;
		c += saved_c;
		d += saved_d;
 
		ptr += 64;
	} while (size -= 64);
 
	ctx->a = a;
	ctx->b = b;
	ctx->c = c;
	ctx->d = d;
 
	return ptr;
}
 
void MD5_Init(MD5_CTX *ctx)
{
	ctx->a = 0x67452301;
	ctx->b = 0xefcdab89;
	ctx->c = 0x98badcfe;
	ctx->d = 0x10325476;
 
	ctx->lo = 0;
	ctx->hi = 0;
}
 
void MD5_Update(MD5_CTX *ctx, const void *data, unsigned long size)
{
	MD5_u32plus saved_lo;
	unsigned long used, available;
 
	saved_lo = ctx->lo;
	if ((ctx->lo = (saved_lo + size) & 0x1fffffff) < saved_lo)
		ctx->hi++;
	ctx->hi += size >> 29;
 
	used = saved_lo & 0x3f;
 
	if (used) {
		available = 64 - used;
 
		if (size < available) {
			memcpy(&ctx->buffer[used], data, size);
			return;
		}
 
		memcpy(&ctx->buffer[used], data, available);
		data = (const unsigned char *)data + available;
		size -= available;
		body(ctx, ctx->buffer, 64);
	}
 
	if (size >= 64) {
		data = body(ctx, data, size & ~(unsigned long)0x3f);
		size &= 0x3f;
	}
 
	memcpy(ctx->buffer, data, size);
}
 
#define MD5_OUT(dst, src) \
	(dst)[0] = (unsigned char)(src); \
	(dst)[1] = (unsigned char)((src) >> 8); \
	(dst)[2] = (unsigned char)((src) >> 16); \
	(dst)[3] = (unsigned char)((src) >> 24);
 
void MD5_Final(unsigned char *result, MD5_CTX *ctx)
{
	unsigned long used, available;
 
	used = ctx->lo & 0x3f;
 
	ctx->buffer[used++] = 0x80;
 
	available = 64 - used;
 
	if (available < 8) {
		memset(&ctx->buffer[used], 0, available);
		body(ctx, ctx->buffer, 64);
		used = 0;
		available = 64;
	}
 
	memset(&ctx->buffer[used], 0, available - 8);
 
	ctx->lo <<= 3;
	MD5_OUT(&ctx->buffer[56], ctx->lo)
	MD5_OUT(&ctx->buffer[60], ctx->hi)
 
	body(ctx, ctx->buffer, 64);
 
	MD5_OUT(&result[0], ctx->a)
	MD5_OUT(&result[4], ctx->b)
	MD5_OUT(&result[8], ctx->c)
	MD5_OUT(&result[12], ctx->d)
 
	memset(ctx, 0, sizeof(*ctx));
}
 
#endif


/* --------------------------------------------------------------------------
   Return MD5 hash in the form of hex string
-------------------------------------------------------------------------- */
char *md5(const char* str)
{
static char result[33];
    unsigned char digest[16];

    MD5_CTX context;

    MD5_Init(&context);
    MD5_Update(&context, str, strlen(str));
    MD5_Final(digest, &context);

    int i;
    for ( i=0; i<16; ++i )
        sprintf(&result[i*2], "%02x", (unsigned int)digest[i]);

    return result;
}




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

	bufin = (const unsigned char *)bufcoded;
	while (pr2six[*(bufin++)] <= 63);

	nprbytes = (bufin - (const unsigned char *)bufcoded) - 1;
	nbytesdecoded = ((nprbytes + 3) / 4) * 3;

	return nbytesdecoded;
}

int Base64decode(char *bufplain, const char *bufcoded)
{
	int nbytesdecoded;
	register const unsigned char *bufin;
	register unsigned char *bufout;
	register int nprbytes;

	bufin = (const unsigned char *)bufcoded;
	while (pr2six[*(bufin++)] <= 63);
	nprbytes = (bufin - (const unsigned char *)bufcoded) - 1;
	nbytesdecoded = ((nprbytes + 3) / 4) * 3;

	bufout = (unsigned char *)bufplain;
	bufin = (const unsigned char *)bufcoded;

	while (nprbytes > 4) {
		*(bufout++) =
			(unsigned char)(pr2six[*bufin] << 2 | pr2six[bufin[1]] >> 4);
		*(bufout++) =
			(unsigned char)(pr2six[bufin[1]] << 4 | pr2six[bufin[2]] >> 2);
		*(bufout++) =
			(unsigned char)(pr2six[bufin[2]] << 6 | pr2six[bufin[3]]);
		bufin += 4;
		nprbytes -= 4;
	}

	/* Note: (nprbytes == 1) would be an error, so just ingore that case */
	if (nprbytes > 1) {
		*(bufout++) =
			(unsigned char)(pr2six[*bufin] << 2 | pr2six[bufin[1]] >> 4);
	}
	if (nprbytes > 2) {
		*(bufout++) =
			(unsigned char)(pr2six[bufin[1]] << 4 | pr2six[bufin[2]] >> 2);
	}
	if (nprbytes > 3) {
		*(bufout++) =
			(unsigned char)(pr2six[bufin[2]] << 6 | pr2six[bufin[3]]);
	}

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

int clock_gettime_win(struct timespec *spec)
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

    return d-1;
}


/* --------------------------------------------------------------------------
   Windows port of stpncpy
-------------------------------------------------------------------------- */
char *stpncpy(char *dest, const char *src, size_t len)
{
    register char *d=dest;
    register const char *s=src;
    int count=0;

    do
        *d++ = *s;
    while (*s++ != '\0' && ++count<len);

    if ( *(s-1) == EOS )
        return d-1;

    return d;
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
