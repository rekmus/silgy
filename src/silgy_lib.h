/* --------------------------------------------------------------------------
   Silgy Web App Engine
   Jurek Muszynski
   silgy.com
-----------------------------------------------------------------------------
   General purpose library
-------------------------------------------------------------------------- */

#ifndef SILGY_LIB_H
#define SILGY_LIB_H

#define ALWAYS(s, ...)          log_write(LOG_ALWAYS, s, ##__VA_ARGS__)
#define ERR(s, ...)             log_write(LOG_ERR, s, ##__VA_ARGS__)
#define WAR(s, ...)             log_write(LOG_WAR, s, ##__VA_ARGS__)
#define INF(s, ...)             log_write(LOG_INF, s, ##__VA_ARGS__)
#define DBG(s, ...)             log_write(LOG_DBG, s, ##__VA_ARGS__)

#define ALWAYS_T(s, ...)        log_write_time(LOG_ALWAYS, s, ##__VA_ARGS__)
#define ERR_T(s, ...)           log_write_time(LOG_ERR, s, ##__VA_ARGS__)
#define WAR_T(s, ...)           log_write_time(LOG_WAR, s, ##__VA_ARGS__)
#define INF_T(s, ...)           log_write_time(LOG_INF, s, ##__VA_ARGS__)
#define DBG_T(s, ...)           log_write_time(LOG_DBG, s, ##__VA_ARGS__)

#define LOREM_IPSUM             "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum."

#define PARAM(p)                (0==strcmp(label,p))

/* Query String Value */

typedef char                    QSVAL[QSBUF];
//typedef struct QSVAL          { char x[QSBUF]; } QSVAL;


#define CONNECT 0
#define READ    1
#define WRITE   2



/* REST calls */

typedef struct {
    char    key[64];
    char    value[256];
} rest_header_t;


#define REST_MAX_HEADERS                100
#define REST_HEADERS_RESET              lib_rest_headers_reset()
#define REST_HEADER_SET(k,v)            lib_rest_header_set(k, v)
#define REST_HEADER_UNSET(k,v)          lib_rest_header_unset(k)


#ifdef JSON_NO_AUTO_AMPERSANDS
#define CALL_REST_HTTP(req,res,m,u,k)   lib_rest_req(req, res, m, u, FALSE, k)
#define CALL_REST_JSON(req,res,m,u,k)   lib_rest_req(req, res, m, u, TRUE, k)
#else
#define CALL_REST_HTTP(req,res,m,u,k)   lib_rest_req(&req, &res, m, u, FALSE, k)
#define CALL_REST_JSON(req,res,m,u,k)   lib_rest_req(&req, &res, m, u, TRUE, k)
#endif

/* aliases -- highest level -- 'keep' always TRUE */
#define CALL_REST_RAW(req,res,m,u)      CALL_REST_HTTP(req, res, m, u, TRUE)
#define CALL_REST(req,res,m,u)          CALL_REST_JSON(req, res, m, u, TRUE)


#define CALL_REST_DEFAULT_TIMEOUT       1000     /* in ms -- to avoid blocking forever */

#define REST_RES_HEADER_LEN             4095
#define REST_ADDRESSES_CACHE_SIZE       100


/* JSON */

#define JSON_STRING         0
#define JSON_INTEGER        1
#define JSON_FLOAT          2
#define JSON_BOOL           3
#define JSON_RECORD         4
#define JSON_ARRAY          5

#define JSON_MAX_ELEMS      50      /* in one JSON struct */
#define JSON_MAX_LEVELS     4

#define JSON_MAX_JSONS      1000    /* size of the array used for auto-initializing JSON variables */
#define JSON_POOL_SIZE      100     /* for storing sub-JSONs */


/* JSON record */

typedef struct {
    char    name[32];
    char    value[256];
    char    type;
} json_rec_t;

/* JSON buffer */

typedef struct {
    int         cnt;
    char        array;
    json_rec_t  rec[JSON_MAX_ELEMS];
} json_buf_t;

typedef json_buf_t JSON;


#define JSON_BUFSIZE                32784


#ifdef JSON_NO_AUTO_AMPERSANDS

#define JSON_TO_STRING(j)           lib_json_to_string(j)
#define JSON_TO_STRING_PRETTY(j)    lib_json_to_string_pretty(j)
#define JSON_FROM_STRING(j,s)       lib_json_from_string(j, s, 0, 0)


#define JSON_ADD_STR(j,n,v)         lib_json_add(j, n, v, 0, 0, JSON_STRING, -1)
#define JSON_ADD_ARRAY_STR(j,i,v)   lib_json_add(j, NULL, v, 0, 0, JSON_STRING, i)
#define JSON_ADD_INT(j,n,v)         lib_json_add(j, n, NULL, v, 0, JSON_INTEGER, -1)
#define JSON_ADD_ARRAY_INT(j,i,v)   lib_json_add(j, NULL, NULL, v, 0, JSON_INTEGER, i)
#define JSON_ADD_FLOAT(j,n,v)       lib_json_add(j, n, NULL, 0, v, JSON_FLOAT, -1)
#define JSON_ADD_ARRAY_FLOAT(j,i,v) lib_json_add(j, NULL, NULL, 0, v, JSON_FLOAT, i)
#define JSON_ADD_BOOL(j,n,v)        lib_json_add(j, n, NULL, v, 0, JSON_BOOL, -1)
#define JSON_ADD_ARRAY_BOOL(j,i,v)  lib_json_add(j, NULL, NULL, v, 0, JSON_BOOL, i)

#define JSON_ADD_RECORD(j,n,v)      lib_json_add_record(j, n, v, FALSE, -1)
#define JSON_ADD_ARRAY_RECORD(j,i,v) lib_json_add_record(j, NULL, v, FALSE, i)

#define JSON_ADD_ARRAY(j,n,v)       lib_json_add_record(j, n, v, TRUE, -1)
#define JSON_ADD_ARRAY_ARRAY(j,i,v) lib_json_add_record(j, NULL, v, TRUE, i)


#define JSON_GET_STR(j,n)           lib_json_get_str(j, n, -1)
#define JSON_GET_ARRAY_STR(j,i)     lib_json_get_str(j, NULL, i)
#define JSON_GET_INT(j,n)           lib_json_get_int(j, n, -1)
#define JSON_GET_ARRAY_INT(j,i)     lib_json_get_int(j, NULL, i)
#define JSON_GET_FLOAT(j,n)         lib_json_get_float(j, n, -1)
#define JSON_GET_ARRAY_FLOAT(j,i)   lib_json_get_float(j, NULL, i)
#define JSON_GET_BOOL(j,n)          lib_json_get_bool(j, n, -1)
#define JSON_GET_ARRAY_BOOL(j,i)    lib_json_get_bool(j, NULL, i)

#define JSON_GET_RECORD(j,n,v)      lib_json_get_record(j, n, v, -1)
#define JSON_GET_ARRAY_RECORD(j,i,v) lib_json_get_record(j, NULL, v, i)

#define JSON_GET_ARRAY(j,n,v)       lib_json_get_record(j, n, v, -1)
#define JSON_GET_ARRAY_ARRAY(j,i,v) lib_json_get_record(j, NULL, v, i)


#define JSON_RESET(j)               lib_json_reset(j)
#define JSON_COUNT(j)               j.cnt

#define JSON_LOG_DBG(j,n)           lib_json_log_dbg(j, n)
#define JSON_LOG_INF(j,n)           lib_json_log_inf(j, n)

#else  /* JSON_NO_AUTO_AMPERSANDS not defined */

#define JSON_TO_STRING(j)           lib_json_to_string(&j)
#define JSON_TO_STRING_PRETTY(j)    lib_json_to_string_pretty(&j)
#define JSON_FROM_STRING(j,s)       lib_json_from_string(&j, s, 0, 0)


#define JSON_ADD_STR(j,n,v)         lib_json_add(&j, n, v, 0, 0, JSON_STRING, -1)
#define JSON_ADD_ARRAY_STR(j,i,v)   lib_json_add(&j, NULL, v, 0, 0, JSON_STRING, i)
#define JSON_ADD_INT(j,n,v)         lib_json_add(&j, n, NULL, v, 0, JSON_INTEGER, -1)
#define JSON_ADD_ARRAY_INT(j,i,v)   lib_json_add(&j, NULL, NULL, v, 0, JSON_INTEGER, i)
#define JSON_ADD_FLOAT(j,n,v)       lib_json_add(&j, n, NULL, 0, v, JSON_FLOAT, -1)
#define JSON_ADD_ARRAY_FLOAT(j,i,v) lib_json_add(&j, NULL, NULL, 0, v, JSON_FLOAT, i)
#define JSON_ADD_BOOL(j,n,v)        lib_json_add(&j, n, NULL, v, 0, JSON_BOOL, -1)
#define JSON_ADD_ARRAY_BOOL(j,i,v)  lib_json_add(&j, NULL, NULL, v, 0, JSON_BOOL, i)

#define JSON_ADD_RECORD(j,n,v)      lib_json_add_record(&j, n, &v, FALSE, -1)
#define JSON_ADD_ARRAY_RECORD(j,i,v) lib_json_add_record(&j, NULL, &v, FALSE, i)

#define JSON_ADD_ARRAY(j,n,v)       lib_json_add_record(&j, n, &v, TRUE, -1)
#define JSON_ADD_ARRAY_ARRAY(j,i,v) lib_json_add_record(&j, NULL, &v, TRUE, i)


#define JSON_GET_STR(j,n)           lib_json_get_str(&j, n, -1)
#define JSON_GET_ARRAY_STR(j,i)     lib_json_get_str(&j, NULL, i)
#define JSON_GET_INT(j,n)           lib_json_get_int(&j, n, -1)
#define JSON_GET_ARRAY_INT(j,i)     lib_json_get_int(&j, NULL, i)
#define JSON_GET_FLOAT(j,n)         lib_json_get_float(&j, n, -1)
#define JSON_GET_ARRAY_FLOAT(j,i)   lib_json_get_float(&j, NULL, i)
#define JSON_GET_BOOL(j,n)          lib_json_get_bool(&j, n, -1)
#define JSON_GET_ARRAY_BOOL(j,i)    lib_json_get_bool(&j, NULL, i)

#define JSON_GET_RECORD(j,n,v)      lib_json_get_record(&j, n, &v, -1)
#define JSON_GET_ARRAY_RECORD(j,i,v) lib_json_get_record(&j, NULL, &v, i)

#define JSON_GET_ARRAY(j,n,v)       lib_json_get_record(&j, n, &v, -1)
#define JSON_GET_ARRAY_ARRAY(j,i,v) lib_json_get_record(&j, NULL, &v, i)


#define JSON_RESET(j)               lib_json_reset(&j)
#define JSON_COUNT(j)               j.cnt

#define JSON_LOG_DBG(j,n)           lib_json_log_dbg(&j, n)
#define JSON_LOG_INF(j,n)           lib_json_log_inf(&j, n)

#endif  /* JSON_NO_AUTO_AMPERSANDS */

/* for backward compatibility */
#define silgy_read_param(p,v)       silgy_read_param_str(p,v)


#ifdef __cplusplus
extern "C" {
#endif
    void silgy_lib_init(void);
    void lib_get_exec_name(char *dst, const char *path);
    void lib_update_time_globals(void);
    void lib_setnonblocking(int sock);
    void lib_rest_headers_reset(void);
    void lib_rest_header_set(const char *key, const char *value);
    void lib_rest_header_unset(const char *key);
    bool lib_rest_req(const void *req, void *res, const char *method, const char *url, bool json, bool keep);
    int lib_finish_with_timeout(int sock, char readwrite, char *buffer, int len, int *msec, void *ssl, int level);
    void lib_get_app_dir(void);
    double lib_elapsed(struct timespec *start);
    long lib_get_memory(void);
    void lib_log_memory(void);
    char *lib_filter_strict(const char *src);
    char *lib_add_spaces(const char *src, int len);
    char *lib_add_lspaces(const char *src, int len);
    char get_res_type(const char *fname);
    void date_str2rec(const char *str, date_t *rec);
    void date_rec2str(char *str, date_t *rec);
    time_t time_http2epoch(const char *str);
    time_t time_db2epoch(const char *str);
    char *time_epoch2http(time_t epoch);
    void lib_set_datetime_formats(const char *lang);
    void amt(char *stramt, long in_amt);
    void amtd(char *stramt, double in_amt);
    void lib_amt(char *stramt, long in_amt);
    void lib_amtd(char *stramt, double in_amt);
    void samts(char *stramt, const char *in_amt);
    void lib_normalize_float(char *str);
    void ftm(char *strtm, long in_tm);
    char const *san(const char *str);
    char *san_long(const char *str);
    char *silgy_sql_esc(const char *str);
    char *silgy_html_esc(const char *str);
    char *silgy_html_unesc(const char *str);
    char *uri_encode(const char *str);
    char *upper(const char *str);
    char *stp_right(char *str);
    bool strdigits(const char *src);
    char *nospaces(char *dst, const char *src);
    void silgy_random(char *dest, int len);
    void msleep(int msec);
    void lib_json_reset(JSON *json);
    char *lib_json_to_string(JSON *json);
    char *lib_json_to_string_pretty(JSON *json);
    void lib_json_from_string(JSON *json, const char *src, int len, int level);
    bool lib_json_add(JSON *json, const char *name, const char *str_value, long int_value, double flo_value, char type, int i);
    bool lib_json_add_record(JSON *json, const char *name, JSON *json_sub, bool is_array, int i);
    bool lib_json_get(JSON *json, const char *name, char *str_value, long *num_value, char type);
    char *lib_json_get_str(JSON *json, const char *name, int i);
    long lib_json_get_int(JSON *json, const char *name, int i);
    double lib_json_get_float(JSON *json, const char *name, int i);
    bool lib_json_get_bool(JSON *json, const char *name, int i);
    bool lib_json_get_record(JSON *json, const char *name, JSON *json_sub, int i);
    void lib_json_log_dbg(JSON *json, const char *name);
    void lib_json_log_inf(JSON *json, const char *name);
    void get_byteorder(void);
    time_t db2epoch(const char *str);
    bool sendemail(int ci, const char *to, const char *subject, const char *message);
    int silgy_minify(char *dest, const char *src);
    void date_inc(char *str, int days, int *dow);
    int date_cmp(const char *str1, const char *str2);
    bool lib_read_conf(const char *file);
    bool silgy_read_param_str(const char *param, char *dest);
    bool silgy_read_param_int(const char *param, int *dest);
    char *lib_create_pid_file(const char *name);
	bool lib_shm_create(long bytes);
	void lib_shm_delete(long bytes);
    bool log_start(const char *prefix, bool test);
    void log_write_time(int level, const char *message, ...);
    void log_write(int level, const char *message, ...);
    void log_long(const char *str, long len, const char *desc);
    void log_flush(void);
    void log_finish(void);
#ifdef ICONV
    char *silgy_convert(char *src, const char *cp_from, const char *cp_to);
#endif

    int Base64encode_len(int len);
    int Base64encode(char * coded_dst, const char *plain_src,int len_plain_src);
    int Base64decode_len(const char * coded_src);
    int Base64decode(char * plain_dst, const char *coded_src);

#ifdef _WIN32   /* Windows */
    int getpid(void);
    int clock_gettime(int dummy, struct timespec *spec);
    char *stpcpy(char *dest, const char *src);
    char *stpncpy(char *dest, const char *src, unsigned long len);
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
