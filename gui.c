#include <Windows.h>
#include <CommCtrl.h>
#include <stdio.h>
#include "resource.h"
#include "anchor_system.h"
#pragma warning(disable:4996)

HINSTANCE g_hinstance=0;

typedef struct{
	HWND hwnd;
	HWND hparent;
	char text[80];
}TAB_DATA;

TAB_DATA tab_data[8]={0};

static struct CONTROL_ANCHOR anchor_settings[]={
	{IDC_USER_NAME,ANCHOR_LEFT|ANCHOR_RIGHT|ANCHOR_TOP,0,0,0},
	{IDC_PASSWORD,ANCHOR_LEFT|ANCHOR_RIGHT|ANCHOR_TOP,0,0,0},
	{IDC_SHOW_PASSWORD,ANCHOR_RIGHT|ANCHOR_TOP,0,0,0},
	{IDC_SAVE_SETTINGS,ANCHOR_RIGHT|ANCHOR_BOTTOM,0,0,0},
};

int print_lasterror()
{
	int error = GetLastError();
	char buffer[128];
	if (FormatMessageA(FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, error, 0, buffer, sizeof(buffer)/sizeof(char), NULL))
		printf("error=%s\n",buffer);
	return 0;
}

int get_dlg_parent(const HWND hwnd,HWND *hparent)
{
	int i,count;
	count=_countof(tab_data);
	for(i=0;i<count;i++){
		TAB_DATA *tmp=&tab_data[i];
		if(hwnd==tmp->hwnd){
			hparent[0]=tmp->hparent;
			return TRUE;
		}
	}
	return FALSE;
}

BOOL CALLBACK settings_func(HWND hwnd,UINT msg, WPARAM wparam, LPARAM lparam)
{
	switch(msg){
	case WM_INITDIALOG:
		{
			HWND hparent=0;
			get_dlg_parent(hwnd,&hparent);
			if(hparent)
				AnchorInit(hparent,anchor_settings,_countof(anchor_settings));
		}
		break;
	}
	return FALSE;
}

int add_tab_page(HWND htab,int idd,DLGPROC dlg_proc,char *text)
{
	int result=FALSE;
	HWND hdlg=0;
	int count;
	TC_ITEM item={0};
	count=TabCtrl_GetItemCount(htab);
	if(count<_countof(tab_data)){
		TAB_DATA *tmp=&tab_data[count];
		tmp->hparent=htab; 
		strncpy(tmp->text,text,sizeof(tmp->text));
		tmp->text[sizeof(tmp->text)-1]=0;
		hdlg=CreateDialog(g_hinstance,MAKEINTRESOURCE(idd),htab,dlg_proc);
		if(0==hdlg)
			return result;
		tmp->hwnd=hdlg;
		SetWindowPos(hdlg,NULL,20,20,100,100,SWP_NOZORDER|SWP_NOSIZE|SWP_SHOWWINDOW);
		//ShowWindow(hdlg,SW_SHOWNORMAL);
		item.mask=TCIF_TEXT|TCIF_PARAM;
		item.pszText=text;
		result=TabCtrl_InsertItem(htab,count,&item);
	}
	return result;
}

BOOL CALLBACK dlg_func(HWND hwnd,UINT msg, WPARAM wparam, LPARAM lparam)
{
	switch(msg){
	case WM_INITDIALOG:
		{
			HWND htab=GetDlgItem(hwnd,IDC_TAB_SHEET);
			add_tab_page(htab,IDD_SETTINGS,&settings_func,"settings");
		}
		break;
	case WM_MOVE:
		break;
	case WM_SIZE:
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
	DialogBox(hinst,MAKEINTRESOURCE(IDD_MAIN_DLG),NULL,dlg_func);
}
