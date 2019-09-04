#include <Windows.h>
#include <CommCtrl.h>
#include <stdio.h>
#include "resource.h"
#include "anchor_system.h"
#include "discord.h"
#include "config.h"
#include "libstring.h"

#pragma warning(disable:4996)

static HINSTANCE g_hinstance=0;

typedef struct{
	HWND hwnd;
	HWND hparent;
	int  IDD;
	char text[80];
}TAB_DATA;

TAB_DATA tab_data[8]={0};

static HWND hedit_list[3]={0};
enum{
	HWND_EDIT_IRC=0,
	HWND_EDIT_DISCORD,
	HWND_EDIT_GATEWAY,
};

static struct CONTROL_ANCHOR anchor_settings[]={
	{IDC_USER_NAME,ANCHOR_LEFT|ANCHOR_RIGHT|ANCHOR_TOP,0,0,0},
	{IDC_PASSWORD,ANCHOR_LEFT|ANCHOR_RIGHT|ANCHOR_TOP,0,0,0},
	{IDC_SHOW_PASSWORD,ANCHOR_RIGHT|ANCHOR_TOP,0,0,0},
	{IDC_SAVE_SETTINGS,ANCHOR_RIGHT|ANCHOR_BOTTOM,0,0,0},
};
static struct CONTROL_ANCHOR anchor_log_irc[]={
	{IDC_EDIT_IRC_LOG,ANCHOR_LEFT|ANCHOR_RIGHT|ANCHOR_TOP|ANCHOR_BOTTOM,0,0,0},
};
static struct CONTROL_ANCHOR anchor_log_gateway[]={
	{IDC_EDIT_GATEWAY_LOG,ANCHOR_LEFT|ANCHOR_RIGHT|ANCHOR_TOP|ANCHOR_BOTTOM,0,0,0},
};
static struct CONTROL_ANCHOR anchor_log_discord[]={
	{IDC_EDIT_DISCORD_LOG,ANCHOR_LEFT|ANCHOR_RIGHT|ANCHOR_TOP|ANCHOR_BOTTOM,0,0,0},
};
static struct CONTROL_ANCHOR anchor_main[]={
	{IDC_TAB_SHEET,ANCHOR_LEFT|ANCHOR_RIGHT|ANCHOR_TOP|ANCHOR_BOTTOM,0,0,0},
	{IDOK,ANCHOR_LEFT|ANCHOR_BOTTOM,0,0,0},
	{IDCANCEL,ANCHOR_RIGHT|ANCHOR_BOTTOM,0,0,0},
};

static int print_lasterror()
{
	int error = GetLastError();
	char buffer[128];
	if (FormatMessageA(FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, error, 0, buffer, sizeof(buffer)/sizeof(char), NULL))
		printf("error=%s\n",buffer);
	return 0;
}

static BOOL CALLBACK settings_func(HWND hwnd,UINT msg, WPARAM wparam, LPARAM lparam)
{
	switch(msg){
	case WM_INITDIALOG:
		{
		}
		break;
	case WM_SIZE:
		AnchorResize(hwnd,anchor_settings,_countof(anchor_settings));
		break;
	case WM_APP:
		if(0==wparam){
			AnchorInit(hwnd,anchor_settings,_countof(anchor_settings));
		}
		break;
	}
	return FALSE;
}

static BOOL CALLBACK log_irc_func(HWND hwnd,UINT msg, WPARAM wparam, LPARAM lparam)
{
	switch(msg){
	case WM_INITDIALOG:
	{
	}
	break;
	case WM_SIZE:
		AnchorResize(hwnd,anchor_log_irc,_countof(anchor_log_irc));
		break;
	case WM_APP:
		if(0==wparam){
			AnchorInit(hwnd,anchor_log_irc,_countof(anchor_log_irc));
		}
		break;
	}
	return FALSE;
}
static BOOL CALLBACK log_gateway_func(HWND hwnd,UINT msg, WPARAM wparam, LPARAM lparam)
{
	switch(msg){
	case WM_INITDIALOG:
	{
	}
	break;
	case WM_SIZE:
		AnchorResize(hwnd,anchor_log_gateway,_countof(anchor_log_gateway));
		break;
	case WM_APP:
		if(0==wparam){
			AnchorInit(hwnd,anchor_log_gateway,_countof(anchor_log_gateway));
		}
		break;
	}
	return FALSE;
}
static BOOL CALLBACK log_discord_func(HWND hwnd,UINT msg, WPARAM wparam, LPARAM lparam)
{
	switch(msg){
	case WM_INITDIALOG:
	{
	}
	break;
	case WM_SIZE:
		AnchorResize(hwnd,anchor_log_discord,_countof(anchor_log_discord));
		break;
	case WM_APP:
		if(0==wparam){
			AnchorInit(hwnd,anchor_log_discord,_countof(anchor_log_discord));
		}
		break;
	}
	return FALSE;
}

static int resize_dlg(HWND htab,HWND hdlg)
{
	int w,h;
	RECT rect={0};
	if(0==htab || 0==hdlg)
		return 0;
	GetClientRect(htab,&rect);
	TabCtrl_AdjustRect(htab,FALSE,&rect);
	w=rect.right-rect.left;
	h=rect.bottom-rect.top;
	return SetWindowPos(hdlg,NULL,rect.left,rect.top,w,h,SWP_NOZORDER);
}
static int resize_all_tab()
{
	int i,count;
	count=_countof(tab_data);
	for(i=0;i<count;i++){
		TAB_DATA *tmp=&tab_data[i];
		if(tmp->hwnd)
			resize_dlg(tmp->hparent,tmp->hwnd);
	}
	return 0;
}
static int show_tab_index(const int index)
{
	int i,count;
	count=_countof(tab_data);
	for(i=0;i<count;i++){
		TAB_DATA *tmp=&tab_data[i];
		int flag=SW_HIDE;
		if(0==tmp->hwnd)
			continue;
		if(index==i)
			flag=SW_SHOW;
		ShowWindow(tmp->hwnd,flag);
	}
	return 0;
}

static HWND add_tab_page(HWND htab,int idd,DLGPROC dlg_proc,char *text)
{
	HWND result=0;
	HWND hdlg=0;
	int count;
	TC_ITEM item={0};
	count=TabCtrl_GetItemCount(htab);
	if(count<_countof(tab_data)){
		int res;
		hdlg=CreateDialog(g_hinstance,MAKEINTRESOURCE(idd),htab,dlg_proc);
		if(0==hdlg)
			return result;
		item.mask=TCIF_TEXT|TCIF_PARAM;
		item.pszText=text;
		res=TabCtrl_InsertItem(htab,count,&item);
		if(res>=0){
			TAB_DATA *tmp=&tab_data[count];
			tmp->hparent=htab; 
			tmp->hwnd=hdlg;
			tmp->IDD=idd;
			strncpy(tmp->text,text,sizeof(tmp->text));
			tmp->text[sizeof(tmp->text)-1]=0;
			SendMessage(hdlg,WM_APP,0,0);
			resize_dlg(htab,hdlg);
			result=hdlg;
		}
	}
	return result;
}

static int get_hedit_list()
{
	int id_list[]={
		HWND_EDIT_IRC,IDD_LOG_IRC,IDC_EDIT_IRC_LOG,
		HWND_EDIT_DISCORD,IDD_LOG_DISCORD,IDC_EDIT_DISCORD_LOG,
		HWND_EDIT_GATEWAY,IDD_LOG_GATEWAY,IDC_EDIT_GATEWAY_LOG,
	};
	int i,count;
	count=_countof(id_list);
	for(i=0;i<count;i+=3){
		int j,tab_count;
		tab_count=_countof(tab_data);
		for(j=0;j<tab_count;j++){
			TAB_DATA *tmp=&tab_data[j];
			int hedit_index=id_list[i];
			int idd=        id_list[i+1];
			int id_edit=    id_list[i+2];
			if(tmp->IDD==idd){
				hedit_list[hedit_index]=GetDlgItem(tmp->hwnd,id_edit);
				break;
			}
		}
	}
	return 0;
}
static void set_hedit_str(int index,const char *str)
{
	HWND hedit=hedit_list[index];
	if(0==hedit || 0==str)
		return;
	SendMessage(hedit,EM_SETSEL,-1,-1);
	SendMessage(hedit,EM_REPLACESEL,FALSE,(LPARAM)str);
}
void add_line_irc_log(const char *str)
{
	set_hedit_str(HWND_EDIT_IRC,str);
}
void add_line_discord_log(const char *str)
{
	set_hedit_str(HWND_EDIT_DISCORD,str);
}
void add_line_gateway_log(const char *str)
{
	set_hedit_str(HWND_EDIT_GATEWAY,str);
}

int init_settings(HWND hwnd)
{
	int result=FALSE;
	typedef struct{
		int id;
		const char *(*func)();
	}SLIST;
	SLIST list[]={
		{IDC_USER_NAME,&get_user_name},
		{IDC_PASSWORD,&get_password},
	};
	int i,count;
	if(0==hwnd)
		return result;
	count=_countof(list);
	for(i=0;i<count;i++){
		SLIST *blah=&list[i];
		const char *str;
		WCHAR *tmp;
		str=blah->func();
		tmp=utf2wchar(str);
		if(tmp){
			int id=blah->id;
			SetDlgItemTextW(hwnd,id,tmp);
			free(tmp);
			result=TRUE;
		}
	}
	return result;
}

static BOOL CALLBACK dlg_func(HWND hwnd,UINT msg, WPARAM wparam, LPARAM lparam)
{
	switch(msg){
	case WM_INITDIALOG:
		{
			HWND hsettings;
			HWND htab=GetDlgItem(hwnd,IDC_TAB_SHEET);
			SetWindowPos(hwnd,NULL,500,500,0,0,SWP_NOSIZE|SWP_SHOWWINDOW|SWP_NOZORDER);
			AnchorInit(hwnd,anchor_main,_countof(anchor_main));
			hsettings=add_tab_page(htab,IDD_SETTINGS,&settings_func,"settings");
			add_tab_page(htab,IDD_LOG_IRC,&log_irc_func,"IRC log");
			add_tab_page(htab,IDD_LOG_DISCORD,&log_discord_func,"discord log");
			add_tab_page(htab,IDD_LOG_GATEWAY,&log_gateway_func,"gateway log");
			TabCtrl_SetCurFocus(htab,0);
			show_tab_index(0);
			get_hedit_list();
			init_settings(hsettings);
			//start_discord();
	}
		break;
	case WM_NOTIFY:
		{
			LPNMHDR hdr=(LPNMHDR)lparam;
			if(0==hdr)
				break;
			if(IDC_TAB_SHEET==hdr->idFrom){
				if(TCN_SELCHANGE==hdr->code){
					int index=TabCtrl_GetCurSel(GetDlgItem(hwnd,IDC_TAB_SHEET));
					show_tab_index(index);
				}
			}
		}
		break;
	case WM_MOVE:
		break;
	case WM_SIZE:
		{
			AnchorResize(hwnd,anchor_main,_countof(anchor_main));
			resize_all_tab();
		}
		break;
	case WM_SIZING:
		{
			RECT dsize={0,0,400,400};
			RECT *size=(RECT*)lparam;
			if(size){
				return ClampMinWindowSize(&dsize,wparam,size);
			}
		}
		break;
	case WM_COMMAND:
		{
			int id = LOWORD(wparam);
			switch(id){
			case IDOK:
				break;
			case IDCANCEL:
				EndDialog(hwnd,0);
				break;
			}
		}
		break;
	}
	return FALSE;
}
int APIENTRY WinMain(HINSTANCE hinst,HINSTANCE hprev,LPSTR cmd_line,int cmd_show)
{
	g_hinstance=hinst;
	InitCommonControls();
	DialogBox(hinst,MAKEINTRESOURCE(IDD_MAIN_DLG),NULL,dlg_func);
}
