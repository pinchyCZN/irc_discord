#include <Windows.h>
#include <stdio.h>
#include "polarssl/net.h"
#include "config.h"
#include "libstring.h"
#include "discord.h"

#pragma warning (disable:4996)
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
	int result=FALSE;
	char *tmp,*ptr;
	int len;
	ptr=strchr(cmd,'#');
	if(0==ptr){
		return result;
	}
	tmp=strdup(ptr+1);
	if(0==tmp){
		return result;
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

int handle_connection(SOCKET s)
{
	static char line[2048];
	char nick[80]={0};
	char name[80]={0};
	int state=0;
	while(1){
		int res;
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
				char chan[40]={0};
				sscanf(line,"%39s%39s",cmd,chan);
				if(startswithi(cmd,"JOIN")){
					handle_join(chan);
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
	return 0;
}

void irc_thread(void *args)
{
	SOCKET s;
	const char *host="127.0.0.1";
	int port;
	int res;
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