/* --------------------------------------------------------------------------
   Silgy Web App
   Jurek Muszynski
   silgy.com
-----------------------------------------------------------------------------
   Hello World Sample Silgy Web Application
-------------------------------------------------------------------------- */

#ifndef SILGY_APP_H
#define SILGY_APP_H

#define APP_WEBSITE                 "Silgy Hello World"
#define APP_DOMAIN                  "example.com"
#define APP_DESCRIPTION             "Hello World Sample Silgy Web Application"
#define APP_VERSION                 "1.0"

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
