#include <Windows.h>
#include <stdio.h>
#include "libstring.h"

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
	res=get_exe_dir_ini_fname(fname,fname_count);
	if(res){
		result=fname;
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

static int rand_seed=0x1337;

static int xor_str(BYTE *data,int data_len)
{
	int i;
	srand(rand_seed);
	for(i=0;i<data_len;i++){
		BYTE a=data[i];
		a^=rand();
		data[i]=a;
	}
	return TRUE;
}

//allocates string
static char *data_to_hex(BYTE *data,int data_len)
{
	int i,len;
	char *tmp;
	len=data_len*2;
	tmp=calloc(len+1,1);
	if(tmp){
		for(i=0;i<len;i++){
			int index=i>>1;
			BYTE a=data[index];
			if(i&1){
				a=a&0xF;
			}else{
				a=a>>4;
				a&=0xF;
			}
			if(a>=10){
				a='A'+(a-10);
			}else{
				a='0'+a;
			}
			tmp[i]=a;
		}
	}
	return tmp;
}

static int hex_to_data(char *hex,BYTE **out,int *out_len)
{
	int result=FALSE;
	int len;
	BYTE *tmp;
	int tmp_size;
	int i,count,index;
	BYTE val;
	len=strlen(hex);
	tmp_size=len/2;
	tmp=calloc(tmp_size,1);
	if(0==tmp){
		return result;
	}
	index=count=0;
	val=0;
	for(i=0;i<len;i++){
		BYTE a=hex[i];
		int x=0;
		count++;
		if(a>='A'){
			a&=~0x20;
			x=a-'A'+10;
		}else{
			x=a-'0';
		}
		val<<=4;
		val|=x&0xF;
		if(count>=2){
			tmp[index]=val;
			count=0;
			index++;
		}
	}
	*out=tmp;
	*out_len=index;
	result=TRUE;
	return result;
}

int save_user_name(const WCHAR *user)
{
	return set_ini_value(L"SETTINGS",L"USERNAME",user);
}
int save_password(const WCHAR *password)
{
	int result=FALSE;
	int len;
	char *tmp;
	int pw_size;
	pw_size=wcslen(password)*sizeof(WCHAR);
	len=pw_size;
	if(len<10)
		len=10;
	len*=sizeof(WCHAR);
	tmp=calloc(len,1);
	if(tmp){
		char *hex;
		memcpy(tmp,password,pw_size);
		xor_str(tmp,len);
		hex=data_to_hex(tmp,len);
		if(hex){
			WCHAR *wstr=utf2wchar(hex);
			if(wstr){
				result=set_ini_value(L"SETTINGS",L"PASSWORD",wstr);
				free(wstr);
			}
			free(hex);
		}
		free(tmp);
	}
	return result;
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
	static char password[512];
	char tmp[512]={0};
	char *data=0;
	int data_len=0;
	memset(password,0,sizeof(password));
	get_ini_value(L"SETTINGS",L"PASSWORD",tmp,sizeof(tmp));
	hex_to_data(tmp,&data,&data_len);
	xor_str(data,data_len);
	if(data_len>sizeof(tmp)){
		data_len=sizeof(tmp);
	}
	if(data){
		char *str;
		int end=sizeof(tmp);
		memset(tmp,0,sizeof(tmp));
		memcpy(tmp,data,data_len);
		tmp[end-1]=0;
		tmp[end-2]=0;
		str=wchar2utf(tmp);
		if(str){
			strncpy(password,str,sizeof(password));
			password[sizeof(password)-1]=0;
			free(str);
		}
		free(data);
	}
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


int validate_ini(HWND hwnd,int show_msgbox)
{
	int result=FALSE;
	static WCHAR *fname;
	const WCHAR *msg=L"";
	WCHAR *tmp=0;
	fname=get_ini_fname();
	if(0==fname){
		msg=L"Unable to get INI file name";
		goto ERROR_VAL;
	}
	if(file_exists(fname)){
		result=TRUE;
	}else{
		FILE *f;
		f=_wfopen(fname,L"wb+");
		if(f){
			fclose(f);
			result=TRUE;
		}else{
			int size;
			size=wcslen(fname)*sizeof(WCHAR);
			size+=1024;
			tmp=calloc(size,1);
			if(tmp){
				int count=size/sizeof(WCHAR);
				_snwprintf(tmp,count,L"Unable to create INI file:\r\n%s",fname);
				msg=tmp;
			}
		}
	}
ERROR_VAL:
	if(!result){
		MessageBoxW(hwnd,msg,L"ERROR",MB_OK|MB_SYSTEMMODAL);
	}
	free(tmp);
	return result;
}