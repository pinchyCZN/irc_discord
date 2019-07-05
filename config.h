#pragma once
extern char g_token[512];
extern char g_gateway[80];
extern int g_timeout;
extern HANDLE g_gwevent;
extern CRITICAL_SECTION g_mutex;


const char *get_user_name();
const char *get_password();
int validate_ini(int show_msgbox);