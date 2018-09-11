# Silgy

In 1995, when I landed my first computer job, my PC had an Intel 286 processor and 1 MB of RAM. Disks had spinning plates, nobody heard of SSD. Our office had Novell file server. And whatever we'd do, programs responded **immediately**.

Fast forward to 2018 and my PC has Intel i5 processor and 8 GB of RAM. Everyone can download GCC for free and there's Stackoverflow.com. And guess what? Web applications doing the same things in my intranet are now **painfully slow**.

Not only this. When I compile and boot my Java or Node.js projects, I have to waste my health for making zillionth coffee that day or to waste my time for waiting, because my computer is useless for a loooong time.

That's why I've written Silgy. I think all the web applications in the world should be written in it. World would be much better off.

In Silgy you just compile and link your logic into one executable that responds immediately to HTTP requests, without creating a new thread or — God forbid — process. There's no VM layer (Hello, Java) nor interpreter, nor external modules' dependencies (Hello, Node.js frameworks). Compilation takes about a second (Hello, Java and Node.js again). You get non-blocking Node.js-like or better performance, with Java-like coding simplicity, C-like instant compilation and instant startup, on much cheaper hardware. By the time my Spring Boot application boots, I can have another RESTful service written and tested with Silgy.

What you get with Silgy:

- **Speed** − response measured in µ-seconds, compilation around one second, boot in a fraction of a second.
- **Safety** − nobody can ever see your application logic nor wander through your filesystem nor run scripts. It has build-in protection against most popular attacks.
- **Small memory footprint** − a couple of MB for demo app − can be easily reduced for embedded apps.
- **Simple coding** − straightforward approach, easy to understand even for a beginner programmer ([jump to Hello World](https://github.com/silgy/silgy#hello-world)).
- **All-In-One** − no need to install external modules; Silgy source already contains all the logic required to run the application, including JSON objects and RESTful calls.
- **Simple deployment / cloud vendor independency** − only one executable file (or files in gateway/services model) to move around.
- **Low TCO** − ~$3 per month for hosting small web application with MySQL server (AWS t2.micro), not even mentioning planet-friendliness.

Silgy is written in ANSI C in order to support as many platforms as possible and it's C++ compilers compatible. Sample [silgy_app.cpp](https://github.com/silgy/silgy/blob/master/src/silgy_app.cpp) source module can be C as well as C++ code. Typical application code will look almost the same as in any of the C family language: C++, Java or JavaScript. All that could be automated, is automated.

It aims to be All-In-One solution for writing typical web application — traditional HTML rendering model, SPA or mixed. It handles HTTPS, and anonymous and registered user sessions. Larger applications or those using potentially blocking resources may want to split logic into the set of services talking to the gateway via POSIX queues in an asynchronous manner, using Silgy's [ASYNC](https://github.com/silgy/silgy/wiki/Silgy-compilation-switches#async) facility. [CALL_ASYNC](https://github.com/silgy/silgy/wiki/Silgy-functions-and-macros#asynchronous-services) macros make it as simple as possible.

Web applications like [Budgeter](https://budgeter.org) or [minishare](https://minishare.com) based on Silgy, fit in free 1GB AWS t2.micro instance, together with MySQL server. Typical processing time (between reading HTTP request and writing response to a socket) on 1 CPU t2.micro is around 100 µs (microseconds). Even with the network latency [it still shows](https://tools.pingdom.com/#!/bu4p3i/https://budgeter.org).
  
<div align="center">
<img src="https://minishare.com/show?p=MWPcAbmY&i=2" width=418/>
</div>

## Simplicity
In silgy_app.cpp:
```source.c++
int app_process_req(int ci)
{
    OUT("Hello World!");
    return OK;
}
```
Compile with `m` script and run `silgy_app` binary (`silgy_app.exe` on Windows). That's it, your application is now listening on the port 80 :) (If you want different port, add it as a command line argument)

## Requirements
Silgy is being developed around the idea of using as much generic environment as possible. Therefore it requires only three things:

1. Computer with operating system (Linux / UNIX / Windows),
2. C/C++ compiler. I recommend GCC (which is known as MinGW on Windows, AFAIK it is also used by CodeBlocks).
3. Silgy [src](https://github.com/silgy/silgy/tree/master/src).

Fuss-free deployment and cloud vendor independency means that production machine requires only operating system and silgy_app executable file(s), and optionally database server if your application uses one.

## Priorities / tradeoffs
Every project on Earth has them. So you'd better know.

1. *Speed*. Usually whenever I get to choose between speed and code duplication, I choose speed.

2. *Speed*. Usually whenever I get to choose between speed and saving memory, I choose speed. So there are mostly statics, stack and arrays, only a few of mallocs. For the same reason static files are read only at the startup and served straight from the memory.

3. *User code simplicity*. Usually whenever I get to choose between versatility and simplicity, I choose simplicity. 99.999% of applications do not require 10 levels of nesting in JSON. If you do need this, there is selection of [libraries](http://json.org/) to choose from, or you're a beginner like every programmer once was, and you still need to sweat your way to simple and clean code.

4. *Independency*. I try to include everything I think a typical web application may need in Silgy engine. If there are external libraries, I try to use most ubiquitous and generic ones, like i.e. OpenSSL and link statically. Of course you may prefer to add any library you want and/or link dynamically, there's nothing in Silgy that prevents you from doing so.

5. *What does deployment mean?* If you've written your app in Silgy, it means copying executable file to production machine which has nothing but operating system installed. OK, add jpegs and css. Oh — wait a minute — you prefer to learn [how to develop on Kubernetes](https://kubernetes.io/blog/2018/05/01/developing-on-kubernetes/) first, because everyone talks so cool about it... Then I can't help you. I'm actually learning it but only because my organization handles tens or hundreds of thousands requests per second, we have money for servers, development teams, admin teams and my boss made me. If you're Google or Amazon then you definitely need to have something. There is also a [hundred or so](https://en.wikipedia.org/wiki/List_of_build_automation_software) of other build automation software. Good luck with choosing the right one. And good luck with paying for the infrastructure. One of my priorities was to make Silgy app not needing this at all.

## Step-by-Step on Windows
### [Getting Started on Windows](https://github.com/silgy/silgy/wiki/Silgy-Hello-World-%E2%80%94-Getting-Started-on-Windows)

## Step-by-Step on Linux
### [Getting Started on Linux](https://github.com/silgy/silgy/wiki/Silgy-Hello-World-%E2%80%94-Getting-Started-on-Linux)

## Where's my program?
Your app logic is in [silgy_app.cpp](https://github.com/silgy/silgy/blob/master/src/silgy_app.cpp) and [app_process_req()](https://github.com/silgy/silgy#int-app_process_reqint-ci) is your main, called with every browser request. After downloading Silgy, there's a third version of [Hello World](https://github.com/silgy/silgy#hello-world) there to help you get on.

## Static Resources
Static resources are simply any content that you rarely change and keep as ordinary disk files, as opposed to dynamic content that is generated in your code, as a unique response to user request. In this regard, Silgy is like any other web server (except it's extremely fast). Statics usually include pictures, css, robots.txt etc.

Static resources are read into memory on startup from **res** directory. Static resources you want to serve minified (CSS and JS), are read into memory and minified on startup from **resmin** directory. Then, every 60 seconds (provided no traffic, that is select() timeouted) both directories are scanned for changes.

Static resources are handled automatically, you don't have to add anything in your app.

In addition to placing your statics in res and resmin directories, you can generate text statics from within your code at the start, and add them to the statics using [silgy_add_to_static_res()](https://github.com/silgy/silgy/wiki/silgy_add_to_static_res).

## Response Header
Response header is generated automatically, however you can overwrite defaults with a couple of [macros](https://github.com/silgy/silgy/wiki/Silgy-functions-and-macros#response).

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
        ret = ERR_NOT_FOUND;
    }

    return OK;
}
```

And this is a tad extended Hello World example to demonstrate query string handling:
```source.c++
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
```

## Configuration File
You can change default behaviour with [configuration parameters](https://github.com/silgy/silgy/wiki/Silgy-configuration-parameters). Note that you can also use config file to pass your own parameters which you can read with [silgy_read_param()](https://github.com/silgy/silgy/wiki/silgy_read_param_str).

## Compilation Switches
Because speed is Silgy's priority, every possible decision is taken at a compile time rather than at runtime. Therefore, unless you specify you want to use some features, they won't be in your executable.

Add your switches to [m](https://github.com/silgy/silgy/blob/master/src/m) before compilation, i.e.:
```
g++ silgy_app.cpp silgy_eng.c silgy_lib.c \
-s -O3 \
-D MEM_SMALL -D HTTPS -D DBMYSQL ...
```
### [Compilation switches specification](https://github.com/silgy/silgy/wiki/Silgy-compilation-switches)

## Functions and macros
### [Full reference is now moving to Wiki](https://github.com/silgy/silgy/wiki/Silgy-functions-and-macros).

Below I'll leave just the most basic ones that are essential for building any web application in Silgy ([REQ](https://github.com/silgy/silgy/wiki/REQ), [OUT](https://github.com/silgy/silgy/wiki/OUT) and [QS](https://github.com/silgy/silgy/wiki/QS)).

### bool REQ(const char \*string)
Return TRUE if first part of request URI matches *string*. 'First part' means everything until **/** or **?**, for example:

URI|Catched by
----|----
`/calc?first=2&second=3`|REQ("calc")
`/customers/123`|REQ("customers")
`/about.html`|REQ("about.html")

Example:  
```source.c++
if ( REQ("calc") )
    process_calc(ci);
else if ( REQ("about") )
    gen_page_about(ci);
else if ( REQ("") )
    gen_page_main(ci);
else
    ret = ERR_NOT_FOUND;
```
### void OUT(const char \*string[, ...])
Send *string* to a browser. Optionally it takes additional arguments, as per [printf function family specification](https://en.wikipedia.org/wiki/Printf_format_string).

Examples:
```source.c++
OUT("<!DOCTYPE html>");
OUT("<p>There are %d records in the table.</p>", records);
```
### bool QS(const char \*param, QSVAL variable)
Search query string for *param* and if found, URI-decode it, copy its value to *variable* and return TRUE. Otherwise return FALSE. For POST, PUT and DELETE methods it assumes query string is in payload.

QSVAL is just a typedef for C-style string, long enough to hold the value, as QS makes the check.

Example:  
```source.c++
QSVAL qs_firstname;

if ( QS("firstname", qs_firstname) )
    OUT("<p>Welcome %s!</p>", qs_firstname);
```
QS comes in four SQL- and XSS-injection security flavours:  
  
QS - default - behaviour depends on [QS_DEF_](https://github.com/silgy/silgy#qs_def_html_escape-qs_def_sql_escape-qs_def_dont_escape) compilation switch (by default it's QS_DEF_HTML_ESCAPE).  
QS_HTML_ESCAPE - value is HTML-escaped  
QS_SQL_ESCAPE - value is SQL-escaped  
QS_DONT_ESCAPE - value is not escaped  
  
And the fifth one:  
  
QS_RAW - value is not URI-decoded  


## Engine callbacks

### void app_done()
Called once, during termination.

### bool app_init(int argc, char \*argv[])
Called once at the beginning, but after server init. Returning *true* means successful initialization. Good place to set authorization levels, generate statics, etc.

### void app_luses_init(int ci)
Called when logged in user session is created.

### int app_process_req(int ci)
This is the main entry point for Silgy web application logic. *ci* is a connection index, as there can be many connections served asynchronously at the same time. **Always pass ci down the calling stack** as this is required by most macros and functions. For examples, see [Hello World](https://github.com/silgy/silgy#hello-world).

### void app_uses_init(int ci)
Called when a new user session is created.

### void app_uses_reset(int usi)
Called when user session is closed.

### void app_async_done(int ci, const char \*service, const char \*data, int err_code)
[ASYNC](https://github.com/silgy/silgy/wiki/Silgy-compilation-switches#async) compilation switch is required.

Finish page rendering after CALL_ASYNC has returned service response.

Example:
```source.c++
void app_async_done(int ci, const char *service, const char *data, int err_code)
{
    if ( S("getCustomer") )
    {
        if ( err_code == ERR_ASYNC_TIMEOUT )
        {
            WAR("getCustomer timeout-ed");
            OUT("<p>There was no response from getCustomer service</p>");
        }
        else if ( err_code == OK )
        {
            OUT("<p>Customer data: %s</p>", data);
        }

        OUT_HTML_FOOTER;
    }
}
```
  
