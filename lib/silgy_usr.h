/* --------------------------------------------------------------------------
   Silgy Web App Engine
   Jurek Muszynski
   silgy.com
-----------------------------------------------------------------------------
   Logged in users' functions
-------------------------------------------------------------------------- */

#ifndef SILGY_USR_H
#define SILGY_USR_H


#define DB_UAGENT_LEN                   250                     /* User-Agent length stored in ulogins table */
#define PASSWD_RESET_KEY_LEN            20                      /* password reset key length */

#ifdef APP_MIN_USERNAME_LEN                                     /* minimum user name length */
#define MIN_USERNAME_LEN                APP_MIN_USERNAME_LEN
#else
#define MIN_USERNAME_LEN                2
#endif

#ifdef APP_MIN_PASSWORD_LEN                                     /* minimum password length */
#define MIN_PASSWORD_LEN                APP_MIN_PASSWORD_LEN
#else
#define MIN_PASSWORD_LEN                5                       /* default minimal password length */
#endif

#ifndef MAX_ULA_BEFORE_FIRST_SLOW                               /* maximum unsuccessful login tries before slowing down to 1 per minute */
#define MAX_ULA_BEFORE_FIRST_SLOW       10
#endif

#ifndef MAX_ULA_BEFORE_SECOND_SLOW                              /* maximum unsuccessful login tries before slowing down to 1 per hour */
#define MAX_ULA_BEFORE_SECOND_SLOW      25
#endif

#ifndef MAX_ULA_BEFORE_THIRD_SLOW                               /* maximum unsuccessful login tries before slowing down to 1 per day */
#define MAX_ULA_BEFORE_THIRD_SLOW       100
#endif

#ifndef MAX_ULA_BEFORE_LOCK                                     /* maximum unsuccessful login tries before user lockout */
#define MAX_ULA_BEFORE_LOCK             1000
#endif


/* user status */

#define USER_STATUS_INACTIVE            0
#define USER_STATUS_ACTIVE              1
#define USER_STATUS_LOCKED              2
#define USER_STATUS_PASSWORD_CHANGE     3
#define USER_STATUS_DELETED             9


/* configurable parameters */

#ifndef DEF_USER_AUTH_LEVEL
#define DEF_USER_AUTH_LEVEL             AUTH_LEVEL_USER         /* default user authorization level */
#endif

#ifndef USER_ACTIVATION_HOURS
#define USER_ACTIVATION_HOURS           48                      /* activate user account within */
#endif

#ifndef USER_KEEP_LOGGED_DAYS
#define USER_KEEP_LOGGED_DAYS           30                      /* ls cookie validity period */
#endif


#define COMMON_PASSWORDS_FILE           "passwords.txt"


#ifndef REFUSE_10_COMMON_PASSWORDS
#ifndef REFUSE_100_COMMON_PASSWORDS
#ifndef REFUSE_1000_COMMON_PASSWORDS
#ifndef REFUSE_10000_COMMON_PASSWORDS
#ifndef DONT_REFUSE_COMMON_PASSWORDS
#define DONT_REFUSE_COMMON_PASSWORDS
#endif
#endif
#endif
#endif
#endif


/* Silgy engine errors are 0 ... 99 */

/* ------------------------------------- */
/* errors -- red */

/* login */
#define ERR_INVALID_LOGIN               101
#define ERR_USERNAME_TOO_SHORT          102
#define ERR_USERNAME_CHARS              103
#define ERR_USERNAME_TAKEN              104
/* ------------------------------------- */
#define ERR_MAX_USR_LOGIN_ERROR         110
/* ------------------------------------- */
/* email */
#define ERR_EMAIL_EMPTY                 111
#define ERR_EMAIL_FORMAT                112
#define ERR_EMAIL_FORMAT_OR_EMPTY       113
#define ERR_EMAIL_TAKEN                 114
/* ------------------------------------- */
#define ERR_MAX_USR_EMAIL_ERROR         120
/* ------------------------------------- */
/* password */
#define ERR_INVALID_PASSWORD            121
#define ERR_PASSWORD_TOO_SHORT          122
#define ERR_IN_10_COMMON_PASSWORDS      123
#define ERR_IN_100_COMMON_PASSWORDS     124
#define ERR_IN_1000_COMMON_PASSWORDS    125
#define ERR_IN_10000_COMMON_PASSWORDS   126
/* ------------------------------------- */
#define ERR_MAX_USR_PASSWORD_ERROR      130
/* ------------------------------------- */
/* repeat password */
#define ERR_PASSWORD_DIFFERENT          131
/* ------------------------------------- */
#define ERR_MAX_USR_REPEAT_PASSWORD_ERROR 140
/* ------------------------------------- */
/* old password */
#define ERR_OLD_PASSWORD                141
/* ------------------------------------- */
#define ERR_MAX_USR_OLD_PASSWORD_ERROR  150
/* ------------------------------------- */
/* session / link / other */
#define ERR_SESSION_EXPIRED             151
#define ERR_LINK_BROKEN                 152
#define ERR_LINK_MAY_BE_EXPIRED         153
#define ERR_LINK_EXPIRED                154
#define ERR_LINK_TOO_MANY_TRIES         155
#define ERR_ROBOT                       156
#define ERR_WEBSITE_FIRST_LETTER        157
#define ERR_NOT_ACTIVATED               158
/* ------------------------------------- */
#define ERR_MAX_USR_ERROR               199
/* ------------------------------------- */

/* ------------------------------------- */
/* warnings -- yellow */

#define WAR_NO_EMAIL                    201
#define WAR_BEFORE_DELETE               202
#define WAR_ULA_FIRST                   203
#define WAR_ULA_SECOND                  204
#define WAR_ULA_THIRD                   205
#define WAR_PASSWORD_CHANGE             206
/* ------------------------------------- */
#define WAR_MAX_USR_WARNING             299
/* ------------------------------------- */

/* ------------------------------------- */
/* messages -- green */

#define MSG_WELCOME_NO_ACTIVATION       301
#define MSG_WELCOME_NEED_ACTIVATION     302
#define MSG_WELCOME_AFTER_ACTIVATION    303
#define MSG_USER_LOGGED_OUT             304
#define MSG_CHANGES_SAVED               305
#define MSG_REQUEST_SENT                306
#define MSG_PASSWORD_CHANGED            307
#define MSG_MESSAGE_SENT                308
#define MSG_PROVIDE_FEEDBACK            309
#define MSG_FEEDBACK_SENT               310
#define MSG_USER_ALREADY_ACTIVATED      311
#define MSG_ACCOUNT_DELETED             312
/* ------------------------------------- */
#define MSG_MAX_USR_MESSAGE             399
/* ------------------------------------- */


#define MSG_CAT_USR_LOGIN               "msgLogin"
#define MSG_CAT_USR_EMAIL               "msgEmail"
#define MSG_CAT_USR_PASSWORD            "msgPassword"
#define MSG_CAT_USR_REPEAT_PASSWORD     "msgPasswordRepeat"
#define MSG_CAT_USR_OLD_PASSWORD        "msgPasswordOld"


#ifndef LUSES_TIMEOUT
#define LUSES_TIMEOUT                   1800                /* logged in user session timeout in seconds (120 for tests / 1800 live) */
#endif                                                      /* it's now how long it stays in cache */

/* user authentication */

#ifndef USERSBYEMAIL
#ifndef USERSBYLOGIN
#define USERSBYLOGIN
#endif
#endif

/* passwords' hashing padding */

#ifndef STR_001
#define STR_001                         "abcde"
#endif
#ifndef STR_002
#define STR_002                         "fghij"
#endif
#ifndef STR_003
#define STR_003                         "klmno"
#endif
#ifndef STR_004
#define STR_004                         "pqrst"
#endif
#ifndef STR_005
#define STR_005                         "uvwxy"
#endif


#define MAX_AVATAR_SIZE                 65536   /* MySQL's BLOB size */


#define SET_USER_STR(key, val)          silgy_usr_set_str(ci, key, val)
#define SET_USR_STR(key, val)           silgy_usr_set_str(ci, key, val)

#define GET_USER_STR(key, val)          silgy_usr_get_str(ci, key, val)
#define GET_USR_STR(key, val)           silgy_usr_get_str(ci, key, val)

#define SET_USER_INT(key, val)          silgy_usr_set_int(ci, key, val)
#define SET_USR_INT(key, val)           silgy_usr_set_int(ci, key, val)

#define GET_USER_INT(key, val)          silgy_usr_get_int(ci, key, val)
#define GET_USR_INT(key, val)           silgy_usr_get_int(ci, key, val)


/*
   Brute-force ls cookie attack protection.
   It essentially defines how many different IPs can take part in a botnet attack.
*/

#ifdef MEM_TINY
#define FAILED_LOGIN_CNT_SIZE           100
#elif defined MEM_MEDIUM
#define FAILED_LOGIN_CNT_SIZE           1000
#elif defined MEM_LARGE
#define FAILED_LOGIN_CNT_SIZE           10000
#elif defined MEM_XLARGE
#define FAILED_LOGIN_CNT_SIZE           10000
#elif defined MEM_XXLARGE
#define FAILED_LOGIN_CNT_SIZE           100000
#elif defined MEM_XXXLARGE
#define FAILED_LOGIN_CNT_SIZE           100000
#elif defined MEM_XXXXLARGE
#define FAILED_LOGIN_CNT_SIZE           100000
#else   /* MEM_SMALL -- default */
#define FAILED_LOGIN_CNT_SIZE           1000
#endif

typedef struct {
    char   ip[INET_ADDRSTRLEN];
    int    cnt;
    time_t when;
} failed_login_cnt_t;


#ifdef __cplusplus
extern "C" {
#endif
    int  silgy_usr_login(int ci);
    int  silgy_usr_password_quality(const char *passwd);
    int  silgy_usr_create_account(int ci);
    int  silgy_usr_add_user(int ci, bool use_qs, const char *login, const char *email, const char *name, const char *passwd, const char *phone, const char *lang, const char *about, char group_id, char auth_level, char status);
    int  silgy_usr_send_message(int ci);
    int  silgy_usr_save_account(int ci);
    int  silgy_usr_email_registered(int ci);
    char *silgy_usr_name(const char *login, const char *email, const char *name, int uid);
    int  silgy_usr_send_passwd_reset_email(int ci);
    int  silgy_usr_verify_passwd_reset_key(int ci, char *linkkey, int *uid);
    int  silgy_usr_activate(int ci);
    int  silgy_usr_save_avatar(int ci, int uid);
    int  silgy_usr_get_avatar(int ci, int uid);
    int  silgy_usr_change_password(int ci);
    int  silgy_usr_reset_password(int ci);
    void silgy_usr_logout(int ci);
    int  silgy_usr_set_str(int ci, const char *us_key, const char *us_val);
    int  silgy_usr_get_str(int ci, const char *us_key, char *us_val);
    int  silgy_usr_set_int(int ci, const char *us_key, int us_val);
    int  silgy_usr_get_int(int ci, const char *us_key, int *us_val);
    /* for the engine */
    void libusr_init(void);
    int  libusr_luses_ok(int ci);
    void libusr_luses_close_timeouted(void);
    void libusr_luses_save_csrft(void);
    void libusr_luses_downgrade(int usi, int ci, bool usr_logout);
#ifdef __cplusplus
}   // extern "C"
#endif


#endif  /* SILGY_USR_H */
