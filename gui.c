#include <Windows.h>
#include <CommCtrl.h>
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include "resource.h"

#include "anchor_system.h"
#include "discord.h"
#include "config.h"
#include "libstring.h"

#pragma warning(disable:4996)
#ifdef _DEBUG
#	define DBGPRINT printf
#else
#	define DBGPRINT void
#endif

static HINSTANCE g_hinstance=0;

typedef struct{
	HWND hwnd;
	HWND hparent;
	int  IDD;
	char text[80];
}TAB_DATA;

static TAB_DATA tab_data[8]={0};

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
	{IDC_RELOAD,ANCHOR_LEFT|ANCHOR_BOTTOM,0,0,0},
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
	{IDC_HIDE,ANCHOR_LEFT|ANCHOR_BOTTOM,0,0,0},
	{IDC_EXIT,ANCHOR_RIGHT|ANCHOR_BOTTOM,0,0,0},
};

void open_console()
{
	static int consoleallocated=FALSE;
	static int consolecreated=FALSE;
	HWND hwnd;
	static int hcrt=0;
	if(!consoleallocated){
		consoleallocated=AllocConsole();
	}
	hwnd=GetConsoleWindow();
	if(0==hwnd)
		return;
	if(consolecreated){
		HWND hcon;
		ShowWindow(hwnd,SW_SHOW);
		SetForegroundWindow(hwnd);
		hcon=(HWND)GetStdHandle(STD_INPUT_HANDLE);
		FlushConsoleInputBuffer(hcon);
		return;
	}
	if(0==hcrt){
		hcrt=_open_osfhandle((long)GetStdHandle(STD_OUTPUT_HANDLE),_O_TEXT);
	}
	if(hcrt!=0){
		static FILE *hf=0;
		ShowWindow(hwnd,SW_SHOW);
		SetForegroundWindow(hwnd);
		fflush(stdin);
		if(0==hf){
			hf=_fdopen(hcrt,"w");
		}
		if(hf){
			FILE *f=stdout;
			memcpy(f,hf,5*sizeof(int)+3*sizeof(void*));
			//*f=*hf;
			setvbuf(stdout,NULL,_IONBF,0);
			consolecreated=TRUE;
		}
	}
}
static void add_hedit_str(int index,const char *str)
{
	HWND hedit=hedit_list[index];
	if(0==hedit || 0==str)
		return;
	SendMessage(hedit,EM_SETSEL,-1,-1);
	SendMessage(hedit,EM_REPLACESEL,FALSE,(LPARAM)str);
}
void add_line_irc_log(const char *str)
{
	add_hedit_str(HWND_EDIT_IRC,str);
}
void add_line_discord_log(const char *str)
{
	add_hedit_str(HWND_EDIT_DISCORD,str);
}
void add_line_gateway_log(const char *str)
{
	add_hedit_str(HWND_EDIT_GATEWAY,str);
}

static int print_lasterror()
{
	int error = GetLastError();
	char buffer[128];
	if (FormatMessageA(FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, error, 0, buffer, sizeof(buffer)/sizeof(char), NULL))
		printf("error=%s\n",buffer);
	return 0;
}

static int init_settings(HWND hwnd)
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
	int id_list[]={
		IDC_USER_NAME,IDC_PASSWORD,IDC_IRC_PORT
	};
	int i,count;
	if(0==hwnd)
		return result;
	count=_countof(id_list);
	for(i=0;i<count;i++){
		SendDlgItemMessage(hwnd,id_list[i],EM_SETLIMITTEXT,512,0);
	}
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
	{
		int flag=BST_UNCHECKED;
		if(load_connect_on_start())
			flag=BST_CHECKED;
		CheckDlgButton(hwnd,IDC_CONNECT_ON_START,flag);
	}
	{
		int val=get_irc_port();
		WCHAR tmp[20]={0};
		_snwprintf(tmp,_countof(tmp),L"%u",val);
		SetDlgItemTextW(hwnd,IDC_IRC_PORT,tmp);
	}
	return result;
}

static int save_settings(HWND hwnd)
{
	int result=FALSE;
	typedef struct{
		int id;
		union f{
			int (*func1)(WCHAR*);
			int (*func2)(int);
		};
		int type;
	}SLIST;
#pragma warning(push)
#pragma warning(disable:4028)
	SLIST list[]={
		{IDC_USER_NAME,&save_user_name,0},
		{IDC_PASSWORD,&save_password,0},
		{IDC_CONNECT_ON_START,&save_connect_on_start,1},
		{IDC_IRC_PORT,&save_irc_port,2},
	};
#pragma warning(pop)
	int i,count;
	if(0==hwnd)
		return result;
	count=_countof(list);
	result=TRUE;
	for(i=0;i<count;i++){
		int res=FALSE;
		SLIST *blah=&list[i];
		int type=blah->type;
		if(0==type){
			WCHAR str[256]={0};
			GetDlgItemTextW(hwnd,blah->id,str,_countof(str));
			res=blah->func1(str);
		}else if(1==type){
			int val=0;
			res=IsDlgButtonChecked(hwnd,blah->id);
			if(BST_CHECKED==res)
				val=1;
			res=blah->func2(val);
		}else if(2==type){
			WCHAR tmp[20]={0};
			int val;
			GetDlgItemTextW(hwnd,blah->id,tmp,_countof(tmp));
			val=_wtoi(tmp);
			res=blah->func2(val);
		}
		if(!res){
			result=FALSE;
		}
	}
	return result;
}

static int validate_settings(HWND hwnd)
{
	int result=TRUE;
	int list[]={IDC_USER_NAME,IDC_PASSWORD,IDC_IRC_PORT};
	int i,count;
	count=_countof(list);
	for(i=0;i<count;i++){
		char tmp[80]={0};
		int id=list[i];
		GetDlgItemTextA(hwnd,id,tmp,sizeof(tmp));
		if(0==tmp[0]){
			result=FALSE;
			break;
		}
	}
	return result;
}

static WNDPROC orig_edit_proc=0;
static LRESULT APIENTRY edit_ctrl_proc(HWND hwnd,UINT msg,WPARAM wparam,LPARAM lparam)
{
	switch(msg){
	case WM_KEYDOWN:
		if('A'==wparam){
			int k=GetKeyState(VK_CONTROL)&0x8000;
			if(k){
				SendMessage(hwnd,EM_SETSEL,0,-1);
				return 0;
			}
		}
		break;
	}
	return CallWindowProc(orig_edit_proc,hwnd,msg,wparam,lparam);
}

static BOOL CALLBACK settings_func(HWND hwnd,UINT msg, WPARAM wparam, LPARAM lparam)
{
	switch(msg){
	case WM_INITDIALOG:
		{
			HWND hpwd=GetDlgItem(hwnd,IDC_PASSWORD);
			if(hpwd){
				const WCHAR *g=(WCHAR*)"\x95\x00";
				SendMessage(hpwd,EM_SETPASSWORDCHAR,g[0],0);
			}
			init_settings(hwnd);
			if(BST_CHECKED==IsDlgButtonChecked(hwnd,IDC_CONNECT_ON_START)){
				if(validate_settings(hwnd)){
					CheckDlgButton(hwnd,IDC_ENABLE_DISCORD,BST_CHECKED);
					start_discord();
				}else{
					CheckDlgButton(hwnd,IDC_CONNECT_ON_START,BST_UNCHECKED);
				}
			}
			orig_edit_proc=(WNDPROC)SetWindowLongPtr(GetDlgItem(hwnd,IDC_USER_NAME),GWL_WNDPROC,(LONG_PTR)&edit_ctrl_proc);
			orig_edit_proc=(WNDPROC)SetWindowLongPtr(GetDlgItem(hwnd,IDC_PASSWORD),GWL_WNDPROC,(LONG_PTR)&edit_ctrl_proc);
			orig_edit_proc=(WNDPROC)SetWindowLongPtr(GetDlgItem(hwnd,IDC_IRC_PORT),GWL_WNDPROC,(LONG_PTR)&edit_ctrl_proc);
	}
		break;
	case WM_SHOWWINDOW:
		if(wparam && 0==lparam)
		{
			HWND hbutton=GetDlgItem(hwnd,IDC_ENABLE_DISCORD);
			if(hbutton){
				SetFocus(hbutton);
			}
		}
		break;
	case WM_COMMAND:
		switch(LOWORD(wparam)){
		case IDC_SHOW_PASSWORD:
			{
				int res=IsDlgButtonChecked(hwnd,IDC_SHOW_PASSWORD);
				if(BST_CHECKED==res){
					SendDlgItemMessage(hwnd,IDC_PASSWORD,EM_SETPASSWORDCHAR,0,0);
				}else{
					const WCHAR *g=(WCHAR*)"\x95\x00";
					SendDlgItemMessage(hwnd,IDC_PASSWORD,EM_SETPASSWORDCHAR,g[0],0);
				}
				InvalidateRect(GetDlgItem(hwnd,IDC_PASSWORD),NULL,TRUE);
			}
			break;
		case IDC_SAVE_SETTINGS:
			{
				int code=HIWORD(wparam);
				if(BN_CLICKED==code){
					if(validate_ini(hwnd,TRUE)){
						int res=save_settings(hwnd);
						if(!res){
							MessageBoxA(hwnd,"Failed to save settings to INI","ERROR",MB_OK|MB_SYSTEMMODAL);
						}
					}
				}
			}
			break;
		case IDC_ENABLE_DISCORD:
			{
				int res=IsDlgButtonChecked(hwnd,IDC_ENABLE_DISCORD);
				if(BST_CHECKED==res){
					const char *msg=0;
					if(validate_settings(hwnd)){
						if(save_settings(hwnd)){
							start_discord();
						}else{
							msg="Unable to save settings";
						}
					}else{
						msg="Settings not valid";
					}
					if(msg){
						CheckDlgButton(hwnd,IDC_ENABLE_DISCORD,BST_UNCHECKED);
						MessageBoxA(hwnd,msg,"ERROR",MB_OK|MB_SYSTEMMODAL);
					}
				}
			}
			break;
		case IDC_RELOAD:
			init_settings(hwnd);
			break;
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
	case WM_SHOWWINDOW:
		if(wparam && 0==lparam)
		{
			HWND hedit=GetDlgItem(hwnd,IDC_EDIT_IRC_LOG);
			if(hedit){
				SetFocus(hedit);
			}
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
	case WM_SHOWWINDOW:
		if(wparam && 0==lparam)
		{
			HWND hedit=GetDlgItem(hwnd,IDC_EDIT_GATEWAY_LOG);
			if(hedit){
				SetFocus(hedit);
			}
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
	case WM_SHOWWINDOW:
		if(wparam && 0==lparam)
		{
			HWND hedit=GetDlgItem(hwnd,IDC_EDIT_DISCORD_LOG);
			if(hedit){
				SetFocus(hedit);
			}
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

static void next_tab(HWND hwnd,int dir)
{
	HWND htab;
	int index,count;
	htab=GetDlgItem(hwnd,IDC_TAB_SHEET);
	if(0==htab)
		return;
	index=TabCtrl_GetCurSel(htab);
	count=TabCtrl_GetItemCount(htab);
	if(index<0 || count<=0)
		return;
	index+=dir;
	if(index<0)
		index=count-1;
	else if(index>=count)
		index=0;
	TabCtrl_SetCurSel(htab,index);
	show_tab_index(index);
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

static int init_dlg_pos(HWND hwnd)
{
	WINDOWPLACEMENT wp={0};
	RECT rect={0};
	int w,h;
	GetWindowRect(hwnd,&rect);
	w=rect.right-rect.left;
	h=rect.bottom-rect.top;
	if(load_window_pos(&wp)){
		wp.length=sizeof(wp);
		wp.showCmd=SW_SHOWNORMAL;
		wp.flags=0;
		clamp_min_rect(&wp.rcNormalPosition,w*7/8,h*7/8);
		clamp_max_rect(&wp.rcNormalPosition,w*2,h*2);
		clamp_nearest_screen(&wp.rcNormalPosition);
		SetWindowPlacement(hwnd,&wp);
	}else{ //center screen
		int show_def=TRUE;
		int x=0,y=0;
		if(w>100 && h>100){
			HWND htmp=GetDesktopWindow();
			if(htmp){
				int sw,sh;
				GetClientRect(htmp,&rect);
				sw=rect.right-rect.left;
				sh=rect.bottom-rect.top;
				x=sw/2-w/2;
				y=sh/2-h/2;
				rect.left=x;
				rect.top=y;
				rect.right=rect.left+w;
				rect.bottom=rect.top+h;
				clamp_nearest_screen(&rect);
				x=rect.left;
				y=rect.top;
				w=rect.right-rect.left;
				h=rect.bottom-rect.top;
				show_def=FALSE;
			}
		}
		if(show_def){
			ShowWindow(hwnd,SW_SHOW);
		}else{
			SetWindowPos(hwnd,NULL,x,y,w,h,SWP_NOZORDER|SWP_SHOWWINDOW);
		}
	}
	return TRUE;
}

static int create_tray_icon(HWND hwnd,HICON hicon)
{
	NOTIFYICONDATA nid={0};
	nid.cbSize = sizeof(NOTIFYICONDATA);
	nid.hWnd = hwnd;
	nid.uID = IDC_TRAY_ICON;
	nid.uCallbackMessage = WM_APP;
	nid.hIcon = hicon;
	nid.uFlags = NIF_ICON|NIF_MESSAGE;
	return Shell_NotifyIcon(NIM_ADD,&nid);
}

static int delete_tray_icon(HWND hwnd)
{
	NOTIFYICONDATA nid={0};
	nid.cbSize = sizeof(NOTIFYICONDATA);
	nid.hWnd = hwnd;
	nid.uID = IDC_TRAY_ICON;
	return Shell_NotifyIcon(NIM_DELETE,&nid);
}

static int end_dialog(HWND hwnd)
{
	WINDOWPLACEMENT wp={0};
	wp.length=sizeof(wp);
	GetWindowPlacement(hwnd,&wp);
	save_window_pos(&wp);
	delete_tray_icon(hwnd);
	PostQuitMessage(0);
	return TRUE;
}

static BOOL CALLBACK dlg_func(HWND hwnd,UINT msg, WPARAM wparam, LPARAM lparam)
{
#ifdef _DEBUG
	DBGPRINT(">msg:%04X %08X %08X %08X\n",msg,hwnd,lparam,wparam);
#endif
	switch(msg){
	case WM_INITDIALOG:
		{
			HICON hicon;
			HWND htab=GetDlgItem(hwnd,IDC_TAB_SHEET);
			SetWindowLong(htab,GWL_EXSTYLE,GetWindowLong(hwnd,GWL_EXSTYLE)|WS_EX_CONTROLPARENT);
			AnchorInit(hwnd,anchor_main,_countof(anchor_main));
			add_tab_page(htab,IDD_SETTINGS,&settings_func,"settings");
			add_tab_page(htab,IDD_LOG_IRC,&log_irc_func,"IRC log");
			add_tab_page(htab,IDD_LOG_DISCORD,&log_discord_func,"discord log");
			add_tab_page(htab,IDD_LOG_GATEWAY,&log_gateway_func,"gateway log");
			TabCtrl_SetCurFocus(htab,0);
			show_tab_index(0);
			get_hedit_list();
			init_dlg_pos(hwnd);
			hicon=LoadIcon(g_hinstance,MAKEINTRESOURCE(IDI_ICON1));
			if(hicon){
				SendMessage(hwnd,WM_SETICON,ICON_BIG,(LPARAM)hicon);
				SendMessage(hwnd,WM_SETICON,ICON_SMALL,(LPARAM)hicon);
			}
			create_tray_icon(hwnd,hicon);
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
	case WM_APP: //system tray msg
		{
			int tmsg=lparam;
			if(WM_LBUTTONDOWN==tmsg){
				int flag=SW_SHOW;
				if(IsWindowVisible(hwnd))
					flag=SW_HIDE;
				ShowWindow(hwnd,flag);
			}
		}
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
	case WM_CLOSE:
		end_dialog(hwnd);
		break;
	case WM_COMMAND:
		{
			int id = LOWORD(wparam);
			switch(id){
			case IDOK:
			case IDCANCEL:
			case IDC_HIDE:
				ShowWindow(hwnd,SW_HIDE);
				break;
			case IDC_EXIT:
				end_dialog(hwnd);
				break;
			}
		}
		break;
	}
	return FALSE;
}

int APIENTRY WinMain(HINSTANCE hinst,HINSTANCE hprev,LPSTR cmd_line,int cmd_show)
{
	MSG msg;
	int res;
	HWND hwnd;
	gui_active=TRUE;
	g_hinstance=hinst;
	InitCommonControls();
	LoadLibrary("RICHED20.DLL");
	//open_console();
	hwnd=CreateDialog(g_hinstance,MAKEINTRESOURCE(IDD_MAIN_DLG),NULL,&dlg_func);
	if(0==hwnd){
		MessageBoxA(NULL,"Unable to create dialog","ERROR",MB_OK|MB_SYSTEMMODAL);
	}
	while((res = GetMessage(&msg,NULL,0,0)) != 0){
		if(res == -1){
			MessageBoxA(NULL,"Error processing message","ERROR",MB_OK|MB_SYSTEMMODAL);
			break;
		}
		if(WM_KEYDOWN==msg.message){
			int key=msg.wParam;
			if(VK_TAB==key){
				int ctrl;
				printf("tab key\n");
				ctrl=0x8000&GetKeyState(VK_CONTROL);
				if(ctrl){
					int dir=1;
					int shift=0x8000&GetKeyState(VK_SHIFT);
					if(shift){
						dir=-1;
					}
					next_tab(hwnd,dir);
					continue;
				}
			}else if(VK_F5==key){
				HWND htab=GetDlgItem(hwnd,IDC_TAB_SHEET);
				if(htab){
					HWND hsettings=tab_data[0].hwnd;
					if(hsettings){
						SendMessage(hsettings,WM_COMMAND,MAKEWPARAM(IDC_RELOAD,0),0);
						continue;
					}
				}
			}
		}
		if(!IsWindow(hwnd) || !IsDialogMessage(hwnd,&msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
}
