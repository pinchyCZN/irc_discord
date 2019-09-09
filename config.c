#include <Windows.h>
#include <stdio.h>

#pragma warning(disable:4996)

char g_token[512]={0};
char g_gateway[80]={0};
int g_timeout=5000;

CRITICAL_SECTION g_mutex;

#define INI_FNAME L"irc_discord.ini"
//find ini file first in current directory then exe folder

int gui_active=FALSE;
int console_active=FALSE;
int enable_dbg_discord=FALSE;
int enable_dbg_gateway=FALSE;
int enable_dbg_irc=FALSE;


static file_exists(const WCHAR *fname)
{
	int result=FALSE;
	DWORD attrib;
	attrib=GetFileAttributesW(fname);
	if(MAXDWORD!=attrib){
		DWORD dir=(attrib&FILE_ATTRIBUTE_DIRECTORY)!=0;
		if(!dir){
			result=TRUE;
		}
	}
	return result;
}

static int join_path(WCHAR *p1,int p1_count,WCHAR *p2)
{
	const WCHAR *sep=L"";
	int p1len;
	p1len=wcslen(p1);
	if(p1len>0){
		if(L'\\'!=p1[p1len-1]){
			sep=L"\\";
		}
	}
	_snwprintf(p1,p1_count,L"%s%s%s",p1,sep,p2);
	if(p1_count>0)
		p1[p1_count-1]=0;
	return TRUE;
}

static int get_exe_folder(WCHAR *path,int path_count)
{
	int result=FALSE;
	DWORD res;
	res=GetModuleFileNameW(NULL,path,path_count);
	if(res){
		int i,plen=wcslen(path);
		for(i=plen-1;i>=0;i--){
			WCHAR a;
			a=path[i];
			if(L'\\'==a){
				path[i]=0;
				break;
			}
		}
		result=TRUE;
	}
	return result;
}
static int get_cur_dir_ini_fname(WCHAR *fname,int fname_count)
{
	int result=FALSE;
	DWORD res;
	res=GetCurrentDirectoryW(fname_count,fname);
	if(res){
		join_path(fname,fname_count,INI_FNAME);
		result=TRUE;
	}
	return result;
}

static int get_exe_dir_ini_fname(WCHAR *fname,int fname_count)
{
	int result=FALSE;
	DWORD res;
	res=get_exe_folder(fname,fname_count);
	if(res){
		join_path(fname,fname_count,INI_FNAME);
		result=TRUE;
	}
	return result;
}


static WCHAR *get_ini_fname()
{
	static WCHAR *result=0;
	static WCHAR fname[4096]={0};
	int fname_count;
	DWORD res;
	fname_count=sizeof(fname)/sizeof(WCHAR);
	res=get_cur_dir_ini_fname(fname,fname_count);
	if(res){
		if(file_exists(fname)){
			result=fname;
		}else{
			res=get_exe_dir_ini_fname(fname,fname_count);
			if(res){
				if(file_exists(fname)){
					result=fname;
				}
			}
		}
	}
	return result;
}

static int get_ini_value(const WCHAR *section,const WCHAR *key,char *out,int out_len)
{
	int result=FALSE;
	const WCHAR *fname;
	fname=get_ini_fname();
	if(fname){
		DWORD res;
		static WCHAR tmp[4096]={0};
		int tmp_count=sizeof(tmp)/sizeof(WCHAR);
		memset(tmp,0,sizeof(tmp));
		res=GetPrivateProfileStringW(section,key,L"",tmp,tmp_count,fname);
		if(res){
			WideCharToMultiByte(CP_UTF8,0,tmp,-1,out,out_len,NULL,NULL);
			if(out_len)
				out[out_len-1]=0;
			result=TRUE;
		}
	}
	return result;
}

static int get_ini_wchar_str(const WCHAR *section,const WCHAR *key,WCHAR *out,int out_count)
{
	int result=FALSE;
	const WCHAR *fname;
	fname=get_ini_fname();
	if(fname){
		DWORD res;
		res=GetPrivateProfileStringW(section,key,L"",out,out_count,fname);
		if(res){
			if(out_count)
				out[out_count-1]=0;
			result=TRUE;
		}
	}
	return result;

}

static int set_ini_value(const WCHAR *section,const WCHAR *key,const WCHAR *str)
{
	int result=FALSE;
	const WCHAR *fname;
	fname=get_ini_fname();
	if(fname){
		DWORD res;
		res=WritePrivateProfileStringW(section,key,str,fname);
		result=res;
	}
	return result;
}

int save_user_name(const WCHAR *user)
{
	return set_ini_value(L"SETTINGS",L"USERNAME",user);
}
int save_password(const WCHAR *password)
{
	return set_ini_value(L"SETTINGS",L"PASSWORD",password);
}

const char *get_user_name()
{
	static char user[255]={0};
	memset(user,0,sizeof(user));
	get_ini_value(L"SETTINGS",L"USERNAME",user,sizeof(user));
	return user;
}

const char *get_password()
{
	static char password[255]={0};
	memset(password,0,sizeof(password));
	get_ini_value(L"SETTINGS",L"PASSWORD",password,sizeof(password));
	return password;
}

int get_irc_port()
{
	const int default_port=6667;
	int result=default_port;
	static char tmp[80]={0};
	get_ini_value(L"SETTINGS",L"IRC_PORT",tmp,sizeof(tmp));
	if(tmp[0]!=0){
		result=atoi(tmp);
		if(result<=0)
			result=default_port;
	}
	return result;
}
int save_irc_port(int port)
{
	WCHAR tmp[20]={0};
	_snwprintf(tmp,_countof(tmp),L"%u",port);
	return set_ini_value(L"SETTINGS",L"IRC_PORT",tmp);
}

int save_window_pos(WINDOWPLACEMENT *win)
{
	int result=FALSE;
	struct SLIST{
		int val;
		const WCHAR *key;
	};
	struct SLIST list[]={
		{win->rcNormalPosition.left,L"LEFT"},
		{win->rcNormalPosition.right,L"RIGHT"},
		{win->rcNormalPosition.top,L"TOP"},
		{win->rcNormalPosition.bottom,L"BOTTOM"},
		{win->showCmd,L"SHOWCMD"},
	};
	int i,count;
	count=_countof(list);
	for(i=0;i<count;i++){
		struct SLIST *tmp=&list[i];
		WCHAR str[20]={0};
		_snwprintf(str,_countof(str),L"%i",tmp->val);
		result=set_ini_value(L"WINDOW_POS",tmp->key,str);
	}
	return result;
}
int load_window_pos(WINDOWPLACEMENT *win)
{
	int result=FALSE;
	struct SLIST{
		int *val;
		const WCHAR *key;
	};
	struct SLIST list[]={
		{&win->rcNormalPosition.left,L"LEFT"},
		{&win->rcNormalPosition.right,L"RIGHT"},
		{&win->rcNormalPosition.top,L"TOP"},
		{&win->rcNormalPosition.bottom,L"BOTTOM"},
		{&win->showCmd,L"SHOWCMD"},
	};
	int i,count,found=0;
	count=_countof(list);
	for(i=0;i<count;i++){
		struct SLIST *tmp=&list[i];
		WCHAR str[20]={0};
		int res;
		res=get_ini_wchar_str(L"WINDOW_POS",tmp->key,str,_countof(str));
		if(res){
			tmp->val[0]=_wtoi(str);
			found++;
		}
	}
	if(found>=count)
		result=TRUE;
	return result;
}

int load_connect_on_start()
{
	int result=FALSE;
	WCHAR tmp[20]={0};
	int res;
	res=get_ini_wchar_str(L"SETTINGS",L"CONNECT_ON_START",tmp,_countof(tmp));
	if(res){
		result=_wtoi(tmp);
	}
	return result;
}
int save_connect_on_start(int val)
{
	WCHAR tmp[20]={0};
	_snwprintf(tmp,_countof(tmp),L"%u",val&1);
	return set_ini_value(L"SETTINGS",L"CONNECT_ON_START",tmp);
}


int validate_ini(int show_msgbox)
{
	int result=FALSE;
	WCHAR *f1name=0;
	WCHAR *f2name=0;
	WCHAR *msg=0;
	int fname_count=4096;
	int msg_count=1024*64;
	WCHAR *ini_fname=0;
	ini_fname=get_ini_fname();
	if(0==ini_fname){
		f1name=calloc(fname_count,sizeof(WCHAR));
		f2name=calloc(fname_count,sizeof(WCHAR));
		msg=calloc(msg_count,sizeof(WCHAR));
		if(0==msg || 0==f1name || 0==f2name){
			goto FUNC_ERROR;
		}
		get_cur_dir_ini_fname(f1name,fname_count);
		get_exe_dir_ini_fname(f2name,fname_count);
		_snwprintf(msg,msg_count,L"INI not found in either:\r\n%s\r\n%s\r\n",f1name,f2name);
		msg[msg_count-1]=0;
		if(show_msgbox){
			MessageBoxW(NULL,msg,L"INI not found\n",MB_OK|MB_SYSTEMMODAL);
		}else{
			printf("%S",msg);
		}
	}else{
		//check if values are populated
		int no_user=FALSE;
		int no_pass=FALSE;
		const char *tmp;
		tmp=get_user_name();
		if(0==tmp[0])
			no_user=TRUE;
		tmp=get_password();
		if(0==tmp[0])
			no_pass=TRUE;
		if(no_user || no_pass){
			msg=calloc(msg_count,sizeof(WCHAR));
			if(msg){
				_snwprintf(msg,msg_count,L"%s%s",no_user?L"No user name found in INI\r\n":L"",no_pass?L"No password found in INI\r\n":L"");
				if(show_msgbox){
					MessageBoxW(NULL,msg,L"INI Settings error",MB_OK|MB_SYSTEMMODAL);
				}else{
					printf("%S",msg);
				}
			}
		}
	}

FUNC_ERROR:
	if(msg)free(msg);
	if(f1name)free(f1name);
	if(f2name)free(f2name);
	return result;
}