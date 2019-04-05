# Silgy

Simple web application framework for C and C++ programmers.

The latency between receiving HTTP request and application logic has been minimized by linking web engine and application into a single executable that becomes a very fast, single-threaded asynchronous web server with custom logic.

Silgy contains everything that is necessary to build a complete, production-grade solution, including session management and remote REST calls facility.

The only third-party dependencies are:

* USERS module requires MySQL library.

* HTTPS requires OpenSSL library.

Besides, any modern C / C++ compiler will do.

ASYNC module requires POSIX message queues, so it's not available on Windows for now.

### [Getting Started on Windows](https://github.com/silgy/silgy/wiki/Silgy-Hello-World-%E2%80%94-Getting-Started-on-Windows)

### [Getting Started on Linux](https://github.com/silgy/silgy/wiki/Silgy-Hello-World-%E2%80%94-Getting-Started-on-Linux)

## Hello World
Simplest Hello World:
```source.c++
void silgy_app_main(int ci)
{
    OUT("Hello World!");
}
```

Simple HTML with 2 pages:
```source.c++
void silgy_app_main(int ci)
{
    if ( REQ("") )  // landing page
    {
        OUT_HTML_HEADER;
        OUT("<h1>%s</h1>", APP_WEBSITE);
        OUT("<h2>Welcome to my web app!</h2>");
        OUT_HTML_FOOTER;
    }
    else if ( REQ("about") )
    {
        OUT_HTML_HEADER;
        OUT("<h1>%s</h1>", APP_WEBSITE);
        OUT("<h2>About</h2>");
        OUT("<p>Hello World Sample Silgy Web Application</p>");
        OUT("<p><a href=\"/\">Back to landing page</a></p>");
        OUT_HTML_FOOTER;
    }
    else  // page not found
    {
        RES_STATUS(404);
    }
}
```

And this is a tad extended Hello World example to demonstrate query string handling:
```source.c++
void silgy_app_main(int ci)
{
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
        RES_STATUS(404);
    }
}
```

## Functions and macros
### [Full reference](https://github.com/silgy/silgy/wiki/Silgy-functions-and-macros).

## Feedback & Support
If you're not sure how to start, or you're not sure whether Silgy will be able to meet your needs, there's something wrong, you think there should be some feature or you just got stuck with your project, please [email me](mailto:silgy.help@gmail.com). I am currently developing about a dozen apps in Silgy, from simple Angular app hosting, through traditional web app, to RESTful API and every project adds some ideas how to make Silgy better, so I will gladly help.
