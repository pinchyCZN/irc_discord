#pragma once
extern char g_token[512];
extern char g_gateway[80];
extern int g_timeout;
extern int gui_active;
extern int console_active;
extern int enable_dbg_discord;
extern int enable_dbg_gateway;
extern int enable_dbg_irc;

const char *get_user_name();
const char *get_password();
int get_irc_port();
int validate_ini(HWND hwnd,int show_msgbox);

int save_user_name(const WCHAR *user);
int save_password(const WCHAR *password);
int save_irc_port(int port);

int save_window_pos(WINDOWPLACEMENT *);
int load_window_pos(WINDOWPLACEMENT *);

int load_connect_on_start();
int save_connect_on_start(int val);
