/* --------------------------------------------------------------------------
   Silgy Web App
   Jurek Muszynski
   Started: August 2015
-----------------------------------------------------------------------------
   Sample service module
-------------------------------------------------------------------------- */


#include "silgy.h"


/* --------------------------------------------------------------------------
   Entry point
-------------------------------------------------------------------------- */
void service_app_process_req(const char *service, const char *req, char *res)
{
	if ( S("hello") )
	{
		strcpy(res, "Hello from Silgy service!");
	}
	else if ( S("upper") )
	{
		strcpy(res, upper(req));
	}
	else if ( S("blocking") )
	{
        sleep(5);
		strcpy(res, "I was just sleeping for 5 seconds");
	}
}


/* --------------------------------------------------------------------------
   Server start
   Return TRUE if successful
-------------------------------------------------------------------------- */
bool service_init()
{
	return TRUE;
}


/* --------------------------------------------------------------------------
   Server stop
-------------------------------------------------------------------------- */
void service_done()
{
}
