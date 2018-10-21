-- ----------------------------------------------------------------------------
-- Silgy user tables -- MySQL version
-- ----------------------------------------------------------------------------

-- users

create table users (
    id integer auto_increment primary key,
    login char(30) not null,
    login_u char(30) not null,      -- uppercase version
    email char(120),
    email_u char(120),              -- uppercase version
    name char(60),
    passwd1 char(30) not null,
    passwd2 char(30) not null,
    about varchar(250),
    status tinyint not null,        -- 0 = inactive, 1 = active
    created datetime not null,
    last_login datetime,
    visits integer not null,
    settings integer not null,
    ula_time datetime,              -- unsuccessful login attempt time
    ula_cnt tinyint not null,       -- and count
    deleted char(1) not null        -- 'Y' / 'N'
);

create index users_login on users (login_u);
create index users_email on users (email_u);


-- user settings

create table users_settings (
    user_id integer,
    us_key char(30),
    us_val varchar(250),
    primary key (user_id, us_key)
);


-- user logins

create table users_logins (
    sesid char(15) primary key,
    uagent varchar(120) not null,
    ip char(15) not null,
    user_id integer not null,
    created datetime not null,
    last_used datetime not null
);


-- account activations

create table users_activations (
    linkkey char(30) primary key,
    user_id integer not null,
    created datetime not null,
    activated char(1) not null
);


-- password resets

create table users_p_resets (
    linkkey char(30) primary key,
    user_id integer not null,
    created datetime not null,
    tries tinyint not null
);

create index users_p_resets_uid on users_p_resets (user_id);


-- messages

create table users_messages (
    user_id integer,
    msg_id integer,
    email varchar(120),
    message text,               -- 64 kB limit
    created datetime not null,
    primary key (user_id, msg_id)
);
