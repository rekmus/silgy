/* --------------------------------------------------------------------------
   Silgy Web App Engine
   Jurek Muszynski
   silgy.com
-----------------------------------------------------------------------------
   Logged in users' functions
-------------------------------------------------------------------------- */


#include <silgy.h>


#ifdef USERS


int G_new_user_id=0;


#ifdef REFUSE_10_COMMON_PASSWORDS
    #define MAX_COMMON  10
#elif defined REFUSE_100_COMMON_PASSWORDS
    #define MAX_COMMON  100
#elif defined REFUSE_1000_COMMON_PASSWORDS
    #define MAX_COMMON  1000
#elif defined REFUSE_10000_COMMON_PASSWORDS
    #define MAX_COMMON  10000
#else   /* DONT_REFUSE_COMMON_PASSWORDS */
    #define MAX_COMMON  1
#endif

char M_common[MAX_COMMON][16]={0};
int  M_common_cnt=0;


static bool valid_username(const char *login);
static bool valid_email(const char *email);
static int  upgrade_uses(int ci, usession_t *us);
static int  user_exists(const char *login);
static int  email_exists(const char *email);
static int  do_login(int ci, usession_t *us, char status, int visits);
static void get_hashes(char *result1, char *result2, const char *login, const char *email, const char *passwd);
static void doit(char *result1, char *result2, const char *usr, const char *email, const char *src);
static int  get_max(int ci, const char *table);
static bool load_common_passwd(void);


/* --------------------------------------------------------------------------
   Library init
-------------------------------------------------------------------------- */
void libusr_init()
{
    DBG("libusr_init");

    silgy_add_message(ERR_INVALID_LOGIN,            "EN-US", "Invalid login and/or password");
    silgy_add_message(ERR_USERNAME_TOO_SHORT,       "EN-US", "User name must be at least %d characters long", MIN_USERNAME_LEN);
    silgy_add_message(ERR_USERNAME_CHARS,           "EN-US", "User name may only contain letters, digits, dots, hyphens, underscores or apostrophes");
    silgy_add_message(ERR_USERNAME_TAKEN,           "EN-US", "Unfortunately this login has already been taken");
    silgy_add_message(ERR_EMAIL_EMPTY,              "EN-US", "Your email address can't be empty");
    silgy_add_message(ERR_EMAIL_FORMAT,             "EN-US", "Please enter valid email address");
    silgy_add_message(ERR_EMAIL_FORMAT_OR_EMPTY,    "EN-US", "Please enter valid email address or leave this field empty");
    silgy_add_message(ERR_EMAIL_TAKEN,              "EN-US", "This email address has already been registered");
    silgy_add_message(ERR_INVALID_PASSWORD,         "EN-US", "Please enter your existing password");
    silgy_add_message(ERR_PASSWORD_TOO_SHORT,       "EN-US", "Password must be at least %d characters long", MIN_PASSWORD_LEN);
    silgy_add_message(ERR_IN_10_COMMON_PASSWORDS,   "EN-US", "Your password is in 10 most common passwords, which makes it too easy to guess");
    silgy_add_message(ERR_IN_100_COMMON_PASSWORDS,  "EN-US", "Your password is in 100 most common passwords, which makes it too easy to guess");
    silgy_add_message(ERR_IN_1000_COMMON_PASSWORDS, "EN-US", "Your password is in 1,000 most common passwords, which makes it too easy to guess");
    silgy_add_message(ERR_IN_10000_COMMON_PASSWORDS,"EN-US", "Your password is in 10,000 most common passwords, which makes it too easy to guess");
    silgy_add_message(ERR_PASSWORD_DIFFERENT,       "EN-US", "Please retype password exactly like in the previous field");
    silgy_add_message(ERR_OLD_PASSWORD,             "EN-US", "Please enter your existing password");
    silgy_add_message(ERR_SESSION_EXPIRED,          "EN-US", "Your session has expired. Please log in to continue:");
    silgy_add_message(ERR_LINK_BROKEN,              "EN-US", "It looks like this link is broken. If you clicked on the link you've received from us in email, you can try to copy and paste it in your browser's address bar instead.");
    silgy_add_message(ERR_LINK_MAY_BE_EXPIRED,      "EN-US", "Your link is invalid or may be expired");
    silgy_add_message(ERR_LINK_EXPIRED,             "EN-US", "It looks like you entered email that doesn't exist in our database or your link has expired");
    silgy_add_message(ERR_LINK_TOO_MANY_TRIES,      "EN-US", "It looks like you entered email that doesn't exist in our database or your link has expired");
    silgy_add_message(ERR_ROBOT,                    "EN-US", "I'm afraid you are a robot?");
    silgy_add_message(ERR_WEBSITE_FIRST_LETTER,     "EN-US", "The first letter of this website's name should be %c", APP_WEBSITE[0]);
    silgy_add_message(ERR_NOT_ACTIVATED,            "EN-US", "Your account requires activation. Please check your mailbox for a message from %s.", APP_WEBSITE);

    silgy_add_message(WAR_NO_EMAIL,                 "EN-US", "You didn't provide your email address. This is fine, however please remember that in case you forget your password, there's no way for us to send you reset link.");
    silgy_add_message(WAR_BEFORE_DELETE,            "EN-US", "You are about to delete your %s's account. All your details and data will be removed from our database. If you are sure you want this, enter your password and click 'Delete my account'.", APP_WEBSITE);
    silgy_add_message(WAR_ULA_FIRST,                "EN-US", "Someone has tried to log in to this account unsuccessfully more than %d times. To protect your account from brute-force attack, this system requires you to wait for at least a minute before trying again.", MAX_ULA_BEFORE_FIRST_SLOW);
    silgy_add_message(WAR_ULA_SECOND,               "EN-US", "Someone has tried to log in to this account unsuccessfully more than %d times. To protect your account from brute-force attack, this system requires you to wait for at least an hour before trying again.", MAX_ULA_BEFORE_SECOND_SLOW);
    silgy_add_message(WAR_ULA_THIRD,                "EN-US", "Someone has tried to log in to this account unsuccessfully more than %d times. To protect your account from brute-force attack, this system requires you to wait for at least 23 hours before trying again.", MAX_ULA_BEFORE_THIRD_SLOW);
    silgy_add_message(WAR_PASSWORD_CHANGE,          "EN-US", "You have to change your password");

    silgy_add_message(MSG_WELCOME_NO_ACTIVATION,    "EN-US", "Welcome to %s! You can now log in:", APP_WEBSITE);
    silgy_add_message(MSG_WELCOME_NEED_ACTIVATION,  "EN-US", "Welcome to %s! Your account requires activation. Please check your mailbox for a message from %s.", APP_WEBSITE, APP_WEBSITE);
    silgy_add_message(MSG_WELCOME_AFTER_ACTIVATION, "EN-US", "Very well! You can now log in:");
    silgy_add_message(MSG_USER_LOGGED_OUT,          "EN-US", "You've been successfully logged out");
    silgy_add_message(MSG_CHANGES_SAVED,            "EN-US", "Your changes have been saved");
    silgy_add_message(MSG_REQUEST_SENT,             "EN-US", "Your request has been sent. Please check your mailbox for a message from %s.", APP_WEBSITE);
    silgy_add_message(MSG_PASSWORD_CHANGED,         "EN-US", "Your password has been changed. You can now log in:");
    silgy_add_message(MSG_MESSAGE_SENT,             "EN-US", "Your message has been sent");
    silgy_add_message(MSG_PROVIDE_FEEDBACK,         "EN-US", "%s would suit me better if...", APP_WEBSITE);
    silgy_add_message(MSG_FEEDBACK_SENT,            "EN-US", "Thank you for your feedback!");
    silgy_add_message(MSG_USER_ALREADY_ACTIVATED,   "EN-US", "Your account has already been activated");
    silgy_add_message(MSG_ACCOUNT_DELETED,          "EN-US", "Your user account has been deleted. Thank you for trying %s!", APP_WEBSITE);

#ifndef DONT_REFUSE_COMMON_PASSWORDS
    load_common_passwd();
#endif
}


/* --------------------------------------------------------------------------
   Return TRUE if user name contains only valid characters
-------------------------------------------------------------------------- */
static bool valid_username(const char *login)
{
    int i;

    for ( i=0; login[i] != EOS; ++i )
    {
        if ( !isalnum(login[i]) && login[i] != '.' && login[i] != '_' && login[i] != '-' && login[i] != '\'' && login[i] != '@' )
            return FALSE;
    }

    return TRUE;
}


/* --------------------------------------------------------------------------
   Return TRUE if email has valid format
-------------------------------------------------------------------------- */
static bool valid_email(const char *email)
{
    int     len;
    const char *at;
    int     i;

    len = strlen(email);

    if ( len < 3 ) return FALSE;

    at = strchr(email, '@');

    if ( !at ) return FALSE;                /* no @ */
    if ( at==email ) return FALSE;          /* @ is first */
    if ( at==email+len-1 ) return FALSE;    /* @ is last */

    for ( i=0; i<len; ++i )
    {
        if ( !isalnum(email[i]) && email[i] != '@' && email[i] != '.' && email[i] != '_' && email[i] != '-' )
            return FALSE;
    }

    return TRUE;
}


/* --------------------------------------------------------------------------
   Set ls cookie expiration time
-------------------------------------------------------------------------- */
static void set_ls_cookie_expiration(int ci, time_t from)
{
    strcpy(conn[ci].cookie_out_l_exp, time_epoch2http(from + 3600*24*USER_KEEP_LOGGED_DAYS));
}


/* --------------------------------------------------------------------------
   Upgrade anonymous user session to logged in
-------------------------------------------------------------------------- */
static int upgrade_uses(int ci, usession_t *us)
{
    DBG("upgrade_uses");

    DBG("Upgrading anonymous session to logged in, usi=%d, sesid [%s]", conn[ci].usi, US.sesid);

    US.logged = TRUE;
    US.uid = us->uid;
    strcpy(US.login, us->login);
    strcpy(US.email, us->email);
    strcpy(US.name, us->name);
    strcpy(US.phone, us->phone);
    strcpy(US.lang, us->lang);
    strcpy(US.about, us->about);

    US.group_id = us->group_id;
    US.auth_level = us->auth_level;

#ifndef SILGY_SVC
    if ( !silgy_app_user_login(ci) )
    {
        libusr_luses_downgrade(conn[ci].usi, ci, FALSE);
        return ERR_INT_SERVER_ERROR;
    }
#endif

    strcpy(conn[ci].cookie_out_a, "x");                     /* no longer needed */
    strcpy(conn[ci].cookie_out_a_exp, G_last_modified);     /* to be removed by browser */

    return OK;
}


#ifndef SILGY_SVC   /* this is for engine only */
/* --------------------------------------------------------------------------
   Verify IP & User-Agent against uid and sesid in uses (logged in users)
   set user session array index (usi) if all ok
-------------------------------------------------------------------------- */
int libusr_luses_ok(int ci)
{
    int ret=OK;
    int i;

    DBG("libusr_luses_ok");

    /* try in hot sessions first */

    if ( conn[ci].usi )   /* existing connection */
    {
        if ( uses[conn[ci].usi].sesid[0]
                && uses[conn[ci].usi].logged
                && 0==strcmp(conn[ci].cookie_in_l, uses[conn[ci].usi].sesid)
                && 0==strcmp(conn[ci].uagent, uses[conn[ci].usi].uagent) )
        {
            DBG("Logged in session found in cache, usi=%d, sesid [%s] (1)", conn[ci].usi, uses[conn[ci].usi].sesid);
            return OK;
        }
        else    /* session was closed */
        {
            conn[ci].usi = 0;
        }
    }
    else    /* fresh connection */
    {
        for ( i=1; i<=MAX_SESSIONS; ++i )
        {
            if ( uses[i].sesid[0]
                    && uses[i].logged
                    && 0==strcmp(conn[ci].cookie_in_l, uses[i].sesid)
                    && 0==strcmp(conn[ci].uagent, uses[i].uagent) )
            {
                DBG("Logged in session found in cache, usi=%d, sesid [%s] (2)", i, uses[i].sesid);
                conn[ci].usi = i;
                return OK;
            }
        }
    }

    DBG("Session [%s] not found in cache", conn[ci].cookie_in_l);

    /* not found in memory -- try database */

    char        sql[SQLBUF];
    MYSQL_RES   *result;
    MYSQL_ROW   row;

    char sanuagent[DB_UAGENT_LEN+1];
    sanitize_sql(sanuagent, conn[ci].uagent, DB_UAGENT_LEN);

    char sanlscookie[SESID_LEN+1];
    strcpy(sanlscookie, silgy_filter_strict(conn[ci].cookie_in_l));

    sprintf(sql, "SELECT uagent, user_id, created, csrft FROM users_logins WHERE sesid = BINARY '%s'", sanlscookie);
    DBG("sql: %s", sql);

    mysql_query(G_dbconn, sql);

    result = mysql_store_result(G_dbconn);

    if ( !result )
    {
        ERR("%u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
        return ERR_INT_SERVER_ERROR;
    }

    row = mysql_fetch_row(result);

    if ( !row )     /* no such session in database */
    {
        mysql_free_result(result);
        WAR("No logged in session in database [%s]", sanlscookie);
        strcpy(conn[ci].cookie_out_l, "x");
        strcpy(conn[ci].cookie_out_l_exp, G_last_modified);     /* expire ls cookie */

        /* ---------------------------------------------------------------------------------- */
        /* brute force ls cookie attack prevention */

        /* maintain the list of last n IPs with failed ls cookie authentication with counters */

        static failed_login_cnt_t failed_cnt[FAILED_LOGIN_CNT_SIZE];
        static int failed_cnt_used=0;
        static int failed_cnt_next=0;
        char found=0;

        for ( i=0; i<failed_cnt_used && i<FAILED_LOGIN_CNT_SIZE; ++i )
        {
            if ( 0==strcmp(conn[ci].ip, failed_cnt[i].ip) )
            {
                if ( (failed_cnt[i].cnt > 10 && failed_cnt[i].when > G_now-60)      /* 10 failed attempts within a minute or */
                    || (failed_cnt[i].cnt > 100 && failed_cnt[i].when > G_now-3600) /* 100 failed attempts within an hour or */
                    || failed_cnt[i].cnt > 1000 )                                   /* 1000 failed attempts */
                {
                    WAR("Looks like brute-force cookie attack, blocking IP");
                    eng_block_ip(conn[ci].ip, TRUE);
                }
                else
                {
                    ++failed_cnt[i].cnt;
                }

                found = 1;
                break;
            }
        }

        if ( !found )   /* add record to failed_cnt array */
        {
            strcpy(failed_cnt[failed_cnt_next].ip, conn[ci].ip);
            failed_cnt[failed_cnt_next].cnt = 1;
            failed_cnt[failed_cnt_next].when = G_now;
            
            if ( failed_cnt_next >= FAILED_LOGIN_CNT_SIZE-1 )    /* last slot was just used -- roll over */
                failed_cnt_next = 0;
            else
            {
                ++failed_cnt_next;

                if ( failed_cnt_used < FAILED_LOGIN_CNT_SIZE )   /* before first roll-over */
                    ++failed_cnt_used;
            }
        }

        /* ---------------------------------------------------------------------------------- */

        return ERR_SESSION_EXPIRED;
    }

    /* verify uagent */

    if ( 0 != strcmp(sanuagent, row[0]) )
    {
        mysql_free_result(result);
        INF("Different uagent in database for sesid [%s]", sanlscookie);
        strcpy(conn[ci].cookie_out_l, "x");
        strcpy(conn[ci].cookie_out_l_exp, G_last_modified);     /* expire ls cookie */
        return ERR_SESSION_EXPIRED;
    }

    DBG("ci=%d, sesid [%s] uagent OK", ci, sanlscookie);

    /* -------------------------------------------------- */
    /* Verify time. If created more than USER_KEEP_LOGGED_DAYS ago -- refuse */

    time_t created = db2epoch(row[2]);

    if ( created < G_now - 3600*24*USER_KEEP_LOGGED_DAYS )
    {
        DBG("Removing old logged in session, usi=%d, sesid [%s], created %s from database", conn[ci].usi, sanlscookie, row[2]);

        mysql_free_result(result);

        sprintf(sql, "DELETE FROM users_logins WHERE sesid = BINARY '%s'", sanlscookie);
        DBG("sql: %s", sql);

        if ( mysql_query(G_dbconn, sql) )
        {
            ERR("%u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
            return ERR_INT_SERVER_ERROR;
        }

        /* tell browser we're logging out */

        strcpy(conn[ci].cookie_out_l, "x");
        strcpy(conn[ci].cookie_out_l_exp, G_last_modified);     /* expire ls cookie */

        INF("Session [%s] expired", sanlscookie);

        return ERR_SESSION_EXPIRED;
    }

    DBG("ci=%d, sesid [%s] created not too long ago => OK", ci, sanlscookie);

    /* -------------------------------------------------- */
    /* cookie has not expired -- log user in */

    usession_t us={0};   /* pass user information (in this case id) over to do_login */

    us.uid = atoi(row[1]);

    char csrft[CSRFT_LEN+1];

    if ( row[3] && row[3][0] )   /* from users_logins */
    {
        DBG("Using previous CSRFT [%s]", row[3]);
        strcpy(csrft, row[3]);
    }
    else
    {
        DBG("Fresh CSRFT will be used");
        csrft[0] = EOS;
    }

    mysql_free_result(result);

    DBG("Logged in session found in database and OK");

    /* -------------------------------------------------- */
    /* start a fresh anonymous session */

    if ( (ret=eng_uses_start(ci, NULL)) != OK )
        return ret;

    if ( csrft[0] )   /* using previous CSRFT */
        strcpy(US.csrft, csrft);

    /* replace sesid */

    if ( csrft[0] )
        sprintf(sql, "UPDATE users_logins SET sesid='%s', last_used='%s' WHERE sesid = BINARY '%s'", US.sesid, DT_NOW, sanlscookie);
    else
        sprintf(sql, "UPDATE users_logins SET sesid='%s', csrft='%s', last_used='%s' WHERE sesid = BINARY '%s'", US.sesid, US.csrft, DT_NOW, sanlscookie);
    DBG("sql: %s", sql);
    if ( mysql_query(G_dbconn, sql) )
    {
        ERR("%u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
        return ERR_INT_SERVER_ERROR;
    }

    /* set cookie */

    strcpy(conn[ci].cookie_out_l, US.sesid);

    set_ls_cookie_expiration(ci, created);

    /* upgrade uses */

    return do_login(ci, &us, 100, 0);
}


/* --------------------------------------------------------------------------
   Close timeouted logged in user sessions
-------------------------------------------------------------------------- */
void libusr_luses_close_timeouted()
{
    int     i;
    time_t  last_allowed;

    last_allowed = G_now - LUSES_TIMEOUT;

    for ( i=1; G_sessions>0 && i<=MAX_SESSIONS; ++i )
    {
        if ( uses[i].sesid[0] && uses[i].logged && uses[i].last_activity < last_allowed )
            libusr_luses_downgrade(i, NOT_CONNECTED, FALSE);
    }
}


/* --------------------------------------------------------------------------
   Save open, logged in sessions' CSRF tokens
-------------------------------------------------------------------------- */
void libusr_luses_save_csrft()
{
    int  i;
    char sql[SQLBUF];

    DBG("libusr_luses_save_csrft");

    int sessions = G_sessions;

    for ( i=1; sessions>0 && i<=MAX_SESSIONS; ++i )
    {
        if ( uses[i].sesid[0] && uses[i].logged )
        {
            sprintf(sql, "UPDATE users_logins SET csrft='%s' WHERE sesid = BINARY '%s'", uses[i].csrft, uses[i].sesid);
            DBG("sql: %s", sql);
            if ( mysql_query(G_dbconn, sql) )
                ERR("%u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));

            sessions--;
        }
    }
}
#endif  /* SILGY_SVC */


/* --------------------------------------------------------------------------
   Downgrade logged in user session to anonymous
-------------------------------------------------------------------------- */
void libusr_luses_downgrade(int usi, int ci, bool usr_logout)
{
    char sql[SQLBUF];

    DBG("libusr_luses_downgrade");

    DBG("Downgrading logged in session to anonymous, usi=%d, sesid [%s]", usi, uses[usi].sesid);

    uses[usi].logged = FALSE;
    uses[usi].uid = 0;
    uses[usi].login[0] = EOS;
    uses[usi].email[0] = EOS;
    uses[usi].name[0] = EOS;
    uses[usi].phone[0] = EOS;

    if ( ci != NOT_CONNECTED )   /* still connected */
        strcpy(uses[usi].lang, conn[ci].lang);
    else
    {
        uses[usi].lang[0] = EOS;
        uses[usi].tz_offset = 0;
    }

    uses[usi].about[0] = EOS;
    uses[usi].group_id = 0;
    uses[usi].auth_level = AUTH_LEVEL_ANONYMOUS;

#ifndef SILGY_SVC
    if ( ci != NOT_CONNECTED )   /* still connected */
    {
        silgy_app_user_logout(ci);
    }
    else    /* trick to maintain consistency across silgy_app_xxx functions */
    {       /* that use ci for everything -- even to get user session data */
        conn[CLOSING_SESSION_CI].usi = usi;
        silgy_app_user_logout(CLOSING_SESSION_CI);
    }
#endif  /* SILGY_SVC */

    if ( usr_logout )   /* explicit user logout */
    {
        sprintf(sql, "DELETE FROM users_logins WHERE sesid = BINARY '%s'", uses[usi].sesid);
        DBG("sql: %s", sql);
        if ( mysql_query(G_dbconn, sql) )
            ERR("%u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));

        if ( ci != NOT_CONNECTED )   /* still connected */
        {
            strcpy(conn[ci].cookie_out_l, "x");
            strcpy(conn[ci].cookie_out_l_exp, G_last_modified);     /* in the past => to be removed by browser straight away */

            strcpy(conn[ci].cookie_out_a, uses[usi].sesid);
        }
    }
    else    /* timeout */
    {
        sprintf(sql, "UPDATE users_logins SET csrft='%s' WHERE sesid = BINARY '%s'", uses[usi].csrft, uses[usi].sesid);
        DBG("sql: %s", sql);
        if ( mysql_query(G_dbconn, sql) )
            ERR("%u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
    }
#ifndef DONT_RESET_AUSES_ON_LOGOUT
    memset(&auses[usi], 0, sizeof(ausession_t));
#endif
}


/* --------------------------------------------------------------------------
   Check whether user exists in database
-------------------------------------------------------------------------- */
static int user_exists(const char *login)
{
    char        sql[SQLBUF];
    MYSQL_RES   *result;
    unsigned    records;

    DBG("user_exists, login [%s]", login);

//  if ( 0==strcmp(sanlogin, "ADMIN") )
//      return ERR_USERNAME_TAKEN;

    sprintf(sql, "SELECT id FROM users WHERE login_u='%s'", upper(login));

    DBG("sql: %s", sql);

    mysql_query(G_dbconn, sql);

    result = mysql_store_result(G_dbconn);

    if ( !result )
    {
        ERR("%u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
        return ERR_INT_SERVER_ERROR;
    }

    records = mysql_num_rows(result);

    mysql_free_result(result);

    if ( 0 != records )
        return ERR_USERNAME_TAKEN;

    return OK;
}


/* --------------------------------------------------------------------------
   Check whether email exists in database
-------------------------------------------------------------------------- */
static int email_exists(const char *email)
{
    char        sql[SQLBUF];
    MYSQL_RES   *result;
    unsigned    records;

    DBG("email_exists, email [%s]", email);

    sprintf(sql, "SELECT id FROM users WHERE email_u='%s'", upper(email));

    DBG("sql: %s", sql);

    mysql_query(G_dbconn, sql);

    result = mysql_store_result(G_dbconn);

    if ( !result )
    {
        ERR("%u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
        return ERR_INT_SERVER_ERROR;
    }

    records = mysql_num_rows(result);

    mysql_free_result(result);

    if ( 0 != records )
        return ERR_EMAIL_TAKEN;

    return OK;
}


/* --------------------------------------------------------------------------
   Log user in -- called either by l_usession_ok or silgy_usr_login
   Authentication has already been done prior to calling this.
   Connection (conn[ci]) has to have an anonymous session.
   us serves here to pass user information
-------------------------------------------------------------------------- */
static int do_login(int ci, usession_t *us, char status, int visits)
{
    int         ret;
    char        sql[SQLBUF];
    MYSQL_RES   *result;
    MYSQL_ROW   row;

    DBG("do_login");

    UID = us->uid;

    if ( status == 100 )    /* login from cookie -- we only have a user id from users_logins */
    {
        sprintf(sql, "SELECT login,email,name,phone,lang,about,group_id,auth_level,status,visits FROM users WHERE id=%d", UID);
        DBG("sql: %s", sql);
        mysql_query(G_dbconn, sql);
        result = mysql_store_result(G_dbconn);
        if ( !result )
        {
            ERR("%u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
            return ERR_INT_SERVER_ERROR;
        }

        row = mysql_fetch_row(result);

        if ( !row )    /* this should never happen */
        {
            mysql_free_result(result);
            ERR("Cookie sesid [%s] does not match user id=%d", US.sesid, UID);
            return ERR_INVALID_LOGIN;   /* invalid user and/or password */
        }

        strcpy(US.login, row[0]?row[0]:"");
        strcpy(US.email, row[1]?row[1]:"");
        strcpy(US.name, row[2]?row[2]:"");
        strcpy(US.phone, row[3]?row[3]:"");
        strcpy(US.lang, row[4]?row[4]:"");
        strcpy(US.about, row[5]?row[5]:"");
        US.group_id = row[6]?atoi(row[6]):0;
        US.auth_level = row[7]?atoi(row[7]):DEF_USER_AUTH_LEVEL;

        /* non-session data */

        status = row[8]?atoi(row[8]):USER_STATUS_ACTIVE;
        visits = row[9]?atoi(row[9]):0;

        mysql_free_result(result);
    }
    else    /* called by silgy_usr_login -- user data already read from users */
    {
        strcpy(US.login, us->login);
        strcpy(US.email, us->email);
        strcpy(US.name, us->name);
        strcpy(US.phone, us->phone);
        strcpy(US.lang, us->lang);
        strcpy(US.about, us->about);
        US.group_id = us->group_id;
        US.auth_level = us->auth_level;
    }

    if ( US.group_id > 0 )    /* auth_level inherited from a group */
    {
        sprintf(sql, "SELECT name,about,auth_level FROM users_groups WHERE id=%d", US.group_id);
        DBG("sql: %s", sql);
        mysql_query(G_dbconn, sql);
        result = mysql_store_result(G_dbconn);
        if ( !result )
        {
            ERR("%u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
            return ERR_INT_SERVER_ERROR;
        }

        row = mysql_fetch_row(result);

        if ( row )
            US.auth_level = row[2]?atoi(row[2]):DEF_USER_AUTH_LEVEL;
        else
            WAR("group_id=%d not found in users_groups", US.group_id);

        mysql_free_result(result);
    }

    /* upgrade anonymous session to logged in */

    if ( (ret=upgrade_uses(ci, &US)) != OK )
        return ret;

    /* update user record */

    sprintf(sql, "UPDATE users SET visits=%d, last_login='%s' WHERE id=%d", visits+1, DT_NOW, UID);
    DBG("sql: %s", sql);
    if ( mysql_query(G_dbconn, sql) )
    {
        ERR("%u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
        return ERR_INT_SERVER_ERROR;
    }

#ifdef USERSBYEMAIL
    INF("User [%s] logged in", US.email);
#else
    INF("User [%s] logged in", US.login);
#endif

    /* password change required? */

    if ( status == USER_STATUS_PASSWORD_CHANGE )
    {
        WAR("User is required to change their password");
        return WAR_PASSWORD_CHANGE;
    }

    return OK;
}


/* --------------------------------------------------------------------------
   Send activation email
-------------------------------------------------------------------------- */
static int generic_user_activation_email(int ci, int uid, const char *email, const char *linkkey)
{
    char subject[256];
    char message[4096];

    OUTP_BEGIN(message);

    OUTP("Dear %s,\n\n", silgy_usr_name(NULL, NULL, NULL, uid));
    OUTP("Welcome to %s! Your account requires activation. Please visit this URL to activate your account:\n\n", conn[ci].website);
#ifdef HTTPS
    if ( G_test )
        OUTP("http://%s/activate_acc?k=%s\n\n", conn[ci].host, linkkey);
    else
        OUTP("https://%s/activate_acc?k=%s\n\n", conn[ci].host, linkkey);
#else
    OUTP("http://%s/activate_acc?k=%s\n\n", conn[ci].host, linkkey);
#endif  /* HTTPS */
    OUTP("Please keep in mind that this link will only be valid for the next %d hours.\n\n", USER_ACTIVATION_HOURS);
    OUTP("If you did this by mistake or it wasn't you, you can safely ignore this email.\n\n");
#ifdef APP_CONTACT_EMAIL
    OUTP("In case you needed any help, please contact us at %s.\n\n", APP_CONTACT_EMAIL);
#endif
    OUTP("Kind Regards\n");
    OUTP("%s\n", conn[ci].website);

    OUTP_END;

    sprintf(subject, "%s Account Activation", conn[ci].website);

    if ( !silgy_email(email, subject, message) )
        return ERR_INT_SERVER_ERROR;

    return OK;
}

    
/* --------------------------------------------------------------------------
   Send activation link
-------------------------------------------------------------------------- */
static int send_activation_link(int ci, int uid, const char *email)
{
    int  ret=OK;
    char linkkey[PASSWD_RESET_KEY_LEN+1];
    char sql[SQLBUF];
    
    /* generate the key */

    silgy_random(linkkey, PASSWD_RESET_KEY_LEN);

    sprintf(sql, "INSERT INTO users_activations (linkkey,user_id,created,activated) VALUES ('%s',%d,'%s','%s')", linkkey, uid, DT_NOW, DT_NULL);
    DBG("sql: %s", sql);

    if ( mysql_query(G_dbconn, sql) )
    {
        ERR("%u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
        return ERR_INT_SERVER_ERROR;
    }

    /* send an email */

#ifdef APP_ACTIVATION_EMAIL
    ret = silgy_app_user_activation_email(ci, uid, email, linkkey);
#else
    ret = generic_user_activation_email(ci, uid, email, linkkey);
#endif

    return ret;
}


/* --------------------------------------------------------------------------
   Verify activation key
-------------------------------------------------------------------------- */
static int silgy_usr_verify_activation_key(int ci, char *linkkey, int *uid)
{
    char        sql[SQLBUF];
    MYSQL_RES   *result;
    MYSQL_ROW   row;
    QSVAL       esc_linkkey;

    DBG("silgy_usr_verify_activation_key");

    if ( strlen(linkkey) != PASSWD_RESET_KEY_LEN )
        return ERR_LINK_BROKEN;

    strcpy(esc_linkkey, silgy_sql_esc(linkkey));

    sprintf(sql, "SELECT user_id, created, activated FROM users_activations WHERE linkkey = BINARY '%s'", esc_linkkey);
    DBG("sql: %s", sql);

    mysql_query(G_dbconn, sql);

    result = mysql_store_result(G_dbconn);

    if ( !result )
    {
        ERR("%u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
        return ERR_INT_SERVER_ERROR;
    }

    row = mysql_fetch_row(result);

    if ( !row )     /* no records with this key in users_activations -- link broken? */
    {
        mysql_free_result(result);
        return ERR_LINK_MAY_BE_EXPIRED;
    }

    /* already activated? */

    if ( row[2] && 0!=strcmp(row[2], DT_NULL) )
    {
        mysql_free_result(result);
        DBG("User already activated");
        return MSG_USER_ALREADY_ACTIVATED;
    }

    /* validate expiry time */

    if ( db2epoch(row[1]) < G_now-3600*USER_ACTIVATION_HOURS )
    {
        WAR("Key created more than %d hours ago", USER_ACTIVATION_HOURS);
        mysql_free_result(result);
        return ERR_LINK_MAY_BE_EXPIRED;
    }

    /* otherwise everything's OK ----------------------------------------- */

    /* get the user id */

    *uid = atoi(row[0]);

    mysql_free_result(result);

    DBG("Key ok, uid = %d", *uid);

    return OK;
}




/* ------------------------------------------------------------------------------------------------------------
    Public user functions
------------------------------------------------------------------------------------------------------------ */

/* --------------------------------------------------------------------------
   Log user in / explicit from Log In page
   Return OK or:
   ERR_INVALID_REQUEST
   ERR_INT_SERVER_ERROR
   ERR_INVALID_LOGIN
   and through do_login:
   ERR_SERVER_TOOBUSY
-------------------------------------------------------------------------- */
int silgy_usr_login(int ci)
{
    int         ret=OK;

    QSVAL       login;
    QSVAL       email;
    QSVAL       passwd;
    QSVAL       keep;

    char        sql[SQLBUF];
    MYSQL_RES   *result;
    MYSQL_ROW   row;
    char        p1[32], p2[32];
    char        str1[32], str2[32];
    char        status;
    int         visits;
    int         ula_cnt;
    char        ula_time[32];
    int         new_ula_cnt;

    usession_t  us={0};    /* pass user information over to do_login */

    DBG("silgy_usr_login");

#ifdef USERSBYEMAIL

    if ( !QS_HTML_ESCAPE("email", email) || !QS_HTML_ESCAPE("passwd", passwd) )
    {
        WAR("Invalid request (URI val missing?)");
        return ERR_INVALID_REQUEST;
    }
    stp_right(email);
    sprintf(sql, "SELECT id,login,email,name,phone,passwd1,passwd2,lang,about,group_id,auth_level,status,visits,ula_cnt,ula_time FROM users WHERE email_u='%s'", upper(email));

#else    /* by login */

    if ( !QS_HTML_ESCAPE("login", login) || !QS_HTML_ESCAPE("passwd", passwd) )
    {
        WAR("Invalid request (URI val missing?)");
        return ERR_INVALID_REQUEST;
    }
    stp_right(login);
    QSVAL ulogin;
    strcpy(ulogin, upper(login));
    sprintf(sql, "SELECT id,login,email,name,phone,passwd1,passwd2,lang,about,group_id,auth_level,status,visits,ula_cnt,ula_time FROM users WHERE login_u='%s' OR email_u='%s'", ulogin, ulogin);

#endif  /* USERSBYEMAIL */

    DBG("sql: %s", sql);

    mysql_query(G_dbconn, sql);

    result = mysql_store_result(G_dbconn);

    if ( !result )
    {
        ERR("%u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
        return ERR_INT_SERVER_ERROR;
    }

    row = mysql_fetch_row(result);

    if ( !row )     /* no records */
    {
        mysql_free_result(result);
        return ERR_INVALID_LOGIN;   /* invalid user and/or password */
    }

    /* login/email found */

    us.uid = atoi(row[0]);
    strcpy(us.login, row[1]?row[1]:"");
    strcpy(us.email, row[2]?row[2]:"");
    strcpy(us.name, row[3]?row[3]:"");
    strcpy(us.phone, row[4]?row[4]:"");
    strcpy(us.lang, row[7]?row[7]:"");
    strcpy(us.about, row[8]?row[8]:"");
    us.group_id = row[9]?atoi(row[9]):0;
    us.auth_level = row[10]?atoi(row[10]):DEF_USER_AUTH_LEVEL;

    /* non-session data */

    strcpy(p1, row[5]?row[5]:"");
    strcpy(p2, row[6]?row[6]:"");
    status = row[11]?atoi(row[11]):USER_STATUS_ACTIVE;
    visits = row[12]?atoi(row[12]):0;
    ula_cnt = row[13]?atoi(row[13]):0;
    strcpy(ula_time, row[14]?row[14]:DT_NULL);

    mysql_free_result(result);

    /* deleted? */

    if ( status == USER_STATUS_DELETED )
    {
        WAR("User deleted");
        return ERR_INVALID_LOGIN;
    }

    /* locked out? */

    if ( status == USER_STATUS_LOCKED )
    {
        WAR("User locked");
        return ERR_INVALID_LOGIN;
    }

    /* check ULA (Unsuccessful Login Attempts) info to prevent brute-force password attacks */

    if ( ula_cnt > MAX_ULA_BEFORE_FIRST_SLOW )
    {
        if ( ula_cnt > MAX_ULA_BEFORE_LOCK )
        {
            WAR("ula_cnt > MAX_ULA_BEFORE_LOCK (%d) => locking user account", MAX_ULA_BEFORE_LOCK);

            sprintf(sql, "UPDATE users SET status=%d WHERE id=%d", USER_STATUS_LOCKED, us.uid);
            DBG("sql: %s", sql);
            if ( mysql_query(G_dbconn, sql) )
                ERR("%u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));

            if ( us.email[0] )   /* notify account owner */
            {
                char subject[256];
                char message[4096];

                OUTP_BEGIN(message);

                OUTP("Dear %s,\n\n", silgy_usr_name(us.login, us.email, us.name, 0));
                OUTP("Someone has tried to log in to your %s account unsuccessfully more than %d times. To protect it from brute-force attack your account has been locked.\n\n", conn[ci].website, MAX_ULA_BEFORE_LOCK);
#ifdef APP_CONTACT_EMAIL
                OUTP("Please contact us at %s.\n\n", APP_CONTACT_EMAIL);
#endif
                OUTP("Kind Regards\n");
                OUTP("%s\n", conn[ci].website);

                OUTP_END;

                sprintf(subject, "%s account locked", conn[ci].website);

                silgy_email(us.email, subject, message);
            }

            return ERR_INVALID_LOGIN;
        }

        WAR("ula_cnt = %d", ula_cnt);

        time_t last_ula_epoch = db2epoch(ula_time);

        if ( ula_cnt <= MAX_ULA_BEFORE_SECOND_SLOW )
        {
            if ( last_ula_epoch > G_now-60 )    /* less than a minute => wait before the next attempt */
            {
                WAR("Trying again too soon (wait a minute)");
                return WAR_ULA_FIRST;
            }
        }
        else if ( ula_cnt <= MAX_ULA_BEFORE_THIRD_SLOW )
        {
            if ( last_ula_epoch > G_now-3600 )  /* less than an hour => wait before the next attempt */
            {
                WAR("Trying again too soon (wait an hour)");
                return WAR_ULA_SECOND;
            }
        }
        else    /* ula_cnt > MAX_ULA_BEFORE_THIRD_SLOW */
        {
            if ( last_ula_epoch > G_now-3600*23 )   /* less than 23 hours => wait before the next attempt */
            {
                WAR("Trying again too soon (wait a day)");
                return WAR_ULA_THIRD;
            }
        }
    }

    /* now check username/email and password pairs as they should be */

    /* generate hashes using entered password */

    get_hashes(str1, str2, us.login, us.email, passwd);

    /* compare them with the stored ones */

    if ( 0 != strcmp(str1, p1) || (us.email[0] && 0 != strcmp(str2, p2)) )   /* passwd1, passwd2 */
    {
        WAR("Invalid password");
        new_ula_cnt = ula_cnt + 1;
        sprintf(sql, "UPDATE users SET ula_cnt=%d, ula_time='%s' WHERE id=%d", new_ula_cnt, DT_NOW, us.uid);
        DBG("sql: %s", sql);
        if ( mysql_query(G_dbconn, sql) )
        {
            ERR("%u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
            return ERR_INT_SERVER_ERROR;
        }
        return ERR_INVALID_LOGIN;   /* invalid user and/or password */
    }

    DBG("Password OK");

    /* activated? */

    if ( status == USER_STATUS_INACTIVE )
    {
        WAR("User not activated");
        return ERR_NOT_ACTIVATED;
    }

    DBG("User activation status OK");

    /* successful login ------------------------------------------------------------ */

    if ( ula_cnt )   /* clear it */
    {
        DBG("Clearing ula_cnt");
        sprintf(sql, "UPDATE users SET ula_cnt=0 WHERE id=%d", us.uid);
        DBG("sql: %s", sql);
        if ( mysql_query(G_dbconn, sql) )
        {
            ERR("%u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
            return ERR_INT_SERVER_ERROR;
        }
    }

    /* use anonymous session if present but refresh sesid  */

    if ( conn[ci].usi )
    {
        silgy_random(US.sesid, SESID_LEN);
        DBG("Using current session usi=%d, generated new sesid [%s]", conn[ci].usi, US.sesid);
    }
    else    /* no session --> start a new one */
    {
        DBG("No session, starting new");

        if ( (ret=eng_uses_start(ci, NULL)) != OK )
            return ret;
    }

    /* save new session to users_logins and set the cookie */

    DBG("Saving user session [%s] in users_logins...", US.sesid);

    char sanuagent[DB_UAGENT_LEN+1];
    sanitize_sql(sanuagent, conn[ci].uagent, DB_UAGENT_LEN);

    sprintf(sql, "INSERT INTO users_logins (sesid,uagent,ip,user_id,csrft,created,last_used) VALUES ('%s','%s','%s',%d,'%s','%s','%s')", US.sesid, sanuagent, conn[ci].ip, us.uid, US.csrft, DT_NOW, DT_NOW);
    DBG("sql: %s", sql);
    if ( mysql_query(G_dbconn, sql) )
    {
        ERR("%u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
        if ( mysql_errno(G_dbconn) != 1062 )    /* duplicate entry */
            return ERR_INT_SERVER_ERROR;
    }

    DBG("User session saved OK");

    /* set cookie */

    strcpy(conn[ci].cookie_out_l, US.sesid);

    /* Keep me logged in -- set cookie expiry date */

    if ( QS_HTML_ESCAPE("keep", keep) && 0==strcmp(keep, "on") )
    {
        DBG("keep is ON");
        set_ls_cookie_expiration(ci, G_now);
    }

    /* upgrade uses */

    return do_login(ci, &us, status, visits);
}


/* --------------------------------------------------------------------------
   Load common passwords list from a file
   Initial list obtained from:
   https://github.com/danielmiessler/SecLists/blob/master/Passwords/Common-Credentials/10k-most-common.txt
-------------------------------------------------------------------------- */
static bool load_common_passwd()
{
    char    fname[1024];
    FILE    *h_file=NULL;
    char    c;
    int     i=0;
    char    value[16];

    INF("Loading common passwords list");

    /* open the file */

    if ( G_appdir[0] )
        sprintf(fname, "%s/bin/%s", G_appdir, COMMON_PASSWORDS_FILE);
    else
        strcpy(fname, COMMON_PASSWORDS_FILE);

    if ( NULL == (h_file=fopen(fname, "r")) )
    {
        WAR("Couldn't open %s\n", fname);
        return FALSE;
    }

    M_common_cnt = 0;

    /* parse the file */

    while ( EOF != (c=fgetc(h_file)) )
    {
        if ( c == ' ' || c == '\t' || c == '\r' ) continue;   /* omit whitespaces */

        if ( c == '\n' )    /* end of value */
        {
            if ( i )   /* non-empty value */
            {
                value[i] = EOS;
                i = 0;
                strcpy(M_common[M_common_cnt++], value);
                if ( M_common_cnt == MAX_COMMON ) break;
            }
        }
        else
        {
            if ( i < 15 )
                value[i++] = c;
        }
    }

    if ( i )   /* end of value */
    {
        value[i] = EOS;
        strcpy(M_common[M_common_cnt++], value);
    }

    if ( NULL != h_file )
        fclose(h_file);

    ALWAYS("%d common passwords", M_common_cnt);

    /* show the list */

#ifdef DUMP
    DBG("");
    for ( i=0; i<M_common_cnt; ++i )
        DBG("[%s]", M_common[i]);
    DBG("");
#endif

    return TRUE;
}


/* --------------------------------------------------------------------------
   Assess password quality
-------------------------------------------------------------------------- */
int silgy_usr_password_quality(const char *passwd)
{
    int len = strlen(passwd);

    if ( len < MIN_PASSWORD_LEN )
        return ERR_PASSWORD_TOO_SHORT;

#ifndef DONT_REFUSE_COMMON_PASSWORDS

    int i;

    for ( i=0; i<M_common_cnt; ++i )
    {
        if ( 0==strcmp(M_common[i], passwd) )
#ifdef REFUSE_100_COMMON_PASSWORDS
            return ERR_IN_100_COMMON_PASSWORDS;
#elif defined REFUSE_1000_COMMON_PASSWORDS
            return ERR_IN_1000_COMMON_PASSWORDS;
#elif defined REFUSE_10000_COMMON_PASSWORDS
            return ERR_IN_10000_COMMON_PASSWORDS;
#else
            return ERR_IN_10_COMMON_PASSWORDS;
#endif
    }

#endif  /* DONT_REFUSE_COMMON_PASSWORDS */

    return OK;
}


/* --------------------------------------------------------------------------
   Create user account using query string values
-------------------------------------------------------------------------- */
static int create_account(int ci, char auth_level, char status, bool current_session)
{
    int     ret=OK;
    QSVAL   tmp;
    char    login[LOGIN_LEN+1];
    char    login_u[LOGIN_LEN+1];
    char    email[EMAIL_LEN+1];
    char    email_u[EMAIL_LEN+1];
    char    name[UNAME_LEN+1];
    char    phone[PHONE_LEN+1];
    QSVAL   lang;
    char    about[ABOUT_LEN+1];
    QSVAL   passwd;
    QSVAL   rpasswd;
    QSVAL   message;
    char    sql[SQLBUF];
    char    str1[32], str2[32];

    DBG("create_account");

    /* get the basics */

    if ( QS_HTML_ESCAPE("login", tmp) )
    {
        COPY(login, tmp, LOGIN_LEN);
        stp_right(login);
        if ( current_session && conn[ci].usi ) strcpy(US.login, login);
    }
    else
        login[0] = EOS;

    if ( QS_HTML_ESCAPE("email", tmp) )
    {
        COPY(email, tmp, EMAIL_LEN);
        stp_right(email);
        if ( current_session && conn[ci].usi ) strcpy(US.email, email);
    }
    else
        email[0] = EOS;

    /* basic verification */

#ifdef USERSBYEMAIL
    if ( !email[0] )    /* email empty */
    {
        ERR("Invalid request (email missing)");
        return ERR_EMAIL_EMPTY;
    }
#endif  /* USERSBYEMAIL */

    if ( !login[0] )    /* login empty */
    {
#ifdef USERSBYEMAIL
        COPY(login, email, LOGIN_LEN);
#else   /* USERSBYLOGIN */
        if ( email[0] )
            COPY(login, email, LOGIN_LEN);
        else
        {
            ERR("Invalid request (login missing)");
            return ERR_INVALID_REQUEST;
        }
#endif
    }

    /* regardless of authentication method */

    if ( G_usersRequireAccountActivation && !email[0] )
    {
        ERR("Invalid request (email missing)");
        return ERR_EMAIL_EMPTY;
    }

    if ( !QS_HTML_ESCAPE("passwd", passwd)
            || !QS_HTML_ESCAPE("rpasswd", rpasswd) )
    {
        ERR("Invalid request (passwd or rpasswd missing)");
        return ERR_INVALID_REQUEST;
    }

    /* optional */

    if ( QS_HTML_ESCAPE("name", tmp) )
    {
        COPY(name, tmp, UNAME_LEN);
        stp_right(name);
        if ( current_session && conn[ci].usi ) strcpy(US.name, name);
    }
    else
        name[0] = EOS;

    if ( QS_HTML_ESCAPE("phone", tmp) )
    {
        COPY(phone, tmp, PHONE_LEN);
        stp_right(phone);
        if ( current_session && conn[ci].usi ) strcpy(US.phone, phone);
    }
    else
        phone[0] = EOS;

    if ( QS_HTML_ESCAPE("lang", lang) )
    {
        lang[LANG_LEN] = EOS;
        stp_right(lang);
    }
    else
        lang[0] = EOS;

    if ( !lang[0] ) strcpy(lang, conn[ci].lang);    /* use current request lang if empty */

    if ( current_session && conn[ci].usi ) strcpy(US.lang, lang);

    if ( QS_HTML_ESCAPE("about", tmp) )
    {
        COPY(about, tmp, ABOUT_LEN);
        stp_right(about);
        if ( current_session && conn[ci].usi ) strcpy(US.about, about);
    }
    else
        about[0] = EOS;

    /* ----------------------------------------------------------------- */

    int plen = strlen(passwd);

    if ( QS_HTML_ESCAPE("message", message) && message[0] )
        return ERR_ROBOT;

#ifdef USERSBYEMAIL
        if ( !email[0] )                                /* email empty */
            return ERR_EMAIL_EMPTY;
        else if ( !valid_email(email) )                 /* invalid email format */
            return ERR_EMAIL_FORMAT;
#else
        if ( strlen(login) < MIN_USERNAME_LEN )         /* user name too short */
            return ERR_USERNAME_TOO_SHORT;
        else if ( !valid_username(login) )              /* only certain chars are allowed in user name */
            return ERR_USERNAME_CHARS;
        else if ( OK != (ret=user_exists(login)) )      /* user name taken */
            return ret;
        else if ( email[0] && !valid_email(email) )     /* invalid email format */
            return ERR_EMAIL_FORMAT_OR_EMPTY;
#endif  /* USERSBYEMAIL */

    if ( email[0] && OK != (ret=email_exists(email)) )  /* email in use */
        return ret;
    else if ( (ret=silgy_usr_password_quality(passwd)) != OK )
        return ret;
    else if ( 0 != strcmp(passwd, rpasswd) )            /* passwords differ */
        return ERR_PASSWORD_DIFFERENT;

    /* welcome! -- and generate password hashes ------------------------------------------------------- */

    get_hashes(str1, str2, login, email, passwd);

    strcpy(login_u, upper(login));
    strcpy(email_u, upper(email));

    sprintf(sql, "INSERT INTO users (id,login,login_u,email,email_u,name,phone,passwd1,passwd2,lang,about,auth_level,status,created,visits,ula_cnt) VALUES (0,'%s','%s','%s','%s','%s','%s','%s','%s','%s','%s',%d,%d,'%s',0,0)", login, login_u, email, email_u, name, phone, str1, str2, lang, about, auth_level, status, DT_NOW);

    DBG("sql: INSERT INTO users (id,login,email,name,phone,...) VALUES (0,'%s','%s','%s','%s',...)", login, email, name, phone);

    if ( mysql_query(G_dbconn, sql) )
    {
        ERR("%u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
        return ERR_INT_SERVER_ERROR;
    }

    G_new_user_id = mysql_insert_id(G_dbconn);

    if ( current_session )
        UID = G_new_user_id;

    if ( G_usersRequireAccountActivation )
    {
        if ( (ret=send_activation_link(ci, G_new_user_id, email)) != OK )
            return ret;
    }

#ifdef USERSBYEMAIL
    INF("User [%s] created from %s", email, US.ip);
#else
    INF("User [%s] created from %s", login, US.ip);
#endif

    return OK;

}


/* --------------------------------------------------------------------------
   Create user account (wrapper)
-------------------------------------------------------------------------- */
int silgy_usr_create_account(int ci)
{
    DBG("silgy_usr_create_account");

    char status;

    if ( G_usersRequireAccountActivation )
        status = USER_STATUS_INACTIVE;
    else
        status = USER_STATUS_ACTIVE;

    return create_account(ci, DEF_USER_AUTH_LEVEL, status, TRUE);
}


/* --------------------------------------------------------------------------
   Send an email about new account
-------------------------------------------------------------------------- */
static int new_account_notification(int ci, const char *login, const char *email, const char *name, const char *passwd, char status)
{
    char subject[256];
    char message[4096];

    OUTP_BEGIN(message);

    OUTP("Dear %s,\n\n", silgy_usr_name(login, email, name, 0));
    OUTP("An account has been created for you at %s.\n\n", conn[ci].website);
    OUTP("Please visit this address to log in:\n\n");
#ifdef HTTPS
    if ( G_test )
        OUTP("http://%s/%s\n\n", conn[ci].host, APP_LOGIN_URI);
    else
        OUTP("https://%s/%s\n\n", conn[ci].host, APP_LOGIN_URI);
#else
    OUTP("http://%s/%s\n\n", conn[ci].host, APP_LOGIN_URI);
#endif
    if ( status == USER_STATUS_PASSWORD_CHANGE )
        OUTP("Your password is %s and you will have to change it on your first login.\n\n", passwd[0]?passwd:"empty");
#ifdef APP_CONTACT_EMAIL
    OUTP("In case you needed any help, please contact us at %s.\n\n", APP_CONTACT_EMAIL);
#endif
    OUTP("Kind Regards\n");
    OUTP("%s\n", conn[ci].website);

    OUTP_END;

    sprintf(subject, "Welcome to %s", conn[ci].website);

    if ( !silgy_email(email, subject, message) )
        return ERR_INT_SERVER_ERROR;

    return OK;
}


/* --------------------------------------------------------------------------
   Create user account
-------------------------------------------------------------------------- */
int silgy_usr_add_user(int ci, bool use_qs, const char *login, const char *email, const char *name, const char *passwd, const char *phone, const char *lang, const char *about, char group_id, char auth_level, char status)
{
    int   ret=OK;
    QSVAL dst_login;
    QSVAL password;

    DBG("silgy_usr_add_user");

    if ( use_qs )   /* use query string / POST payload */
    {
//        if ( (ret=create_account(ci, auth_level, USER_STATUS_PASSWORD_CHANGE, FALSE)) != OK )
        if ( (ret=create_account(ci, auth_level, status, FALSE)) != OK )
        {
            ERR("create_account failed");
            return ret;
        }

        QS_HTML_ESCAPE("passwd", password);
    }
    else    /* use function arguments */
    {
#ifdef USERSBYEMAIL
        if ( !email || !email[0] )    /* email empty */
        {
            ERR("Invalid request (email missing)");
            return ERR_EMAIL_EMPTY;
        }
#endif  /* USERSBYEMAIL */

        if ( !login || !login[0] )    /* login empty */
        {
#ifdef USERSBYEMAIL
            COPY(dst_login, email, LOGIN_LEN);
#else   /* USERSBYLOGIN */
            if ( email[0] )
                COPY(dst_login, email, LOGIN_LEN);
            else
            {
                ERR("Either login or email is required");
                return ERR_INVALID_REQUEST;
            }
#endif
        }
        else    /* login supplied */
        {
            COPY(dst_login, login, LOGIN_LEN);
        }

        /* --------------------------------------------------------------- */

        if ( passwd )   /* use the one supplied */
            strcpy(password, passwd);
        else    /* generate */
            silgy_random(password, MIN_PASSWORD_LEN);

        /* --------------------------------------------------------------- */

#ifdef USERSBYEMAIL
        if ( !email[0] )                                /* email empty */
            return ERR_EMAIL_EMPTY;
        else if ( !valid_email(email) )                 /* invalid email format */
            return ERR_EMAIL_FORMAT;
#else   /* USERSBYLOGIN */
        if ( strlen(dst_login) < MIN_USERNAME_LEN )     /* user name too short */
            return ERR_USERNAME_TOO_SHORT;
        else if ( !valid_username(dst_login) )          /* only certain chars are allowed in user name */
            return ERR_USERNAME_CHARS;
        else if ( OK != (ret=user_exists(dst_login)) )  /* user name taken */
            return ret;
        else if ( email[0] && !valid_email(email) )     /* invalid email format */
            return ERR_EMAIL_FORMAT_OR_EMPTY;
#endif  /* USERSBYEMAIL */

        if ( email[0] && OK != (ret=email_exists(email)) )  /* email in use */
            return ret;
        else if ( (ret=silgy_usr_password_quality(password)) != OK )
            return ret;

        /* --------------------------------------------------------------- */

        char str1[32], str2[32];

        get_hashes(str1, str2, dst_login, email, password);

        /* --------------------------------------------------------------- */

        QSVAL login_u;
        QSVAL email_u;
        char sql[SQLBUF];

        strcpy(login_u, upper(dst_login));
        strcpy(email_u, upper(email));

        sprintf(sql, "INSERT INTO users (id,login,login_u,email,email_u,name,phone,passwd1,passwd2,lang,about,group_id,auth_level,status,created,visits,ula_cnt) VALUES (0,'%s','%s','%s','%s','%s','%s','%s','%s','%s','%s',%d,%d,%d,'%s',0,0)", dst_login, login_u, email, email_u, name?name:"", phone?phone:"", str1, str2, lang?lang:"", about?about:"", group_id, auth_level, status, DT_NOW);

        DBG("sql: INSERT INTO users (id,login,email,name,phone,...) VALUES (0,'%s','%s','%s','%s',...)", dst_login, email, name?name:"", phone?phone:"");

        if ( mysql_query(G_dbconn, sql) )
        {
            ERR("%u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
            return ERR_INT_SERVER_ERROR;
        }

        G_new_user_id = mysql_insert_id(G_dbconn);

        /* --------------------------------------------------------------- */

#ifdef USERSBYEMAIL
        INF("User [%s] created", email);
#else
        INF("User [%s] created", dst_login);
#endif
    }

#ifndef DONT_NOTIFY_NEW_USER
    QSVAL email_;
    if ( use_qs )
        QS("email", email_);
    else
        strcpy(email_, email);

    if ( email_[0] )
        new_account_notification(ci, dst_login, email_, name, password, status);
#endif

    return ret;
}


/* --------------------------------------------------------------------------
   Save user message
-------------------------------------------------------------------------- */
int silgy_usr_send_message(int ci)
{
    QSVAL_TEXT message;

    DBG("silgy_usr_send_message");

    if ( !QS_TEXT_DONT_ESCAPE("msg_box", message) )
    {
        WAR("Invalid request (URI val missing?)");
        return ERR_INVALID_REQUEST;
    }

    QSVAL   email;
static char sanmessage[MAX_LONG_URI_VAL_LEN+1];
static char sql[MAX_LONG_URI_VAL_LEN*2];

    if ( QS_HTML_ESCAPE("email", email) )
        stp_right(email);

    sprintf(sanmessage, "From %s\n\n", conn[ci].ip);
    COPY(sanmessage+strlen(sanmessage), silgy_sql_esc(message), MAX_LONG_URI_VAL_LEN);

    sprintf(sql, "INSERT INTO users_messages (user_id,msg_id,email,message,created) VALUES (%d,%d,'%s','%s','%s')", UID, get_max(ci, "messages")+1, email, sanmessage, DT_NOW);
    DBG("sql: INSERT INTO users_messages (user_id,msg_id,email,...) VALUES (%d,get_max(),'%s',...)", UID, email);

    if ( mysql_query(G_dbconn, sql) )
    {
        ERR("%u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
        return ERR_INT_SERVER_ERROR;
    }

    /* email admin */

#ifdef APP_CONTACT_EMAIL
    silgy_email(APP_CONTACT_EMAIL, "New message!", sanmessage);
#endif

    return OK;
}


/* --------------------------------------------------------------------------
   Save changes to user account
-------------------------------------------------------------------------- */
int silgy_usr_save_account(int ci)
{
    int         ret=OK;
    QSVAL       tmp;
    char        login[LOGIN_LEN+1];
    char        email[EMAIL_LEN+1];
    char        name[UNAME_LEN+1];
    char        phone[PHONE_LEN+1];
    QSVAL       lang;
    char        about[ABOUT_LEN+1];
    QSVAL       passwd;
    QSVAL       rpasswd;
    QSVAL       opasswd;
    QSVAL       uemail_old;
    QSVAL       uemail_new;
    QSVAL       strdelete;
    QSVAL       strdelconf;
    QSVAL       save;
    int         plen;
    char        sql[SQLBUF];
    char        str1[32], str2[32];
    MYSQL_RES   *result;
    MYSQL_ROW   row;

    DBG("silgy_usr_save_account");

    if ( !QS_HTML_ESCAPE("opasswd", opasswd) )
    {
        WAR("Invalid request (opasswd missing)");
        return ERR_INVALID_REQUEST;
    }

    if ( QS_HTML_ESCAPE("login", tmp) )
    {
        COPY(login, tmp, LOGIN_LEN);
        stp_right(login);
    }
    else
        login[0] = EOS;

    if ( QS_HTML_ESCAPE("email", tmp) )
    {
        COPY(email, tmp, EMAIL_LEN);
        stp_right(email);
    }
    else
        email[0] = EOS;

#ifdef USERSBYEMAIL
    if ( !email[0] )    /* email empty */
    {
        ERR("Invalid request (email missing)");
        return ERR_EMAIL_EMPTY;
    }
#else
    if ( !login[0] )    /* login empty */
    {
        ERR("Invalid request (login missing)");
        return ERR_INVALID_REQUEST;
    }
#endif  /* USERSBYEMAIL */

    /* optional */

    if ( !QS_HTML_ESCAPE("passwd", passwd) )
        passwd[0] = EOS;

    if ( !QS_HTML_ESCAPE("rpasswd", rpasswd) )
        rpasswd[0] = EOS;

    if ( QS_HTML_ESCAPE("name", tmp) )
    {
        COPY(name, tmp, UNAME_LEN);
        stp_right(name);
    }
    else
        name[0] = EOS;

    if ( QS_HTML_ESCAPE("phone", tmp) )
    {
        COPY(phone, tmp, PHONE_LEN);
        stp_right(phone);
    }
    else
        phone[0] = EOS;

    if ( QS_HTML_ESCAPE("lang", lang) )
        stp_right(lang);
    else
        lang[0] = EOS;

    if ( QS_HTML_ESCAPE("about", tmp) )
    {
        COPY(about, tmp, ABOUT_LEN);
        stp_right(about);
    }
    else
        about[0] = EOS;

    /* remember form fields */
    /* US.email contains old email */

    usession_t us_new;

    strcpy(us_new.login, login);
    strcpy(us_new.email, email);
    strcpy(us_new.name, name);
    strcpy(us_new.phone, phone);

    strncpy(us_new.lang, lang, LANG_LEN);
    us_new.lang[LANG_LEN] = EOS;
    strcpy(us_new.about, about);

    /* basic validation */

    plen = strlen(passwd);

#ifdef USERSBYEMAIL
    if ( !email[0] )
        return ERR_EMAIL_EMPTY;
    else if ( !valid_email(email) )
        return ERR_EMAIL_FORMAT;
#else
    if ( email[0] && !valid_email(email) )
        return ERR_EMAIL_FORMAT_OR_EMPTY;
#endif  /* USERSBYEMAIL */
    else if ( plen && (ret=silgy_usr_password_quality(passwd)) != OK )
        return ret;
    else if ( plen && 0 != strcmp(passwd, rpasswd) )
        return ERR_PASSWORD_DIFFERENT;

    /* if email change, check if the new one has not already been registered */

    strcpy(uemail_old, upper(US.email));
    strcpy(uemail_new, upper(email));

    if ( uemail_new[0] && strcmp(uemail_old, uemail_new) != 0 && (ret=silgy_usr_email_registered(ci)) != OK )
        return ret;

    /* verify existing password against login/email/passwd1 */

#ifdef USERSBYEMAIL
    doit(str1, str2, email, email, opasswd);
    sprintf(sql, "SELECT passwd1 FROM users WHERE email_u='%s'", upper(email));
#else
    doit(str1, str2, login, login, opasswd);
    sprintf(sql, "SELECT passwd1 FROM users WHERE login_u='%s'", upper(login));
#endif  /* USERSBYEMAIL */
    DBG("sql: %s", sql);

    mysql_query(G_dbconn, sql);

    result = mysql_store_result(G_dbconn);

    if ( !result )
    {
        ERR("%u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
        return ERR_INT_SERVER_ERROR;
    }

    row = mysql_fetch_row(result);

    if ( !row )
    {
        ERR("Weird: no such user");
        mysql_free_result(result);
        return ERR_INT_SERVER_ERROR;
    }

    if ( 0 != strcmp(str1, row[0]) )
    {
        ERR("Invalid old password");
        mysql_free_result(result);
        return ERR_OLD_PASSWORD;
    }

    mysql_free_result(result);

    /* Old password OK ---------------------------------------- */

    DBG("Old password OK");

    if ( QS_HTML_ESCAPE("delete", strdelete) && 0==strcmp(strdelete, "on") )    /* delete user account */
    {
        if ( !QS_HTML_ESCAPE("delconf", strdelconf) || 0 != strcmp(strdelconf, "1") )
            return WAR_BEFORE_DELETE;
        else
        {
            sprintf(sql, "UPDATE users SET status=%d WHERE id=%d", USER_STATUS_DELETED, UID);
            DBG("sql: %s", sql);
            if ( mysql_query(G_dbconn, sql) )
            {
                ERR("%u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
                return ERR_INT_SERVER_ERROR;
            }

            libusr_luses_downgrade(conn[ci].usi, ci, TRUE);   /* log user out */

            return MSG_ACCOUNT_DELETED;
        }
    }

    /* anything else than deleting account -- changing email and/or name and/or password */

    get_hashes(str1, str2, login, email, plen?passwd:opasswd);

    sprintf(sql, "UPDATE users SET login='%s', email='%s', email_u='%s', name='%s', phone='%s', passwd1='%s', passwd2='%s', lang='%s', about='%s' WHERE id=%d", login, email, upper(email), name, phone, str1, str2, lang, about, UID);
    DBG("sql: UPDATE users SET login='%s', email='%s', name='%s', phone='%s',... WHERE id=%d", login, email, name, phone, UID);

    if ( mysql_query(G_dbconn, sql) )
    {
        ERR("%u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
        return ERR_INT_SERVER_ERROR;
    }

    DBG("Updating login, email, name, phone & about in user session");

    strcpy(US.login, us_new.login);
    strcpy(US.email, us_new.email);
    strcpy(US.name, us_new.name);
    strcpy(US.phone, us_new.phone);
    strcpy(US.lang, us_new.lang);
    strcpy(US.about, us_new.about);

    /* On password change invalidate all existing sessions except of the current one */

    if ( passwd[0] && 0!=strcmp(passwd, opasswd) )
    {
        DBG("Password change => invalidating all other session tokens");

        sprintf(sql, "DELETE FROM users_logins WHERE user_id = %d AND sesid != BINARY '%s'", UID, US.sesid);
        DBG("sql: %s", sql);
        if ( mysql_query(G_dbconn, sql) )
        {
            ERR("%u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
            return ERR_INT_SERVER_ERROR;
        }

        /* downgrade all currently active sessions belonging to this user */
        /* except of the current one */

        eng_uses_downgrade_by_uid(UID, ci);
    }

    return OK;
}


/* --------------------------------------------------------------------------
   Email taken?
-------------------------------------------------------------------------- */
int silgy_usr_email_registered(int ci)
{
    QSVAL email;

    DBG("silgy_usr_email_registered");

    if ( !QS_HTML_ESCAPE("email", email) )
    {
        WAR("Invalid request (URI val missing?)");
        return ERR_INVALID_REQUEST;
    }

    stp_right(email);

    return email_exists(email);
}


/* --------------------------------------------------------------------------
   Return the best version of the user name for "Dear ..."
-------------------------------------------------------------------------- */
char *silgy_usr_name(const char *login, const char *email, const char *name, int uid)
{
static char dest[128];

    DBG("silgy_usr_name");

    if ( name && name[0] )
    {
        strcpy(dest, name);
    }
    else if ( login && login[0] )
    {
        strcpy(dest, login);
    }
    else if ( email && email[0] )
    {
        int i=0;
        while ( email[i]!='@' && email[i] && i<100 ) dest[i++] = email[i];
        dest[i] = EOS;
    }
    else if ( uid )
    {
        char            sql[SQLBUF];
        MYSQL_RES       *result;
        MYSQL_ROW       row;

        sprintf(sql, "SELECT login, email, name FROM users WHERE id=%d", uid);
        DBG("sql: %s", sql);
        mysql_query(G_dbconn, sql);
        result = mysql_store_result(G_dbconn);
        if ( !result )
        {
            ERR("%u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
            strcpy(dest, "User");
        }
        else    /* OK */
        {
            row = mysql_fetch_row(result);

            if ( !row )
            {
                mysql_free_result(result);
                strcpy(dest, "User");
            }
            else
            {
                char db_login[128];
                char db_email[128];
                char db_name[128];

                strcpy(db_login, row[0]?row[0]:"");
                strcpy(db_email, row[1]?row[1]:"");
                strcpy(db_name, row[2]?row[2]:"");

                mysql_free_result(result);

                /* we can't use recursion with char * function... */
                if ( db_name && db_name[0] )
                    strcpy(dest, db_name);
                else if ( db_login && db_login[0] )
                    strcpy(dest, db_login);
                else if ( db_email && db_email[0] )
                {
                    int i=0;
                    while ( db_email[i]!='@' && db_email[i] && i<100 ) dest[i++] = db_email[i];
                    dest[i] = EOS;
                }
                else
                    strcpy(dest, "User");
            }
        }
    }
    else
    {
        strcpy(dest, "User");
    }

    return dest;
}


/* --------------------------------------------------------------------------
   Send an email with password reset link
-------------------------------------------------------------------------- */
int silgy_usr_send_passwd_reset_email(int ci)
{
    QSVAL       email;
    QSVAL       submit;
    char        sql[SQLBUF];
    MYSQL_RES   *result;
    MYSQL_ROW   row;

    DBG("silgy_usr_send_passwd_reset_email");

    if ( !QS_HTML_ESCAPE("email", email) )
    {
        WAR("Invalid request (URI val missing?)");
        return ERR_INVALID_REQUEST;
    }

    stp_right(email);

    if ( !valid_email(email) )      /* invalid email format */
        return ERR_EMAIL_FORMAT;

    sprintf(sql, "SELECT id, login, name, status FROM users WHERE email_u='%s'", upper(email));
    DBG("sql: %s", sql);
    mysql_query(G_dbconn, sql);
    result = mysql_store_result(G_dbconn);
    if ( !result )
    {
        ERR("%u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
        return ERR_INT_SERVER_ERROR;
    }

    row = mysql_fetch_row(result);

    if ( !row )
    {
        mysql_free_result(result);
        WAR("Password reset link requested for non-existent [%s]", email);
        return OK;
    }

    /* -------------------------------------------------------------------------- */

    if ( atoi(row[3]) == USER_STATUS_DELETED )
    {
        mysql_free_result(result);
        WAR("Password reset link requested for [%s] but user is deleted", email);
        return OK;
    }

    /* -------------------------------------------------------------------------- */

    int uid;
    char login[128];
    char name[128];

    uid = atoi(row[0]);
    strcpy(login, row[1]?row[1]:"");
    strcpy(name, row[2]?row[2]:"");

    mysql_free_result(result);

    /* -------------------------------------------------------------------------- */
    /* generate a key */

    char linkkey[PASSWD_RESET_KEY_LEN+1];

    silgy_random(linkkey, PASSWD_RESET_KEY_LEN);

    sprintf(sql, "INSERT INTO users_p_resets (linkkey,user_id,created,tries) VALUES ('%s',%d,'%s',0)", linkkey, uid, DT_NOW);
    DBG("sql: %s", sql);

    if ( mysql_query(G_dbconn, sql) )
    {
        ERR("%u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
        return ERR_INT_SERVER_ERROR;
    }

    /* -------------------------------------------------------------------------- */
    /* send an email */

    char subject[256];
    char message[4096];

    OUTP_BEGIN(message);

    OUTP("Dear %s,\n\n", silgy_usr_name(login, email, name, 0));
    OUTP("You have requested to have your password reset for your account at %s. Please visit this URL to reset your password:\n\n", conn[ci].website);
#ifdef HTTPS
    if ( G_test )
        OUTP("http://%s/preset?k=%s\n\n", conn[ci].host, linkkey);
    else
        OUTP("https://%s/preset?k=%s\n\n", conn[ci].host, linkkey);
#else
    OUTP("http://%s/preset?k=%s\n\n", conn[ci].host, linkkey);
#endif  /* HTTPS */
    OUTP("Please keep in mind that this link will only be valid for the next 24 hours.\n\n");
    OUTP("If you did this by mistake or it wasn't you, you can safely ignore this email.\n\n");
#ifdef APP_CONTACT_EMAIL
    OUTP("In case you needed any help, please contact us at %s.\n\n", APP_CONTACT_EMAIL);
#endif
    OUTP("Kind Regards\n");
    OUTP("%s\n", conn[ci].website);

    OUTP_END;

    sprintf(subject, "%s Password Reset", conn[ci].website);

    if ( !silgy_email(email, subject, message) )
        return ERR_INT_SERVER_ERROR;

    /* -------------------------------------------------------------------------- */

    INF("Password reset link requested for [%s]", email);

    return OK;
}


/* --------------------------------------------------------------------------
   Verify the link key for password reset
-------------------------------------------------------------------------- */
int silgy_usr_verify_passwd_reset_key(int ci, char *linkkey, int *uid)
{
    char        sql[SQLBUF];
    MYSQL_RES   *result;
    MYSQL_ROW   row;
    char        esc_linkkey[256];
    int         tries;

    DBG("silgy_usr_verify_passwd_reset_key");

    if ( strlen(linkkey) != PASSWD_RESET_KEY_LEN )
        return ERR_LINK_BROKEN;

    strcpy(esc_linkkey, silgy_sql_esc(linkkey));

    sprintf(sql, "SELECT user_id, created, tries FROM users_p_resets WHERE linkkey = BINARY '%s'", esc_linkkey);
    DBG("sql: %s", sql);

    mysql_query(G_dbconn, sql);

    result = mysql_store_result(G_dbconn);

    if ( !result )
    {
        ERR("%u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
        return ERR_INT_SERVER_ERROR;
    }

    row = mysql_fetch_row(result);

    if ( !row )     /* no records with this key in users_p_resets -- link broken? */
    {
        mysql_free_result(result);
        return ERR_LINK_MAY_BE_EXPIRED;
    }

    /* validate expiry time */

    if ( db2epoch(row[1]) < G_now-3600*24 )  /* older than 24 hours? */
    {
        WAR("Key created more than 24 hours ago");
        mysql_free_result(result);
        return ERR_LINK_MAY_BE_EXPIRED;
    }

    /* validate tries */

    tries = atoi(row[2]);

    if ( tries > 12 )
    {
        WAR("Key tried more than 12 times");
        mysql_free_result(result);
        return ERR_LINK_TOO_MANY_TRIES;
    }

    /* otherwise everything's OK ----------------------------------------- */

    /* get the user id */

    *uid = atoi(row[0]);

    mysql_free_result(result);

    DBG("Key ok, uid = %d", *uid);

    /* update tries counter */

    sprintf(sql, "UPDATE users_p_resets SET tries=%d WHERE linkkey = BINARY '%s'", tries+1, esc_linkkey);
    DBG("sql: %s", sql);
    if ( mysql_query(G_dbconn, sql) )
    {
        ERR("%u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
        return ERR_INT_SERVER_ERROR;
    }

    return OK;
}


/* --------------------------------------------------------------------------
   Activate user account
-------------------------------------------------------------------------- */
int silgy_usr_activate(int ci)
{
    int         ret;
    QSVAL       linkkey;
    int         uid;
    char        sql[SQLBUF];

    DBG("silgy_usr_activate");

    if ( !QS_HTML_ESCAPE("k", linkkey) )
    {
        WAR("Invalid request (URI val missing?)");
        return ERR_INVALID_REQUEST;
    }

    /* verify the key */

	if ( (ret=silgy_usr_verify_activation_key(ci, linkkey, &uid)) != OK )
		return ret;

    /* everything's OK -- activate user -------------------- */

    UID = uid;

    sprintf(sql, "UPDATE users SET status=%d WHERE id=%d", USER_STATUS_ACTIVE, uid);
    DBG("sql: %s", sql);
    if ( mysql_query(G_dbconn, sql) )
    {
        ERR("%u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
        return ERR_INT_SERVER_ERROR;
    }

    /* remove activation link */

    sprintf(sql, "UPDATE users_activations SET activated='%s' WHERE linkkey = BINARY '%s'", DT_NOW, linkkey);
    DBG("sql: %s", sql);
    if ( mysql_query(G_dbconn, sql) )
    {
        ERR("%u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
//        return ERR_INT_SERVER_ERROR;  ignore it
    }

    return OK;
}


/* --------------------------------------------------------------------------
   Save user's avatar
-------------------------------------------------------------------------- */
int silgy_usr_save_avatar(int ci, int uid)
{
    QSVAL   name;
    QSVAL   name_filtered;
    char    *img_raw;                       /* raw image data */
static char img_esc[MAX_AVATAR_SIZE*2];     /* image data escaped */
static char sql[MAX_AVATAR_SIZE*2+1024];
    unsigned len;

    DBG("silgy_usr_save_avatar");

    img_raw = get_qs_param_multipart(ci, "data", &len, name);

    if ( !img_raw || len < 1 )
    {
        WAR("data is required");
        return ERR_INVALID_REQUEST;
    }

    strcpy(name_filtered, silgy_filter_strict(name));

    DBG("Image file name [%s], size = %d", name_filtered, len);

    if ( len > MAX_AVATAR_SIZE )
    {
        WAR("This file is too big");
        return ERR_FILE_TOO_BIG;
    }
    else
    {
        mysql_real_escape_string(G_dbconn, img_esc, img_raw, len);

        sprintf(sql, "UPDATE users SET avatar_name='%s', avatar_data='%s' WHERE id=%d", name_filtered, img_esc, uid);

        DBG("UPDATE users SET avatar_name='%s', avatar_data=<binary> WHERE id=%d", name_filtered, uid);

        if ( mysql_query(G_dbconn, sql) )
        {
            ERR("%u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
            return ERR_INT_SERVER_ERROR;
        }
    }

    return OK;
}


/* --------------------------------------------------------------------------
   Get user's avatar
-------------------------------------------------------------------------- */
int silgy_usr_get_avatar(int ci, int uid)
{
    char        sql[SQLBUF];
    MYSQL_RES   *result;
    MYSQL_ROW   row;
    unsigned long *lengths;

    DBG("silgy_usr_get_avatar");

    sprintf(sql, "SELECT avatar_name, avatar_data FROM users WHERE id=%d", uid);
    DBG("sql: %s", sql);
    mysql_query(G_dbconn, sql);
    result = mysql_store_result(G_dbconn);
    if ( !result )
    {
        ERR("%u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
        return ERR_INT_SERVER_ERROR;
    }

    row = mysql_fetch_row(result);

    if ( !row )
    {
        mysql_free_result(result);
        WAR("User not found");
        return ERR_NOT_FOUND;
    }

    lengths = mysql_fetch_lengths(result);

    OUT_BIN(row[1], lengths[1]);

    conn[ci].ctype = get_res_type(row[0]);

    DBG("File: [%s], size = %ul", row[0], lengths[1]);

    mysql_free_result(result);

    return OK;
}


/* --------------------------------------------------------------------------
   Change logged in user password and change its status to USER_STATUS_ACTIVE
   Used in case user is forced to change their password
   (status == USER_STATUS_PASSWORD_CHANGE)
-------------------------------------------------------------------------- */
int silgy_usr_change_password(int ci)
{
    int         ret;
    QSVAL       opasswd;
    QSVAL       passwd;
    QSVAL       rpasswd;
    QSVAL       submit;
    int         uid;
    char        sql[SQLBUF];
    char        str1[32], str2[32];
    MYSQL_RES   *result;
    MYSQL_ROW   row;

    DBG("silgy_usr_change_password");

    if ( !QS_HTML_ESCAPE("opasswd", opasswd)
            || !QS_HTML_ESCAPE("passwd", passwd)
            || !QS_HTML_ESCAPE("rpasswd", rpasswd) )
    {
        WAR("Invalid request (URI val missing?)");
        return ERR_INVALID_REQUEST;
    }

    /* verify existing password against login/email/passwd1 */

#ifdef USERSBYEMAIL
    doit(str1, str2, US.email, US.email, opasswd);
    sprintf(sql, "SELECT passwd1 FROM users WHERE email_u='%s'", upper(US.email));
#else
    doit(str1, str2, US.login, US.login, opasswd);
    sprintf(sql, "SELECT passwd1 FROM users WHERE login_u='%s'", upper(US.login));
#endif  /* USERSBYEMAIL */
    DBG("sql: %s", sql);

    mysql_query(G_dbconn, sql);

    result = mysql_store_result(G_dbconn);

    if ( !result )
    {
        ERR("%u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
        return ERR_INT_SERVER_ERROR;
    }

    row = mysql_fetch_row(result);

    if ( !row )
    {
        ERR("Weird: no such user");
        mysql_free_result(result);
        return ERR_INT_SERVER_ERROR;
    }

    if ( 0 != strcmp(str1, row[0]) )
    {
        ERR("Invalid old password");
        mysql_free_result(result);
        return ERR_OLD_PASSWORD;
    }

    mysql_free_result(result);

    /* Old password OK ---------------------------------------- */

    DBG("Old password OK");

    /* new password validation */

    int plen = strlen(passwd);

    if ( (ret=silgy_usr_password_quality(passwd)) != OK )
        return ret;
    else if ( 0 != strcmp(passwd, rpasswd) )   /* passwords differ */
        return ERR_PASSWORD_DIFFERENT;

    DBG("New password OK");

    /* everything's OK -- update password -------------------------------- */

    get_hashes(str1, str2, US.login, US.email, passwd);

    DBG("Updating users...");

    sprintf(sql, "UPDATE users SET passwd1='%s', passwd2='%s', status=%d WHERE id=%d", str1, str2, USER_STATUS_ACTIVE, UID);
    DBG("sql: UPDATE users SET passwd1=...");
    if ( mysql_query(G_dbconn, sql) )
    {
        ERR("%u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
        return ERR_INT_SERVER_ERROR;
    }

    return OK;
}


/* --------------------------------------------------------------------------
   Save new password after reset
-------------------------------------------------------------------------- */
int silgy_usr_reset_password(int ci)
{
    int         ret;
    QSVAL       email;
    QSVAL       linkkey;
    QSVAL       passwd;
    QSVAL       rpasswd;
    QSVAL       submit;
    int         uid;
    char        sql[SQLBUF];
    char        str1[32], str2[32];
    MYSQL_RES   *result;
    MYSQL_ROW   row;

    DBG("silgy_usr_reset_password");

    if ( !QS_HTML_ESCAPE("email", email)
            || !QS_HTML_ESCAPE("k", linkkey)
            || !QS_HTML_ESCAPE("passwd", passwd)
            || !QS_HTML_ESCAPE("rpasswd", rpasswd) )
    {
        WAR("Invalid request (URI val missing?)");
        return ERR_INVALID_REQUEST;
    }

    stp_right(email);

    /* general validation */

    int plen = strlen(passwd);

    if ( !valid_email(email) )
        return ERR_EMAIL_FORMAT;
    else if ( (ret=silgy_usr_password_quality(passwd)) != OK )
        return ret;
    else if ( 0 != strcmp(passwd, rpasswd) )    /* passwords differ */
        return ERR_PASSWORD_DIFFERENT;

    /* verify the key */

	if ( (ret=silgy_usr_verify_passwd_reset_key(ci, linkkey, &uid)) != OK )
		return ret;

    /* verify that emails match each other */

    sprintf(sql, "SELECT login, email FROM users WHERE id=%d", uid);
    DBG("sql: %s", sql);
    mysql_query(G_dbconn, sql);
    result = mysql_store_result(G_dbconn);
    if ( !result )
    {
        ERR("%u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
        return ERR_INT_SERVER_ERROR;
    }

    row = mysql_fetch_row(result);

    if ( !row )     /* password reset link expired or invalid email */
    {
        mysql_free_result(result);
        return ERR_LINK_EXPIRED;
    }

    if ( 0 != strcmp(row[1], email) )   /* emails different */
    {
        mysql_free_result(result);
        return ERR_LINK_EXPIRED;    /* password reset link expired or invalid email */
    }


    /* everything's OK -- update password -------------------------------- */

    get_hashes(str1, str2, row[0], email, passwd);

    mysql_free_result(result);

    DBG("Updating users...");

    sprintf(sql, "UPDATE users SET passwd1='%s', passwd2='%s' WHERE id=%d", str1, str2, uid);
    DBG("sql: UPDATE users SET passwd1=...");
    if ( mysql_query(G_dbconn, sql) )
    {
        ERR("%u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
        return ERR_INT_SERVER_ERROR;
    }

    /* Invalidate all existing sessions */

    DBG("Invalidating all session tokens...");

    sprintf(sql, "DELETE FROM users_logins WHERE user_id = %d", uid);
    DBG("sql: %s", sql);
    if ( mysql_query(G_dbconn, sql) )
    {
        ERR("%u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
        return ERR_INT_SERVER_ERROR;
    }

    /* downgrade all currently active sessions belonging to this user */

    eng_uses_downgrade_by_uid(uid, -1);

    /* remove all password reset keys */

    DBG("Deleting from users_p_resets...");

    sprintf(sql, "DELETE FROM users_p_resets WHERE user_id=%d", uid);
    DBG("sql: %s", sql);
    if ( mysql_query(G_dbconn, sql) )
    {
        ERR("%u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
//        return ERR_INT_SERVER_ERROR;  ignore it
    }

    return OK;
}


/* --------------------------------------------------------------------------
   Log user out
-------------------------------------------------------------------------- */
void silgy_usr_logout(int ci)
{
    DBG("silgy_usr_logout");
    libusr_luses_downgrade(conn[ci].usi, ci, TRUE);
}


/* --------------------------------------------------------------------------
   Generate password hashes
-------------------------------------------------------------------------- */
static void doit(char *result1, char *result2, const char *login, const char *email, const char *src)
{
    char    tmp[4096];
    unsigned char digest[SHA1_DIGEST_SIZE];
    int     i, j=0;

    sprintf(tmp, "%s%s%s%s", STR_001, upper(login), STR_002, src); /* login */
    libSHA1((unsigned char*)tmp, strlen(tmp), digest);
    Base64encode(tmp, (char*)digest, SHA1_DIGEST_SIZE);
    for ( i=0; tmp[i] != EOS; ++i ) /* drop non-alphanumeric characters */
    {
        if ( isalnum(tmp[i]) )
            result1[j++] = tmp[i];
    }
    result1[j] = EOS;

    j = 0;

    sprintf(tmp, "%s%s%s%s", STR_003, upper(email), STR_004, src); /* email */
    libSHA1((unsigned char*)tmp, strlen(tmp), digest);
    Base64encode(tmp, (char*)digest, SHA1_DIGEST_SIZE);
    for ( i=0; tmp[i] != EOS; ++i ) /* drop non-alphanumeric characters */
    {
        if ( isalnum(tmp[i]) )
            result2[j++] = tmp[i];
    }
    result2[j] = EOS;
}


/* --------------------------------------------------------------------------
   doit wrapper
-------------------------------------------------------------------------- */
static void get_hashes(char *result1, char *result2, const char *login, const char *email, const char *passwd)
{
#ifdef USERSBYLOGIN
    doit(result1, result2, login, email[0]?email:STR_005, passwd);
#else
    doit(result1, result2, email, email, passwd);
#endif
}


/* --------------------------------------------------------------------------
   Save user string setting
-------------------------------------------------------------------------- */
int silgy_usr_set_str(int ci, const char *us_key, const char *us_val)
{
    int  ret;
    char sql[SQLBUF];

    ret = silgy_usr_get_str(ci, us_key, NULL);

    if ( ret == ERR_NOT_FOUND )
    {
        sprintf(sql, "INSERT INTO users_settings (user_id,us_key,us_val) VALUES (%d,'%s','%s')", UID, us_key, us_val);

        DBG("sql: %s", sql);

        if ( mysql_query(G_dbconn, sql) )
        {
            ERR("%u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
            return ERR_INT_SERVER_ERROR;
        }
    }
    else if ( ret != OK )
    {
        ERR("%u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
        return ERR_INT_SERVER_ERROR;
    }
    else
    {
        sprintf(sql, "UPDATE users_settings SET us_val='%s' WHERE user_id=%d AND us_key='%s'", us_val, UID, us_key);

        DBG("sql: %s", sql);

        if ( mysql_query(G_dbconn, sql) )
        {
            ERR("%u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
            return ERR_INT_SERVER_ERROR;
        }
    }

    return OK;
}


/* --------------------------------------------------------------------------
   Read user string setting
-------------------------------------------------------------------------- */
int silgy_usr_get_str(int ci, const char *us_key, char *us_val)
{
    char        sql[SQLBUF];
    MYSQL_RES   *result;
    MYSQL_ROW   row;

    sprintf(sql, "SELECT us_val FROM users_settings WHERE user_id=%d AND us_key='%s'", UID, us_key);

    DBG("sql: %s", sql);

    mysql_query(G_dbconn, sql);

    result = mysql_store_result(G_dbconn);

    if ( !result )
    {
        ERR("%u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
        return ERR_INT_SERVER_ERROR;
    }

    row = mysql_fetch_row(result);

    if ( !row )
    {
        mysql_free_result(result);
        return ERR_NOT_FOUND;
    }

    if ( us_val )
        strcpy(us_val, row[0]);

    mysql_free_result(result);

    return OK;
}


/* --------------------------------------------------------------------------
   Save user number setting
-------------------------------------------------------------------------- */
int silgy_usr_set_int(int ci, const char *us_key, int us_val)
{
    char val[64];

    sprintf(val, "%d", us_val);
    return silgy_usr_set_str(ci, us_key, val);
}


/* --------------------------------------------------------------------------
   Read user number setting
-------------------------------------------------------------------------- */
int silgy_usr_get_int(int ci, const char *us_key, int *us_val)
{
    int  ret;
    char val[64];

    if ( (ret=silgy_usr_get_str(ci, us_key, val)) == OK )
        *us_val = atoi(val);

    return ret;
}


/* --------------------------------------------------------------------------
   Get MAX(msg_id) from users_messages for current user
-------------------------------------------------------------------------- */
static int get_max(int ci, const char *table)
{
    char        sql[SQLBUF];
    MYSQL_RES   *result;
    MYSQL_ROW   row;
    int         max=0;

    /* US.uid = 0 for anonymous session */

    if ( 0==strcmp(table, "messages") )
        sprintf(sql, "SELECT MAX(msg_id) FROM users_messages WHERE user_id=%d", UID);
    else
        return 0;

    DBG("sql: %s", sql);

    mysql_query(G_dbconn, sql);

    result = mysql_store_result(G_dbconn);

    if ( !result )
    {
        ERR("%u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
        return ERR_INT_SERVER_ERROR;
    }

    row = mysql_fetch_row(result);

    if ( row[0] )
        max = atoi(row[0]);

    mysql_free_result(result);

    DBG("get_max for uid=%d  max = %d", UID, max);

    return max;
}

#endif  /* USERS */
