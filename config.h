#pragma once
extern char g_token[512];
extern char g_gateway[80];
extern int g_timeout;


const char *get_user_name();
const char *get_password();
int get_irc_port();
int validate_ini(int show_msgbox);