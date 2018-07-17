/* --------------------------------------------------------------------------
   Silgy Web App Engine
   Jurek Muszynski
   silgy.com
   Some parts are Public Domain from other authors, see below
-----------------------------------------------------------------------------
   General purpose library
-------------------------------------------------------------------------- */

#include <iconv.h>

#include "silgy.h"


/* globals */

FILE        *G_log=NULL;            /* log file handle */
char        G_logLevel=0;           /* log level */
char        G_appdir[256]=".";      /* application root dir */
char        G_test=0;               /* test run */
int         G_pid=0;                /* pid */
time_t      G_now=0;                /* current time (GMT) */
struct tm   *G_ptm={0};             /* human readable current time */
char        G_dt[20]="";            /* datetime for database or log (YYYY-MM-DD hh:mm:ss) */
char        G_tmp[TMP_BUFSIZE];     /* temporary string buffer */
char        *G_shm_segptr=NULL;     /* SHM pointer */


/* locals */

static char *M_conf=NULL;           /* config file content */

static char M_df=0;                 /* date format */
static char M_tsep=' ';             /* thousand separator */
static char M_dsep='.';             /* decimal separator */

static int  M_shmid;                /* SHM id */

static char *uri_decode(char *src, int srclen, char *dest, int maxlen);
static char *uri_decode_html_esc(char *src, int srclen, char *dest, int maxlen);
static char *uri_decode_sql_esc(char *src, int srclen, char *dest, int maxlen);
static int xctod(int c);
static void minify_1(char *dest, const char *src);
static int minify_2(char *dest, const char *src);
static void get_byteorder32(void);
static void get_byteorder64(void);



/* --------------------------------------------------------------------------
   Set G_appdir
-------------------------------------------------------------------------- */
void lib_get_app_dir()
{
    char *appdir=NULL;

    if ( NULL != (appdir=getenv("SILGYDIR")) )
    {
        strcpy(G_appdir, appdir);
    }
    else if ( NULL != (appdir=getenv("HOME")) )
    {
        strcpy(G_appdir, appdir);
        printf("No SILGYDIR defined. Assuming app dir = %s\n", G_appdir);
    }
    else
    {
        strcpy(G_appdir, ".");
//        printf("No SILGYDIR defined. Assuming app dir = .\n");
    }

    int len = strlen(G_appdir);
    if ( G_appdir[len-1] == '/' ) G_appdir[len-1] = EOS;
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
   Send error description as plain, pipe-delimited text
-------------------------------------------------------------------------- */
void lib_send_ajax_msg(int ci, int errcode)
{
    char    id[4]="msg";        /* HTML id */
    char    msg[256];
    char    cat='E';            /* category = 'Error' by default */

    if ( errcode == OK )
    {
        strcpy(id, "0");
        cat = 'I';
    }
//  else if ( errcode < 0 )     /* server error */
//  {
//  }
    else if ( errcode > 0 && errcode < 20 ) /* login */
    {
        strcpy(id, "loe");
    }
    else if ( errcode < 30 )    /* email */
    {
        strcpy(id, "eme");
    }
    else if ( errcode < 40 )    /* password */
    {
        strcpy(id, "pae");
    }
    else if ( errcode < 50 )    /* repeat password */
    {
        strcpy(id, "pre");
    }
    else if ( errcode < 60 )    /* old password */
    {
        strcpy(id, "poe");
    }
//  else if ( errcode < 100 )   /* other error */
//  {
//  }
    else if ( errcode < 200 )   /* warning (yellow) */
    {
        cat = 'W';
    }
    else if ( errcode < 1000 )  /* info (green) */
    {
        cat = 'I';
    }
//  else    /* app error */
//  {
//  }

#ifndef ASYNC_SERVICE
    eng_get_msg_str(ci, msg, errcode);
    OUT("%s|%s|%c", id, msg, cat);

    DBG("lib_send_ajax_msg: [%s]", G_tmp);

    conn[ci].ctype = RES_TEXT;
    RES_DONT_CACHE;
#endif
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
   Get query string value and URI-decode. Return TRUE if found.
-------------------------------------------------------------------------- */
bool get_qs_param(int ci, const char *fieldname, char *retbuf)
{
#ifndef ASYNC_SERVICE
static char buf[MAX_URI_VAL_LEN*2+1];

    if ( get_qs_param_raw(ci, fieldname, buf, MAX_URI_VAL_LEN*2) )
    {
        if ( retbuf ) uri_decode(buf, strlen(buf), retbuf, MAX_URI_VAL_LEN);
        return TRUE;
    }
    else if ( retbuf ) retbuf[0] = EOS;
#endif
    return FALSE;
}


/* --------------------------------------------------------------------------
   Get, URI-decode and HTML-escape query string value. Return TRUE if found.
-------------------------------------------------------------------------- */
bool get_qs_param_html_esc(int ci, const char *fieldname, char *retbuf)
{
#ifndef ASYNC_SERVICE
static char buf[MAX_URI_VAL_LEN*2+1];

    if ( get_qs_param_raw(ci, fieldname, buf, MAX_URI_VAL_LEN*2) )
    {
        if ( retbuf ) uri_decode_html_esc(buf, strlen(buf), retbuf, MAX_URI_VAL_LEN);
        return TRUE;
    }
    else if ( retbuf ) retbuf[0] = EOS;
#endif
    return FALSE;
}


/* --------------------------------------------------------------------------
   Get, URI-decode and SQL-escape query string value. Return TRUE if found.
-------------------------------------------------------------------------- */
bool get_qs_param_sql_esc(int ci, const char *fieldname, char *retbuf)
{
#ifndef ASYNC_SERVICE
static char buf[MAX_URI_VAL_LEN*2+1];

    if ( get_qs_param_raw(ci, fieldname, buf, MAX_URI_VAL_LEN*2) )
    {
        if ( retbuf ) uri_decode_sql_esc(buf, strlen(buf), retbuf, MAX_URI_VAL_LEN);
        return TRUE;
    }
    else if ( retbuf ) retbuf[0] = EOS;
#endif
    return FALSE;
}


/* --------------------------------------------------------------------------
   Get query string value. Return TRUE if found.
-------------------------------------------------------------------------- */
bool get_qs_param_raw(int ci, const char *fieldname, char *retbuf, int maxlen)
{
#ifndef ASYNC_SERVICE
    int     fnamelen;
    char    *p, *p2, *p3;
    int     len1;       /* fieldname len */
    int     len2;       /* value len */
    char    *querystring;
    int     vallen;

    fnamelen = strlen(fieldname);

    if ( conn[ci].post )
        querystring = conn[ci].data;
    else
        querystring = strchr(conn[ci].uri, '?');

    if ( querystring == NULL )
    {
        if ( retbuf ) retbuf[0] = EOS;
        return FALSE;    /* no question mark => no values */
    }

    if ( !conn[ci].post )
        ++querystring;      /* skip the question mark */

    for ( p=querystring; *p!=EOS; )
    {
        p2 = strchr(p, '=');    /* end of field name */
        p3 = strchr(p, '&');    /* end of value */

        if ( p3 != NULL )   /* more than one field */
            len2 = p3 - p;
        else            /* only one field in URI */
            len2 = strlen(p);

        if ( p2 == NULL || p3 != NULL && p2 > p3 )
        {
            /* no '=' present in this field */
            p3 += len2;
            continue;
        }

        len1 = p2 - p;  /* field name length */

        if ( len1 == fnamelen && strncmp(fieldname, p, len1) == 0 )
        {
            /* found it */

            if ( retbuf )
            {
//                DBG("get_qs_param_raw p2+1: [%s]", p2+1);

                vallen = len2 - len1 - 1;
                if ( vallen > maxlen )
                    vallen = maxlen;

                strncpy(retbuf, p2+1, vallen);
                retbuf[vallen] = EOS;
            }

            return TRUE;
        }

        /* try next value */

        p += len2;      /* skip current value */
        if ( *p == '&' ) ++p;   /* skip & */
    }

    /* not found */

    if ( retbuf ) retbuf[0] = EOS;
#endif
    return FALSE;
}


/* --------------------------------------------------------------------------
   Get incoming request data -- long string version. TRUE if found.
-------------------------------------------------------------------------- */
bool get_qs_param_long(int ci, const char *fieldname, char *retbuf)
{
#ifndef ASYNC_SERVICE
static char buf[MAX_LONG_URI_VAL_LEN+1];

    if ( get_qs_param_raw(ci, fieldname, buf, MAX_LONG_URI_VAL_LEN) )
    {
        uri_decode(buf, strlen(buf), retbuf, MAX_LONG_URI_VAL_LEN);
        return TRUE;
    }
#endif
    return FALSE;
}


/* --------------------------------------------------------------------------
   Get text value from multipart-form-data
-------------------------------------------------------------------------- */
bool get_qs_param_multipart_txt(int ci, const char *fieldname, char *retbuf)
{
    char    *p;
    long    len;

    p = get_qs_param_multipart(ci, fieldname, &len, NULL);

    if ( !p ) return FALSE;

    if ( len > MAX_URI_VAL_LEN ) return FALSE;

    strncpy(retbuf, p, len);
    retbuf[len] = EOS;

    return TRUE;
}


/* --------------------------------------------------------------------------
   Experimental multipart-form-data receipt
   Return length or -1 if error
   If retfname is not NULL then assume binary data and it must be the last
   data element
-------------------------------------------------------------------------- */
char *get_qs_param_multipart(int ci, const char *fieldname, long *retlen, char *retfname)
{
    int     blen;           /* boundary length */
    char    *cp;            /* current pointer */
#ifndef ASYNC_SERVICE
    char    *p;             /* tmp pointer */
    long    b;              /* tmp bytes count */
    char    fn[MAX_LABEL_LEN+1];    /* field name */
    char    *end;
    long    len;

    /* Couple of checks to make sure it's properly formatted multipart content */

    if ( conn[ci].in_ctype != CONTENT_TYPE_MULTIPART )
    {
        WAR("This is not multipart/form-data");
        return NULL;
    }

    if ( conn[ci].clen < 10 )
    {
        WAR("Content length seems to be too small for multipart (%ld)", conn[ci].clen);
        return NULL;
    }

    cp = conn[ci].data;

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
            WAR("Boundary appears to be too short (%ld)", b);
            return NULL;
        }
        else if ( b > 255 )
        {
            WAR("Boundary appears to be too long (%ld)", b);
            return NULL;
        }

        strncpy(conn[ci].boundary, cp+2, b);
        if ( conn[ci].boundary[b-1] == '\r' )
            conn[ci].boundary[b-1] = EOS;
        else
            conn[ci].boundary[b] = EOS;
    }

    blen = strlen(conn[ci].boundary);

    if ( conn[ci].data[conn[ci].clen-4] != '-' || conn[ci].data[conn[ci].clen-3] != '-' )
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
            WAR("Field name too long (%ld)", b);
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

        if ( b > 255 )
        {
            WAR("File name too long (%ld)", b);
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
        len = conn[ci].clen - (cp - conn[ci].data) - blen - 8;  /* fast version */
                                                                /* Note that the file content must come as last! */
    }

    if ( len < 0 )
    {
        WAR("Ooops, something went terribly wrong! Data length = %ld", len);
        return NULL;
    }

    /* everything looks good so far */

    *retlen = len;

    if ( retfname )
        strcpy(retfname, fn);
#endif
    return cp;
}


/* --------------------------------------------------------------------------
   URI-decode src
-------------------------------------------------------------------------- */
static char *uri_decode(char *src, int srclen, char *dest, int maxlen)
{
    char    *endp=src+srclen;
    char    *srcp;
    char    *destp=dest;
    int     nwrote=0;

    for ( srcp=src; srcp<endp; ++srcp )
    {
        if ( *srcp == '+' )
            *destp++ = ' ';
        else if ( *srcp == '%' )
        {
            *destp++ = 16 * xctod(*(srcp+1)) + xctod(*(srcp+2));
            srcp += 2;
        }
        else    /* copy as it is */
            *destp++ = *srcp;

        ++nwrote;

        if ( nwrote == maxlen )
        {
            WAR("URI val truncated");
            break;
        }
    }

    *destp = EOS;

    return dest;
}


/* --------------------------------------------------------------------------
   URI-decode src, HTML-escape
   Duplicated code for speed
-------------------------------------------------------------------------- */
static char *uri_decode_html_esc(char *src, int srclen, char *dest, int maxlen)
{
    char    *endp=src+srclen;
    char    *srcp;
    char    *destp=dest;
    int     nwrote=0;
    char    tmp;

    maxlen -= 7;

    for ( srcp=src; srcp<endp; ++srcp )
    {
        if ( *srcp == '+' )
        {
            *destp++ = ' ';
            ++nwrote;
        }
        else if ( *srcp == '%' )
        {
            tmp = 16 * xctod(*(srcp+1)) + xctod(*(srcp+2));
            srcp += 2;

            if ( tmp == '\'' )      /* single quote */
            {
                *destp++ = '&';
                *destp++ = 'a';
                *destp++ = 'p';
                *destp++ = 'o';
                *destp++ = 's';
                *destp++ = ';';
                nwrote += 6;
            }
            else if ( tmp == '"' )  /* double quote */
            {
                *destp++ = '&';
                *destp++ = 'q';
                *destp++ = 'u';
                *destp++ = 'o';
                *destp++ = 't';
                *destp++ = ';';
                nwrote += 6;
            }
            else if ( tmp == '\\' ) /* backslash */
            {
                *destp++ = '\\';
                *destp++ = '\\';
                nwrote += 2;
            }
            else if ( tmp == '<' )
            {
                *destp++ = '&';
                *destp++ = 'l';
                *destp++ = 't';
                *destp++ = ';';
                nwrote += 4;
            }
            else if ( tmp == '>' )
            {
                *destp++ = '&';
                *destp++ = 'g';
                *destp++ = 't';
                *destp++ = ';';
                nwrote += 4;
            }
            else if ( tmp == '&' )
            {
                *destp++ = '&';
                *destp++ = 'a';
                *destp++ = 'm';
                *destp++ = 'p';
                *destp++ = ';';
                nwrote += 5;
            }
            else if ( tmp != '\r' && tmp != '\n' )
            {
                *destp++ = tmp;
                ++nwrote;
            }
        }
        else if ( *srcp == '\'' )    /* ugly but fast -- everything again */
        {
            *destp++ = '&';
            *destp++ = 'a';
            *destp++ = 'p';
            *destp++ = 'o';
            *destp++ = 's';
            *destp++ = ';';
            nwrote += 6;
        }
        else if ( *srcp == '"' )    /* double quote */
        {
            *destp++ = '&';
            *destp++ = 'q';
            *destp++ = 'u';
            *destp++ = 'o';
            *destp++ = 't';
            *destp++ = ';';
            nwrote += 6;
        }
        else if ( *srcp == '\\' )   /* backslash */
        {
            *destp++ = '\\';
            *destp++ = '\\';
            nwrote += 2;
        }
        else if ( *srcp == '<' )
        {
            *destp++ = '&';
            *destp++ = 'l';
            *destp++ = 't';
            *destp++ = ';';
            nwrote += 4;
        }
        else if ( *srcp == '>' )
        {
            *destp++ = '&';
            *destp++ = 'g';
            *destp++ = 't';
            *destp++ = ';';
            nwrote += 4;
        }
        else if ( *srcp == '&' )
        {
            *destp++ = '&';
            *destp++ = 'a';
            *destp++ = 'm';
            *destp++ = 'p';
            *destp++ = ';';
            nwrote += 5;
        }
        else if ( *srcp != '\r' && *srcp != '\n' )
        {
            *destp++ = *srcp;
            ++nwrote;
        }

        if ( nwrote > maxlen )
        {
            WAR("URI val truncated");
            break;
        }
    }

    *destp = EOS;

    return dest;
}


/* --------------------------------------------------------------------------
   URI-decode src, SQL-escape
   Duplicated code for speed
-------------------------------------------------------------------------- */
static char *uri_decode_sql_esc(char *src, int srclen, char *dest, int maxlen)
{
    char    *endp=src+srclen;
    char    *srcp;
    char    *destp=dest;
    int     nwrote=0;
    char    tmp;

    maxlen -= 3;

    for ( srcp=src; srcp<endp; ++srcp )
    {
        if ( *srcp == '+' )
        {
            *destp++ = ' ';
            ++nwrote;
        }
        else if ( *srcp == '%' )
        {
            tmp = 16 * xctod(*(srcp+1)) + xctod(*(srcp+2));
            srcp += 2;

            if ( tmp == '\'' )      /* single quote */
            {
                *destp++ = '\\';
                *destp++ = '\'';
                nwrote += 2;
            }
            else if ( *srcp == '"' )    /* double quote */
            {
                *destp++ = '\\';
                *destp++ = '"';
                nwrote += 2;
            }
            else if ( tmp == '\\' )     /* backslash */
            {
                *destp++ = '\\';
                *destp++ = '\\';
                nwrote += 2;
            }
        }
        else if ( *srcp == '\'' )   /* ugly but fast -- everything again */
        {
            *destp++ = '\\';
            *destp++ = '\'';
            nwrote += 2;
        }
        else if ( *srcp == '"' )    /* double quote */
        {
            *destp++ = '\\';
            *destp++ = '"';
            nwrote += 2;
        }
        else if ( *srcp == '\\' )   /* backslash */
        {
            *destp++ = '\\';
            *destp++ = '\\';
            nwrote += 2;
        }

        if ( nwrote > maxlen )
        {
            WAR("URI val truncated");
            break;
        }
    }

    *destp = EOS;

    return dest;
}


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
void msleep(long n)
{
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = n * 1000;
    select(0, NULL, NULL, NULL, &tv);
}


/* --------------------------------------------------------------------------
   Get index from JSON buffer
-------------------------------------------------------------------------- */
static int json_get_i(JSON *json, const char *name)
{
    int i;

    for ( i=0; i<json->cnt; ++i )
        if ( 0==strcmp(json->rec[i].name, name) )
            return i;

    return -1;
}


/* --------------------------------------------------------------------------
   Convert Silgy JSON format to JSON string
-------------------------------------------------------------------------- */
char *lib_json_to_string(JSON *json)
{
static char dst[JSON_BUFSIZE];
    char    *p=dst;
    int     i;

    p = stpcpy(p, "{");

    for ( i=0; i<json->cnt; ++i )
    {
        if ( json->rec[i].type == JSON_STRING )
        {
            p = stpcpy(p, "\"");
            p = stpcpy(p, json->rec[i].value);
            p = stpcpy(p, "\"");
        }
        else if ( json->rec[i].type == JSON_NUMBER || json->rec[i].type == JSON_BOOL )
        {
            p = stpcpy(p, json->rec[i].value);
        }

        if ( i < json->cnt-1 )
            p = stpcpy(p, ",");
    }

    p = stpcpy(p, "}");

    *p = EOS;

    return dst;
}


/* --------------------------------------------------------------------------
   Get JSON element as a string
-------------------------------------------------------------------------- */
static char *get_json_elem(JSON *json, int i)
{
static char retbuf[1024];

/*    if ( US.rest_fld[JSON_MAX_ELEMS*level+i].type == JSON_STRING )
    {
        sprintf(retbuf, "\"%s\"", US.rest_fld[JSON_MAX_ELEMS*level+i].value);
        return retbuf;
    }
    else if ( US.rest_fld[JSON_MAX_ELEMS*level+i].type == JSON_RECORD )
    {
        if ( level >= JSON_MAX_LEVELS )
        {
            retbuf[0] = EOS;
        }
        else
        {
            sprintf(retbuf, "{\"%s\":%s}", US.rest_fld[JSON_MAX_ELEMS*level+i].name, get_json_elem(ci, i, level+1));
        }
        return retbuf;
    } */
//    else    /* number or bool */
/*    {
        strcpy(retbuf, US.rest_fld[JSON_MAX_ELEMS*level+i].value);
        return retbuf;
    } */
}


/* --------------------------------------------------------------------------
   Get array element as a string from JSON REST buffer
-------------------------------------------------------------------------- */
char *get_json_array_elem(const char *name, int index)
{
}


/* --------------------------------------------------------------------------
   Get record as a string from JSON REST buffer
-------------------------------------------------------------------------- */
char *get_json_record(const char *name)
{
}


/* --------------------------------------------------------------------------
   Copy JSON string to Silgy JSON format
-------------------------------------------------------------------------- */
void lib_json_from_string(JSON *json, const char *src)
{
    char key[32];
    char value[256];
    char now_key=1, now_value=0, is_string=0, is_array=0;

    int j = 0;

    while ( *src )
    {
        if ( !now_value )
            while ( *src && (*src==' ' || *src=='\t' || *src=='\r' || *src=='\n' || *src=='{') ) ++src;

        if ( now_key )
            while ( *src && *src=='"' ) ++src;

        if ( now_key && *src==':' )  /* end of key */
        {
//            if ( j > 31 ) j = 31;
            key[j] = EOS;
            j = 0;
            now_key = 0;

            while ( *src && (*src==' ' || *src=='\t') ) ++src;

            if ( *src=='"' )
            {
                is_string = 1;
                ++src;
            }
            else
                is_string = 0;

            if ( *src=='[' )
            {
                is_array = 1;
                ++src;
            }
            else
                is_array = 0;
        }
        else if ( now_value && *src==',' || *src=='}' || (is_string && *src=='"') )  /* end of value */
        {
//            if ( j > 255 ) j = 255;
            value[j] = EOS;
            j = 0;

            if ( is_string )
                JSON_SET_STR(*json, key, value);
            else
                JSON_SET_NUM(*json, key, atol(value));
            now_value = 0;

            now_key = 1;
        }
        else
        {
            if ( now_key )
            {
                if ( j < 30 )
                    key[j++] = *src;
            }
            else    /* value */
            {
                if ( j < 254 )
                    value[j++] = *src;
            }
        }

        if ( !*src ) break;

        ++src;
    }
}


/* --------------------------------------------------------------------------
   Log JSON REST buffer
-------------------------------------------------------------------------- */
void lib_json_log_dbg(JSON *json)
{
    int i;

    DBG("-------------------------------------------------------------------------------");
    DBG("REST buffer:");

    for ( i=0; i<json->cnt; ++i )
        DBG("%s [%s]", json->rec[i].name, json->rec[i].value);

    DBG("-------------------------------------------------------------------------------");
}


/* --------------------------------------------------------------------------
   Log JSON REST buffer
-------------------------------------------------------------------------- */
void lib_json_log_inf(JSON *json)
{
    int i;

    INF("-------------------------------------------------------------------------------");
    INF("REST buffer:");

    for ( i=0; i<json->cnt; ++i )
        INF("%s [%s]", json->rec[i].name, json->rec[i].value);

    INF("-------------------------------------------------------------------------------");
}


/* --------------------------------------------------------------------------
   Add/set value to a JSON REST buffer
-------------------------------------------------------------------------- */
bool lib_json_set(JSON *json, const char *name, const char *str_value, long num_value, char type)
{
    int i;

    i = json_get_i(json, name);

    if ( i==-1 )    /* not present */
    {
        if ( json->cnt >= JSON_MAX_ELEMS ) return FALSE;
        i = json->cnt;
        ++json->cnt;
    }

    strncpy(json->rec[i].name, name, 31);
    json->rec[i].name[31] = EOS;

    if ( type == JSON_STRING )
    {
        strncpy(json->rec[i].value, str_value, 255);
        json->rec[i].value[255] = EOS;
    }
    else if ( type == JSON_BOOL )
    {
        if ( num_value )
            strcpy(json->rec[i].value, "true");
        else
            strcpy(json->rec[i].value, "false");
    }
    else
    {
        char tmp[64];
        sscanf(tmp, "%ld", num_value);
        strcpy(json->rec[i].value, tmp);
    }

    json->rec[i].type = type;

    return TRUE;
}


/* --------------------------------------------------------------------------
   Get value from JSON buffer
-------------------------------------------------------------------------- */
bool lib_json_get(JSON *json, const char *name, char *str_value, long *num_value, char type)
{
    int i;

    for ( i=0; i<json->cnt; ++i )
    {
        if ( 0==strcmp(json->rec[i].name, name) )
        {
            if ( type == JSON_STRING && json->rec[i].type == JSON_STRING )
            {
                strcpy(str_value, json->rec[i].value);
                return TRUE;
            }
            else if ( type == JSON_NUMBER && json->rec[i].type == JSON_NUMBER )
            {
                *num_value = atol(json->rec[i].value);
                return TRUE;
            }
            else if ( type == JSON_BOOL && json->rec[i].type == JSON_BOOL )
            {
                if ( json->rec[i].value[0] == 't' )
                    *num_value = 1;
                else
                    *num_value = 0;
                return TRUE;
            }
            else if ( type == JSON_STRING && json->rec[i].type == JSON_NUMBER )
            {
                strcpy(str_value, json->rec[i].value);
                return TRUE;
            }
            else if ( type == JSON_STRING && json->rec[i].type == JSON_BOOL )
            {
                strcpy(str_value, json->rec[i].value);
                return TRUE;
            }
            else if ( type == JSON_BOOL && json->rec[i].type == JSON_STRING )
            {
                if ( 0==strcmp(json->rec[i].value, "true") )
                    *num_value = 1;
                else
                    *num_value = 0;
                return TRUE;
            }

            return FALSE;   /* types don't match or couldn't convert */
        }
    }

    return FALSE;   /* no such field */
}


/* --------------------------------------------------------------------------
   Get value from JSON buffer
-------------------------------------------------------------------------- */
char *lib_json_get_str(JSON *json, const char *name)
{
static char dst[256];
    int     i;

    for ( i=0; i<json->cnt; ++i )
    {
        if ( 0==strcmp(json->rec[i].name, name) )
        {
            if ( json->rec[i].type == JSON_STRING )
            {
                strcpy(dst, json->rec[i].value);
                return dst;
            }
            else if ( json->rec[i].type == JSON_NUMBER )
            {
                strcpy(dst, json->rec[i].value);
                return dst;
            }
            else if ( json->rec[i].type == JSON_BOOL )
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
long lib_json_get_num(JSON *json, const char *name)
{
    int i;

    for ( i=0; i<json->cnt; ++i )
    {
        if ( 0==strcmp(json->rec[i].name, name) )
        {
            if ( json->rec[i].type == JSON_NUMBER )
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
bool lib_json_get_bool(JSON *json, const char *name)
{
    int i;

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
   Add script to HTML head
-------------------------------------------------------------------------- */
void add_script(int ci, const char *fname, bool first)
{
#ifndef ASYNC_SERVICE
    if ( first )
    {
        DBG("first = TRUE; Defining ldscript()");
        OUT("function ldscript(n){var f=document.createElement('script');f.setAttribute(\"type\",\"text/javascript\");f.setAttribute(\"src\",n);document.getElementsByTagName(\"head\")[0].appendChild(f);}");
    }
    OUT("ldscript('%s');", fname);
#endif
}


/* --------------------------------------------------------------------------
   Add CSS link to HTML head
-------------------------------------------------------------------------- */
void add_css(int ci, const char *fname, bool first)
{
#ifndef ASYNC_SERVICE
    if ( first )
    {
        DBG("first = TRUE; Defining ldlink()");
        OUT("function ldlink(n){var f=document.createElement('link');f.setAttribute(\"rel\",\"stylesheet\");f.setAttribute(\"type\",\"text/css\");f.setAttribute(\"href\",n);document.getElementsByTagName(\"head\")[0].appendChild(f);}");
    }
    OUT("ldlink('%s');", fname);
#endif
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
   Read & parse conf file and set global parameters
-------------------------------------------------------------------------- */
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


/* --------------------------------------------------------------------------
   Get param from config file
---------------------------------------------------------------------------*/
bool silgy_read_param(const char *param, char *dest)
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
        if ( dest ) dest[0] = EOS;
        return FALSE;
    }

    /* string present but is it label or value? */

    if ( p > M_conf && *(p-1) != '\n' )
    {
        /* looks like it's a value or it's commented out */
        if ( dest ) dest[0] = EOS;
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
  start a log. uses global G_log as file handler
-------------------------------------------------------------------------- */
bool log_start(const char *prefix, bool test)
{
    char    fprefix[64]="";     /* formatted prefix */
    char    fname[512];         /* file name */
    char    ffname[512];        /* full file name */

    if ( prefix && prefix[0] )
        sprintf(fprefix, "%s_", prefix);

    sprintf(fname, "%s/logs/%s%d%02d%02d_%02d%02d", G_appdir, fprefix, G_ptm->tm_year+1900, G_ptm->tm_mon+1, G_ptm->tm_mday, G_ptm->tm_hour, G_ptm->tm_min);

    if ( test )
        sprintf(ffname, "%s_t.log", fname);
    else
        sprintf(ffname, "%s.log", fname);

    if ( NULL == (G_log=fopen(ffname, "a")) )
    {
        /* try in current directory */

        sprintf(fname, "%s%d%02d%02d_%02d%02d", fprefix, G_ptm->tm_year+1900, G_ptm->tm_mon+1, G_ptm->tm_mday, G_ptm->tm_hour, G_ptm->tm_min);

        if ( test )
            sprintf(ffname, "%s_t.log", fname);
        else
            sprintf(ffname, "%s.log", fname);

        if ( NULL == (G_log=fopen(ffname, "a")) )
        {
            printf("ERROR: Couldn't open log file. Make sure SILGYDIR is defined in your environment and there is a `logs' directory there.\n");
            return FALSE;
        }
    }

    if ( fprintf(G_log, "----------------------------------------------------------------------------------------------\n") < 0 )
    {
        perror("fprintf");
        return FALSE;
    }

    ALWAYS(" %s  Starting %s's log. Server version: %s, app version: %s", G_dt, APP_WEBSITE, WEB_SERVER_VERSION, APP_VERSION);

    fprintf(G_log, "----------------------------------------------------------------------------------------------\n\n");

    return TRUE;
}


/* --------------------------------------------------------------------------
   Write to log with date/time. Uses global G_log as file handler
-------------------------------------------------------------------------- */
void log_write_time(int level, const char *message, ...)
{
    va_list     plist;
static char     buffer[MAX_LOG_STR_LEN+1+64];   /* don't use stack */

    if ( level > G_logLevel ) return;

    /* output timestamp */

    fprintf(G_log, "[%s] ", G_dt);

    if ( LOG_ERR == level )
        fprintf(G_log, "ERROR: ");
    else if ( LOG_WAR == level )
        fprintf(G_log, "WARNING: ");

    /* compile message with arguments into buffer */

    va_start(plist, message);
    vsprintf(buffer, message, plist);
    va_end(plist);

    /* write to log file */

    fprintf(G_log, "%s\n", buffer);

#ifdef DUMP
    fflush(G_log);
#else
    if ( G_logLevel >= LOG_DBG ) fflush(G_log);
#endif
}


/* --------------------------------------------------------------------------
   Write to log. Uses global G_log as file handler
-------------------------------------------------------------------------- */
void log_write(int level, const char *message, ...)
{
    va_list     plist;
static char     buffer[MAX_LOG_STR_LEN+1+64];   /* don't use stack */

    if ( level > G_logLevel ) return;

    if ( LOG_ERR == level )
        fprintf(G_log, "ERROR: ");
    else if ( LOG_WAR == level )
        fprintf(G_log, "WARNING: ");

    /* compile message with arguments into buffer */

    va_start(plist, message);
    vsprintf(buffer, message, plist);
    va_end(plist);

    /* write to log file */

    fprintf(G_log, "%s\n", buffer);

#ifdef DUMP
    fflush(G_log);
#else
    if ( G_logLevel >= LOG_DBG ) fflush(G_log);
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
   Close log. uses global G_log as file handler
-------------------------------------------------------------------------- */
void log_finish()
{
    if ( !G_log ) return;

    ALWAYS("Closing log");
    fclose(G_log);
}


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


#ifndef stpcpy
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
#endif


#ifndef stpncpy
/* --------------------------------------------------------------------------
   Windows port of stpcpy
-------------------------------------------------------------------------- */
char *stpncpy(char *dest, const char *src, int len)
{
    register char *d=dest;
    register const char *s=src;
    int count=0;

    do
        *d++ = *s;
    while (*s++ != '\0' && ++count<len);

    return d - 1;
}
#endif

#endif  /* _WIN32 */
