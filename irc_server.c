#include <winsock2.h>
#include <Windows.h>
#include <stdio.h>
#include "mbedtls/net.h"
#include "config.h"
#include "libstring.h"
#include "discord.h"
#include "irc_server.h"

#pragma warning (disable:4996)

static HANDLE g_irc_event=0;
static CRITICAL_SECTION irc_mutex={0};
static int irc_mutex_init=FALSE;
static char **irc_msg=0;
static int irc_msg_count=0;

static void DBGPRINT(const char *fmt,...)
{
	va_list ap;
	va_start(ap,fmt);
	if(gui_active){
		void add_line_irc_log(const char *str);
		static char tmp[4096];
		_vsnprintf(tmp,sizeof(tmp),fmt,ap);
		tmp[sizeof(tmp)-1]=0;
		add_line_irc_log(tmp);
	}
	if(console_active){
		if(!enable_dbg_irc)
			return;
		vprintf(fmt,ap);
	}
}

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
topic:
:my.server.name 332 test123 #test_serv1.general :some topic
*/

static int read_line(mbedtls_net_context *ctx,unsigned char *data,int data_len)
{
	int index=0;
	while(1){
		int res;
		unsigned char tmp[4]={0};
		if(index>=data_len){
			break;
		}
		res=mbedtls_net_recv(ctx,tmp,1);
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

static int net_send_str(mbedtls_net_context *ctx,const char *str)
{
	int slen;
	slen=strlen(str);
	return mbedtls_net_send(ctx,str,slen);
}
static int send_motd(mbedtls_net_context *ctx,char *nick)
{
	int result=TRUE;
	char tmp[80]={0};
	_snprintf(tmp,sizeof(tmp),":discord.server 001 %s :Welcome to irc discord bridge\r\n",nick);
	net_send_str(ctx,tmp);
	_snprintf(tmp,sizeof(tmp),"PING :%u\r\n",GetTickCount());
	net_send_str(ctx,tmp);
	_snprintf(tmp,sizeof(tmp),":discord.server 376 %s :MOTD BLAH BLAH\r\n",nick);
	net_send_str(ctx,tmp);
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

typedef struct{
	const char *str;
	int code;
}CODE_MAP;
static CODE_MAP code_map[]={
	{"START_CHAN_LIST",START_CHAN_LIST},
	{"CHAN_LIST",CHAN_LIST},
	{"END_CHAN_LIST",END_CHAN_LIST},
	{"CHAN_MSG",CHAN_MSG},
	{"PRIV_MSG",PRIV_MSG},
	{"OK_JOIN_CHAN",OK_JOIN_CHAN},
	{"NAME_LIST",NAME_LIST},
	{"END_NAME_LIST",END_NAME_LIST},
	{"UNKNOWN_CHAN",UNKNOWN_CHAN},
	{"CHAN_TOPIC",CHAN_TOPIC},
	{"SERVER_INFO",SERVER_INFO},
};

static int get_irc_msg_code(const char *str)
{
	int result=0;
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
const char *get_irc_msg_str(int code)
{
	const char *result=0;
	int i,count;
	count=_countof(code_map);
	for(i=0;i<count;i++){
		if(code==code_map[i].code){
			result=code_map[i].str;
			break;
		}
	}
	return result;
}

static int reply_generic_msg(mbedtls_net_context *ctx,int code,const char *fmt,...)
{
	int result;
	char msg[400]={0};
	char str[512]={0};
	va_list ap;
	va_start(ap,fmt);
	vsnprintf(msg,sizeof(msg),fmt,ap);
	msg[sizeof(msg)-1]=0;
	__snprintf(str,sizeof(str),":discord.server %u info :%s\r\n",code,msg);
	result=net_send_str(ctx,str);
	return result>0;
}

/*
RECV: :my.server.name 321 test123 Channel :Users  Name
RECV: :my.server.name 322 test123 #test1234 1 :
RECV: :my.server.name 322 test123 #test123 1 :
RECV: :my.server.name 323 test123 :End of /LIST
*/

static int handle_msg(mbedtls_net_context *ctx,const char *str,const char *nick)
{
	int result=FALSE;
	int code;
	char tmp[512]={0};
	const char *data;
	code=get_irc_msg_code(str);
	data=seek_next_word(str);
	switch(code){
	case START_CHAN_LIST:
		__snprintf(tmp,sizeof(tmp),":discord.server 321 %s Channel :Users Name\r\n",nick);
		break;
	case CHAN_LIST:
		if(data){
			__snprintf(tmp,sizeof(tmp),":discord.server 322 %s %s\r\n",nick,data);
		}
		break;
	case END_CHAN_LIST:
		__snprintf(tmp,sizeof(tmp),":discord.server 323 %s :End of /LIST\r\n",nick);
		break;
	case OK_JOIN_CHAN:
		if(data){
			__snprintf(tmp,sizeof(tmp),":%s!~uname@discord.server JOIN :%s\r\n",nick,data);
		}
		break;
	case NAME_LIST:
		if(data){
			__snprintf(tmp,sizeof(tmp),":discord.server 353 %s = %s\r\n",nick,data);
		}
		break;
	case END_NAME_LIST:
		if(data){
			__snprintf(tmp,sizeof(tmp),":discord.server 366 %s %s :End of /NAMES list.\r\n",nick,data);
		}
		break;
	case CHAN_MSG:
		if(data){
			char chan[160]={0};
			char msg_nick[80]={0};
			const char *msg=0;
			const char *ptr=data;
			//chan,nick,content
			get_word(ptr,chan,sizeof(chan));
			ptr=seek_next_word(ptr);
			get_word(ptr,msg_nick,sizeof(msg_nick));
			ptr=seek_next_word(ptr);
			msg=ptr;
			if(chan[0] && msg_nick[0] && msg){
				//:chopchop1!~chopchop1@my.server.name PRIVMSG #test123 :asdfsadf
				__snprintf(tmp,sizeof(tmp),":%s!~uname@discord.server PRIVMSG %s :%s\r\n",msg_nick,chan,msg);
			}
		}
		break;
	case UNKNOWN_CHAN:
		__snprintf(tmp,sizeof(tmp),":discord.server 437 %s %s :Unknown channel\r\n",nick,data);
		break;
	case CHAN_TOPIC:
		{
			char chan[160]={0};
			const char *ptr=data;
			const char *topic;
			get_word(ptr,chan,sizeof(chan));
			topic=seek_next_word(ptr);
			if(topic && topic[0]!=0)
				__snprintf(tmp,sizeof(tmp),":discord.server 332 nick %s :%s\r\n",chan,topic);
		}
		break;
	case PRIV_MSG:
		{
			char msg_nick[80]={0};
			const char *ptr=data;
			//nick,msg
			get_word(ptr,msg_nick,sizeof(msg_nick));
			ptr=seek_next_word(ptr);
			if(msg_nick[0] && ptr)
				__snprintf(tmp,sizeof(tmp),":%s!~uname@discord.server PRIVMSG %s :%s\r\n",msg_nick,nick,ptr);
		}
		break;
	case SERVER_INFO:
		{
			if(data)
				__snprintf(tmp,sizeof(tmp),":discord.server 999 info :%s\r\n",data);
		}
		break;
	default:
		DBGPRINT("ERROR:unhandled code:%i\n",code);
		break;
	}
	if(tmp[0]){
		net_send_str(ctx,tmp);
		result=TRUE;
	}
	return result;
}

static int process_discord_cmd(mbedtls_net_context *ctx,const char *args)
{
	int result=FALSE;
	char w1[20]={0};
	const char *str=args;
	get_word(str,w1,sizeof(w1));
	str=seek_next_word(str);
	if(startswithi(w1,"INVITE")){
		char w2[20]={0};
		get_word(str,w2,sizeof(w2));
		str=seek_next_word(str);
		if(startswithi(w2,"GET")){
		}
	}else if(startswithi(w1,"JOIN")){
		char w2[80]={0};
		get_word(str,w2,sizeof(w2));
		if(w2[0]!=0){
			result=TRUE;
			add_discord_cmd(CMD_INVITE_USE,w2);
		}
	}else if(startswithi(w1,"LEAVE")){
		char w2[160]={0};
		get_word(str,w2,sizeof(w2));
		if(w2[0]!=0)
			add_discord_cmd(CMD_GUILD_LEAVE,w2);
	}
	return result;
}

int push_irc_msg(const char *str)
{
	int result=FALSE;
	int list_size;
	int count;
	int index;
	char **tmp_list;
	if(0==str){
		return result;
	}
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

static int data_avail(mbedtls_net_context *ctx,int timeout,int *has_error)
{
	int result=FALSE;
	int res;
	fd_set readfd={0};
	struct timeval time={0};
	time.tv_usec=timeout*1000;
	readfd.fd_count=1;
	readfd.fd_array[0]=ctx->fd;
	res=select(1,&readfd,NULL,NULL,&time);
	if(1==res){
		result=TRUE;
	}else if(SOCKET_ERROR==res){
		*has_error=TRUE;
	}
	return result;
}

static int handle_connection(mbedtls_net_context *ctx)
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
				res=pop_irc_msg(&msg);
				if(!res){
					break;
				}
				if(0==msg){
					continue;
				}
				handle_msg(ctx,msg,nick);
				free(msg);
			}
		}
		has_error=FALSE;
		res=data_avail(ctx,30,&has_error);
		if(has_error){
			DBGPRINT("IRC socket select error\n");
			break;
		}
		if(res){
			memset(line,0,sizeof(line));
			res=read_line(ctx,line,sizeof(line));
			if(res<=0){
				break;
			}
			trim_right(line);
			DBGPRINT("RECV:%s\n",line);
			switch(state){
			case 0:
				if(startswithi(line,"USER ")){
					sscanf(line,"%*s%79s",name);
					send_motd(ctx,nick);
				}else if(startswithi(line,"NICK ")){
					sscanf(line,"%*s%79s",nick);
				}
				if(nick[0]!=0 && name[0]!=0){
					DBGPRINT("nick=%s name=%s\n",nick,name);
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
						const char *tmp;
						tmp=seek_next_word(line);
						if(0==tmp)
							tmp="";
						add_discord_cmd(CMD_LIST_CHAN,tmp);
						cmd_valid=TRUE;
					}else if(startswithi(cmd,"PRIVMSG")){
						const char *tmp;
						tmp=seek_next_word(line);
						if(tmp){
							cmd_valid=TRUE;
							add_discord_cmd(CMD_POST_MSG,tmp);
						}
					}else if(startswithi(cmd,"NAMES")){
						const char *chan;
						chan=seek_next_word(line);
						if(chan){
							cmd_valid=TRUE;
							add_discord_cmd(CMD_GET_NAMES,chan);
						}
					}else if(startswithi(cmd,"GETMSG")){
						const char *tmp;
						tmp=seek_next_word(line);
						if(0==tmp)
							tmp="";
						cmd_valid=TRUE;
						add_discord_cmd(CMD_GET_MSGS,tmp);
					}else if(startswithi(cmd,"PART")){
						const char *tmp;
						tmp=seek_next_word(line);
						if(0==tmp)
							tmp="";
						cmd_valid=TRUE;
						add_discord_cmd(CMD_PART,tmp);
					}else if(startswithi(cmd,"DISCORD")){
						const char *tmp;
						cmd_valid=TRUE;
						tmp=seek_next_word(line);
						if(tmp)
							process_discord_cmd(ctx,tmp);
						else{
							const char *help[]={
								"DISCORD command help",
								"DISCORD JOIN xyz_code",
								"DISCORD INVITE #guild.channel",
								"DISCORD LEAVE #guild|dm_chan",
							};
							int i,count;
							count=_countof(help);
							for(i=0;i<count;i++)
								reply_generic_msg(ctx,999,"%s",help[i]);
						}
					}else if(startswithi(cmd,"HELP")){
						const char *help[]={
							"DISCORD <> IRC Bridge command help",
							"GETMSG #guild.channel [before|after|around] [timestamp] [count]",
							"GETMSG #guild.channel pin",
							"GETMSG #guild.channel info",
							"GETMSG #guild.channel find text",
							"DISCORD JOIN|INVITE|LEAVE",
							"LIST r|p|d {Refresh list,refresh Private/Direct message list}"
						};
						int i,count;
						count=_countof(help);
						for(i=0;i<count;i++)
							reply_generic_msg(ctx,999,"%s",help[i]);
						cmd_valid=TRUE;

					}else if(startswithi(cmd,"QUIT")){
						cmd_valid=TRUE;
						state=99;
					}else if(startswithi(cmd,"PONG") || startswithi(cmd,"PING")){
						reply_generic_msg(ctx,999,"ACK %s",cmd);
						cmd_valid=TRUE;
					}
					if(!cmd_valid){
						_snprintf(line,sizeof(line),":discord.server 421 %s %s :unknown command\r\n",nick,cmd);
						net_send_str(ctx,line);
					}
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
	mbedtls_net_context ctx={0};
	const char *host="127.0.0.1";
	int port;
	char str[40]={0};
	int res;
	if(0==g_irc_event){
		g_irc_event=CreateEventA(NULL,FALSE,FALSE,"IRC_EVENT");
	}
	if(!irc_mutex_init){
		irc_mutex_init=TRUE;
		InitializeCriticalSection(&irc_mutex);
	}
	port=get_irc_port();
	_snprintf(str,sizeof(str),"%u",port);
	mbedtls_net_init(&ctx);
	res=mbedtls_net_bind(&ctx,host,str,MBEDTLS_NET_PROTO_TCP);
	if(0!=res){
		DBGPRINT("failed to bind to %s:%i\n",host,port);
		return;
	}
	while(1){
		mbedtls_net_context client={0};
		DBGPRINT("IRC server waiting for connection\n");
		res=mbedtls_net_accept(&ctx,&client,0,0,0);
		if(0==res){
			DBGPRINT("connection accepted\n");
			handle_connection(&client);
			mbedtls_net_free(&client);
		}else{
			Sleep(1000);
		}
	}
}
