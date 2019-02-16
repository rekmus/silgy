/* --------------------------------------------------------------------------
   Silgy Web App Engine
   Jurek Muszynski
   silgy.com
-----------------------------------------------------------------------------
   Logged in users' functions
-------------------------------------------------------------------------- */


#include <silgy.h>


#ifdef USERS

static bool valid_username(const char *login);
static bool valid_email(const char *email);
static bool start_new_luses(int ci, long uid, const char *login, const char *email, const char *name, const char *phone, const char *about, const char *sesid);
static int user_exists(const char *login);
static int email_exists(const char *email);
static int do_login(int ci, long uid, char *p_login, char *p_email, char *p_name, char *p_phone, char *p_about, long visits, const char *sesid);
static void doit(char *result1, char *result2, const char *usr, const char *email, const char *src);
static long get_max(int ci, const char *table);


/* --------------------------------------------------------------------------
   Return TRUE if user name contains only valid characters
-------------------------------------------------------------------------- */
static bool valid_username(const char *login)
{
    int i;

    for ( i=0; login[i] != EOS; ++i )
    {
        if ( !isalnum(login[i]) && login[i] != '.' && login[i] != '_' && login[i] != '-' && login[i] != '\'' )
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
   Start new logged in user session
-------------------------------------------------------------------------- */
static bool start_new_luses(int ci, long uid, const char *login, const char *email, const char *name, const char *phone, const char *about, const char *sesid)
{
    DBG("start_new_luses");

//    if ( !conn[ci].usi && !eng_uses_start(ci) )    /* no anonymous session -- try to start one */
//    {
//        return FALSE;
//    }

    DBG("Upgrading anonymous session to logged in, usi=%d, sesid [%s]", conn[ci].usi, sesid);
    strcpy(conn[ci].cookie_out_a, "x");     /* no longer needed */
    strcpy(conn[ci].cookie_out_a_exp, G_last_modified);     /* to be removed by browser */

    US.logged = TRUE;
    strcpy(US.sesid, sesid);
    strcpy(US.login, login);
    strcpy(US.email, email);
    strcpy(US.name, name);
    strcpy(US.phone, phone);
    strcpy(US.about, about);
    strcpy(US.login_tmp, login);
    strcpy(US.email_tmp, email);
    strcpy(US.name_tmp, name);
    strcpy(US.phone_tmp, phone);
    strcpy(US.about_tmp, about);
    US.uid = uid;

    return TRUE;
}


/* --------------------------------------------------------------------------
   Verify IP & User-Agent against uid and sesid in uses (logged in users)
   set user session array index (usi) if all ok
-------------------------------------------------------------------------- */
int libusr_l_usession_ok(int ci)
{
    int         i;
    char        sanuagent[DB_UAGENT_LEN+1];
    char        sql_query[SQLBUF];
    MYSQL_RES   *result;
    MYSQL_ROW   sql_row;
unsigned long   sql_records;
    long        uid;
    time_t      created;

    DBG("libusr_l_usession_ok");

    /* try in hot sessions first */

    for ( i=1; i<=MAX_SESSIONS; ++i )
    {
        if ( uses[i].sesid[0] && uses[i].logged && 0==strcmp(conn[ci].cookie_in_l, uses[i].sesid)
/*              && 0==strcmp(conn[ci].ip, uses[i].ip) */
                && 0==strcmp(conn[ci].uagent, uses[i].uagent) )
        {
            DBG("Logged in session found in cache, usi=%d, sesid [%s]", i, uses[i].sesid);
            conn[ci].usi = i;
            return OK;
        }
    }

    /* not found in memory -- try database */

    strncpy(sanuagent, silgy_sql_esc(conn[ci].uagent), DB_UAGENT_LEN);
    sanuagent[DB_UAGENT_LEN] = EOS;
    if ( sanuagent[DB_UAGENT_LEN-1]=='\'' && sanuagent[DB_UAGENT_LEN-2]!='\'' )
        sanuagent[DB_UAGENT_LEN-1] = EOS;

//    sprintf(sql_query, "SELECT user_id, created FROM users_logins WHERE sesid='%s' AND uagent='%s'", silgy_sql_esc(conn[ci].cookie_in_l), sanuagent);
    sprintf(sql_query, "SELECT uagent, user_id, created FROM users_logins WHERE sesid='%s'", silgy_sql_esc(conn[ci].cookie_in_l));
    DBG("sql_query: %s", sql_query);

    mysql_query(G_dbconn, sql_query);

    result = mysql_store_result(G_dbconn);

    if ( !result )
    {
        ERR("Error %u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
        return ERR_INT_SERVER_ERROR;
    }

    sql_records = mysql_num_rows(result);

    DBG("users_logins: %lu record(s) found", sql_records);

    if ( 0 == sql_records )     /* no such session in database */
    {
        mysql_free_result(result);
        DBG("No logged in session in database [%s]", conn[ci].cookie_in_l);
        strcpy(conn[ci].cookie_out_l, "x");
        strcpy(conn[ci].cookie_out_l_exp, G_last_modified);     /* expire ls cookie */
        return ERR_SESSION_EXPIRED;
    }

    /* we've got some user login cookie remembered */

    sql_row = mysql_fetch_row(result);

    /* verify uagent */

    if ( 0 != strcmp(sanuagent, sql_row[0]) )
    {
        mysql_free_result(result);
        DBG("Different uagent in database for sesid [%s]", conn[ci].cookie_in_l);
        strcpy(conn[ci].cookie_out_l, "x");
        strcpy(conn[ci].cookie_out_l_exp, G_last_modified);     /* expire ls cookie */
        return ERR_SESSION_EXPIRED;
    }

    /* -------------------------------------- */

    uid = atol(sql_row[1]);

    /* Verify time. If created more than 30 days ago -- refuse */

    created = db2epoch(sql_row[2]);

    if ( created < G_now - 3600*24*30 )
    {
        DBG("Removing old logged in session, usi=%d, sesid [%s], created %s from database", conn[ci].usi, conn[ci].cookie_in_l, sql_row[2]);

        mysql_free_result(result);

        sprintf(sql_query, "DELETE FROM users_logins WHERE sesid='%s'", conn[ci].cookie_in_l);
        DBG("sql_query: %s", sql_query);

        if ( mysql_query(G_dbconn, sql_query) )
        {
            ERR("Error %u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
            return ERR_INT_SERVER_ERROR;
        }

        /* tell browser we're logging out */

        strcpy(conn[ci].cookie_out_l, "x");
        strcpy(conn[ci].cookie_out_l_exp, G_last_modified);     /* expire ls cookie */

        return ERR_SESSION_EXPIRED;
    }

    mysql_free_result(result);

    /* cookie has not expired -- log user in */

    DBG("Logged in session found in database");

    /* start a fresh session */

    if ( !eng_uses_start(ci) )
    {
        return ERR_SERVER_TOOBUSY;
    }
    else    /* keep the old sesid */
    {
        strcpy(US.sesid, conn[ci].cookie_in_l);
    }

    sprintf(sql_query, "UPDATE users_logins SET last_used='%s' WHERE sesid='%s'", G_dt, US.sesid);
    DBG("sql_query: %s", sql_query);
    if ( mysql_query(G_dbconn, sql_query) )
    {
        ERR("Error %u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
        return ERR_INT_SERVER_ERROR;
    }

    return do_login(ci, uid, NULL, NULL, NULL, NULL, NULL, 0, US.sesid);
}


/* --------------------------------------------------------------------------
   Close timeouted logged in user sessions
-------------------------------------------------------------------------- */
void libusr_close_luses_timeout()
{
    int     i;
    time_t  last_allowed;

    last_allowed = G_now - LUSES_TIMEOUT;

    for (i=1; i<=G_sessions; ++i)
    {
        if ( uses[i].logged && uses[i].last_activity < last_allowed )
            libusr_close_l_uses(-1, i);
    }
}


/* --------------------------------------------------------------------------
   Close  / downgrade logged in user session
   If ci != -1 it was on user demand
   otherwise it's just memory housekeeping
-------------------------------------------------------------------------- */
void libusr_close_l_uses(int ci, int usi)
{
    char    sql_query[SQLBUF];

    if ( ci != -1 )     /* explicit user logout -- downgrade to anonymous */
    {
        DBG("Downgrading logged in session to anonymous, usi=%d, sesid [%s]", usi, uses[usi].sesid);

//        sprintf(sql_query, "DELETE FROM users_logins WHERE user_id=%ld", uses[usi].uid);  /* use indexes */
        sprintf(sql_query, "DELETE FROM users_logins WHERE sesid='%s'", uses[usi].sesid);
        DBG("sql_query: %s", sql_query);

        if ( mysql_query(G_dbconn, sql_query) )
            ERR("Error %u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));

        strcpy(conn[ci].cookie_out_l, "x");
        strcpy(conn[ci].cookie_out_l_exp, G_last_modified);     /* in the past => to be removed by browser straight away */

        strcpy(conn[ci].cookie_out_a, uses[usi].sesid);

        uses[usi].logged = FALSE;
        uses[usi].uid = 0;
        uses[usi].login[0] = EOS;
        uses[usi].email[0] = EOS;
        uses[usi].name[0] = EOS;
        uses[usi].phone[0] = EOS;
        uses[usi].about[0] = EOS;
        uses[usi].login_tmp[0] = EOS;
        uses[usi].email_tmp[0] = EOS;
        uses[usi].name_tmp[0] = EOS;
        uses[usi].phone_tmp[0] = EOS;
        uses[usi].about_tmp[0] = EOS;
    }
    else    /* timeout'ed */
    {
        DBG("Closing logged in session, usi=%d, sesid [%s]", usi, uses[usi].sesid);
        eng_uses_close(usi);
    }
}


/* --------------------------------------------------------------------------
   Check whether user exists in database
-------------------------------------------------------------------------- */
static int user_exists(const char *login)
{
    char        sql_query[SQLBUF];
    MYSQL_RES   *result;
    long        records;

    DBG("user_exists, login [%s]", login);

//  if ( 0==strcmp(sanlogin, "ADMIN") )
//      return ERR_USERNAME_TAKEN;

    sprintf(sql_query, "SELECT id FROM users WHERE login_u='%s'", upper(login));

    DBG("sql_query: %s", sql_query);

    mysql_query(G_dbconn, sql_query);

    result = mysql_store_result(G_dbconn);

    if ( !result )
    {
        ERR("Error %u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
        return ERR_INT_SERVER_ERROR;
    }

    records = mysql_num_rows(result);

    DBG("users: %ld record(s) found", records);

    mysql_free_result(result);

    if ( 0 != records )
        return ERR_USERNAME_TAKEN;

    return OK;
}


/* --------------------------------------------------------------------------
  check whether email exists in database
-------------------------------------------------------------------------- */
static int email_exists(const char *email)
{
    char        sql_query[SQLBUF];
    MYSQL_RES   *result;
    long        records;

    DBG("email_exists, email [%s]", email);

    sprintf(sql_query, "SELECT id FROM users WHERE email_u='%s'", upper(email));

    DBG("sql_query: %s", sql_query);

    mysql_query(G_dbconn, sql_query);

    result = mysql_store_result(G_dbconn);

    if ( !result )
    {
        ERR("Error %u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
        return ERR_INT_SERVER_ERROR;
    }

    records = mysql_num_rows(result);

    DBG("users: %ld record(s) found", records);

    mysql_free_result(result);

    if ( 0 != records )
        return ERR_EMAIL_TAKEN;

    return OK;
}


/* --------------------------------------------------------------------------
   Log user in -- called either by l_usession_ok or silgy_usr_login
   Authentication has already been done prior to calling this
-------------------------------------------------------------------------- */
static int do_login(int ci, long uid, char *p_login, char *p_email, char *p_name, char *p_phone, char *p_about, long visits, const char *sesid)
{
    char        sql_query[SQLBUF];
    MYSQL_RES   *result;
    MYSQL_ROW   sql_row;
unsigned long   sql_records;
    char        login[LOGIN_LEN+1];
    char        email[EMAIL_LEN+1];
    char        name[UNAME_LEN+1];
    char        phone[PHONE_LEN+1];
    char        about[256];

    DBG("do_login");

    /* get user record by id */

    if ( !p_login )  /* login from cookie */
    {
        sprintf(sql_query, "SELECT login,email,name,phone,about,visits FROM users WHERE id=%ld", uid);
        DBG("sql_query: %s", sql_query);
        mysql_query(G_dbconn, sql_query);
        result = mysql_store_result(G_dbconn);
        if ( !result )
        {
            ERR("Error %u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
            return ERR_INT_SERVER_ERROR;
        }

        sql_records = mysql_num_rows(result);

        DBG("users: %lu record(s) found", sql_records);

        if ( 0 == sql_records )
        {
            mysql_free_result(result);
            WAR("Cookie sesid does not match user id");
            return ERR_INVALID_LOGIN;   /* invalid user and/or password */
        }

        /* user found */

        sql_row = mysql_fetch_row(result);

        strcpy(login, sql_row[0]?sql_row[0]:"");
        strcpy(email, sql_row[1]?sql_row[1]:"");
        strcpy(name, sql_row[2]?sql_row[2]:"");
        strcpy(phone, sql_row[3]?sql_row[3]:"");
        strcpy(about, sql_row[4]?sql_row[4]:"");
        visits = atol(sql_row[5]);

        mysql_free_result(result);
    }
    else
    {
        strcpy(login, p_login);
        strcpy(email, p_email);
        strcpy(name, p_name);
        strcpy(phone, p_phone);
        strcpy(about, p_about);
    }

    /* admin? */
#ifdef USERSBYEMAIL
#ifdef APP_ADMIN_EMAIL
    if ( 0==strcmp(email, APP_ADMIN_EMAIL) )
        strcpy(login, "admin");
#endif
#endif
    /* add record to uses */

    if ( !start_new_luses(ci, uid, login, email, name, phone, about, sesid) )
        return ERR_SERVER_TOOBUSY;

    /* update user record */

    sprintf(sql_query, "UPDATE users SET visits=%ld, last_login='%s' WHERE id=%ld", visits+1, G_dt, uid);
    DBG("sql_query: %s", sql_query);
    if ( mysql_query(G_dbconn, sql_query) )
    {
        ERR("Error %u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
        return ERR_INT_SERVER_ERROR;
    }

    /* init app user session */

    app_luses_init(ci);

#ifdef USERSBYEMAIL
    INF("User [%s] logged in", US.email);
#else
    INF("User [%s] logged in", US.login);
#endif

    return OK;
}


/* --------------------------------------------------------------------------
   Send activation link
-------------------------------------------------------------------------- */
static int send_activation_link(int ci, const char *login, const char *email)
{
    char linkkey[PASSWD_RESET_KEY_LEN+1];
    char sql_query[SQLBUF];
    char subject[256];
    char message[4096];
    
    /* generate the key */

    silgy_random(linkkey, PASSWD_RESET_KEY_LEN);

    sprintf(sql_query, "INSERT INTO users_activations (linkkey,user_id,created,activated) VALUES ('%s',%ld,'%s','N')", linkkey, US.uid, G_dt);
    DBG("sql_query: %s", sql_query);

    if ( mysql_query(G_dbconn, sql_query) )
    {
        ERR("Error %u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
        return ERR_INT_SERVER_ERROR;
    }

    /* send an email */

    char tmp[1024];
    char *p=message;

    sprintf(tmp, "Dear %s,\n\n", login[0]?login:"User");
    p = stpcpy(p, tmp);
    sprintf(tmp, "Welcome to %s! Your account requires activation. Please visit this URL to activate your account:\n\n", conn[ci].website);
    p = stpcpy(p, tmp);

#ifdef HTTPS
    if ( G_test )
        sprintf(tmp, "http://%s/activate_acc?k=%s\n\n", conn[ci].host, linkkey);
    else
        sprintf(tmp, "https://%s/activate_acc?k=%s\n\n", conn[ci].host, linkkey);
#else
    sprintf(tmp, "http://%s/activate_acc?k=%s\n\n", conn[ci].host, linkkey);
#endif
    p = stpcpy(p, tmp);

    sprintf(tmp, "Please keep in mind that this link will only be valid for the next %d hours.\n\n", USER_ACTIVATION_HOURS);
    p = stpcpy(p, tmp);
    p = stpcpy(p, "If you did this by mistake or it wasn't you, you can safely ignore this email.\n\n");
#ifdef APP_CONTACT_EMAIL
    sprintf(tmp, "In case you needed any help, please contact us at %s.\n\n", APP_CONTACT_EMAIL);
    p = stpcpy(p, tmp);
#endif
    p = stpcpy(p, "Kind Regards\n");

    sprintf(tmp, "%s\n", conn[ci].website);
    p = stpcpy(p, tmp);

    sprintf(subject, "%s Account Activation", conn[ci].website);

    if ( !silgy_email(email, subject, message) )
        return ERR_INT_SERVER_ERROR;

    return OK;
}


/* --------------------------------------------------------------------------
   Verify activation key
-------------------------------------------------------------------------- */
static int silgy_usr_verify_activation_key(int ci, char *linkkey, long *uid)
{
    char        sql_query[SQLBUF];
    MYSQL_RES   *result;
    MYSQL_ROW   sql_row;
unsigned long   sql_records;
    char        esc_linkkey[256];

    DBG("silgy_usr_verify_activation_key");

    if ( strlen(linkkey) != PASSWD_RESET_KEY_LEN )
        return ERR_LINK_BROKEN;

    strcpy(esc_linkkey, silgy_sql_esc(linkkey));

    sprintf(sql_query, "SELECT user_id, created, activated FROM users_activations WHERE linkkey='%s'", esc_linkkey);
    DBG("sql_query: %s", sql_query);

    mysql_query(G_dbconn, sql_query);

    result = mysql_store_result(G_dbconn);

    if ( !result )
    {
        ERR("Error %u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
        return ERR_INT_SERVER_ERROR;
    }

    sql_records = mysql_num_rows(result);

    DBG("users_activations: %lu row(s) found", sql_records);

    if ( !sql_records )     /* no records with this key in users_activations -- link broken? */
    {
        mysql_free_result(result);
        return ERR_LINK_MAY_BE_EXPIRED;
    }

    sql_row = mysql_fetch_row(result);

    /* already activated? */

    if ( sql_row[2] && sql_row[2][0]=='Y' )
    {
        mysql_free_result(result);
        DBG("User already activated");
        return MSG_USER_ALREADY_ACTIVATED;
    }

    /* validate expiry time */

    if ( db2epoch(sql_row[1]) < G_now-3600*USER_ACTIVATION_HOURS )
    {
        WAR("Key created more than %d hours ago", USER_ACTIVATION_HOURS);
        mysql_free_result(result);
        return ERR_LINK_MAY_BE_EXPIRED;
    }

    /* otherwise everything's OK ----------------------------------------- */

    /* get the user id */

    *uid = atol(sql_row[0]);

    mysql_free_result(result);

    DBG("Key ok, uid = %ld", *uid);

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
    QSVAL       login;
    QSVAL       email;
    char        name[UNAME_LEN+1];
    char        phone[PHONE_LEN+1];
    char        about[256];
    short       user_status;
    QSVAL       passwd;
    QSVAL       keep;
    char        ulogin[MAX_VALUE_LEN*2+1];
    char        sql_query[SQLBUF];
    char        p1[32], p2[32];
    char        str1[32], str2[32];
    long        ula_cnt;
    char        ula_time[32];
    time_t      ula_time_epoch;
    MYSQL_RES   *result;
    MYSQL_ROW   sql_row;
unsigned long   sql_records;
    long        uid;
    char        sesid[SESID_LEN+1]="";
    long        new_ula_cnt;
    char        sanuagent[DB_UAGENT_LEN+1];
    time_t      sometimeahead;
    long        visits;
    char        deleted[4];

    DBG("silgy_usr_login");

#ifdef USERSBYEMAIL

    if ( !QS_HTML_ESCAPE("email", email) || !QS_HTML_ESCAPE("passwd", passwd) )
    {
        WAR("Invalid request (URI val missing?)");
        return ERR_INVALID_REQUEST;
    }
    stp_right(email);
    sprintf(sql_query, "SELECT id,login,email,name,phone,passwd1,passwd2,about,status,ula_time,ula_cnt,visits,deleted FROM users WHERE email_u='%s'", upper(email));

#else    /* by login */

    if ( !QS_HTML_ESCAPE("login", login) || !QS_HTML_ESCAPE("passwd", passwd) )
    {
        WAR("Invalid request (URI val missing?)");
        return ERR_INVALID_REQUEST;
    }
    stp_right(login);
    strcpy(ulogin, upper(login));
    sprintf(sql_query, "SELECT id,login,email,name,phone,passwd1,passwd2,about,status,ula_time,ula_cnt,visits,deleted FROM users WHERE (login_u='%s' OR email_u='%s')", ulogin, ulogin);

#endif

    DBG("sql_query: %s", sql_query);

    mysql_query(G_dbconn, sql_query);

    result = mysql_store_result(G_dbconn);

    if ( !result )
    {
        ERR("Error %u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
        return ERR_INT_SERVER_ERROR;
    }

    sql_records = mysql_num_rows(result);

    DBG("users: %lu record(s) found", sql_records);

    if ( 0 == sql_records )     /* no records */
    {
        mysql_free_result(result);
        return ERR_INVALID_LOGIN;   /* invalid user and/or password */
    }

    /* user name found */

    sql_row = mysql_fetch_row(result);

    uid = atol(sql_row[0]);
    strcpy(login, sql_row[1]?sql_row[1]:"");
    strcpy(email, sql_row[2]?sql_row[2]:"");
    strcpy(name, sql_row[3]?sql_row[3]:"");
    strcpy(phone, sql_row[4]?sql_row[4]:"");
    strcpy(p1, sql_row[5]);
    strcpy(p2, sql_row[6]);
    strcpy(about, sql_row[7]?sql_row[7]:"");
    user_status = atoi(sql_row[8]);
    strcpy(ula_time, sql_row[9]?sql_row[9]:"");
    ula_cnt = atol(sql_row[10]);
    visits = atol(sql_row[11]);
    strcpy(deleted, sql_row[12]?sql_row[12]:"N");

    mysql_free_result(result);

    /* deleted? */

    if ( deleted[0]=='Y' )
    {
        WAR("User deleted");
        return ERR_INVALID_LOGIN;   /* invalid user and/or password */
    }

    /* check ULA (Unsuccessful Login Attempts) info to prevent brute-force password attacks */

    ula_time_epoch = db2epoch(ula_time);

    if ( (ula_cnt > 5 && ula_time_epoch > G_now-3600)           /* 3600 secs = 1 hour */
            || (ula_cnt == 5 && ula_time_epoch > G_now-600)     /* 600 secs = 10 mins */
            || (ula_cnt == 4 && ula_time_epoch > G_now-60) )    /* 60 secs = 1 min */
    {
        return WAR_ULA; /* wait before the next attempt */
    }

    /* now check username/email and password pairs as they should be */

#ifdef USERSBYEMAIL
    doit(str1, str2, email, email, passwd);
#else
    doit(str1, str2, login, email[0]?email:STR_005, passwd);
#endif

    /* are these as expected? */

    if ( 0 != strcmp(str1, p1) || (email[0] && 0 != strcmp(str2, p2)) ) /* passwd1, passwd2 */
    {
        DBG("Invalid password");
        new_ula_cnt = ula_cnt + 1;
        sprintf(sql_query, "UPDATE users SET ula_cnt=%ld, ula_time='%s' WHERE id=%ld", new_ula_cnt, G_dt, uid);
        DBG("sql_query: %s", sql_query);
        if ( mysql_query(G_dbconn, sql_query) )
        {
            ERR("Error %u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
            return ERR_INT_SERVER_ERROR;
        }
        return ERR_INVALID_LOGIN;   /* invalid user and/or password */
    }

    DBG("Password OK");

    /* activated? */

    if ( user_status != USER_STATUS_ACTIVE )
    {
        WAR("User not activated");
        return ERR_NOT_ACTIVATED;
    }

    DBG("User activation status OK");

    /* successful login ------------------------------------------------------------ */

    if ( ula_cnt )  /* clear it */
    {
        DBG("Clearing ula_cnt");
        sprintf(sql_query, "UPDATE users SET ula_cnt=0 WHERE id=%ld", uid);
        DBG("sql_query: %s", sql_query);
        if ( mysql_query(G_dbconn, sql_query) )
        {
            ERR("Error %u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
            return ERR_INT_SERVER_ERROR;
        }
    }

    /* try to use anonymous sesid if present */

    if ( conn[ci].usi )
    {
        DBG("Using current session usi=%d, sesid [%s]", conn[ci].usi, US.sesid);
    }
    else if ( !eng_uses_start(ci) )
    {
        return ERR_SERVER_TOOBUSY;
    }

    strcpy(sesid, US.sesid);

    /* save new session to users_logins and set the cookie */

    DBG("Saving user session [%s] into users_logins...", sesid);

    strncpy(sanuagent, silgy_sql_esc(conn[ci].uagent), DB_UAGENT_LEN);
    sanuagent[DB_UAGENT_LEN] = EOS;
    if ( sanuagent[DB_UAGENT_LEN-1]=='\'' && sanuagent[DB_UAGENT_LEN-2]!='\'' )
        sanuagent[DB_UAGENT_LEN-1] = EOS;

    sprintf(sql_query, "INSERT INTO users_logins (sesid,uagent,ip,user_id,created,last_used) VALUES ('%s','%s','%s',%ld,'%s','%s')", sesid, sanuagent, conn[ci].ip, uid, G_dt, G_dt);
    DBG("sql_query: %s", sql_query);
    if ( mysql_query(G_dbconn, sql_query) )
    {
        ERR("Error %u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
        return ERR_INT_SERVER_ERROR;
    }

    DBG("User session saved OK");

    /* set cookie */

    strcpy(conn[ci].cookie_out_l, sesid);

    /* Keep me logged in -- set cookie expiry date */

    if ( QS_HTML_ESCAPE("keep", keep) && 0==strcmp(keep, "on") )
    {
        DBG("keep is ON!");
        sometimeahead = G_now + 3600*24*30; /* 30 days */
        G_ptm = gmtime(&sometimeahead);
        strftime(conn[ci].cookie_out_l_exp, 32, "%a, %d %b %Y %T GMT", G_ptm);
//      DBG("conn[ci].cookie_out_l_exp: [%s]", conn[ci].cookie_out_l_exp);
        G_ptm = gmtime(&G_now); /* make sure G_ptm is always up to date */
    }

    /* finish logging user in */

    return do_login(ci, uid, login, email, name, phone, about, visits, sesid);
}


/* --------------------------------------------------------------------------
   Create user account
   Return OK or:
   ERR_INVALID_REQUEST
   ERR_WEBSITE_FIRST_LETTER
   ERR_USERNAME_TOO_SHORT
   ERR_USERNAME_CHARS
   ERR_USERNAME_TAKEN
   ERR_EMAIL_FORMAT_OR_EMPTY
   ERR_PASSWORD_TOO_SHORT
   ERR_PASSWORD_DIFFERENT
   ERR_INT_SERVER_ERROR
-------------------------------------------------------------------------- */
int silgy_usr_create_account(int ci)
{
    int     ret=OK;
    QSVAL   login;
    QSVAL   login_u;
    QSVAL   email;
    QSVAL   email_u;
    QSVAL   name;
    QSVAL   phone;
    QSVAL   about;
    QSVAL   passwd;
    QSVAL   rpasswd;
    QSVAL   message;
    int     plen;
    char    sql_query[SQLBUF];
    char    str1[32], str2[32];

    DBG("silgy_usr_create_account");

    if ( QS_HTML_ESCAPE("login", login) )
    {
        stp_right(login);
        strncpy(US.login, login, LOGIN_LEN);
        US.login[LOGIN_LEN] = EOS;
    }

#ifndef USERSBYEMAIL
    if ( !login[0] )
    {
        WAR("Invalid request (login missing)");
        return ERR_INVALID_REQUEST;
    }
#endif

    if ( QS_HTML_ESCAPE("email", email) )
    {
        stp_right(email);
        strncpy(US.email, email, EMAIL_LEN);
        US.email[EMAIL_LEN] = EOS;
    }

    if ( G_usersRequireAccountActivation && !email[0] )
    {
        WAR("Invalid request (email missing)");
        return ERR_EMAIL_EMPTY;
    }

#ifdef USERSBYEMAIL
    if ( !email[0] )
    {
        WAR("Invalid request (email missing)");
        return ERR_EMAIL_EMPTY;
    }
#endif

    /* regardless of auth method */

    if ( !QS_HTML_ESCAPE("passwd", passwd)
            || !QS_HTML_ESCAPE("rpasswd", rpasswd) )
    {
        WAR("Invalid request (URI val missing?)");
        return ERR_INVALID_REQUEST;
    }

    /* optional */

    if ( QS_HTML_ESCAPE("name", name) )
    {
        stp_right(name);
        strncpy(US.name, name, UNAME_LEN);
        US.name[UNAME_LEN] = EOS;
    }

    if ( QS_HTML_ESCAPE("phone", phone) )
    {
        stp_right(phone);
        strncpy(US.phone, phone, PHONE_LEN);
        US.phone[PHONE_LEN] = EOS;
    }

    if ( QS_HTML_ESCAPE("about", about) )
    {
        stp_right(about);
        strncpy(US.about, about, ABOUT_LEN);
        US.about[ABOUT_LEN] = EOS;
    }

    /* ----------------------------------------------------------------- */

    plen = strlen(passwd);

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
#endif

    if ( email[0] && OK != (ret=email_exists(email)) )  /* email in use */
        return ret;
    else if ( plen < MIN_PASSWORD_LEN )                 /* password too short */
        return ERR_PASSWORD_TOO_SHORT;
    else if ( 0 != strcmp(passwd, rpasswd) )            /* passwords differ */
        return ERR_PASSWORD_DIFFERENT;

    /* welcome! -- and generate password hashes ------------------------------------------------------- */

#ifdef USERSBYEMAIL
    doit(str1, str2, email, email, passwd);
#else
    doit(str1, str2, login, email[0]?email:STR_005, passwd);
#endif

    strcpy(login_u, upper(login));
    strcpy(email_u, upper(email));

    short user_status;

    if ( G_usersRequireAccountActivation )
        user_status = USER_STATUS_INACTIVE;
    else
        user_status = USER_STATUS_ACTIVE;

    sprintf(sql_query, "INSERT INTO users (id,login,login_u,email,email_u,name,phone,passwd1,passwd2,about,status,created,visits,settings,ula_cnt,deleted) VALUES (0,'%s','%s','%s','%s','%s','%s','%s','%s','%s',%hd,'%s',0,0,0,'N')", login, login_u, email, email_u, name, phone, str1, str2, about, user_status, G_dt);

    DBG("sql_query: INSERT INTO users (id,login,email,name,phone,...) VALUES (0,'%s','%s','%s','%s',...)", login, email, name, phone);

    if ( mysql_query(G_dbconn, sql_query) )
    {
        ERR("Error %u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
        return ERR_INT_SERVER_ERROR;
    }

    US.uid = mysql_insert_id(G_dbconn);

    if ( G_usersRequireAccountActivation )
    {
        if ( (ret=send_activation_link(ci, login, email)) != OK )
            return ret;
    }

#ifdef USERSBYEMAIL
    INF("User [%s] created", email);
#else
    INF("User [%s] created", login);
#endif

    return OK;

}


/* --------------------------------------------------------------------------
   Save user message
-------------------------------------------------------------------------- */
int silgy_usr_send_message(int ci)
{
static char message[MAX_LONG_URI_VAL_LEN+1];
static char sanmessage[MAX_LONG_URI_VAL_LEN+1];
    QSVAL   email;
static char sql_query[MAX_LONG_URI_VAL_LEN*2];

    DBG("silgy_usr_send_message");

    if ( !get_qs_param_long(ci, "msg_box", message) )
    {
        WAR("Invalid request (URI val missing?)");
        return ERR_INVALID_REQUEST;
    }

    if ( QS_HTML_ESCAPE("email", email) )
        stp_right(email);

    strcpy(sanmessage, silgy_html_esc(message));

    /* remember user details in case of error or warning to correct */

    strcpy(US.email_tmp, email);

    sprintf(sql_query, "INSERT INTO users_messages (user_id,msg_id,email,message,created) VALUES (%ld,%ld,'%s','%s','%s')", US.uid, get_max(ci, "messages")+1, email, sanmessage, G_dt);
    DBG("sql_query: INSERT INTO users_messages (user_id,msg_id,email,...) VALUES (%ld,get_max(),'%s',...)", US.uid, email);

    if ( mysql_query(G_dbconn, sql_query) )
    {
        ERR("Error %u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
        return ERR_INT_SERVER_ERROR;
    }

    /* send an email to admin */

#ifdef APP_CONTACT_EMAIL
    silgy_email(APP_CONTACT_EMAIL, "New message!", message);
#endif

    return OK;
}


/* --------------------------------------------------------------------------
   Save changes to user account
-------------------------------------------------------------------------- */
int silgy_usr_save_account(int ci)
{
    int         ret=OK;
    QSVAL       login;
    QSVAL       email;
    QSVAL       name;
    QSVAL       phone;
    QSVAL       about;
    QSVAL       passwd;
    QSVAL       rpasswd;
    QSVAL       opasswd;
    QSVAL       uemail_old;
    QSVAL       uemail_new;
    QSVAL       strdelete;
    QSVAL       strdelconf;
    QSVAL       save;
    int         plen;
    char        sql_query[SQLBUF];
    char        str1[32], str2[32];
    MYSQL_RES   *result;
unsigned long   sql_records;
    MYSQL_ROW   sql_row;

    DBG("silgy_usr_save_account");

    if ( !QS_HTML_ESCAPE("opasswd", opasswd)
#ifndef USERSBYEMAIL
            || !QS_HTML_ESCAPE("login", login)
#endif
            || !QS_HTML_ESCAPE("email", email)
            || !QS_HTML_ESCAPE("passwd", passwd)
            || !QS_HTML_ESCAPE("rpasswd", rpasswd) )
    {
        WAR("Invalid request (URI val missing?)");
        return ERR_INVALID_REQUEST;
    }

#ifdef USERSBYEMAIL
    if ( QS_HTML_ESCAPE("login", login) )     /* try to get login anyway */
        stp_right(login);
#endif

    stp_right(email);   /* always present but can be empty */

    if ( QS_HTML_ESCAPE("name", name) ) /* optional */
        stp_right(name);

    if ( QS_HTML_ESCAPE("phone", phone) ) /* optional */
        stp_right(phone);

    if ( QS_HTML_ESCAPE("about", about) ) /* optional */
        stp_right(about);

    /* remember form fields */
    /* US.email contains old email */

    strncpy(US.login_tmp, login, LOGIN_LEN);
    US.login_tmp[LOGIN_LEN] = EOS;
    strncpy(US.email_tmp, email, EMAIL_LEN);
    US.email_tmp[EMAIL_LEN] = EOS;
    strncpy(US.name_tmp, name, UNAME_LEN);
    US.name_tmp[UNAME_LEN] = EOS;
    strncpy(US.phone_tmp, phone, PHONE_LEN);
    US.phone_tmp[PHONE_LEN] = EOS;
    strncpy(US.about_tmp, about, ABOUT_LEN);
    US.about_tmp[ABOUT_LEN] = EOS;

    DBG("login_tmp: [%s]", US.login_tmp);
    DBG("email_tmp: [%s]", US.email_tmp);
    DBG(" name_tmp: [%s]", US.name_tmp);
    DBG("phone_tmp: [%s]", US.phone_tmp);
    DBG("about_tmp: [%s]", US.about_tmp);

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
#endif
    else if ( plen && plen < MIN_PASSWORD_LEN )
        return ERR_PASSWORD_TOO_SHORT;
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
    sprintf(sql_query, "SELECT passwd1 FROM users WHERE email_u='%s'", upper(email));
#else
    doit(str1, str2, login, login, opasswd);
    sprintf(sql_query, "SELECT passwd1 FROM users WHERE login_u='%s'", upper(login));
#endif
    DBG("sql_query: %s", sql_query);

    mysql_query(G_dbconn, sql_query);

    result = mysql_store_result(G_dbconn);

    if ( !result )
    {
        ERR("Error %u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
        return ERR_INT_SERVER_ERROR;
    }

    sql_records = mysql_num_rows(result);

    if ( 0 == sql_records )
    {
        ERR("Weird: no such user");
        mysql_free_result(result);
        return ERR_INT_SERVER_ERROR;
    }

    sql_row = mysql_fetch_row(result);

    if ( 0 != strcmp(str1, sql_row[0]) )
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
            sprintf(sql_query, "UPDATE users SET deleted='Y' WHERE id=%ld", US.uid);
            DBG("sql_query: %s", sql_query);
            if ( mysql_query(G_dbconn, sql_query) )
            {
                ERR("Error %u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
                return ERR_INT_SERVER_ERROR;
            }

            libusr_close_l_uses(ci, conn[ci].usi);  /* log user out */

            return MSG_ACCOUNT_DELETED;
        }
    }

    /* anything else than deleting account -- changing email and/or name and/or password */

#ifdef USERSBYEMAIL
    doit(str1, str2, email, email, plen?passwd:opasswd);
#else
    doit(str1, str2, login, email[0]?email:STR_005, plen?passwd:opasswd);
#endif

    sprintf(sql_query, "UPDATE users SET login='%s', email='%s', name='%s', phone='%s', passwd1='%s', passwd2='%s', about='%s' WHERE id=%ld", login, email, name, phone, str1, str2, about, US.uid);
    DBG("sql_query: UPDATE users SET login='%s', email='%s', name='%s', phone='%s',... WHERE id=%ld", login, email, name, phone, US.uid);

    if ( mysql_query(G_dbconn, sql_query) )
    {
        ERR("Error %u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
        return ERR_INT_SERVER_ERROR;
    }

    DBG("Updating login, email, name, phone & about in user session");

    strcpy(US.login, US.login_tmp);
    strcpy(US.email, US.email_tmp);
    strcpy(US.name, US.name_tmp);
    strcpy(US.phone, US.phone_tmp);
    strcpy(US.about, US.about_tmp);

    return OK;
}


/* --------------------------------------------------------------------------
   Email taken?
-------------------------------------------------------------------------- */
int silgy_usr_email_registered(int ci)
{
    QSVAL   email;

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
   Send an email with password reset link
-------------------------------------------------------------------------- */
int silgy_usr_send_passwd_reset_email(int ci)
{
    QSVAL       email;
    QSVAL       submit;
    char        sql_query[SQLBUF];
    MYSQL_RES   *result;
unsigned long   sql_records;
    MYSQL_ROW   sql_row;
    long        uid;
    char        login_name[LOGIN_LEN+1];
    char        linkkey[256];
    char        subject[256];
    char        message[4096];

    DBG("silgy_usr_send_passwd_reset_email");

    if ( !QS_HTML_ESCAPE("email", email) )
    {
        WAR("Invalid request (URI val missing?)");
        return ERR_INVALID_REQUEST;
    }

    stp_right(email);

    if ( !valid_email(email) )      /* invalid email format */
        return ERR_EMAIL_FORMAT;

#ifdef USERSBYEMAIL
    sprintf(sql_query, "SELECT id, name, deleted FROM users WHERE email_u='%s'", upper(email));
#else
    sprintf(sql_query, "SELECT id, login, deleted FROM users WHERE email_u='%s'", upper(email));
#endif

    DBG("sql_query: %s", sql_query);

    mysql_query(G_dbconn, sql_query);

    result = mysql_store_result(G_dbconn);

    if ( !result )
    {
        ERR("Error %u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
        return ERR_INT_SERVER_ERROR;
    }

    sql_records = mysql_num_rows(result);

    DBG("users: %lu record(s) found", sql_records);

    if ( sql_records )
    {
        sql_row = mysql_fetch_row(result);

        if ( sql_row[2][0]=='Y' )   /* deleted */
        {
            mysql_free_result(result);
            WAR("Password reset link requested for [%s] but user is deleted", email);
            return OK;
        }

        uid = atol(sql_row[0]);     /* user id */
        strcpy(login_name, sql_row[1]);

        mysql_free_result(result);

        /* generate a key */

        silgy_random(linkkey, PASSWD_RESET_KEY_LEN);

        sprintf(sql_query, "INSERT INTO users_p_resets (linkkey,user_id,created,tries) VALUES ('%s',%ld,'%s',0)", linkkey, uid, G_dt);
        DBG("sql_query: %s", sql_query);

        if ( mysql_query(G_dbconn, sql_query) )
        {
            ERR("Error %u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
            return ERR_INT_SERVER_ERROR;
        }

        /* send an email */

        char tmp[1024];
        char *p=message;

        sprintf(tmp, "Dear %s,\n\n", login_name);
        p = stpcpy(p, tmp);
        sprintf(tmp, "You have requested to have your password reset for your account at %s. Please visit this URL to reset your password:\n\n", conn[ci].website);
        p = stpcpy(p, tmp);

#ifdef HTTPS
        if ( G_test )
            sprintf(tmp, "http://%s/preset?k=%s\n\n", conn[ci].host, linkkey);
        else
            sprintf(tmp, "https://%s/preset?k=%s\n\n", conn[ci].host, linkkey);
#else
        sprintf(tmp, "http://%s/preset?k=%s\n\n", conn[ci].host, linkkey);
#endif
        p = stpcpy(p, tmp);

        p = stpcpy(p, "Please keep in mind that this link will only be valid for the next 24 hours.\n\n");
        p = stpcpy(p, "If you did this by mistake or it wasn't you, you can safely ignore this email.\n\n");
#ifdef APP_CONTACT_EMAIL
        sprintf(tmp, "In case you needed any help, please contact us at %s.\n\n", APP_CONTACT_EMAIL);
        p = stpcpy(p, tmp);
#endif
        p = stpcpy(p, "Kind Regards\n");

        sprintf(tmp, "%s\n", conn[ci].website);
        p = stpcpy(p, tmp);

        sprintf(subject, "%s Password Reset", conn[ci].website);

        if ( !silgy_email(email, subject, message) )
            return ERR_INT_SERVER_ERROR;
    }
    else
    {
        mysql_free_result(result);
    }

    INF("Password reset link requested for [%s]", email);

    return OK;
}


/* --------------------------------------------------------------------------
   Verify the link key for password reset
-------------------------------------------------------------------------- */
int silgy_usr_verify_passwd_reset_key(int ci, char *linkkey, long *uid)
{
    char        sql_query[SQLBUF];
    MYSQL_RES   *result;
    MYSQL_ROW   sql_row;
unsigned long   sql_records;
    char        esc_linkkey[256];
    int         tries;

    DBG("silgy_usr_verify_passwd_reset_key");

    if ( strlen(linkkey) != PASSWD_RESET_KEY_LEN )
        return ERR_LINK_BROKEN;

    strcpy(esc_linkkey, silgy_sql_esc(linkkey));

    sprintf(sql_query, "SELECT user_id, created, tries FROM users_p_resets WHERE linkkey='%s'", esc_linkkey);
    DBG("sql_query: %s", sql_query);

    mysql_query(G_dbconn, sql_query);

    result = mysql_store_result(G_dbconn);

    if ( !result )
    {
        ERR("Error %u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
        return ERR_INT_SERVER_ERROR;
    }

    sql_records = mysql_num_rows(result);

    DBG("users_p_resets: %lu row(s) found", sql_records);

    if ( !sql_records )     /* no records with this key in users_p_resets -- link broken? */
    {
        mysql_free_result(result);
        return ERR_LINK_MAY_BE_EXPIRED;
    }

    sql_row = mysql_fetch_row(result);

    /* validate expiry time */

    if ( db2epoch(sql_row[1]) < G_now-3600*24 )  /* older than 24 hours? */
    {
        WAR("Key created more than 24 hours ago");
        mysql_free_result(result);
        return ERR_LINK_MAY_BE_EXPIRED;
    }

    /* validate tries */

    tries = atoi(sql_row[2]);

    if ( tries > 12 )
    {
        WAR("Key tried more than 12 times");
        mysql_free_result(result);
        return ERR_LINK_TOO_MANY_TRIES;
    }

    /* otherwise everything's OK ----------------------------------------- */

    /* get the user id */

    *uid = atol(sql_row[0]);

    mysql_free_result(result);

    DBG("Key ok, uid = %ld", *uid);

    /* update tries counter */

    sprintf(sql_query, "UPDATE users_p_resets SET tries=%d WHERE linkkey='%s'", tries+1, esc_linkkey);
    DBG("sql_query: %s", sql_query);
    if ( mysql_query(G_dbconn, sql_query) )
    {
        ERR("Error %u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
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
    long        uid;
    char        sql_query[SQLBUF];
    MYSQL_RES   *result;
    MYSQL_ROW   sql_row;
unsigned long   sql_records;

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

    sprintf(sql_query, "UPDATE users SET status=%d WHERE id=%ld", USER_STATUS_ACTIVE, uid);
    DBG("sql_query: %s", sql_query);
    if ( mysql_query(G_dbconn, sql_query) )
    {
        ERR("Error %u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
        return ERR_INT_SERVER_ERROR;
    }

    /* remove activation link */

//    sprintf(sql_query, "DELETE FROM users_activations WHERE linkkey='%s'", linkkey);
    sprintf(sql_query, "UPDATE users_activations SET activated='Y' WHERE linkkey='%s'", linkkey);
    DBG("sql_query: %s", sql_query);
    if ( mysql_query(G_dbconn, sql_query) )
    {
        ERR("Error %u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
//        return ERR_INT_SERVER_ERROR;  ignore it
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
    int         plen;
    long        uid;
    char        sql_query[SQLBUF];
    char        str1[32], str2[32];
    MYSQL_RES   *result;
    MYSQL_ROW   sql_row;
unsigned long   sql_records;

    DBG("silgy_usr_reset_password");

//    strcpy(linkkey, US.additional);   /* from here instead of URI */

    if ( !QS_HTML_ESCAPE("email", email)
            || !QS_HTML_ESCAPE("k", linkkey)
            || !QS_HTML_ESCAPE("passwd", passwd)
            || !QS_HTML_ESCAPE("rpasswd", rpasswd) )
    {
        WAR("Invalid request (URI val missing?)");
        return ERR_INVALID_REQUEST;
    }

    stp_right(email);

    plen = strlen(passwd);

    /* remember form fields */

    strcpy(US.email_tmp, email);

    /* general validation */

    if ( !valid_email(email) )
        return ERR_EMAIL_FORMAT;
    else if ( plen < MIN_PASSWORD_LEN )       /* password too short */
        return ERR_PASSWORD_TOO_SHORT;
    else if ( 0 != strcmp(passwd, rpasswd) )    /* passwords differ */
        return ERR_PASSWORD_DIFFERENT;

    /* verify the key */

	if ( (ret=silgy_usr_verify_passwd_reset_key(ci, linkkey, &uid)) != OK )
		return ret;

    /* verify that emails match each other */

#ifdef USERSBYEMAIL
    sprintf(sql_query, "SELECT name, email FROM users WHERE id=%ld", uid);
#else
    sprintf(sql_query, "SELECT login, email FROM users WHERE id=%ld", uid);
#endif
    DBG("sql_query: %s", sql_query);
    mysql_query(G_dbconn, sql_query);
    result = mysql_store_result(G_dbconn);
    if ( !result )
    {
        ERR("Error %u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
        return ERR_INT_SERVER_ERROR;
    }

    sql_records = mysql_num_rows(result);

    DBG("users: %lu record(s) found", sql_records);

    if ( 0 == sql_records )     /* password reset link expired or invalid email */
    {
        mysql_free_result(result);
        return ERR_LINK_EXPIRED;
    }

    sql_row = mysql_fetch_row(result);

    if ( 0 != strcmp(sql_row[1], email) )   /* emails different */
    {
        mysql_free_result(result);
        return ERR_LINK_EXPIRED;    /* password reset link expired or invalid email */
    }


    /* everything's OK -- update password -------------------------------- */

#ifdef USERSBYEMAIL
    doit(str1, str2, email, email, passwd);
#else
    doit(str1, str2, sql_row[0], email, passwd);
#endif

    mysql_free_result(result);

    sprintf(sql_query, "UPDATE users SET passwd1='%s', passwd2='%s' WHERE id=%ld", str1, str2, uid);
// !!!!!!   DBG("sql_query: %s", sql_query);
    if ( mysql_query(G_dbconn, sql_query) )
    {
        ERR("Error %u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
        return ERR_INT_SERVER_ERROR;
    }

    /* remove all password reset keys */

    sprintf(sql_query, "DELETE FROM users_p_resets WHERE user_id=%ld", uid);
    DBG("sql_query: %s", sql_query);
    if ( mysql_query(G_dbconn, sql_query) )
    {
        ERR("Error %u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
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
    libusr_close_l_uses(ci, conn[ci].usi);
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
   Save user string setting
-------------------------------------------------------------------------- */
int silgy_usr_set_str(int ci, const char *us_key, const char *us_val)
{
    int         ret=OK;
    char        sql_query[SQLBUF];

    ret = silgy_usr_get_str(ci, us_key, NULL);

    if ( ret == ERR_NOT_FOUND )
    {
        sprintf(sql_query, "INSERT INTO users_settings (user_id,us_key,us_val) VALUES (%ld,'%s','%s')", US.uid, us_key, us_val);

        DBG("sql_query: %s", sql_query);

        if ( mysql_query(G_dbconn, sql_query) )
        {
            ERR("Error %u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
            return ERR_INT_SERVER_ERROR;
        }
    }
    else if ( ret != OK )
    {
        ERR("Error %u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
        return ERR_INT_SERVER_ERROR;
    }
    else
    {
        sprintf(sql_query, "UPDATE users_settings SET us_val='%s' WHERE user_id=%ld AND us_key='%s'", us_val, US.uid, us_key);

        DBG("sql_query: %s", sql_query);

        if ( mysql_query(G_dbconn, sql_query) )
        {
            ERR("Error %u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
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
    char        sql_query[SQLBUF];
    MYSQL_RES   *result;
    MYSQL_ROW   sql_row;
unsigned long   sql_records;

    sprintf(sql_query, "SELECT us_val FROM users_settings WHERE user_id=%ld AND us_key='%s'", US.uid, us_key);

    DBG("sql_query: %s", sql_query);

    mysql_query(G_dbconn, sql_query);

    result = mysql_store_result(G_dbconn);

    if ( !result )
    {
        ERR("Error %u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
        return ERR_INT_SERVER_ERROR;
    }

    sql_records = mysql_num_rows(result);

    DBG("users_settings: %lu record(s) found", sql_records);

    if ( 0 == sql_records )
    {
        mysql_free_result(result);
        return ERR_NOT_FOUND;
    }

    sql_row = mysql_fetch_row(result);

    if ( us_val )
        strcpy(us_val, sql_row[0]);

    mysql_free_result(result);

    return OK;
}


/* --------------------------------------------------------------------------
   Save user number setting
-------------------------------------------------------------------------- */
int silgy_usr_set_int(int ci, const char *us_key, long us_val)
{
    char    val[64];

    sprintf(val, "%ld", us_val);
    return silgy_usr_set_str(ci, us_key, val);
}


/* --------------------------------------------------------------------------
   Read user number setting
-------------------------------------------------------------------------- */
int silgy_usr_get_int(int ci, const char *us_key, long *us_val)
{
    int     ret;
    char    val[64];

    if ( (ret=silgy_usr_get_str(ci, us_key, val)) == OK )
        *us_val = atol(val);

    return ret;
}


/* --------------------------------------------------------------------------
   Get MAX id
-------------------------------------------------------------------------- */
static long get_max(int ci, const char *table)
{
    char        sql_query[SQLBUF];
    MYSQL_RES   *result;
    MYSQL_ROW   sql_row;
    long        max=0;

    /* US.uid = 0 for anonymous session */

    if ( 0==strcmp(table, "messages") )
        sprintf(sql_query, "SELECT MAX(msg_id) FROM users_messages WHERE user_id=%ld", US.uid);
    else
        return 0;

    DBG("sql_query: %s", sql_query);

    mysql_query(G_dbconn, sql_query);

    result = mysql_store_result(G_dbconn);

    if ( !result )
    {
        ERR("Error %u: %s", mysql_errno(G_dbconn), mysql_error(G_dbconn));
        return ERR_INT_SERVER_ERROR;
    }

    sql_row = mysql_fetch_row(result);

    if ( sql_row[0] != NULL )
        max = atol(sql_row[0]);

    mysql_free_result(result);

    DBG("get_max for uid=%ld  max = %ld", US.uid, max);

    return max;
}


/* --------------------------------------------------------------------------
  get error description for user
  called from eng_get_msg_str()
-------------------------------------------------------------------------- */
void libusr_get_msg_str(int ci, char *dest, int errcode)
{
    if ( errcode == ERR_INVALID_LOGIN )
        strcpy(dest, "Invalid login and/or password");
    else if ( errcode == MSG_WELCOME )
    {
        if ( G_usersRequireAccountActivation )
            sprintf(dest, "Welcome to %s! Your account requires activation. Please check your mailbox for a message from %s.", conn[ci].website, conn[ci].website);
        else
            sprintf(dest, "Welcome to %s! You can now log in:", conn[ci].website);
    }
    else if ( errcode == MSG_WELCOME_AFTER_ACTIVATION )
        strcpy(dest, "Very well! You can now log in:");
    else if ( errcode == MSG_USER_LOGGED_OUT )
        strcpy(dest, "You've been successfully logged out.");
    else if ( errcode == ERR_INVALID_PASSWORD )
        sprintf(dest, "Please enter your existing password");
    else if ( errcode == WAR_NO_EMAIL )
        strcpy(dest, "You didn't provide your email address. This is fine, however please remember that in case you forget your password, there's no way for us to send you reset link.");
    else if ( errcode == ERR_SESSION_EXPIRED )
        strcpy(dest, "Your session has expired. Please log in to continue:");
    else if ( errcode == MSG_CHANGES_SAVED )
        strcpy(dest, "Your changes have been saved.");
    else if ( errcode == ERR_USERNAME_TOO_SHORT )
        sprintf(dest, "User name must be at least %d characters long", MIN_USERNAME_LEN);
    else if ( errcode == ERR_USERNAME_CHARS )
        sprintf(dest, "User name may only contain letters, digits, dots, hyphens, underscores or apostrophes");
    else if ( errcode == ERR_USERNAME_TAKEN )
        strcpy(dest, "Unfortunately this login has already been taken!");
    else if ( errcode == ERR_EMAIL_FORMAT_OR_EMPTY )
        strcpy(dest, "Please enter valid email address or leave this field empty");
    else if ( errcode == ERR_PASSWORD_TOO_SHORT )
        sprintf(dest, "Password must be at least %d characters long", MIN_PASSWORD_LEN);
    else if ( errcode == ERR_PASSWORD_DIFFERENT )
        strcpy(dest, "Please retype password exactly like in the previous field");
    else if ( errcode == ERR_OLD_PASSWORD )
        strcpy(dest, "Please enter your existing password");
    else if ( errcode == ERR_ROBOT )
        strcpy(dest, "I'm afraid you are a robot?");
    else if ( errcode == ERR_WEBSITE_FIRST_LETTER )
        sprintf(dest, "The first letter of this website's name should be %c", conn[ci].website[0]);
    else if ( errcode == ERR_NOT_ACTIVATED )
        sprintf(dest, "Your account requires activation. Please check your mailbox for a message from %s.", conn[ci].website);
    else if ( errcode == ERR_EMAIL_EMPTY )
        strcpy(dest, "Your email address can't be empty");
    else if ( errcode == ERR_EMAIL_FORMAT )
        strcpy(dest, "Please enter valid email address");
    else if ( errcode == ERR_EMAIL_TAKEN )
        strcpy(dest, "This email address has already been registered");
    else if ( errcode == MSG_REQUEST_SENT )
        sprintf(dest, "Your request has been sent. Please check your mailbox for a message from %s.", conn[ci].website);
    else if ( errcode == ERR_LINK_BROKEN )
        strcpy(dest, "It looks like this link is broken. If you clicked on the link you've received from us in email, you can try to copy and paste it in your browser's address bar instead.");
    else if ( errcode == ERR_LINK_MAY_BE_EXPIRED )
        strcpy(dest, "Your link is invalid or may be expired.");
    else if ( errcode == ERR_LINK_EXPIRED || errcode == ERR_LINK_TOO_MANY_TRIES )
        strcpy(dest, "It looks like you entered email that doesn't exist in our database or your link has expired.");
    else if ( errcode == MSG_PASSWORD_CHANGED )
        strcpy(dest, "Your password has been changed. You can now log in:");
    else if ( errcode == MSG_MESSAGE_SENT )
        strcpy(dest, "Your message has been sent.");
    else if ( errcode == MSG_PROVIDE_FEEDBACK )
        sprintf(dest, "%s would suit me better if...", conn[ci].website);
    else if ( errcode == MSG_FEEDBACK_SENT )
        strcpy(dest, "Thank you for your feedback!");
    else if ( errcode == MSG_USER_ALREADY_ACTIVATED )
        strcpy(dest, "Your account has already been activated.");
    else if ( errcode == WAR_ULA )
        strcpy(dest, "Someone tried to log in to this account unsuccessfully more than 3 times. To protect your account from brute-force attack, this system requires some wait: 1 minute, then 10 minutes, then 1 hour before trying again.");
    else if ( errcode == WAR_BEFORE_DELETE )
        sprintf(dest, "You are about to delete your %s's account. All your details and plans will be removed from our database. If you are sure you want this, enter your password and click 'Delete my account'.", conn[ci].website);
    else if ( errcode == MSG_ACCOUNT_DELETED )
        sprintf(dest, "Your user account has been deleted. Thank you for trying %s!", conn[ci].website);
    else
        sprintf(dest, "Unknown error (%d)", errcode);
}

#endif /* USERS */
