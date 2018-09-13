/* --------------------------------------------------------------------------
   Silgy Web App
   Jurek Muszynski
   silgy.com
-----------------------------------------------------------------------------
   Hello World Sample Silgy Web Application
-------------------------------------------------------------------------- */

#include <silgy.h>

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

    if ( REQ("") )  // landing page
    {
        OUT_HTML_HEADER;
        OUT("<h1>%s</h1>", APP_WEBSITE);
        OUT("<h2>Welcome to my web app!</h2>");
        OUT("<p>Click <a href=\"welcome\">here</a> to try my welcoming bot.</p>");
        OUT_HTML_FOOTER;
    }
    else if ( REQ("welcome") )  // welcoming bot
    {
        OUT_HTML_HEADER;
        OUT("<h1>%s</h1>", APP_WEBSITE);

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

        OUT_HTML_FOOTER;
    }
    else  // page not found
    {
        ret = ERR_NOT_FOUND;  // this will return status 404 to the browser
    }

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

   Finish page rendering after CALL_ASYNC has returned service response

   Example:

    if ( S("getCustomer") )
    {
        if ( err_code == ERR_ASYNC_TIMEOUT )
        {
            ERR("getCustomer timeout-ed");
            OUT("<p>There was no response from getCustomer service</p>");
        }
        else if ( err_code != OK )
        {
            ERR("getCustomer failed with %d", err_code);
            OUT("<p>getCustomer service returned an error %d</p>", err_code);
        }
        else
        {
            OUT("<p>Customer data: %s</p>", data);
        }

        OUT_HTML_FOOTER;
    }

-------------------------------------------------------------------------- */
void app_async_done(int ci, const char *service, const char *data, int err_code)
{
}
#endif
