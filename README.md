# Silgy

Simple web application framework for C and C++ programmers.

The latency between receiving HTTP request and application logic has been minimized by linking web engine and application into a single executable that becomes a very fast, single-threaded asynchronous web server with custom logic.

Silgy Hello World handles ~20,000 requests per second on a single CPU.

Silgy contains everything that is necessary to build a complete, production-grade solution, including session management and REST calls.

The only third-party dependencies are:

* USERS module requires MySQL library.

* HTTPS requires OpenSSL library.

Besides, any modern C / C++ compiler will do.

ASYNC module requires POSIX message queues, so it's not available on Windows for now.

Silgy has build-in (and enabled by default) protection against most popular attacks, including BEAST, SQL-injection, XSS, and password and cookie brute-force. It does not directly expose the filesystem nor allows any scripting. Its random string generator is FIPS-compliant.

TCO for a mid-sized web application with a small database and moderate load (<1,000,000 requests/day) can be as low as $3 per month (Amazon EC2 t2.micro).

Silgy Hello World's [quick performance test](https://github.com/silgy/silgy/wiki/Performance-test:-select()-vs-poll()) on the free-tier AWS EC2 instance.

## [Getting Started on Windows](https://github.com/silgy/silgy/wiki/Silgy-Hello-World-%E2%80%93-Getting-Started-on-Windows)

## [Getting Started on Linux](https://github.com/silgy/silgy/wiki/Silgy-Hello-World-%E2%80%93-Getting-Started-on-Linux)

## [Silgy functions and macros](https://github.com/silgy/silgy/wiki/Silgy-functions-and-macros)

## [Full documentation](https://github.com/silgy/silgy/wiki)

## [Application upgrade from Silgy 3.x to 4.0](https://github.com/silgy/silgy/wiki/Application-upgrade-from-Silgy-3.x-to-4.0)

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
            INF("query string arrived with firstname %s", qs_firstname);  // this will write to the log file
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

## Feedback & Support

Write to silgy.help@gmail.com if you got stuck.
