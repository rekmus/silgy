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
#define APP_MIN_USER_NAME_LEN       2                           /* minimum user name length */
#define APP_MIN_PASSWD_LEN          5                           /* minimum password length */
#define APP_ADMIN_EMAIL             "admin@example.com"
#define APP_CONTACT_EMAIL           "contact@example.com"

/* for ASYNC */

#define APP_ASYNC_ID                1

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



#endif  /* SILGY_APP_H */
