/* --------------------------------------------------------------------------
   Silgy Web App Engine
   Jurek Muszynski
   silgy.com
-----------------------------------------------------------------------------
   Logged in users' functions
-------------------------------------------------------------------------- */

#ifndef SILGY_USR_H
#define SILGY_USR_H

#define LUSES_TIMEOUT               1800            /* logged in user session timeout in seconds (120 for tests / 1800 live) */
                                                    /* it's now how long it stays in cache */

#define DB_UAGENT_LEN               120             /* User-Agent length stored in ulogins table */
#ifdef APP_MIN_USERNAME_LEN                         /* minimum user name length */
#define MIN_USERNAME_LEN            APP_MIN_USERNAME_LEN               
#else
#define MIN_USERNAME_LEN            2               
#endif
#ifdef APP_MIN_PASSWORD_LEN                         /* minimum password length */
#define MIN_PASSWD_LEN              APP_MIN_PASSWORD_LEN
#else
#define MIN_PASSWD_LEN              5               /* default minimal password length */
#endif
#define PASSWD_RESET_KEY_LEN        30              /* password reset key length */

/* errors -- red */

/* login */
#define ERR_INVALID_LOGIN           10
#define ERR_USERNAME_TOO_SHORT      11
#define ERR_USERNAME_CHARS          12
#define ERR_USERNAME_TAKEN          13
/* email */
#define ERR_EMAIL_EMPTY             20
#define ERR_EMAIL_FORMAT            21
#define ERR_EMAIL_FORMAT_OR_EMPTY   22
#define ERR_EMAIL_TAKEN             23
/* password */
#define ERR_INVALID_PASSWD          30
#define ERR_PASSWORD_TOO_SHORT      31
/* repeat password */
#define ERR_PASSWORD_DIFFERENT      40
/* old password */
#define ERR_OLD_PASSWORD            50
/* session / link / other */
#define ERR_SESSION_EXPIRED         60
#define ERR_LINK_BROKEN             61
#define ERR_LINK_MAY_BE_EXPIRED     62
#define ERR_LINK_EXPIRED            63
#define ERR_ROBOT                   64
#define ERR_WEBSITE_FIRST_LETTER    65
/* internal errors */

/* warnings -- yellow */

#define WAR_NO_EMAIL                101
#define WAR_BEFORE_DELETE           102
#define WAR_ULA                     103

/* messages -- green */

#define MSG_WELCOME                 202
#define MSG_USER_LOGGED_OUT         203
#define MSG_CHANGES_SAVED           204
#define MSG_REQUEST_SENT            205
#define MSG_PASSWORD_CHANGED        206
#define MSG_MESSAGE_SENT            207
#define MSG_ACCOUNT_DELETED         208

#define LUSES_TIMEOUT               1800            /* logged in user session timeout in seconds (120 for tests / 1800 live) */
                                                    /* it's now how long it stays in cache */

/* user authentication */

#ifndef USERSBYEMAIL
#ifndef USERSBYLOGIN
#define USERSBYLOGIN
#endif
#endif

/* passwords' hashing */

#ifndef STR_001
#define STR_001                     "abcde"
#endif
#ifndef STR_002
#define STR_002                     "fghij"
#endif
#ifndef STR_003
#define STR_003                     "klmno"
#endif
#ifndef STR_004
#define STR_004                     "pqrst"
#endif
#ifndef STR_005
#define STR_005                     "uvwxy"
#endif


#define LOGGED                      US.logged
#define ADMIN                       (LOGGED && 0==strcmp(US.login, "admin"))


#define SET_USER_STR(key, val)      silgy_usr_set_str(ci, key, val)
#define GET_USER_STR(key, val)      silgy_usr_get_str(ci, key, val)
#define SET_USER_INT(key, val)      silgy_usr_set_int(ci, key, val)
#define GET_USER_INT(key, val)      silgy_usr_get_int(ci, key, val)


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
