-- ----------------------------------------------------------------------------
-- Silgy user tables -- MySQL version
-- ----------------------------------------------------------------------------

-- users

create table users (
    id int auto_increment primary key,
    login char(30),
    login_u char(30),               -- uppercase version
    email char(120),
    email_u char(120),              -- uppercase version
    name varchar(120),
    phone varchar(30),
    passwd1 char(30),               -- base64 of SHA1 hash
    passwd2 char(30),               -- base64 of SHA1 hash
    lang char(5),
    about varchar(250),
    avatar_name varchar(60),
    avatar_data blob,               -- 64 kB
    group_id int,
    auth_level tinyint,             -- 10 = user, 20 = customer, 30 = staff, 40 = moderator, 50 = admin, 100 = root
    status tinyint,                 -- 0 = inactive, 1 = active, 2 = locked, 3 = requires password change, 9 = deleted
    created datetime,
    last_login datetime,
    visits int,
    ula_cnt int,                    -- unsuccessful login attempt count
    ula_time datetime               -- and time
);

create index users_login on users (login_u);
create index users_email on users (email_u);
create index users_last_login on users (last_login);


-- groups

create table users_groups (
    id int auto_increment primary key,
    name varchar(120),
    about varchar(250),
    auth_level tinyint
);


-- user settings

create table users_settings (
    user_id int,
    us_key char(30),
    us_val varchar(250),
    primary key (user_id, us_key)
);


-- user logins

create table users_logins (
    sesid char(15) primary key,
    uagent varchar(250),
    ip char(15),
    user_id int,
    csrft char(7),
    created datetime,
    last_used datetime
);

create index users_logins_uid on users_logins (user_id);


-- account activations

create table users_activations (
    linkkey char(30) primary key,
    user_id int,
    created datetime,
    activated datetime
);


-- password resets

create table users_p_resets (
    linkkey char(20) primary key,
    user_id int,
    created datetime,
    tries smallint
);

create index users_p_resets_uid on users_p_resets (user_id);


-- messages

create table users_messages (
    user_id int,
    msg_id int,
    email varchar(120),
    message text,               -- 64 kB limit
    created datetime,
    primary key (user_id, msg_id)
);
