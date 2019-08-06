#include <winsock2.h>
#include <Windows.h>
#include <stdio.h>
#include "polarssl/net.h"
#include "config.h"
#include "libstring.h"
#include "discord.h"

#pragma warning (disable:4996)

static HANDLE g_irc_event=0;
static CRITICAL_SECTION irc_mutex={0};
static int irc_mutex_init=FALSE;
static char **irc_msg=0;
static int irc_msg_count=0;

/*
":my.server.name 001 test123 :Welcome to the Internet Relay Network test123"
"PING :675028717"
":my.server.name 004 test123 my.server.name beware1.6.3 dgikoswx biklmnoprstv"
":my.server.name NOTICE test123 :on 1 ca 1(4) ft 10(10) tr"
USER uname unknown unknown :realname
JOIN #test123
:test123!~uname@my.server.name JOIN :#test123
":my.server.name 353 test123 = #test123 :@test123"
":my.server.name 366 test123 #test123 :End of /NAMES list."

SEND: sadfadf
RECV: :my.server.name 421 test123 sadfadf :Unknown command
RECV: :chopchop1!~chopchop1@my.server.name JOIN :#test123
RECV: :chopchop1!~chopchop1@my.server.name PRIVMSG #test123 :asdfsadf

whois
":my.server.name 311 test123 test123 ~123 my.server.name * :qwe"
":my.server.name 319 test123 test123 :@#test123"
":my.server.name 312 test123 test123 my.server.name :I'm too lazy to edit ircd.conf"
":my.server.name 317 test123 test123 4 1562449299 :seconds idle, signon time"
":my.server.name 318 test123 test123 :End of /WHOIS list."

*/

static read_line(SOCKET s,unsigned char *data,int data_len)
{
	int index=0;
	while(1){
		int res;
		unsigned char tmp[4]={0};
		if(index>=data_len){
			break;
		}
		res=net_recv((void*)&s,tmp,1);
		if(res>0){
			unsigned char a;
			a=tmp[0];
			data[index]=a;
			if('\n'==a){
				break;
			}
			index++;
		}else{
			break;
		}
	}
	return index;
}

static int net_send_str(SOCKET s,const char *str)
{
	int slen;
	slen=strlen(str);
	return net_send(&s,str,slen);
}
int send_motd(SOCKET s,char *nick)
{
	int result=TRUE;
	char tmp[80]={0};
	_snprintf(tmp,sizeof(tmp),":discord.server 001 %s :Welcome to irc discord bridge\r\n",nick);
	net_send_str(s,tmp);
	_snprintf(tmp,sizeof(tmp),"PING :%u\r\n",GetTickCount());
	net_send_str(s,tmp);
	_snprintf(tmp,sizeof(tmp),":discord.server 376 %s :MOTD BLAH BLAH\r\n",nick);
	net_send_str(s,tmp);
	return result;
}

static void handle_join(const char *cmd)
{
	char *tmp,*ptr;
	int len;
	ptr=strchr(cmd,'#');
	if(0==ptr){
		return;
	}
	tmp=strdup(ptr+1);
	if(0==tmp){
		return;
	}
	trim_right(tmp);
	len=strlen(tmp);
	if(len){
		char a=tmp[len-1];
		if(':'==a){
			tmp[len-1]=0;
			trim_right(tmp);
		}
	}
	len=strlen(tmp);
	if(len){
		add_discord_cmd(CMD_JOIN_CHAN,tmp);
	}
	free(tmp);
}

enum{
	START_CHAN_LIST=321,
	CHAN_LIST=322,
	END_CHAN_LIST=323,
	CHAN_MSG=100,
	PRIV_MSG=110,
};
static int get_irc_msg_code(const char *str)
{
	int result=0;
	typedef struct{
		const char *str;
		int code;
	}CODE_MAP;
	CODE_MAP code_map[]={
		{"START_CHAN_LIST",START_CHAN_LIST},
		{"CHAN_LIST",CHAN_LIST},
		{"END_CHAN_LIST",END_CHAN_LIST},
		{"CHAN_MSG",CHAN_MSG},
		{"PRIV_MSG",PRIV_MSG},
	};
	int i,count;
	count=_countof(code_map);
	for(i=0;i<count;i++){
		if(startswithi(str,code_map[i].str)){
			result=code_map[i].code;
			break;
		}
	}
	return result;
}

static int handle_msg(SOCKET s,const char *str,const char *nick)
{
	if(startswithi(str,"START_CHAN_LIST")){
	}
}

int push_irc_msg(const char *str)
{
	int result=FALSE;
	int list_size;
	int count;
	int index;
	char **tmp_list;
	EnterCriticalSection(&irc_mutex);
	count=irc_msg_count;
	index=irc_msg_count;
	count++;
	list_size=sizeof(char*)*count;
	tmp_list=realloc(irc_msg,list_size);
	if(tmp_list){
		char *tmp;
		tmp=strdup(str);
		if(tmp){
			result=TRUE;
			irc_msg=tmp_list;
			irc_msg[index]=tmp;
			irc_msg_count=count;
		}
	}
	LeaveCriticalSection(&irc_mutex);
	SetEvent(g_irc_event);
	return result;
}

static int pop_irc_msg(char **msg)
{
	int result=FALSE;
	EnterCriticalSection(&irc_mutex);
	if(irc_msg_count){
		char **tmp_list;
		int list_size,count;
		int index;
		index=0;
		count=irc_msg_count-1;
		list_size=sizeof(char*)*count;
		result=TRUE;
		*msg=irc_msg[index];
		memcpy(irc_msg,irc_msg+1,list_size);
		tmp_list=realloc(irc_msg,list_size);
		if(tmp_list || (0==list_size && 0==tmp_list)){
			irc_msg=tmp_list;
			irc_msg_count=count;
		}
	}
	LeaveCriticalSection(&irc_mutex);
	return result;
}

static int data_avail(SOCKET s,int timeout,int *has_error)
{
	int result=FALSE;
	int res;
	fd_set readfd={0};
	struct timeval time={0};
	time.tv_usec=timeout*1000;
	readfd.fd_count=1;
	readfd.fd_array[0]=s;
	res=select(1,&readfd,NULL,NULL,&time);
	if(1==res){
		result=TRUE;
	}else if(SOCKET_ERROR==res){
		*has_error=TRUE;
	}
	return result;
}

int handle_connection(SOCKET s)
{
	static char line[2048];
	char nick[80]={0};
	char name[80]={0};
	int state=0;
	while(1){
		int res;
		int has_error;
		res=WaitForSingleObject(g_irc_event,30);
		if(WAIT_OBJECT_0==res){
			int exit=FALSE;
			while(1){
				int res;
				char *msg=0;
				int msg_len;
				res=pop_irc_msg(&msg);
				if(!res){
					break;
				}
				if(0==msg){
					continue;
				}
				msg_len=strlen(msg);
				if(msg_len){
					if('\n'!=msg[msg_len-1]){
						append_printf(&msg,&msg_len,"\r\n");
					}
				}
				net_send_str(s,msg);
				free(msg);
			}
		}
		has_error=FALSE;
		res=data_avail(s,30,&has_error);
		if(has_error){
			printf("IRC socket select error\n");
			break;
		}
		if(res){
			memset(line,0,sizeof(line));
			res=read_line(s,line,sizeof(line));
			if(res<=0){
				break;
			}
			printf("RECV:%s",line);
			switch(state){
			case 0:
				if(startswithi(line,"USER ")){
					sscanf(line,"%*s%79s",name);
					send_motd(s,nick);
				}else if(startswithi(line,"NICK ")){
					sscanf(line,"%*s%79s",nick);
				}
				if(nick[0]!=0 && name[0]!=0){
					printf("nick=%s name=%s\n",nick,name);
					state=1;
				}
				break;
			case 1:
				{
					char cmd[40]={0};
					int cmd_valid=FALSE;
					sscanf(line,"%39s",cmd);
					if(startswithi(cmd,"JOIN")){
						char *tmp;
						tmp=strchr(line,'#');
						if(tmp){
							handle_join(tmp);
							cmd_valid=TRUE;
						}
					}else if(startswithi(cmd,"LIST")){
						add_discord_cmd(CMD_LIST_CHAN,"");
					}
					_snprintf(line,sizeof(line),":discord.server 421 %s %s :unknown command\r\n",nick,cmd);
					net_send_str(s,line);
				}
				break;
			}
			if(state>=99){
				break;
			}
		}
	}
	return 0;
}

void irc_thread(void *args)
{
	SOCKET s;
	const char *host="127.0.0.1";
	int port;
	int res;
	if(0==g_irc_event){
		g_irc_event=CreateEventA(NULL,FALSE,FALSE,"IRC_EVENT");
	}
	if(!irc_mutex_init){
		irc_mutex_init=TRUE;
		InitializeCriticalSection(&irc_mutex);
	}
	port=get_irc_port();
	res=net_bind(&s,host,port);
	if(0!=res){
		printf("failed to bind to %s:%i\n",host,port);
		return;
	}
	while(1){
		SOCKET client=0;
		printf("IRC server waiting for connection\n");
		res=net_accept(s,&client,NULL);
		if(0==res){
			printf("connection accepted\n");
			handle_connection(client);
			net_close(client);
		}else{
			Sleep(1000);
		}
	}
}
