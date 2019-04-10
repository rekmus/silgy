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


/* app user session */

typedef struct {
    int id;
    // add your own struct members here
} ausession_t;


#endif  /* SILGY_APP_H */
