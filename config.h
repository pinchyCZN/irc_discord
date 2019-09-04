#pragma once
extern char g_token[512];
extern char g_gateway[80];
extern int g_timeout;
extern int gui_active;
extern int console_active;

const char *get_user_name();
const char *get_password();
int get_irc_port();
int validate_ini(int show_msgbox);

int save_user_name(const WCHAR *user);
int save_password(const WCHAR *password);
int save_irc_port(int port);
int save_connect_discord(int val);

int save_window_pos(WINDOWPLACEMENT *);
int get_window_pos(WINDOWPLACEMENT *);

int get_enable_discord();
int save_enable_discord(int val);
