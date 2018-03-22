/* --------------------------------------------------------------------------
   Silgy Web App
   Jurek Muszynski
   silgy.com
-----------------------------------------------------------------------------
   Hello World Sample Silgy Web Application
-------------------------------------------------------------------------- */

#include "silgy.h"

/* --------------------------------------------------------------------------
   Main entry point for a single request

   Returns OK or internal error code.

   The engine will set response status code & content type automatically.

   return                   status code
   ------                   -----------
   OK                       200
   ERR_INVALID_REQUEST      400
   ERR_UNAUTHORIZED         401
   ERR_FORBIDDEN            403
   ERR_NOT_FOUND            404
   ERR_INT_SERVER_ERROR     500
   ERR_SERVER_TOOBUSY       503

   If you want to overwrite that, use:

   o RES_STATUS to set response status code, i.e. RES_STATUS(501)

   o RES_CONTENT_TYPE, i.e. RES_CONTENT_TYPE("text/plain")

   If you want to redirect (with status 303), use RES_LOCATION(<new url>)

   Most macros use ci (connection index) so always pass it down the calling stack.
-------------------------------------------------------------------------- */
int app_process_req(int ci)
{
    int ret=OK;

    OUT("<!DOCTYPE html>");
    OUT("<head>");
    OUT("<title>%s</title>", APP_WEBSITE);
    if ( REQ_MOB )  // if mobile request
        OUT("<meta name=\"viewport\" content=\"width=device-width\">");
    OUT("</head>");

    OUT("<body>");
    OUT("<h1>%s</h1>", APP_WEBSITE);

    if ( REQ("") )  // landing page
    {
        OUT("<h2>Welcome to my web app!</h2>");
        OUT("<p>Click <a href=\"welcome\">here</a> to try my welcoming bot.</p>");
    }
    else if ( REQ("welcome") )  // welcoming bot
    {
        // show form

        OUT("<p>Please enter your name:</p>");
        OUT("<form action=\"welcome\"><input name=\"firstname\" autofocus> <input type=\"submit\" value=\"Run\"></form>");

        QSVAL qs_firstname;  // query string value

        // bid welcome

        if ( QS("firstname", qs_firstname) )  // firstname present in query string, copy it to qs_firstname
        {
            DBG("query string arrived with firstname %s", qs_firstname);  // this will write to the log file
            OUT("<p>Welcome %s, my dear friend!</p>", qs_firstname);
        }

        // show link to main page

        OUT("<p><a href=\"/\">Back to landing page</a></p>");
    }
    else  // page not found
    {
        ret = ERR_NOT_FOUND;  // this will return status 404 to the browser
    }

    OUT("</body>");
    OUT("</html>");

    return ret;
}









/* ================================================================================================ */
/* ENGINE CALLBACKS                                                                                 */
/* ================================================================================================ */

/* --------------------------------------------------------------------------
   App custom init
   Return TRUE if successful
-------------------------------------------------------------------------- */
bool app_init(int argc, char *argv[])
{
    return TRUE;
}


/* --------------------------------------------------------------------------
   App clean-up
-------------------------------------------------------------------------- */
void app_done()
{
}


/* --------------------------------------------------------------------------
   Called when starting new anonymous user session
-------------------------------------------------------------------------- */
void app_uses_init(int ci)
{
}


#ifdef USERS
/* --------------------------------------------------------------------------
   Called when starting new logged in user session
-------------------------------------------------------------------------- */
void app_luses_init(int ci)
{
}
#endif


/* --------------------------------------------------------------------------
   Called when closing user session
-------------------------------------------------------------------------- */
void app_uses_reset(int usi)
{
}


/* --------------------------------------------------------------------------
   Custom message page can be generated here
   if return TRUE it means custom page has been generated
   otherwise generic page will be displayed by the engine
-------------------------------------------------------------------------- */
bool app_gen_page_msg(int ci, int msg)
{
    return FALSE;   /* use engine generic page */
}


/* --------------------------------------------------------------------------
   Get error description for user
-------------------------------------------------------------------------- */
void app_get_msg_str(int ci, char *dest, int errcode)
{
}


#ifdef ASYNC
/* --------------------------------------------------------------------------

   Process asynchronous service call response

   Example:

    if ( S("get_customer") )
    {
        gen_header(ci);
        if ( timeouted )
        {
            WAR("get_customer timeout-ed");
            OUT("There was no response from get_customer service");
        }
        else
            OUT(data);
        gen_footer(ci);
    }
    else if ( S("get_records") )
    {
        if ( timeouted )
        {
            WAR("get_records timeout-ed");
            OUT("-|get_records timeout-ed|\n");
        }
        else
            OUT(data);
    }

-------------------------------------------------------------------------- */
void app_async_done(int ci, const char *service, const char *data, bool timeouted)
{
}
#endif


#ifdef EVERY_SECOND
/* --------------------------------------------------------------------------
   Called every second
-------------------------------------------------------------------------- */
void app_every_second()
{
}
#endif
