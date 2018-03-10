/* --------------------------------------------------------------------------
   Silgy Web App
   Jurek Muszynski
   silgy.com
-----------------------------------------------------------------------------
   Hello World Sample Silgy Web Application
-------------------------------------------------------------------------- */

#ifndef SILGY_APP_H
#define SILGY_APP_H

#define APP_WEBSITE                 "Silgy Hello World"         /* website name */
#define APP_DOMAIN                  "example.com"               /* website domain */
#define APP_DESCRIPTION             "Hello World Sample Silgy Web Application"
#define APP_KEYWORDS                "hello world"
#define APP_VERSION                 "1.0"
#define APP_IP                      "1.1.1.1"
#define APP_COPYRIGHT               "Author"
#define APP_LOGIN_URI               "login"                     /* redirect here if login required */
#define APP_DEF_AUTH_LEVEL          AUTH_LEVEL_ANONYMOUS        /* default authorization level */
#define APP_ADMIN_EMAIL             "admin@example.com"
#define APP_CONTACT_EMAIL           "contact@example.com"

/* for USERS module passwords' hashing */

#define STR_001                     "abcde"
#define STR_002                     "fghij"
#define STR_003                     "klmno"
#define STR_004                     "pqrst"
#define STR_005                     "uvwxy"


/* App error codes start from 1000 */

#define ERR_EXAMPLE_ERROR           1000


/* app user session */

typedef struct {
    int id;
} ausession_t;

extern ausession_t  auses[MAX_SESSIONS+1];          /* app user sessions */


/* engine callbacks */

#ifdef __cplusplus
extern "C" {
#endif
    bool app_init(int argc, char *argv[]);
    void app_done(void);
    void app_set_param(const char *label, const char *value);
    int app_process_req(int ci);
    void app_uses_init(int ci);
#ifdef USERS
    void app_luses_init(int ci);
#endif
    void app_uses_reset(int usi);
#ifdef ASYNC
    void app_async_done(int ci, const char *service, const char *data, bool timeouted);
#endif
    bool app_gen_page_msg(int ci, int msg);
    void app_get_msg_str(int ci, char *dest, int errcode);
#ifdef EVERY_SECOND
    void app_every_second(void);
#endif
#ifdef __cplusplus
}   // extern "C"
#endif


#endif  /* SILGY_APP_H */
