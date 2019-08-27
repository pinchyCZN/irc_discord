#include <Windows.h>
#include "resource.h"
HINSTANCE g_hinstance=0;

BOOL CALLBACK tab_func(HWND hwnd,UINT msg, WPARAM wparam, LPARAM lparam)
{
	return FALSE;
}

BOOL CALLBACK dlg_func(HWND hwnd,UINT msg, WPARAM wparam, LPARAM lparam)
{
	switch(msg){
	case WM_INITDIALOG:
		{
			
			HWND hdlg,hparent=GetDlgItem(hwnd,IDC_TAB_SHEET);
			hdlg=CreateDialog(g_hinstance,MAKEINTRESOURCE(IDD_SETTINGS),hparent,tab_func);
			ShowWindow(hdlg,SW_SHOW);
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
	DialogBox(hinst,MAKEINTRESOURCE(IDD_MAIN_DLG),NULL,dlg_func);
}
