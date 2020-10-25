# Silgy

Silgy is an extreme speed asynchronous web application backend engine. It can render pages in **microseconds**, even with a database, when used with efficient [DAO/ORM class](https://silgy.org/mysqldaogen) (see [live demo](https://silgy.org/products)).

It's open-source and free.

It can act as:

* a static web server;
* a standalone, self-contained, single-process web application;
* a standalone, multi-process web application, connected and load-balanced via standard POSIX queues;
* an HTTP servlet/microservice that makes up a building block of the more complex, distributed architecture.

## Why bother?

Back in 2015 I decided to write a web application and had a wishlist:

1. It has to be **responsive**, in a way compiled programs used locally are responsive. It needs to feel solid and reliable. My application consistently gets "Faster than 100% of tested sites" [badge from Pingdom](https://minishare.com/show?p=MWPcAbmY).

![Pingdom](https://minishare.com/show?p=MWPcAbmY&i=2 =400)

1. It needs to be **universal**. Silgy applications work on _every_ currently used browser.

1. Host it for **free** or at negligible cost. Websites that don't survive at least 10 years (or have a huge marketing budget) don't matter – it's not 1995 anymore. I wanted to make sure I'll be able to afford hosting for the rest of my life, or until users count would reach at least a million. I actually pay $3 per month for my ~1000 users' application and use ~1% of the server's resources.

1. **Security**. Again, it's not 1995 anymore. Put your server online, set *logLevel* to 4 and see what's coming in. In literally few days you'll be flooded by bots trying every known hole in PHP, Wordpress and whatever-was-ever-cool. Silgy inverted the philosophy the older servers were once built on: instead of putting up the wall between the gates, Silgy is a fortress with a gate only where you want it. My application consistently gets grade **A** from [SSL Labs](https://www.ssllabs.com/ssltest/analyze.html?d=silgy.org).

![SSL Labs](https://minishare.com/show?p=K8GvQDag&i=2 =1110)

1. **Independency**. AWS getting nasty? It'd take 60 minutes to move anywhere else, most of them spent on setting up an account at my new cloud provider. Then I only need to install GCC, MySQL, restore database from backup, clone my repo, `m`, Enter and `silgystart`, Enter.

1. **Simplicity**. Before writing my own web server, I tried a couple of libraries. They were either complicated, not intuitive, bloated or slow. Some look promising (Node.js), however database connections proved to be too slow for my needs. There's also a tendency for using every new fireworks that's available in town, like i.e. functional programming. I just don't feel comfortable with this, so I wanted **straight, intuitive, procedural, single-threaded code**. All the complexity needs to be hidden in the library.

1. **Beautiful URL**. I want `full/control/over/my/URIs`.

1. **Planet-friendliness**. Even if I could afford fat, 64-core, 512 GB RAM VS, why would I waste the Earth? It turns out, data centers have become a major pollutant: [Short BBC video on this](https://www.bbc.com/news/av/stories-51742336/dirty-streaming-the-internet-s-big-secret).

As my wishes had come true, I decided to open my code and become famous. You're welcome.

## IP2Location Demo

Here: https://github.com/silgy/ip2loc is the simple demo web application using Silgy to log visitors with their location in database. It uses [IP2Location™ LITE IP-COUNTRY](https://lite.ip2location.com/database/ip-country) free database. Live instance: http://silgy.org:2020

## Node++ announcement

I am starting working on Silgy's successor: [Node++](https://github.com/silgy/nodeplusplus). As I gathered a lot of experience from writing about 20 web applications myself and helping others, I feel I'm ready to take a bigger step to the new name and a few breaches that will, I believe, make Silgy applications better. No worries: I'll be supporting Silgy project for at least a year from the first Node++ stable release and will provide an automated conversion tool for existing applications, as I will need this myself.

I need to thank you all for all your questions, encouragement and kind words!

Please keep asking, suggesting and express your wishes: here in Issues or silgy.help@gmail.com.

\*\*\*

## What's in the box?

As I am writing this in May 2019, Silgy lib contains everything that is necessary to build a complete, production-grade solution, including [session management](https://github.com/silgy/silgy/wiki/Sessions-in-Silgy), [users accounts](https://github.com/silgy/silgy/wiki/USERS-Module), [REST calls](https://github.com/silgy/silgy/wiki/RESTful-calls-from-Silgy) and [multi-language support](https://github.com/silgy/silgy/wiki/Silgy-multi-language-support).

The only third-party dependencies are:

* USERS module requires MySQL library.

* HTTPS requires OpenSSL library.

Silgy Hello World handles [~20,000 requests per second](https://github.com/silgy/silgy/wiki/Performance-test:-select()-vs-poll()) on a single CPU, free AWS instance.

Silgy has build-in (and enabled by default) protection against most popular attacks, including BEAST, SQL-injection, XSS, and password and cookie brute-force. It does not directly expose the filesystem nor allows any scripting. Its random string generator is FIPS-compliant. CSRF protection is as easy as adding [3 lines to the code](https://github.com/silgy/silgy/wiki/CSRFT_OK).

TCO for a mid-sized web application with a small database and moderate load (<1,000,000 requests/day) can be as low as $3 per month (Amazon [EC2 t2.micro](https://aws.amazon.com/ec2/instance-types/t2/)).

In case of using Silgy under heavy load or with external API calls, there's the [ASYNC facility](https://github.com/silgy/silgy/wiki/Silgy-ASYNC) designed to prevent main (silgy_app) process blocking. ASYNC allows developer to split (or move) the functionality between gateway and multiple service processes.

## [Getting Started on Windows](https://github.com/silgy/silgy/wiki/Silgy-Hello-World-%E2%80%93-Getting-Started-on-Windows)

## [Getting Started on Linux](https://github.com/silgy/silgy/wiki/Silgy-Hello-World-%E2%80%93-Getting-Started-on-Linux)

## [Silgy functions and macros](https://github.com/silgy/silgy/wiki/Silgy-functions-and-macros)

## [Full documentation](https://github.com/silgy/silgy/wiki)

## [Generate RESTful API](https://silgy.org/restapigen) as Silgy project

## Hello Worlds

### Empty application = static web server

This is just an extremely fast static web server. If no resource is requested it'll look for `index.html` in [res](https://github.com/silgy/silgy#res) directory. If requested file is not present in `res` nor `resmin`, it'll return 404.

```source.c++
void silgy_app_main(int ci)
{
    RES_STATUS(404);
}
```

### Simplest Hello World

Return static file if present, otherwise "Hello World!".

```source.c++
void silgy_app_main(int ci)
{
    OUT("Hello World!");
}
```

### Simple HTML with 2 pages

Application, yet without moving parts.

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

### Using query string value

Finally some logic. [QS()](https://github.com/silgy/silgy/wiki/QS) will automatically choose between query string or payload, depending on the HTTP request method.

```source.c++
void silgy_app_main(int ci)
{
    if ( REQ("") )  // landing page
    {
        OUT_HTML_HEADER;

        OUT("<h1>%s</h1>", APP_WEBSITE);
        OUT("<h2>Welcome to my web app!</h2>");

        if ( REQ_DSK )
            OUT("<p>You're on desktop.</p>");
        else  /* REQ_MOB */
            OUT("<p>You're on the phone.</p>");

        OUT("<p>Click <a href=\"/welcome\">here</a> to try my welcoming bot.</p>");

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

## Real world application example

[Silgy Home Page source code](https://github.com/silgy/silgy.org). You can try it live at [silgy.org](https://silgy.org).

## Directories

Although not necessary, it's good to have $SILGYDIR set in the environment, pointing to the project directory. Silgy engine always first looks in `$SILGYDIR/<dir>` for the particular file, with `<dir>` being one of the below:

### `src`

*(Required only for development)*

* Application sources. It has to contain at least `silgy_app.h` and `silgy_app.cpp` with `silgy_app_main()` inside.
* Compilation script: `m`

### `lib`

*(Required only for development)*

* Silgy engine & library source
* `users.sql`

### `bin`

* Executables, i.e. `silgy_app`, `silgy_svc`, `silgy_watcher`
* Runtime scripts, i.e. `silgystart`, `silgystop`
* Configuration: `silgy.conf`
* Strings in additional languages: `strings.LL-CC`
* Blacklist, i.e. `blacklist.txt`
* Whitelist, i.e. `whitelist.txt`
* 10,000 most common passwords: `passwords.txt`

### `res`

Static resources. The whole tree under `res` is publicly available. All the files are read on startup and served straight from the memory. File list is updated once a minute.

* images
* static HTML files
* text files
* fonts
* ...

### `resmin`

Static resources to be minified. The whole tree under `resmin` is publicly available. All the files are read on startup, minified and served straight from the memory. File list is updated once a minute.

* CSS
* JS

### `snippets`

Static parts of rendered content, i.e. HTML snippets.

### `logs`

* Log files


## Feedback & Support

Write to silgy.help@gmail.com if you got stuck, need a feature or have a side gig for me. I have reasonable prices but need advance booking. Please report bugs in [Issues](https://github.com/silgy/silgy/issues).
