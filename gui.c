#include <Windows.h>
#include <CommCtrl.h>
#include "resource.h"
HINSTANCE g_hinstance=0;

typedef struct{
	int index;
	HWND hwnd;
	char text[80];
}TAB_DATA;

TAB_DATA tab_data[8]={0};

BOOL CALLBACK settings_func(HWND hwnd,UINT msg, WPARAM wparam, LPARAM lparam)
{
	return FALSE;
}

int add_tab_page(HWND htab,int idd,DLGPROC dlg_proc,char *text)
{
	int result=FALSE;
	HWND hdlg=0;
	int count;
	TC_ITEM item={0};
	count=TabCtrl_GetItemCount(htab);
	hdlg=CreateDialog(g_hinstance,MAKEINTRESOURCE(idd),htab,dlg_proc);
	if(0==hdlg)
		return result;
	ShowWindow(hdlg,SW_SHOWNORMAL);
	item.mask=TCIF_TEXT|TCIF_PARAM;
	item.pszText=text;
	result=TabCtrl_InsertItem(htab,count,&item);
	return result;
}

BOOL CALLBACK dlg_func(HWND hwnd,UINT msg, WPARAM wparam, LPARAM lparam)
{
	switch(msg){
	case WM_INITDIALOG:
		{
			
			HWND htab=GetDlgItem(hwnd,IDC_TAB_SHEET);
			add_tab_page(htab,IDD_SETTINGS,dlg_func,"settings");
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
