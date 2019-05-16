# Silgy

Simple and fast web application backend framework for C and C++ programmers.

The latency between receiving HTTP request and application logic has been reduced to single microseconds by linking web engine and application into a single executable that becomes a very fast, single-threaded asynchronous web server with custom logic.

## Why bother?

Back in 2015 I decided to write a web application and had a wishlist:

1. Host it for **free** or at negligible cost. Websites that don't survive at least 10 years (or have a huge marketing budget) don't matter – it's not 2000 anymore. I wanted to make sure I'll be able to afford hosting for the rest of my life, or until users count would reach at least a million. I actually pay $3 per month for my ~500 users application and use ~1% of the server's resources.

2. It has to be **responsive**, in a way compiled programs used locally are responsive. I hate slow programs. My application consistently gets "Faster than 100% of tested sites" [badge from Pingdom](https://minishare.com/show?p=MWPcAbmY).

3. **Independency**. AWS getting nasty? It'd take 60 minutes to move anywhere else, most of them spent on setting up an account at my new cloud provider. Then I only need to install GCC, MySQL, restore database from backup, clone my repo and type `silgystart`, Enter.

As my wishes had come true, I decided to open my code and become famous. You're welcome.

## What's in the box?

Silgy lib contains everything that is necessary to build a complete, production-grade solution, including [session management](https://github.com/silgy/silgy/wiki/Sessions-in-Silgy) and [REST calls](https://github.com/silgy/silgy/wiki/RESTful-calls-from-Silgy).

The only third-party dependencies are:

* USERS module requires MySQL library.

* HTTPS requires OpenSSL library.

Silgy Hello World handles [~20,000 requests per second](https://github.com/silgy/silgy/wiki/Performance-test:-select()-vs-poll()) on a single CPU, free AWS instance.

Silgy has build-in (and enabled by default) protection against most popular attacks, including BEAST, SQL-injection, XSS, and password and cookie brute-force. It does not directly expose the filesystem nor allows any scripting. Its random string generator is FIPS-compliant.

TCO for a mid-sized web application with a small database and moderate load (<1,000,000 requests/day) can be as low as $3 per month (Amazon [EC2 t2.micro](https://aws.amazon.com/ec2/instance-types/t2/)).

In case of using Silgy under heavy load or with external API calls, there's the ASYNC facility designed to prevent main (silgy_app) process blocking. ASYNC allows developer to split (or move) the functionality between gateway and multiple service processes.

## [Getting Started on Windows](https://github.com/silgy/silgy/wiki/Silgy-Hello-World-%E2%80%93-Getting-Started-on-Windows)

## [Getting Started on Linux](https://github.com/silgy/silgy/wiki/Silgy-Hello-World-%E2%80%93-Getting-Started-on-Linux)

## [Silgy functions and macros](https://github.com/silgy/silgy/wiki/Silgy-functions-and-macros)

## [Full documentation](https://github.com/silgy/silgy/wiki)

## Hello World

I believe in practical approach. In this case it means the simplest way to render HTML on the server side. Of course, nothing prevents you from using Silgy for SPA.

The simplest Hello World:

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

        OUT("<p>Please enter your name:</p>");
        OUT("<form action=\"welcome\"><input name=\"firstname\" autofocus> <input type=\"submit\" value=\"Run\"></form>");

        QSVAL qs_firstname;   // query string value

        if ( QS("firstname", qs_firstname) )    // if present, bid welcome
            OUT("<p>Welcome %s, my dear friend!</p>", qs_firstname);

        OUT("<p><a href=\"/\">Back to landing page</a></p>");

        OUT_HTML_FOOTER;
    }
    else  // page not found
    {
        RES_STATUS(404);
    }
}
```

## Directories

Although not necessary, it's good to have $SILGYDIR set in the environment, pointing to the project directory. Silgy engine always first looks in `$SILGYDIR/blah` for the particular file, with `blah` being one of the below:

### `src`

*(Required only for development)*

* application sources. It has to contain at least `silgy_app.h` and `silgy_app.cpp` with `silgy_app_main()` inside.
* compilation scripts, i.e. `m` (compiles silgy_app), `ms` (compiles silgy_svc), `mw` (compiles silgy_watcher)

### `lib`

*(Required only for development)*

* Silgy engine & library source
* `users.sql`

### `bin`

* executables, i.e. `silgy_app`, `silgy_svc`, `silgy_watcher`
* runtime scripts, i.e. `silgystart`, `silgystop`
* configuration: `silgy.conf`

### `res`

Static resources. The whole tree under `res` is publicly available.

* images
* static HTML files
* text files
* fonts
* ...

### `resmin`

Static resources to be minified. The whole tree under `resmin` is publicly available.

* CSS
* JS

### `logs`

* log files


## Feedback & Support

Write to silgy.help@gmail.com if you got stuck.
