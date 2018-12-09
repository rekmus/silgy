/* --------------------------------------------------------------------------
   Silgy Web App Engine
   Jurek Muszynski
   silgy.com
-----------------------------------------------------------------------------
   Logged in users' functions
-------------------------------------------------------------------------- */

#ifndef SILGY_USR_H
#define SILGY_USR_H


#define DB_UAGENT_LEN                   120                     /* User-Agent length stored in ulogins table */
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
#define PASSWD_RESET_KEY_LEN            30                      /* password reset key length */


/* user status */

#define USER_STATUS_INACTIVE            0
#define USER_STATUS_ACTIVE              1

#ifdef APP_USER_ACTIVATION_HOURS                                /* activate user account within */
#define USER_ACTIVATION_HOURS           APP_USER_ACTIVATION_HOURS
#else
#define USER_ACTIVATION_HOURS           24
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
#define WAR_ULA                         203
/* ------------------------------------- */
#define WAR_MAX_USR_WARNING             299
/* ------------------------------------- */

/* ------------------------------------- */
/* messages -- green */

#define MSG_WELCOME                     301
#define MSG_WELCOME_AFTER_ACTIVATION    302
#define MSG_USER_LOGGED_OUT             303
#define MSG_CHANGES_SAVED               304
#define MSG_REQUEST_SENT                305
#define MSG_PASSWORD_CHANGED            306
#define MSG_MESSAGE_SENT                307
#define MSG_PROVIDE_FEEDBACK            308
#define MSG_FEEDBACK_SENT               309
#define MSG_USER_ALREADY_ACTIVATED      310
#define MSG_ACCOUNT_DELETED             311
/* ------------------------------------- */
#define MSG_MAX_USR_MESSAGE             399
/* ------------------------------------- */


#define MSG_CAT_USR_LOGIN               "msgLogin"
#define MSG_CAT_USR_EMAIL               "msgEmail"
#define MSG_CAT_USR_PASSWORD            "msgPassword"
#define MSG_CAT_USR_REPEAT_PASSWORD     "msgPasswordRepeat"
#define MSG_CAT_USR_OLD_PASSWORD        "msgPasswordOld"


#define LUSES_TIMEOUT                   1800                /* logged in user session timeout in seconds (120 for tests / 1800 live) */
                                                            /* it's now how long it stays in cache */

/* user authentication */

#ifndef USERSBYEMAIL
#ifndef USERSBYLOGIN
#define USERSBYLOGIN
#endif
#endif

/* passwords' hashing */

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


#define LOGGED                          US.logged
#define ADMIN                           (LOGGED && 0==strcmp(US.login, "admin"))


#define SET_USER_STR(key, val)          silgy_usr_set_str(ci, key, val)
#define GET_USER_STR(key, val)          silgy_usr_get_str(ci, key, val)
#define SET_USER_INT(key, val)          silgy_usr_set_int(ci, key, val)
#define GET_USER_INT(key, val)          silgy_usr_get_int(ci, key, val)


#ifdef __cplusplus
extern "C" {
#endif
    int silgy_usr_login(int ci);
    int silgy_usr_create_account(int ci);
    int silgy_usr_send_message(int ci);
    int silgy_usr_save_account(int ci);
    int silgy_usr_email_registered(int ci);
    int silgy_usr_send_passwd_reset_email(int ci);
    int silgy_usr_verify_passwd_reset_key(int ci, char *linkkey, long *uid);
    int silgy_usr_activate(int ci);
    int silgy_usr_reset_password(int ci);
    void silgy_usr_logout(int ci);
    int silgy_usr_set_str(int ci, const char *us_key, const char *us_val);
    int silgy_usr_get_str(int ci, const char *us_key, char *us_val);
    int silgy_usr_set_int(int ci, const char *us_key, long us_val);
    int silgy_usr_get_int(int ci, const char *us_key, long *us_val);
    /* for the engine */
    int libusr_l_usession_ok(int ci);
    void libusr_close_luses_timeout(void);
    void libusr_close_l_uses(int ci, int usi);
    void libusr_get_msg_str(int ci, char *dest, int errcode);
#ifdef __cplusplus
}   // extern "C"
#endif


#endif  /* SILGY_USR_H */
