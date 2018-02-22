# silgy
Silgy is a back-end framework for ultra fast, small and mid-sized web applications. It contains asynchronous (non-blocking) web engine that allows you to compile and link your logic into one executable that responds immediately to requests, without creating new thread or — God forbid — process. It's small enough to fit on free 1GB AWS t2.micro instance, together with MySQL server. Typical response time is around 100 µs (microseconds) (see https://budgeter.org performance for proof).  
  
Silgy supports https, anonymous and registered user sessions, binary data upload and rudimentary asynchronous services mechanism to use it in microservices architecture.  
  
Silgy requires Linux/UNIX computer with C or C++ compiler for development. Production machine requires only operating system and silgy_app executable file(s), and optionally database server if your application uses one.  
  
## Quick Start Guide
  
1. Create a project directory, i.e. web:  
```
mkdir web  
```

2. Set SILGYDIR environment variable to your project directory. If you use bash, that would be in .bash_profile in your home directory:  
```
export SILGYDIR=/home/ec2-user/web  
```
  
Then you need to either restart your shell session or execute above command.  
  
3. In your project directory, create some others:  
```
cd web  
mkdir src  
mkdir bin  
mkdir res  
mkdir resmin  
mkdir logs  
```
  
4. Throw all the files to src. m, mobj and te must have executable flag. In src:  
```
chmod u+x m*  
chmod u+x te  
```
  
5. Go to src and compile everything:  
```
mobj  
```
Make your executable:  
```
m  
```
Run:  
```
sudo te  
```
  
That's it. Your app should now be online.  
  
Your app logic is in silgy_app.cpp and app_process_req() is your main, called with every browser request.  
  
This is a tad extended Hello World example to demonstrate query string handling:  
```
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
