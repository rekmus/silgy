# Silgy
Silgy is a simple back-end framework for ultra fast, small and mid-sized web applications or microservices. It contains asynchronous (non-blocking) web engine that allows you to compile and link your logic into one executable that responds immediately to HTTP requests, without creating new thread or — God forbid — process. It's small enough to fit on free 1GB AWS t2.micro instance, together with MySQL server. Typical response time is around 100 µs (microseconds) (see https://budgeter.org performance for proof).  
  
Silgy supports https, anonymous and registered user sessions, binary data upload and rudimentary asynchronous services mechanism to use it in microservices architecture.  
  
Silgy requires Linux/UNIX computer with C or C++ compiler for development. Production machine requires only operating system and silgy_app executable file(s), and optionally database server if your application uses one.
## Quick Start Guide
I assume that you know how to log in to your Linux. If not, and you're on Windows, like me, I recommend installing [PuTTY](https://www.putty.org/) and [WinSCP](https://winscp.net/eng/index.php), then to spend a quarter on watching some introductory video (I'll try to find something good later or any suggestions are welcome).
### 1. In your $HOME, create a project directory, i.e. **web**:  
```
mkdir web
```
### 2. Set SILGYDIR environment variable to your project directory.
If you use bash, that would be in .bash_profile in your home directory:
```
export SILGYDIR=/home/ec2-user/web
```
Then you need to either restart your shell session or execute above command.  
  
### 3. Add SILGYDIR to sudoers:
```
sudo visudo
```
then find the block starting with:
```
Defaults    env_reset
```
and add a new line like this:
```
Defaults    env_keep += "SILGYDIR"
```
(If you don't know vi yet, here is a [cheat sheet](http://www.atmos.albany.edu/daes/atmclasses/atm350/vi_cheat_sheet.pdf).)  
  
### 4. In your project directory, create some others:
```
cd web
mkdir src      # all the sources from this repository's src
mkdir bin      # executable(s), silgy.conf, blacklist.txt will be there
mkdir res      # static resources like pictures, html-s, robots.txt etc.
mkdir resmin   # resources to be minified (CSS and JS)
mkdir logs     # peek there if there's something wrong
```
### 5. Throw all the files from src directory here to your src.
**m** and **te** scripts must have executable flag:
```
cd src
chmod u+x m
chmod u+x te
```
### 6. Compile:
```
m
```
or — if you don't have a . (dot) in your PATH — you may need to do:
```
./m
```
instead.  
  
### 7. Run:
```
sudo te
```
That's it. Your app should now be online.  
  
Your app logic is in [silgy_app.cpp](https://github.com/silgy/silgy/blob/master/src/silgy_app.cpp) and **app_process_req()** is your main, called with every browser request.  
  
## Static Resources
Static resources are simply the files you want to serve from disk, as opposed to dynamic content that is generated in your code. Statics usually include pictures, css, robots.txt etc.  
  
Static resources are read into memory on startup from **res** directory. Static resources you want to serve minified (CSS and JS), are read into memory and minified on startup from **resmin** directory.  
  
Static resources are handled automatically, you don't have to add anything in your app.  
  
## Response Header
Response header is generated automatically, however you can overwrite defaults with a couple of [macros](https://github.com/silgy/silgy/blob/master/README.md#void-res_statusint-code).  
  
## Hello World
Simplest Hello World:
```source.c++
int app_process_req(int ci)
{
    OUT("Hello World!");
    return OK;
}
```
  
Simple HTML with 2 pages:
```source.c++
int app_process_req(int ci)
{
    OUT("<!DOCTYPE html>");
    OUT("<head>");
    OUT("<title>%s</title>", APP_WEBSITE);
    OUT("</head>");
    OUT("<body>");
    OUT("<h1>%s</h1>", APP_WEBSITE);

    if ( REQ("") )  // landing page
    {
        OUT("<h2>Welcome to my web app!</h2>");
    }
    else if ( REQ("about") )
    {
        OUT("<h2>About</h2>");
        OUT("<p>Hello World Sample Silgy Web Application</p>");
        OUT("<p><a href=\"/\">Back to landing page</a></p>");
    }
    else  // page not found
    {
        ret = ERR_NOT_FOUND;
    }

    OUT("</body>");
    OUT("</html>");

    return OK;
}
```
  
And this is a tad extended Hello World example to demonstrate query string handling:
```source.c++
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
```
  
## Configuration File
By default silgy server starts listening on the port 80. As long as you use the same computer for development and production, you need a way to test your application with different port, since only one process can listen on the port 80. Also, if you want to use https, you will need to pass your certificate file path. You can set these and some more in a configuration file. Default name is silgy.conf and you can overwrite this with SILGY_CONF environment variable. Create a new file in $SILGYDIR/bin and paste the below:
```
# ----------------------------------------------------------------------------
# between 1...4 (most detailed)
logLevel=4

# ----------------------------------------------------------------------------
# ports
httpPort=80
httpsPort=443

# ----------------------------------------------------------------------------
# HTTPS

# mandatory
certFile=/etc/letsencrypt/live/example.com/fullchain.pem
keyFile=/etc/letsencrypt/live/example.com/privkey.pem

# optional
# below cipher list will support IE8 on Windows XP but SSLLabs would cap the grade to B
#cipherList=ECDH+AESGCM:DH+AESGCM:ECDH+AES256:DH+AES256:ECDH+AES128:DH+AES:RSA+AESGCM:RSA+AES:!aNULL:!MD5:!DSS+RC4:RC4
certChainFile=/etc/letsencrypt/live/example.com/chain.pem

# ----------------------------------------------------------------------------
# database connection details
dbName=mydatabase
dbUser=mysqluser
dbPassword=mysqlpassword

# ----------------------------------------------------------------------------
# IP blacklist
blockedIPList=/home/ec2-user/web/bin/blacklist.txt

# ----------------------------------------------------------------------------
# setting this to 1 will add _t to the log file name
# slightly different behaviour with https redirections
test=0

# ----------------------------------------------------------------------------
# Custom params
myParam1=someValue
myParam2=someOtherValue
```
Change the contents to your taste. Note that you can use config file to pass your own parameters.  
  
## Compilation Switches
Because speed is Silgy's priority, every possible decision is taken at compile time rather than at runtime. Therefore, unless you specify you want to use some features, they won't be in your executable.  
Add your switches to [dev.env](https://github.com/silgy/silgy/blob/master/src/dev.env) before compilation, i.e.:
```
WEB_CFLAGS="-D HTTPS -D DBMYSQL"
```
### HTTPS
Use HTTPS. Both ports will be open and listened to. You need to supply *certFile*, *keyFile* and optionally *certChainFile* (for example Let's Encrypt generates this separately).  
  
### DBMYSQL
Open MySQL connection at the start and close it at clean up. Use *dbName*, *dbUser* and *dbPassword*.  
  
### DBMYSQLRECONNECT
Open MySQL connection with auto-reconnect option. I recommend not to use this option at the beginning, until you're familiar with MySQL settings, for reconnect happens quietly and without you knowing, this may be impacting your app's performance. However, if you know what [wait_timeout](https://dev.mysql.com/doc/refman/5.7/en/server-system-variables.html#sysvar_wait_timeout) is and you know how often you may expect those quiet reconnects, by all means — use it.  
  
### USERS
Use users module. It provides an API for handling all registered users logic, including common things like i.e. password reset. You need to have DBMYSQL defined as well. (and some tables in the database, specs coming soon)  
  
### DOMAINONLY
Always redirect to APP_DOMAIN.  
  
### BLACKLISTAUTOUPDATE
Automatically add malicious IPs to file defined in *blockedIPList*.  
  
## API Reference
I am trying to document everything here, however the first three macros (REQ, OUT and QS) is enough to write simple web application in silgy.
### bool REQ(string)
Return TRUE if first part of request URI matches *string*. 'First part' means everything until **/** or **?**, for example:  
```
URI: /calc?first=2&second=3  will be catched by  REQ("calc")
URI: /customers/123          will be catched by  REQ("customers")
URI: /about.html             will be catched by  REQ("about.html")
```
Example:  
```source.c++
if ( REQ("calc") )
    process_calc(ci);
```
  
### void OUT(string[, ...])
Send *string* to a browser. Optionally it takes additional arguments, as per [printf function family specification](https://en.wikipedia.org/wiki/Printf_format_string).  
Examples:
```source.c++
OUT("<!DOCTYPE html>");
OUT("<p>There are %d records in the table.</p>", records);
```
  
### bool QS(param, variable)
Search query string for *param* and if found, URI-decode it, copy its value to *variable* and return TRUE. Otherwise return FALSE. For POST, PUT and DELETE methods it assumes query string is in payload.  
QSVAL is just a typedef for C-style string, long enough to hold the value, as QS makes the check.  
Example:  
```source.c++
QSVAL qs_firstname;
if ( QS("firstname", qs_firstname) )
    OUT("<p>Welcome %s!</p>", qs_firstname);
```
QS comes with four SQL- and XSS-injection security flavours:  
  
QS - default - behaviour depends on QS_DEF_ compilation switch (by default it's QS_DEF_HTML_ESCAPE).  
QS_HTML_ESCAPE - value is HTML-escaped  
QS_SQL_ESCAPE - value is SQL-escaped  
QS_DONT_ESCAPE - value is not escaped  
  
And the fifth one:  
  
QS_RAW - value is not URI-decoded  
  
### bool URI(string)
Return TRUE if URI matches *string*.  
Example:
```source.c++
if ( URI("temp/document.pdf") )
    send_pdf(ci);
```
  
### bool REQ_METHOD(string)
Return TRUE if request method matches *string*.  
Example:  
```source.c++
if ( REQ_METHOD("OPTIONS") )
    show_options(ci);
```
  
### char* REQ_URI
Request URI.
  
### bool REQ_GET
Return TRUE if request method is GET.
  
### bool REQ_POST
Return TRUE if request method is POST.
  
### bool REQ_PUT
Return TRUE if request method is PUT.
  
### bool REQ_DELETE
Return TRUE if request method is DELETE.
  
### bool REQ_DSK
Return TRUE if request user agent is desktop.
  
### bool REQ_MOB
Return TRUE if request user agent is mobile.  
Example:  
```source.c++
if ( REQ_MOB )
    OUT("<meta name=\"viewport\" content=\"width=device-width\">");
```
  
### char* REQ_LANG
User agent language code.  
  
### bool HOST(string)
Return TRUE if HTTP request *Host* header matches *string*. Case is ignored.  
Example:
```source.c++
if ( HOST("example.com") )
    process_example(ci);
```
  
### void RES_STATUS(int code)
Set response status to *code*.  
Example:
```source.c++
RES_STATUS(501);
```
  
### void RES_CONTENT_TYPE(string)
Set response content type to *string*.  
Example:
```source.c++
RES_CONTENT_TYPE("text/plain");
```
  
### void RES_LOCATION(string)
Redirect browser to *string*.  
Example:
```source.c++
RES_LOCATION("login");
```
  
### void RES_DONT_CACHE
Prevent response from being cached by browser.  
  
### void REDIRECT_TO_LANDING
Redirect browser to landing page.  
  
### void ALWAYS(string[, ...]), void ERR(string[, ...]), void WAR(string[, ...]), void INF(string[, ...]), void DBG(string[, ...])
Write *string* to log, depending on log level set in [conf file](https://github.com/silgy/silgy/blob/master/README.md#configuration-file). Optionally it takes additional arguments, as per [printf function family specification](https://en.wikipedia.org/wiki/Printf_format_string).
```
ALWAYS - regardless of log level  
ERR - only if log level >= 1, writes ERROR: before string  
WAR - only if log level >= 2, writes WARNING: before string  
INF - only if log level >= 3  
DBG - only if log level >= 4  
```
Examples:
```source.c++
ALWAYS("Server is starting");
DBG("in a while loop, i = %d", i);
```
Note: if log level is set to 4, every call flushes the buffer.  
  
### void CALL_ASYNC(const char \*service, const char \*data, int timeout)
Call *service*. *timeout* is in seconds. When the response arrives or timeout passes, app_async_done() will be called with the same *service*. If timeout is < 1 or > ASYNC_MAX_TIMEOUT (currently 1800 seconds), it is set to ASYNC_MAX_TIMEOUT.  
Example:
```source.c++
CALL_ASYNC("get_customer", cust_id, 10);
```
  
### void CALL_ASYNC_NR(const char \*service, const char \*data)
Call *service*. Response is not required.  
Example:
```source.c++
CALL_ASYNC_NR("set_counter", counter);
```
  
### bool S(string)
Return TRUE if service matches *string*.  
Example: see [app_async_done](https://github.com/silgy/silgy/blob/master/README.md#void-app_async_doneint-ci-const-char-service-const-char-data-bool-timeouted).  
  
### void app_async_done(int ci, const char \*service, const char \*data, bool timeouted)
Process anynchronous call response.  
Example:
```source.c++
void app_async_done(int ci, const char *service, const char *data, bool timeouted)
{
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
}
```
  
