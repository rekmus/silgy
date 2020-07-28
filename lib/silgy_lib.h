/* --------------------------------------------------------------------------
   Silgy Web App Engine
   Jurek Muszynski
   silgy.com
-----------------------------------------------------------------------------
   General purpose library
-------------------------------------------------------------------------- */

#ifndef SILGY_LIB_H
#define SILGY_LIB_H


#define ALWAYS(str, ...)                log_write(LOG_ALWAYS, str, ##__VA_ARGS__)
#define ERR(str, ...)                   log_write(LOG_ERR, str, ##__VA_ARGS__)
#define WAR(str, ...)                   log_write(LOG_WAR, str, ##__VA_ARGS__)
#define INF(str, ...)                   log_write(LOG_INF, str, ##__VA_ARGS__)
#define DBG(str, ...)                   log_write(LOG_DBG, str, ##__VA_ARGS__)

#define ALWAYS_T(str, ...)              log_write_time(LOG_ALWAYS, str, ##__VA_ARGS__)
#define ERR_T(str, ...)                 log_write_time(LOG_ERR, str, ##__VA_ARGS__)
#define WAR_T(str, ...)                 log_write_time(LOG_WAR, str, ##__VA_ARGS__)
#define INF_T(str, ...)                 log_write_time(LOG_INF, str, ##__VA_ARGS__)
#define DBG_T(str, ...)                 log_write_time(LOG_DBG, str, ##__VA_ARGS__)

#define LOG_LINE                        "--------------------------------------------------"
#define LOG_LINE_N                      "--------------------------------------------------\n"
#define LOG_LINE_NN                     "--------------------------------------------------\n\n"
#define ALWAYS_LINE                     ALWAYS(LOG_LINE)
#define INF_LINE                        INF(LOG_LINE)
#define DBG_LINE                        DBG(LOG_LINE)

#define LOG_LINE_LONG                   "--------------------------------------------------------------------------------------------------"
#define LOG_LINE_LONG_N                 "--------------------------------------------------------------------------------------------------\n"
#define LOG_LINE_LONG_NN                "--------------------------------------------------------------------------------------------------\n\n"
#define ALWAYS_LINE_LONG                ALWAYS(LOG_LINE_LONG)
#define INF_LINE_LONG                   INF(LOG_LINE_LONG)
#define DBG_LINE_LONG                   DBG(LOG_LINE_LONG)

#define LOREM_IPSUM                     "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum."

#define PARAM(param)                    (0==strcmp(label, param))

#define UTF8_ANY(c)                     (((unsigned char)c & 0x80) == 0x80)
#define UTF8_START(c)                   (UTF8_ANY(c) && ((unsigned char)c & 0x40) == 0x40)

#define COPY(dst, src, dst_len)         silgy_safe_copy(dst, src, dst_len)


/* Query String Value */

typedef char                            QSVAL[QSBUF];
//typedef struct QSVAL                  { char x[QSBUF]; } QSVAL;

typedef char                            QSVAL1K[1024];
typedef char                            QSVAL2K[2048];
typedef char                            QSVAL4K[4096];
typedef char                            QSVAL8K[8192];
typedef char                            QSVAL16K[16384];
typedef char                            QSVAL32K[32768];
typedef char                            QSVAL64K[65536];
typedef char                            QSVAL_TEXT[65536];


/* query string values' retrieval */

#define ESC_NONE                        (char)0
#define ESC_SQL                         (char)1
#define ESC_HTML                        (char)2

#define QS_DONT_ESCAPE(param, val)      get_qs_param(ci, param, val, MAX_URI_VAL_LEN, ESC_NONE)
#define QS_SQL_ESCAPE(param, val)       get_qs_param(ci, param, val, MAX_URI_VAL_LEN, ESC_SQL)
#define QS_HTML_ESCAPE(param, val)      get_qs_param(ci, param, val, MAX_URI_VAL_LEN, ESC_HTML)

#define QS_TEXT_DONT_ESCAPE(param, val) get_qs_param(ci, param, val, 65535, ESC_NONE)
#define QS_TEXT_SQL_ESCAPE(param, val)  get_qs_param(ci, param, val, 65535, ESC_SQL)
#define QS_TEXT_HTML_ESCAPE(param, val) get_qs_param(ci, param, val, 65535, ESC_HTML)

#ifdef QS_DEF_HTML_ESCAPE
#define QS(param, val)                  get_qs_param(ci, param, val, MAX_URI_VAL_LEN, ESC_HTML)
#define QS1K(param, val)                get_qs_param(ci, param, val, 1023, ESC_HTML)
#define QS2K(param, val)                get_qs_param(ci, param, val, 2047, ESC_HTML)
#define QS4K(param, val)                get_qs_param(ci, param, val, 4095, ESC_HTML)
#define QS8K(param, val)                get_qs_param(ci, param, val, 8191, ESC_HTML)
#define QS16K(param, val)               get_qs_param(ci, param, val, 16383, ESC_HTML)
#define QS32K(param, val)               get_qs_param(ci, param, val, 32767, ESC_HTML)
#define QS64K(param, val)               get_qs_param(ci, param, val, 65535, ESC_HTML)
#define QS_TEXT(param, val)             get_qs_param(ci, param, val, 65535, ESC_HTML)
#endif
#ifdef QS_DEF_SQL_ESCAPE
#define QS(param, val)                  get_qs_param(ci, param, val, MAX_URI_VAL_LEN, ESC_SQL)
#define QS1K(param, val)                get_qs_param(ci, param, val, 1023, ESC_SQL)
#define QS2K(param, val)                get_qs_param(ci, param, val, 2047, ESC_SQL)
#define QS4K(param, val)                get_qs_param(ci, param, val, 4095, ESC_SQL)
#define QS8K(param, val)                get_qs_param(ci, param, val, 8191, ESC_SQL)
#define QS16K(param, val)               get_qs_param(ci, param, val, 16383, ESC_SQL)
#define QS32K(param, val)               get_qs_param(ci, param, val, 32767, ESC_SQL)
#define QS64K(param, val)               get_qs_param(ci, param, val, 65535, ESC_SQL)
#define QS_TEXT(param, val)             get_qs_param(ci, param, val, 65535, ESC_SQL)
#endif
#ifdef QS_DEF_DONT_ESCAPE
#define QS(param, val)                  get_qs_param(ci, param, val, MAX_URI_VAL_LEN, ESC_NONE)
#define QS1K(param, val)                get_qs_param(ci, param, val, 1023, ESC_NONE)
#define QS2K(param, val)                get_qs_param(ci, param, val, 2047, ESC_NONE)
#define QS4K(param, val)                get_qs_param(ci, param, val, 4095, ESC_NONE)
#define QS8K(param, val)                get_qs_param(ci, param, val, 8191, ESC_NONE)
#define QS16K(param, val)               get_qs_param(ci, param, val, 16383, ESC_NONE)
#define QS32K(param, val)               get_qs_param(ci, param, val, 32767, ESC_NONE)
#define QS64K(param, val)               get_qs_param(ci, param, val, 65535, ESC_NONE)
#define QS_TEXT(param, val)             get_qs_param(ci, param, val, 65535, ESC_NONE)
#endif

#define QSI(param, val)                 lib_qsi(ci, param, val)
#define QSU(param, val)                 lib_qsu(ci, param, val)
#define QSF(param, val)                 lib_qsf(ci, param, val)
#define QSD(param, val)                 lib_qsd(ci, param, val)
#define QSB(param, val)                 lib_qsb(ci, param, val)


#define RES_HEADER(key, val)            lib_res_header(ci, key, val)


#ifdef APP_EMAIL_FROM_USER
#define EMAIL_FROM_USER                 APP_EMAIL_FROM_USER
#else
#define EMAIL_FROM_USER                 "noreply"
#endif


#define CONNECT     0
#define READ        1
#define WRITE       2
#define SHUTDOWN    3


#define MAX_SHM_SEGMENTS                100


/* API authorization system */

#define AUTH_NONE                       0x00
#define AUTH_CREATE                     0x01
#define AUTH_READ                       0x02
#define AUTH_UPDATE                     0x04
#define AUTH_DELETE                     0x08
#define AUTH_FULL                       0xFF

#define IS_AUTH_CREATE(flags)           ((flags & AUTH_CREATE) == AUTH_CREATE)
#define IS_AUTH_READ(flags)             ((flags & AUTH_READ) == AUTH_READ)
#define IS_AUTH_UPDATE(flags)           ((flags & AUTH_UPDATE) == AUTH_UPDATE)
#define IS_AUTH_DELETE(flags)           ((flags & AUTH_DELETE) == AUTH_DELETE)



/* languages */

#ifndef MAX_LANGUAGES
#define MAX_LANGUAGES                   250
#endif

typedef struct {
    char lang[LANG_LEN+1];
    int  first_index;
    int  next_lang_index;
} lang_t;



/* messages */

#ifndef MAX_MSG_LEN
#define MAX_MSG_LEN                     255
#endif

#ifndef MAX_MESSAGES
#define MAX_MESSAGES                    1000
#endif

typedef struct {
    int  code;
    char lang[LANG_LEN+1];
    char message[MAX_MSG_LEN+1];
} message_t;


#define silgy_message(code)             lib_get_message(ci, code)
#define MSG(code)                       lib_get_message(ci, code)
#define MSG_CAT_GREEN(code)             silgy_is_msg_main_cat(code, MSG_CAT_MESSAGE)
#define MSG_CAT_ORANGE(code)            silgy_is_msg_main_cat(code, MSG_CAT_WARNING)
#define MSG_CAT_RED(code)               silgy_is_msg_main_cat(code, MSG_CAT_ERROR)

#define OUT_MSG_DESCRIPTION(code)       lib_send_msg_description(ci, code)

#define OUT_HTML_HEADER                 lib_out_html_header(ci)
#define OUT_HTML_FOOTER                 lib_out_html_footer(ci)
#define OUT_SNIPPET(name)               lib_out_snippet(ci, name)
#define OUT_SNIPPET_MD(name)            lib_out_snippet_md(ci, name)

#define GET_COOKIE(key, val)            lib_get_cookie(ci, key, val)
#define SET_COOKIE(key, val, days)      lib_set_cookie(ci, key, val, days)


/* strings */

#ifndef STRINGS_SEP
#define STRINGS_SEP                     '|'
#endif

#ifndef STRINGS_LANG
#define STRINGS_LANG                    "EN-US"
#endif

#ifndef MAX_STR_LEN
#define MAX_STR_LEN                     255
#endif

#ifndef MAX_STRINGS
#define MAX_STRINGS                     1000
#endif

typedef struct {
    char lang[LANG_LEN+1];
    char string_upper[MAX_STR_LEN+1];
    char string_in_lang[MAX_STR_LEN+1];
} string_t;



/* format amount */

#define AMT(val)                        silgy_amt(val)



/* REST calls */

#define REST_HEADER_KEY_LEN                         255
#define REST_HEADER_VAL_LEN                         1023

typedef struct {
    char    key[REST_HEADER_KEY_LEN+1];
    char    value[REST_HEADER_VAL_LEN+1];
} rest_header_t;


#define REST_MAX_HEADERS                            100
#define REST_HEADERS_RESET                          lib_rest_headers_reset()
#define REST_HEADER_SET(key, val)                   lib_rest_header_set(key, val)
#define REST_HEADER_UNSET(key)                      lib_rest_header_unset(key)


#ifdef JSON_NO_AUTO_AMPERSANDS
#define CALL_REST_HTTP(req, res, method, url, keep) lib_rest_req(req, res, method, url, FALSE, keep)
#define CALL_REST_JSON(req, res, method, url, keep) lib_rest_req(req, res, method, url, TRUE, keep)
#else
#define CALL_REST_HTTP(req, res, method, url, keep) lib_rest_req((char*)req, (char*)res, method, url, FALSE, keep)
#define CALL_REST_JSON(req, res, method, url, keep) lib_rest_req(&req, &res, method, url, TRUE, keep)
#endif

/* aliases -- highest level -- 'keep' always TRUE */
#define CALL_REST_RAW(req, res, method, url)        CALL_REST_HTTP(req, res, method, url, TRUE)
#define CALL_HTTP(req, res, method, url)            CALL_REST_HTTP(req, res, method, url, TRUE)
#define CALL_REST(req, res, method, url)            CALL_REST_JSON(req, res, method, url, TRUE)


#define CALL_REST_DEFAULT_TIMEOUT                   10000     /* in ms -- to avoid blocking forever */

#define REST_RES_HEADER_LEN                         4095
#define REST_ADDRESSES_CACHE_SIZE                   100


#define CALL_HTTP_STATUS                            G_rest_status
#define CALL_REST_STATUS                            CALL_HTTP_STATUS
#define CALL_HTTP_CONTENT_TYPE                      G_rest_content_type
#define CALL_REST_CONTENT_TYPE                      CALL_HTTP_CONTENT_TYPE
#define CALL_HTTP_STATUS_OK                         (G_rest_status>=200 && G_rest_status<=204)
#define CALL_REST_STATUS_OK                         CALL_HTTP_STATUS_OK


/* JSON */

#define JSON_INTEGER            0
#define JSON_UNSIGNED           1
#define JSON_FLOAT              2
#define JSON_DOUBLE             3
#define JSON_STRING             4
#define JSON_BOOL               5
#define JSON_RECORD             6
#define JSON_ARRAY              7

#ifndef JSON_KEY_LEN
#define JSON_KEY_LEN            31
#endif
#ifndef JSON_VAL_LEN
#define JSON_VAL_LEN            255
#endif

#ifdef APP_JSON_MAX_ELEMS       /* in one JSON struct */
#define JSON_MAX_ELEMS          APP_JSON_MAX_ELEMS
#else
#define JSON_MAX_ELEMS          10
#endif

#ifdef APP_JSON_MAX_LEVELS
#define JSON_MAX_LEVELS         APP_JSON_MAX_LEVELS
#else
#ifdef MEM_TINY
#define JSON_MAX_LEVELS         2
#else
#define JSON_MAX_LEVELS         4
#endif
#endif  /* APP_JSON_MAX_LEVELS */

#ifdef MEM_TINY
#define JSON_POOL_SIZE          100     /* for storing sub-JSONs */
#else
#define JSON_POOL_SIZE          1000    /* for storing sub-JSONs */
#endif


/* single JSON element */

typedef struct {
    char    name[JSON_KEY_LEN+1];
    char    value[JSON_VAL_LEN+1];
    char    type;
} json_elem_t;

/* JSON object */

typedef struct {
    int         cnt;
    char        array;
    json_elem_t rec[JSON_MAX_ELEMS];
} json_t;

typedef json_t JSON;


#define JSON_MAX_FLOAT_LEN                  8


#ifdef APP_JSON_BUFSIZE
#define JSON_BUFSIZE                        APP_JSON_BUFSIZE
#else
#define JSON_BUFSIZE                        65568
#endif


#ifdef JSON_NO_AUTO_AMPERSANDS

#define JSON_TO_STRING(json)                lib_json_to_string(json)
#define JSON_TO_STRING_PRETTY(json)         lib_json_to_string_pretty(json)
#define JSON_FROM_STRING(json, str)         lib_json_from_string(json, str, 0, 0)


#define JSON_ADD_STR(json, name, val)       lib_json_add(json, name, val, 0, 0, 0, 0, JSON_STRING, -1)
#define JSON_ADD_STR_A(json, i, val)        lib_json_add(json, NULL, val, 0, 0, 0, 0, JSON_STRING, i)
#define JSON_ADD_INT(json, name, val)       lib_json_add(json, name, NULL, val, 0, 0, 0, JSON_INTEGER, -1)
#define JSON_ADD_INT_A(json, i, val)        lib_json_add(json, NULL, NULL, val, 0, 0, 0, JSON_INTEGER, i)
#define JSON_ADD_UINT(json, name, val)      lib_json_add(json, name, NULL, 0, val, 0, 0, JSON_UNSIGNED, -1)
#define JSON_ADD_UINT_A(json, i, val)       lib_json_add(json, NULL, NULL, 0, val, 0, 0, JSON_UNSIGNED, i)
#define JSON_ADD_FLOAT(json, name, val)     lib_json_add(json, name, NULL, 0, 0, val, 0, JSON_FLOAT, -1)
#define JSON_ADD_FLOAT_A(json, i, val)      lib_json_add(json, NULL, NULL, 0, 0, val, 0, JSON_FLOAT, i)
#define JSON_ADD_DOUBLE(json, name, val)    lib_json_add(json, name, NULL, 0, 0, 0, val, JSON_DOUBLE, -1)
#define JSON_ADD_DOUBLE_A(json, i, val)     lib_json_add(json, NULL, NULL, 0, 0, 0, val, JSON_DOUBLE, i)
#define JSON_ADD_BOOL(json, name, val)      lib_json_add(json, name, NULL, val, 0, 0, 0, JSON_BOOL, -1)
#define JSON_ADD_BOOL_A(json, i, val)       lib_json_add(json, NULL, NULL, val, 0, 0, 0, JSON_BOOL, i)

#define JSON_ADD_RECORD(json, name, val)    lib_json_add_record(json, name, val, FALSE, -1)
#define JSON_ADD_RECORD_A(json, i ,val)     lib_json_add_record(json, NULL, val, FALSE, i)

#define JSON_ADD_ARRAY(json, name, val)     lib_json_add_record(json, name, val, TRUE, -1)
#define JSON_ADD_ARRAY_A(json, i, val)      lib_json_add_record(json, NULL, val, TRUE, i)

#define JSON_PRESENT(json, name)            lib_json_present(json, name)

#define JSON_GET_STR(json, name)            lib_json_get_str(json, name, -1)
#define JSON_GET_STR_A(json, i)             lib_json_get_str(json, NULL, i)
#define JSON_GET_INT(json, name)            lib_json_get_int(json, name, -1)
#define JSON_GET_INT_A(json, i)             lib_json_get_int(json, NULL, i)
#define JSON_GET_UINT(json, name)           lib_json_get_uint(json, name, -1)
#define JSON_GET_UINT_A(json, i)            lib_json_get_uint(json, NULL, i)
#define JSON_GET_FLOAT(json, name)          lib_json_get_float(json, name, -1)
#define JSON_GET_FLOAT_A(json, i)           lib_json_get_float(json, NULL, i)
#define JSON_GET_DOUBLE(json, name)         lib_json_get_double(json, name, -1)
#define JSON_GET_DOUBLE_A(json, i)          lib_json_get_double(json, NULL, i)
#define JSON_GET_BOOL(json, name)           lib_json_get_bool(json, name, -1)
#define JSON_GET_BOOL_A(json, i)            lib_json_get_bool(json, NULL, i)

#define JSON_GET_RECORD(json, name, val)    lib_json_get_record(json, name, val, -1)
#define JSON_GET_RECORD_A(json, i, val)     lib_json_get_record(json, NULL, val, i)

#define JSON_GET_ARRAY(json, name, val)     lib_json_get_record(json, name, val, -1)
#define JSON_GET_ARRAY_A(json, i, val)      lib_json_get_record(json, NULL, val, i)


#define JSON_RESET(json)                    lib_json_reset(json)
#define JSON_COUNT(json)                    json.cnt

#define JSON_LOG_DBG(json, name)            lib_json_log_dbg(json, name)
#define JSON_LOG_INF(json, name)            lib_json_log_inf(json, name)

#else  /* JSON_NO_AUTO_AMPERSANDS not defined */

#define JSON_TO_STRING(json)                lib_json_to_string(&json)
#define JSON_TO_STRING_PRETTY(json)         lib_json_to_string_pretty(&json)
#define JSON_FROM_STRING(json, str)         lib_json_from_string(&json, str, 0, 0)


#define JSON_ADD_STR(json, name, val)       lib_json_add(&json, name, val, 0, 0, 0, 0, JSON_STRING, -1)
#define JSON_ADD_STR_A(json, i, val)        lib_json_add(&json, NULL, val, 0, 0, 0, 0, JSON_STRING, i)
#define JSON_ADD_INT(json, name, val)       lib_json_add(&json, name, NULL, val, 0, 0, 0, JSON_INTEGER, -1)
#define JSON_ADD_INT_A(json, i, val)        lib_json_add(&json, NULL, NULL, val, 0, 0, 0, JSON_INTEGER, i)
#define JSON_ADD_UINT(json, name, val)      lib_json_add(&json, name, NULL, 0, val, 0, 0, JSON_UNSIGNED, -1)
#define JSON_ADD_UINT_A(json, i, val)       lib_json_add(&json, NULL, NULL, 0, val, 0, 0, JSON_UNSIGNED, i)
#define JSON_ADD_FLOAT(json, name, val)     lib_json_add(&json, name, NULL, 0, 0, val, 0, JSON_FLOAT, -1)
#define JSON_ADD_FLOAT_A(json, i, val)      lib_json_add(&json, NULL, NULL, 0, 0, val, 0, JSON_FLOAT, i)
#define JSON_ADD_DOUBLE(json, name, val)    lib_json_add(&json, name, NULL, 0, 0, 0, val, JSON_DOUBLE, -1)
#define JSON_ADD_DOUBLE_A(json, i, val)     lib_json_add(&json, NULL, NULL, 0, 0, 0, val, JSON_DOUBLE, i)
#define JSON_ADD_BOOL(json, name, val)      lib_json_add(&json, name, NULL, val, 0, 0, 0, JSON_BOOL, -1)
#define JSON_ADD_BOOL_A(json, i, val)       lib_json_add(&json, NULL, NULL, val, 0, 0, 0, JSON_BOOL, i)

#define JSON_ADD_RECORD(json, name, val)    lib_json_add_record(&json, name, &val, FALSE, -1)
#define JSON_ADD_RECORD_A(json, i, val)     lib_json_add_record(&json, NULL, &val, FALSE, i)

#define JSON_ADD_ARRAY(json, name, val)     lib_json_add_record(&json, name, &val, TRUE, -1)
#define JSON_ADD_ARRAY_A(json, i, val)      lib_json_add_record(&json, NULL, &val, TRUE, i)

#define JSON_PRESENT(json, name)            lib_json_present(&json, name)

#define JSON_GET_STR(json, name)            lib_json_get_str(&json, name, -1)
#define JSON_GET_STR_A(json, i)             lib_json_get_str(&json, NULL, i)
#define JSON_GET_INT(json, name)            lib_json_get_int(&json, name, -1)
#define JSON_GET_INT_A(json, i)             lib_json_get_int(&json, NULL, i)
#define JSON_GET_UINT(json, name)           lib_json_get_uint(&json, name, -1)
#define JSON_GET_UINT_A(json, i)            lib_json_get_uint(&json, NULL, i)
#define JSON_GET_FLOAT(json, name)          lib_json_get_float(&json, name, -1)
#define JSON_GET_FLOAT_A(json, i)           lib_json_get_float(&json, NULL, i)
#define JSON_GET_DOUBLE(json, name)         lib_json_get_double(&json, name, -1)
#define JSON_GET_DOUBLE_A(json, i)          lib_json_get_double(&json, NULL, i)
#define JSON_GET_BOOL(json, name)           lib_json_get_bool(&json, name, -1)
#define JSON_GET_BOOL_A(json, i)            lib_json_get_bool(&json, NULL, i)

#define JSON_GET_RECORD(json, name, val)    lib_json_get_record(&json, name, &val, -1)
#define JSON_GET_RECORD_A(json, i, val)     lib_json_get_record(&json, NULL, &val, i)

#define JSON_GET_ARRAY(json, name, val)     lib_json_get_record(&json, name, &val, -1)
#define JSON_GET_ARRAY_A(json, i, val)      lib_json_get_record(&json, NULL, &val, i)


#define JSON_RESET(json)                    lib_json_reset(&json)
#define JSON_COUNT(json)                    json.cnt

#define JSON_LOG_DBG(json, name)            lib_json_log_dbg(&json, name)
#define JSON_LOG_INF(json, name)            lib_json_log_inf(&json, name)

#endif  /* JSON_NO_AUTO_AMPERSANDS */


/* for backward compatibility */
#define silgy_read_param(param, val)        silgy_read_param_str(param, val)


#define AI_USERS_ALL                        0       /* all users */
#define AI_USERS_YAU                        1       /* yearly active */
#define AI_USERS_MAU                        2       /* monthly active */
#define AI_USERS_DAU                        3       /* daily active */


#ifdef __cplusplus
extern "C" {
#endif
    void silgy_lib_init(void);
    void silgy_lib_done(void);
    void silgy_safe_copy(char *dst, const char *src, size_t dst_len);
    void silgy_set_tz(int ci);
    time_t silgy_ua_time(int ci);
    char *silgy_ua_today(int ci);
    char *silgy_today(void);
    char *silgy_render_md(char *dest, const char *src, size_t len);
    char *silgy_json_enc(const char *src);
    bool lib_csrft_ok(int ci);
    void silgy_add_message(int code, const char *lang, const char *message, ...);
    int  compare_messages(const void *a, const void *b);
    void sort_messages(void);
    char *lib_get_message(int ci, int code);
    bool silgy_is_msg_main_cat(int code, const char *cat);
    void silgy_add_string(const char *lang, const char *str, const char *str_lang);
    const char *lib_get_string(int ci, const char *str);
    char *urlencode(const char *src);
    bool lib_open_db(void);
    void lib_close_db(void);
    bool lib_file_exists(const char *fname);
    void lib_get_exec_name(char *dst, const char *path);
    void lib_update_time_globals(void);
    bool read_snippets(bool first_scan, const char *path);
    char *silgy_get_snippet(const char *name);
    unsigned silgy_get_snippet_len(const char *name);
    void lib_out_snippet(int ci, const char *name);
    void lib_out_snippet_md(int ci, const char *name);
    void lib_setnonblocking(int sock);
    void lib_out_html_header(int ci);
    void lib_out_html_footer(int ci);
    void lib_append_css(int ci, const char *fname, bool first);
    void lib_append_script(int ci, const char *fname, bool first);
    char *uri_decode(char *src, int srclen, char *dest, int maxlen);
    bool get_qs_param(int ci, const char *fieldname, char *retbuf, int maxlen, char esc_type);
    bool get_qs_param_raw(int ci, const char *fieldname, char *retbuf, int maxlen);
    char *get_qs_param_multipart(int ci, const char *fieldname, unsigned *retlen, char *retfname);
    bool lib_qsi(int ci, const char *fieldname, int *retbuf);
    bool lib_qsu(int ci, const char *fieldname, unsigned *retbuf);
    bool lib_qsf(int ci, const char *fieldname, float *retbuf);
    bool lib_qsd(int ci, const char *fieldname, double *retbuf);
    bool lib_qsb(int ci, const char *fieldname, bool *retbuf);
    void lib_set_res_status(int ci, int status);
    bool lib_res_header(int ci, const char *hdr, const char *val);
    bool lib_get_cookie(int ci, const char *key, char *value);
    bool lib_set_cookie(int ci, const char *key, const char *value, int days);
    void lib_set_res_content_type(int ci, const char *str);
    void lib_set_res_location(int ci, const char *str, ...);
    void lib_set_res_content_disposition(int ci, const char *str, ...);
    void lib_send_msg_description(int ci, int code);
    void silgy_admin_info(int ci, int users, admin_info_t ai[], int ai_cnt, bool header_n_footer);
    void lib_rest_headers_reset(void);
    void lib_rest_header_set(const char *key, const char *value);
    void lib_rest_header_unset(const char *key);
    bool lib_rest_req(const void *req, void *res, const char *method, const char *url, bool json, bool keep);
    int  lib_finish_with_timeout(int sock, char oper, char readwrite, char *buffer, int len, int *msec, void *ssl, int level);
    void log_ssl_error(int ssl_err);
    void lib_get_app_dir(void);
    double lib_elapsed(struct timespec *start);
    int  lib_get_memory(void);
    void lib_log_memory(void);
    char *silgy_filter_strict(const char *src);
    char *lib_add_spaces(const char *src, int len);
    char *lib_add_lspaces(const char *src, int len);
    char *get_file_ext(const char *fname);
    char get_res_type(const char *fname);
    void date_str2rec(const char *str, date_t *rec);
    void date_rec2str(char *str, date_t *rec);
    time_t time_http2epoch(const char *str);
    time_t time_db2epoch(const char *str);
    char *time_epoch2http(time_t epoch);
    char *time_epoch2db(time_t epoch);
    void lib_set_datetime_formats(const char *lang);
    char *silgy_amt(double val);
    void amt(char *stramt, long long in_amt);
    void amtd(char *stramt, double in_amt);
    void lib_amt(char *stramt, long in_amt);
    void lib_amtd(char *stramt, double in_amt);
    void samts(char *stramt, const char *in_amt);
    void lib_normalize_float(char *str);
    void ftm(char *strtm, long in_tm);
    char *fmt_date(short year, short month, short day);
    char const *san(const char *str);
    char *san_long(const char *str);
    char *silgy_sql_esc(const char *str);
    char *silgy_html_esc(const char *str);
    void sanitize_sql(char *dst, const char *str, int len);
    void sanitize_html(char *dst, const char *str, int len);
    char *silgy_html_unesc(const char *str);
    char *uri_encode(const char *str);
    char *upper(const char *str);
    char *stp_right(char *str);
    bool strdigits(const char *src);
    char *nospaces(char *dst, const char *src);
    void init_random_numbers(void);
    void silgy_random(char *dest, int len);
    void msleep(int msec);
    void lib_json_reset(JSON *json);
    char *lib_json_to_string(JSON *json);
    char *lib_json_to_string_pretty(JSON *json);
    bool lib_json_from_string(JSON *json, const char *src, int len, int level);
    bool lib_json_add(JSON *json, const char *name, const char *str_value, int int_value, unsigned uint_value, float flo_value, double dbl_value, char type, int i);
    bool lib_json_add_record(JSON *json, const char *name, JSON *json_sub, bool is_array, int i);
    bool lib_json_get(JSON *json, const char *name, char *str_value, int *num_value, char type);
    bool lib_json_present(JSON *json, const char *name);
    char *lib_json_get_str(JSON *json, const char *name, int i);
    int  lib_json_get_int(JSON *json, const char *name, int i);
    unsigned lib_json_get_uint(JSON *json, const char *name, int i);
    float lib_json_get_float(JSON *json, const char *name, int i);
    double lib_json_get_double(JSON *json, const char *name, int i);
    bool lib_json_get_bool(JSON *json, const char *name, int i);
    bool lib_json_get_record(JSON *json, const char *name, JSON *json_sub, int i);
    void lib_json_log_dbg(JSON *json, const char *name);
    void lib_json_log_inf(JSON *json, const char *name);
    void get_byteorder(void);
    time_t db2epoch(const char *str);
    bool silgy_email(const char *to, const char *subject, const char *message);
    bool silgy_email_attach(const char *to, const char *subject, const char *message, const char *att_name, const char *att_data, int att_data_len);
    int  silgy_minify(char *dest, const char *src);
    void date_inc(char *str, int days, int *dow);
    int  date_cmp(const char *str1, const char *str2);
    bool lib_read_conf(const char *file);
    bool silgy_read_param_str(const char *param, char *dest);
    bool silgy_read_param_int(const char *param, int *dest);
    char *lib_create_pid_file(const char *name);
    char *lib_shm_create(unsigned bytes, int index);
    void lib_shm_delete(int index);
    bool log_start(const char *prefix, bool test);
    void log_write_time(int level, const char *message, ...);
    void log_write(int level, const char *message, ...);
    void log_long(const char *str, int len, const char *desc);
    void log_flush(void);
    void log_finish(void);
#ifdef ICONV
    char *silgy_convert(const char *src, const char *cp_from, const char *cp_to);
#endif

    char *md5(const char* str);

    int Base64encode_len(int len);
    int Base64encode(char *coded_dst, const char *plain_src, int len_plain_src);
    int Base64decode_len(const char *coded_src);
    int Base64decode(char *plain_dst, const char *coded_src);

#ifdef _WIN32   /* Windows */
    int getpid(void);
    int clock_gettime_win(struct timespec *spec);
    char *stpcpy(char *dest, const char *src);
    char *stpncpy(char *dest, const char *src, size_t len);
#endif  /* _WIN32 */

#ifndef strnstr
    char *strnstr(const char *haystack, const char *needle, size_t len);
#endif 

#ifdef __cplusplus
}   // extern "C"
#endif


#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t state[5];
    uint32_t count[2];
    uint8_t  buffer[64];
} SHA1_CTX;

#define SHA1_DIGEST_SIZE 20

void libSHA1(unsigned char *ptr, unsigned int size, unsigned char *outbuf);

void digest_to_hex(const uint8_t digest[SHA1_DIGEST_SIZE], char *output);


#ifdef __cplusplus
}
#endif

#endif  /* SILGY_LIB_H */
