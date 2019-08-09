#define CINTERFACE
#include <winsock2.h>
#include <Windows.h>
#include <process.h>
#include <stdio.h>
#include <conio.h>
#include <WinInet.h>


#include "libstring.h"
#include "parson.h"
#include "network.h"
#include "config.h"
#include "gateway.h"
#include "irc_server.h"
#include "discord.h"

#pragma warning(disable:4996)

typedef struct{
	char *msg;
	char *id;
	char *author;
	char *auth_id;
	char *timestamp;
}MESSAGE;
typedef struct{
	MESSAGE *m;
	int count;
}MESSAGE_LIST;
typedef struct{
	char **nick;
	int nick_count;
}NICK_LIST;
typedef struct{
	char *name;
	char *topic;
	char *id;
	MESSAGE_LIST msgs;
	MESSAGE_LIST pin_msgs;
	NICK_LIST nicks;
}CHANNEL;
typedef struct{
	CHANNEL *chan;
	int count;
}CHANNEL_LIST;
typedef struct{
	char *name;
	char *id;
	CHANNEL_LIST channels;
}GUILD;
typedef struct{
	GUILD *guild;
	int count;
}GUILD_LIST;

static GUILD_LIST g_guild_list={0};

static int add_guild(GUILD_LIST *glist,GUILD *g)
{
	int result=FALSE;
	int count,index;
	int size;
	GUILD *ptr;
	char *id;
	char *name;
	id=strdup(g->id);
	name=strdup(g->name);
	if(0==id || 0==name){
		goto ERROR_ADD_GUILD;
	}
	count=glist->count;
	index=count;
	count++;
	size=sizeof(GUILD)*count;
	ptr=realloc(glist->guild,size);
	if(ptr){
		GUILD *tmp=&ptr[index];
		memset(tmp,0,sizeof(GUILD));
		memcpy(&tmp->channels,&g->channels,sizeof(tmp->channels));
		tmp->id=id;
		tmp->name=name;
		glist->count=count;
		glist->guild=ptr;
		result=TRUE;
	}
ERROR_ADD_GUILD:
	if(!result){
		free(id);
		free(name);
	}
	return result;
}

static int add_channel(CHANNEL_LIST *clist,CHANNEL *chan)
{
	int result=FALSE;
	int count,index;
	CHANNEL *ptr;
	int size;
	char *name,*topic,*id;
	name=strdup(chan->name);
	topic=strdup(chan->topic);
	id=strdup(chan->id);
	if(0==name || 0==topic || 0==id){
		goto ERROR_ADD_CHAN;
	}
	count=clist->count;
	index=count;
	count++;
	size=sizeof(CHANNEL)*count;
	ptr=realloc(clist->chan,size);
	if(ptr){
		CHANNEL *tmp;
		tmp=&ptr[index];
		memset(tmp,0,sizeof(CHANNEL));
		memcpy(&tmp->msgs,&chan->msgs,sizeof(tmp->msgs));
		tmp->id=id;
		tmp->name=name;
		tmp->topic=topic;
		clist->chan=ptr;
		clist->count=count;
		result=TRUE;
	}
ERROR_ADD_CHAN:
	if(!result){
		free(name);free(topic);free(id);
	}
	return result;
}
static int add_nick(NICK_LIST *nlist,const char *nick)
{
	int result=FALSE;
	int i,count;
	count=nlist->nick_count;
	if(count>=1000){
		return result;
	}
	for(i=0;i<count;i++){
		char *name=nlist->nick[i];
		if(0==strcmp(name,nick)){
			result=TRUE;
			break;
		}
	}
	if(!result){
		char *tmp_nick;
		tmp_nick=strdup(nick);
		if(tmp_nick){
			char **tmp;
			int index;
			index=count;
			count++;
			tmp=realloc(nlist->nick,count*sizeof(char*));
			if(tmp){
				tmp[index]=tmp_nick;
				nlist->nick=tmp;
				nlist->nick_count=count;
				result=TRUE;
			}else{
				free(tmp_nick);
			}
		}
	}
	return result;
}

static int add_message(MESSAGE_LIST *mlist,MESSAGE *msg)
{
	int result=FALSE;
	int index,count;
	MESSAGE *ptr;
	char *author,*auth_id,*id,*content,*timestamp;
	int size;
	author=strdup(msg->author);
	auth_id=strdup(msg->auth_id);
	id=strdup(msg->id);
	content=strdup(msg->msg);
	timestamp=strdup(msg->timestamp);
	if(0==author || 0==auth_id || 0==id || 0==content || 0==timestamp){
		goto ERROR_ADD_MSG;
	}
	count=mlist->count;
	index=count;
	count++;
	size=sizeof(MESSAGE)*count;
	ptr=realloc(mlist->m,size);
	if(ptr){
		MESSAGE *tmp;
		tmp=&ptr[index];
		memset(tmp,0,sizeof(MESSAGE));
		tmp->author=author;
		tmp->auth_id=auth_id;
		tmp->id=id;
		tmp->msg=content;
		tmp->timestamp=timestamp;
		mlist->m=ptr;
		mlist->count=count;
		result=TRUE;
	}
ERROR_ADD_MSG:
	if(!result){
		free(author);
		free(auth_id);
		free(id);
		free(content);
		free(timestamp);
	}
	return result;
}

static int remove_all_msg(MESSAGE_LIST *mlist)
{
	int i,count;
	count=mlist->count;
	for(i=0;i<count;i++){
		MESSAGE *m;
		m=&mlist->m[i];
		free(m->author);
		free(m->auth_id);
		free(m->id);
		free(m->msg);
		free(m->timestamp);
	}
	free(mlist->m);
	mlist->m=0;
	mlist->count=0;
	return TRUE;
}
static int remove_all_nicks(NICK_LIST *nlist)
{
	int i,count;
	count=nlist->nick_count;
	for(i=0;i<count;i++){
		char *tmp=nlist->nick[i];
		if(tmp){
			free(tmp);
			nlist->nick[i]=0;
		}
	}
	nlist->nick=0;
	nlist->nick_count=0;
	return TRUE;
}
static int remove_all_channels(CHANNEL_LIST *clist)
{
	int i,count;
	count=clist->count;
	for(i=0;i<count;i++){
		CHANNEL *tmp;
		tmp=&clist->chan[i];
		free(tmp->id);
		free(tmp->name);
		free(tmp->topic);
		remove_all_msg(&tmp->msgs);
		remove_all_nicks(&tmp->nicks);
	}
	if(count>0){
		free(clist->chan);
		clist->chan=0;
		clist->count=0;
	}
	return TRUE;
}
static int remove_all_guilds(GUILD_LIST *glist)
{
	int i,count;
	count=glist->count;
	for(i=0;i<count;i++){
		GUILD *tmp;
		tmp=&glist->guild[i];
		remove_all_channels(&tmp->channels);
		free(tmp->id);
		free(tmp->name);
	}
	free(glist->guild);
	glist->guild=0;
	glist->count=0;
	return TRUE;
}

int is_chunked(const char *str)
{
	if(strstri(str,"chunked")){
		return TRUE;
	}
	return FALSE;
}

//does not allocate new string
static int get_content(char *data,int data_len,char **out,int *out_len)
{
	int result=FALSE;
	int i;
	int line_count;
	int content_len=0;
	int chunked=FALSE;
	char *content_start=0;
	typedef struct{
		const char *name;
		int *val;
		int (*func)(const char*);
	}HEADER_MAP;
	HEADER_MAP headers[2]={
		{"Content-Length",&content_len,0},
		{"Transfer-Encoding",&chunked,&is_chunked},
	};
	line_count=get_line_count(data,data_len);
	for(i=0;i<line_count;i++){
		int res;
		char line[512]={0};
		res=get_line(data,data_len,i,line,sizeof(line));
		if(res){
			int j,hdr_count;
			res=strlen(line);
			if(0==res){
				get_line_offset(data,data_len,i+1,&content_start);
				break;
			}
			hdr_count=_countof(headers);
			for(j=0;j<hdr_count;j++){
				HEADER_MAP *hdr=&headers[j];
				if(startswithi(line,hdr->name)){
					if(hdr->func){
						*hdr->val=hdr->func(line);
					}else{
						char *tmp=strchr(line,':');
						if(tmp){
							*hdr->val=atoi(tmp+1);
						}
					}
					break;
				}
			}

		}
	}
	if(content_len && content_start){
		char *a,*b;
		a=content_start+content_len;
		b=data+data_len;
		if(a<=b){
			*out=content_start;
			*out_len=content_len;
			result=TRUE;
		}
	}
	if(chunked && content_start){
		char *buf;
		int buf_len=0x10000;
		buf=(char*)calloc(buf_len,1);
		if(buf){
			SIZE_T delta;
			char *tmp;
			int tmp_len;
			delta=content_start-data;
			tmp=content_start;
			tmp_len=data_len-(int)delta;
			line_count=get_line_count(tmp,tmp_len);
			for(i=0;i<line_count;i++){
				int res;
				res=get_line(tmp,tmp_len,i,buf,buf_len);
				if(res){
					if(i&1){
						get_line_offset(tmp,tmp_len,i,&content_start);
						*out=content_start;
						*out_len=content_len;
						result=TRUE;
						break;
					}else{
						content_len=strtoul(buf,NULL,16);
					}
				}else{
					break;
				}
			}
			free(buf);
		}
	}
	return result;
}

int is_resp_complete(char *data,int data_len)
{
	int result=FALSE;
	int i,lines;
	char *content=0;
	int content_len=0;
	int chunked=FALSE;
	int start_data=-1;
	char *data_end;
	int state=0;
	typedef struct{
		const char *name;
		int *val;
		int (*func)(const char*);
	}HEADER_MAP;
	HEADER_MAP headers[2]={
		{"Content-Length",&content_len,0},
		{"Transfer-Encoding",&chunked,&is_chunked},
	};
	data_end=data+data_len;
	lines=get_line_count(data,data_len);
	for(i=0;i<lines;i++){
		int res;
		char line[512]={0};
		res=get_line(data,data_len,i,line,sizeof(line));
		if(res){
			switch(state){
			case 0: //reading headers
				{
					int j,hdr_count,line_len;
					line_len=strlen(line);
					hdr_count=_countof(headers);
					for(j=0;j<hdr_count;j++){
						HEADER_MAP *hdr=&headers[j];
						if(startswithi(line,hdr->name)){
							if(hdr->func){
								*hdr->val=hdr->func(line);
							}else{
								char *tmp;
								tmp=strchr(line,':');
								if(tmp){
									*hdr->val=atoi(tmp+1);
								}
							}
							break;
						}
					}
					if(0==line_len){
						if(chunked)
							state=1;
						else
							state=2;
					}
				}
				break;
			case 1: //reading chunks
				{
					int clen;
					clen=strtoul(line,0,16);
					if(0==clen){
						result=TRUE;
					}
					i++;
				}
				break;
			case 2: //check if content is complete
				{
					char *tmp;
					res=get_line_offset(data,data_len,i,&tmp);
					if(res){
						SIZE_T delta;
						int clen;
						delta=tmp-data;
						clen=data_len-(int)delta;
						if(clen>=content_len){
							result=TRUE;
						}
					}
				}
				break;
			}
			if(result){
				break;
			}
		}
	}
	return result;
}


// adds null terminator but keeps resp_len as original
static int get_response(CONNECTION *c,char **resp,int *resp_len)
{
	int result=FALSE;
	char *tmp=0;
	int tmp_len=8*1024;
	ssl_context *ssl;
	ssl=&c->ssl;
	tmp=(char*)calloc(tmp_len,1);
	if(tmp){
		DWORD tick,delta;
		int offset=0;
		tick=GetTickCount();
		while(1){
			int n;
			int avail;
			avail=tmp_len-offset;
			if(avail<=0){
				char *blk;
				int blk_len;
				blk_len=tmp_len+64*1024;
				if(blk_len>0x800000){
					break;
				}
				blk=(char*)realloc(tmp,blk_len);
				if(blk){
					tmp=blk;
					tmp_len=blk_len;
				}else{
					break;
				}
			}
			n=ssl_read(ssl,(BYTE*)tmp+offset,avail);
			if(n>=0){
				int res;
				char *data;
				int data_len;
				offset+=n;
				data=tmp;
				data_len=offset;
				res=is_resp_complete(data,data_len);
				if(res){
					data=(char*)dupe_mem_null(data,data_len);
					if(data){
						*resp=data;
						*resp_len=data_len;
						result=TRUE;
					}
					break;
				}
				if(0==n)
					Sleep(10);
			}else{
				break;
			}
			delta=GetTickCount()-tick;
			if(delta>(DWORD)g_timeout){
				break;
			}
		}
		free(tmp);
	}
	return result;
}

static do_http_req(CONNECTION *c,const char *req,char **resp_content,int *resp_content_len)
{
	int result=FALSE;
	ssl_context *ssl;
	int msg_len;
	char *resp=0;
	int resp_len=0;
	int res;
	ssl=&c->ssl;
	msg_len=(int)strlen(req);
	ssl_write(ssl,(BYTE*)req,msg_len);
	//printf("REQ:%s\n---\n",req);
	res=get_response(c,&resp,&resp_len);
	if(res){
		res=get_resp_code(resp,resp_len);
		if(200==res){
			char *content=0;
			int content_len=0;
			res=get_content(resp,resp_len,&content,&content_len);
			if(res){
				char *tmp;
				int tmp_size=content_len+1;
				tmp=malloc(tmp_size);
				if(tmp){
					memcpy(tmp,content,content_len);
					tmp[tmp_size-1]=0;
					*resp_content=tmp;
					*resp_content_len=content_len;
					result=TRUE;
				}
			}
		}else{
			printf("RESP:\n%.*s\n",resp_len,resp);
		}
	}else{
		printf("failed to get response\n");
	}
	if(resp){
		free(resp);
	}
	return result;
}

int connect_disc(CONNECTION *c)
{
	int result=FALSE;
	int res;
	ssl_context *ssl;
	const char *host;
	int port=443;
	ssl=&c->ssl;
	memset(ssl,0,sizeof(ssl_context));
	host="discordapp.com";
	port=443;
	res=ssl_connect(ssl,0,host,port,(int*)&c->sock);
	if(res){
		char *data=0;
		int data_len=0;
		char *json=0;
		int json_len=0;
		const char *user;
		const char *pw;
		user=get_user_name();
		pw=get_password();
		/*
		POST /api/v6/auth/login HTTP/1.1
		Content-Type: application/json
		Host: localhost:50000
		Content-Length: 66
		Connection: Keep-Alive
		Cache-Control: no-cache

		HTTP/1.1 200 OK
		Date: Sat, 22 Jun 2019 15:55:57 GMT
		Content-Type: application/json
		Content-Length: 72
		Connection: keep-alive
		Set-Cookie: __cfduid=xyz; expires=Sun, 21-Jun-20 15:55:57 GMT; path=/; domain=.discordapp.com; HttpOnly
		Strict-Transport-Security: max-age=31536000; includeSubDomains
		Via: 1.1 google
		Alt-Svc: clear
		Expect-CT: max-age=604800, report-uri="https://report-uri.cloudflare.com/cdn-cgi/beacon/expect-ct"
		Server: cloudflare
		CF-RAY: 4eaf68d2dc0eccea-EWR

		{"token": "BLAHBLAH"}

		*/
		append_printf(&data,&data_len,"POST /api/v6/auth/login HTTP/1.1\r\n");
		append_printf(&data,&data_len,"Content-Type: application/json\r\n");
		append_printf(&data,&data_len,"Host: discordapp.com:443\r\n");
		append_printf(&data,&data_len,"Connection: Keep-Alive\r\n");
		append_printf(&data,&data_len,"Cache-Control: no-cache\r\n");
		append_printf(&json,&json_len,"{%\"email\":\"%s\",\"password\":\"%s\"}",user,pw);
		if(data && json){
			char *resp=0;
			int resp_len=0;
			append_printf(&data,&data_len,"Content-Length: %u\r\n\r\n",strlen(json));
			append_printf(&data,&data_len,"%s",json);
			char *content=0;
			int content_len=0;
			res=do_http_req(c,data,&content,&content_len);
			if(res){
				char *token=0;
				content[content_len-1]=0;
				res=extract_token(content,&token);
				if(res){
					strncpy(g_token,token,sizeof(g_token));
					result=TRUE;
				}
				if(token)
					free(token);
			}
			if(content)
				free(content);
		}
		if(data)
			free(data);
		if(json)
			free(json);
	}
	return result;
}


int get_gateway(CONNECTION *c)
{
	int result=FALSE;
	char *data=0;
	int data_len=0;
	/*
	GET /api/v6/gateway HTTP/1.1
	Host: localhost:50000
	Accept-Encoding: identity
	Content-Type: application/json
	Authorization: blahblah
	*/
	append_printf(&data,&data_len,"GET /api/v6/gateway HTTP/1.1\r\n");
	append_printf(&data,&data_len,"Host: discordapp.com:443\r\n");
	append_printf(&data,&data_len,"Accept-Encoding: identity\r\n");
	append_printf(&data,&data_len,"Connection: Keep-Alive\r\n");
	append_printf(&data,&data_len,"Authorization: %s\r\n\r\n",g_token);
	if(data){
		int res;
		char *content=0;
		int content_len=0;
		res=do_http_req(c,data,&content,&content_len);
		if(res){
			JSON_Value *root;
			//{"url": "wss://gateway.discord.gg"}
			root=json_parse_string(content);
			if(json_value_get_type(root)==JSONObject){
				JSON_Object *obj;
				JSON_Value *val;
				const char *url;
				obj=json_value_get_object(root);
				val=json_object_get_value(obj,"url");
				url=json_value_get_string(val);
				if(url){
					int dst_len=sizeof(g_gateway);
					const char *prefix="wss://";
					if(startswithi(url,prefix)){
						url+=strlen(prefix);
					}
					strncpy(g_gateway,url,sizeof(g_gateway));
					g_gateway[sizeof(g_gateway)-1]=0;
					result=TRUE;
				}
			}else{
				printf("Error paring json to get gateway\n");
			}
			json_value_free(root);
		}
		if(content)
			free(content);
		free(data);

	}
	return result;
}

int get_guilds(CONNECTION *c,GUILD_LIST *glist)
{
	int result=FALSE;
	char *data=0;
	int data_len=0;
	remove_all_guilds(glist);
	append_printf(&data,&data_len,"GET /api/v6/users/@me/guilds HTTP/1.1\r\n");
	append_printf(&data,&data_len,"Host: discordapp.com:443\r\n");
	append_printf(&data,&data_len,"Accept-Encoding: identity\r\n");
	append_printf(&data,&data_len,"Connection: Keep-Alive\r\n");
	append_printf(&data,&data_len,"Content-Type: application/json\r\n");
	append_printf(&data,&data_len,"Authorization: %s\r\n\r\n",g_token);
	if(data){
		char *content=0;
		int content_len=0;
		int res;
		res=do_http_req(c,data,&content,&content_len);
		if(res){
			JSON_Value *root;
			root=json_parse_string(content);
			if(json_value_get_type(root)==JSONArray){
				JSON_Array *guilds;
				int i,count;
				result=TRUE;
				guilds=json_value_get_array(root);
				count=json_array_get_count(guilds);
				for(i=0;i<count;i++){
					char *id,*name;
					JSON_Object *guild=json_array_get_object(guilds,i);
					id=strdup(json_object_get_string(guild,"id"));
					name=strdup(json_object_get_string(guild,"name"));
					if(id && name){
						GUILD g={0};
						g.id=id;
						g.name=name;
						fix_spaced_str(name);
						if(!add_guild(glist,&g)){
							result=FALSE;
						}
					}else{
						result=FALSE;
					}
					free(id);
					free(name);
				}
			}
			json_value_free(root);
		}
		if(content)
			free(content);
		free(data);
	}
	return result;

}

static int get_messages(CONNECTION *c,MESSAGE_LIST *mlist,const char *chan_id,int count,const char *before_id,int pinned)
{
	int result=FALSE;
	char *data=0;
	int data_len=0;
	if(count>100)
		count=100;
	if(pinned){
		append_printf(&data,&data_len,"GET /api/v6/channels/%s/pins",chan_id);
	}else{
		append_printf(&data,&data_len,"GET /api/v6/channels/%s/messages",chan_id);
		append_printf(&data,&data_len,"?limit=%u",count);
		if(before_id && before_id[0]!=0){
			append_printf(&data,&data_len,"&before=",before_id);
		}
	}
	append_printf(&data,&data_len," HTTP/1.1\r\n");
	append_printf(&data,&data_len,"Host: discordapp.com:443\r\n");
	append_printf(&data,&data_len,"Accept-Encoding: identity\r\n");
	append_printf(&data,&data_len,"Connection: Keep-Alive\r\n");
	append_printf(&data,&data_len,"Content-Type: application/json\r\n");
	append_printf(&data,&data_len,"Authorization: %s\r\n\r\n",g_token);
	if(data){
		char *content=0;
		int content_len=0;
		int res;
		res=do_http_req(c,data,&content,&content_len);
		if(res){
			JSON_Value *root;
			root=json_parse_string(content);
			if(json_value_get_type(root)==JSONArray){
				JSON_Array *msgs;
				int i,count;
				msgs=json_value_get_array(root);
				count=json_array_get_count(msgs);
				for(i=0;i<count;i++){
					JSON_Object *msg;
					const char *id;
					const char *content;
					const char *timestamp;
					const char *auth;
					const char *auth_id;
					msg=json_array_get_object(msgs,i);
					id=json_object_get_string(msg,"id");
					content=json_object_get_string(msg,"content");
					timestamp=json_object_get_string(msg,"timestamp");
					auth=json_object_dotget_string(msg,"author.username");
					auth_id=json_object_dotget_string(msg,"author.id");
					if(id && content && timestamp){
						MESSAGE msg={0};
						if(0==auth)
							auth="unknown";
						if(0==auth_id)
							auth_id="unknown";
						msg.author=(char*)auth;
						msg.auth_id=(char*)auth_id;
						msg.id=(char*)id;
						msg.msg=(char*)content;
						msg.timestamp=(char*)timestamp;
						//if(pinned)
						//	printf("msg:%s %s\n",auth,content);
						if(!add_message(mlist,&msg)){
							printf("Failed to add msg %s\n",id);
						}
					}
				}
			}
			json_value_free(root);
			result=TRUE;
		}
		if(content)
			free(content);
		free(data);

	}
	return result;
}

static int get_channels_for_guild(CONNECTION *c,const char *guild_id,CHANNEL_LIST *clist)
{
	int result=FALSE;
	char *data=0;
	int data_len=0;
	append_printf(&data,&data_len,"GET /api/v6/guilds/%s/channels HTTP/1.1\r\n",guild_id);
	append_printf(&data,&data_len,"Host: discordapp.com:443\r\n");
	append_printf(&data,&data_len,"Accept-Encoding: identity\r\n");
	append_printf(&data,&data_len,"Connection: Keep-Alive\r\n");
	append_printf(&data,&data_len,"Content-Type: application/json\r\n");
	append_printf(&data,&data_len,"Authorization: %s\r\n\r\n",g_token);
	if(data){
		ssl_context *ssl;
		int msg_len;
		char *resp=0;
		int resp_len=0;
		int res;
		//printf("REQ:%s\n",data);
		//printf("---\n");
		ssl=&c->ssl;
		msg_len=strlen(data);
		ssl_write(ssl,(BYTE*)data,msg_len);
		res=get_response(c,&resp,&resp_len);
		if(res){
			char *content;
			int content_len=0;
			//printf("RESP:\n%.*s\n",resp_len,resp);
			res=get_content(resp,resp_len,&content,&content_len);
			if(res){
				JSON_Value *root;
				root=json_parse_string(content);
				if(json_value_get_type(root)==JSONArray){
					JSON_Array *chans;
					int i,count;
					chans=json_value_get_array(root);
					count=json_array_get_count(chans);
					for(i=0;i<count;i++){
						JSON_Object *chan;
						double x;
						chan=json_array_get_object(chans,i);
						x=json_object_get_number(chan,"type");
						if(0==x){ //text channels only
							char *id,*name,*topic;
							id=strdup(json_object_get_string(chan,"id"));
							name=strdup(json_object_get_string(chan,"name"));
							topic=strdup(json_object_get_string(chan,"topic"));
							if(id && name){
								CHANNEL chan={0};
								chan.id=id;
								chan.name=name;
								fix_spaced_str(name);
								if(0==topic)
									chan.topic="no topic set";
								else
									chan.topic=topic;
								add_channel(clist,&chan);
							}
							free(name);free(topic);free(id);
						}
					}
				}
				json_value_free(root);
				result=TRUE;
			}else{
				printf("failed to get content:\nresp=%.*s\n",resp_len,resp);
			}
		}else{
			printf("failed to get response\n");
		}
		free(data);

	}
	return result;
}
int get_all_channels(CONNECTION *c,GUILD_LIST *glist)
{
	int result=TRUE;
	int i,count;
	count=glist->count;
	for(i=0;i<count;i++){
		int res;
		GUILD *g;
		g=&glist->guild[i];
		remove_all_channels(&g->channels);
		res=get_channels_for_guild(c,g->id,&g->channels);
		if(!res){
			result=FALSE;
		}
	}
	return result;
}
int get_all_messages(CONNECTION *c,GUILD_LIST *glist)
{
	int result=FALSE;
	int i,count;
	count=glist->count;
	for(i=0;i<count;i++){
		int j,chan_count;
		GUILD *g=&glist->guild[i];
		chan_count=g->channels.count;
		for(j=0;j<chan_count;j++){
			CHANNEL *chan;
			int k,msg_count;
			chan=&g->channels.chan[j];
			remove_all_msg(&chan->msgs);
			get_messages(c,&chan->msgs,chan->id,100,NULL,FALSE);
			get_messages(c,&chan->pin_msgs,chan->id,0,NULL,TRUE);
			printf("%s %s msg count %i\n",g->name,chan->name,chan->msgs.count);
			msg_count=chan->msgs.count;
			for(k=0;k<msg_count;k++){
				MESSAGE *m=&chan->msgs.m[k];
				add_nick(&chan->nicks,m->author);
			}
			msg_count=chan->pin_msgs.count;
			for(k=0;k<msg_count;k++){
				MESSAGE *m=&chan->pin_msgs.m[k];
				add_nick(&chan->nicks,m->author);
			}
		}
	}
	return result;
}

static int post_message(CONNECTION *c,const char *chan_id,const char *msg)
{
	int result=FALSE;
	char *data=0;
	int data_len=0;
	char *json_str=0;
	JSON_Value *root_val;
	JSON_Object *root_obj;
	int do_post=FALSE;
	append_printf(&data,&data_len,"POST /api/v6/channels/%s/messages HTTP/1.1\r\n",chan_id);
	append_printf(&data,&data_len,"Host: discordapp.com:443\r\n");
	append_printf(&data,&data_len,"Accept-Encoding: identity\r\n");
	append_printf(&data,&data_len,"Connection: Keep-Alive\r\n");
	append_printf(&data,&data_len,"Content-Type: application/json\r\n");
	append_printf(&data,&data_len,"Authorization: %s\r\n",g_token);
	root_val=json_value_init_object();
	root_obj=json_value_get_object(root_val);
	/*
	{
	"content": "Hello, World!",
	"tts": false,
	}
	*/
	json_object_set_string(root_obj,"content",msg);
	json_object_set_boolean(root_obj,"tts",0);
	json_str=json_serialize_to_string(root_val);
	if(json_str){
		append_printf(&data,&data_len,"Content-Length: %u\r\n",strlen(json_str));
		append_printf(&data,&data_len,"\r\n%s",json_str);
		json_free_serialized_string(json_str);
		do_post=TRUE;
	}
	json_value_free(root_val);
	if(!do_post){
		printf("unable to create POST msg request\n");
		goto ERROR_POST;
	}
	{
		ssl_context *ssl;
		int msg_len;
		char *resp=0;
		int resp_len=0;
		int res;
		//printf("REQ:%s\n",data);
		//printf("---\n");
		ssl=&c->ssl;
		msg_len=strlen(data);
		ssl_write(ssl,(BYTE*)data,msg_len);
		res=get_response(c,&resp,&resp_len);
		if(res){
			res=get_resp_code(resp,resp_len);
			if(200!=res){
				printf("failed to POST message:%*s",resp_len,resp);
			}
		}else{
			printf("failed to get response for POST message\n");
		}
	}
ERROR_POST:
	free(data);
	return result;
}

static int dump_message_list(MESSAGE_LIST *mlist,const char *prefix)
{
	int i,count;
	count=mlist->count;
	for(i=0;i<count;i++){
		MESSAGE *msg;
		msg=&mlist->m[i];
		printf("%s<%s> %s %s\n",prefix,msg->author,msg->timestamp,msg->msg);
	}
	return 0;
}
static int dump_guild_stuff(GUILD_LIST *glist)
{
	int i,count;
	count=glist->count;
	for(i=0;i<count;i++){
		int j,chan_count;
		GUILD *g=&glist->guild[i];
		printf("GUILD %s %s\n",g->name,g->id);
		chan_count=g->channels.count;
		for(j=0;j<chan_count;j++){
			CHANNEL *chan;
			chan=&g->channels.chan[j];
			printf(" %s %s %s\n",chan->name,chan->topic,chan->id);
			dump_message_list(&chan->msgs,"  :");

		}
	}
	return 0;
}

static CRITICAL_SECTION g_mutex={0};
static int mutex_ready=FALSE;
static HANDLE g_event=0;
typedef struct{
	int cmd;
	char *data;
	void *next;
	void *prev;
}DISCORD_CMD;
DISCORD_CMD *g_cmd_list=0;

static void init_mutex()
{
	if(!mutex_ready){
		mutex_ready=TRUE;
		InitializeCriticalSection(&g_mutex);
	}
}
static void enter_mutex()
{
	EnterCriticalSection(&g_mutex);
}
static void leave_mutex()
{
	LeaveCriticalSection(&g_mutex);
}
int add_discord_cmd(int cmd,const char *cmd_data)
{
	int result=FALSE;
	DISCORD_CMD *tmp;
	tmp=calloc(1,sizeof(DISCORD_CMD));
	if(tmp){
		char *str;
		tmp->cmd=cmd;
		str=strdup(cmd_data);
		if(str){
			tmp->data=str;
			result=TRUE;
		}
		if(result){
			enter_mutex();
			if(0==g_cmd_list){
				g_cmd_list=tmp;
			}else{
				DISCORD_CMD *node=g_cmd_list;
				while(node->next){
					node=node->next;
				}
				node->next=tmp;
				tmp->prev=node;
			}
			leave_mutex();
			SetEvent(g_event);
		}
	}
	return result;
}

int pop_discord_cmd(DISCORD_CMD *cmd)
{
	int result=FALSE;
	enter_mutex();
	if(0==g_cmd_list){
		goto ERROR_POP;
	}
	cmd->cmd=g_cmd_list->cmd;
	cmd->data=g_cmd_list->data;
	cmd->next=0;
	cmd->prev=0;
	g_cmd_list=g_cmd_list->next;
	if(g_cmd_list)
		g_cmd_list->prev=0;
	result=TRUE;
ERROR_POP:
	leave_mutex();
	return result;
}

static void free_discord_cmd(DISCORD_CMD *cmd)
{
	if(0==cmd)
		return;
	if(cmd->data){
		free(cmd->data);
		cmd->data=0;
		cmd->cmd=0;
		cmd->next=0;
		cmd->prev=0;
	}
}

static int copy_str(char *dst,int dst_size,const char *start,const char *end)
{
	int result=0;
	SIZE_T len;
	if(start>=end){
		return result;
	}
	len=end-start;
	if(dst_size>0){
		if(len>(SIZE_T)dst_size){
			len=dst_size;
		}
		strncpy(dst,start,len);
		dst[dst_size-1]=0;
	}
	result=len;
	return result;
}
static const char *last_nonspace(const char *str)
{
	const char *result=str;
	int i,count;
	count=strlen(str);
	for(i=count-1;i>=0;i--){
		char a;
		a=str[i];
		if(!isspace(a)){
			result=str+i+1;
			break;
		}
	}
	return result;
}
static int extract_guild_chan(const char *str,char *guild,int guild_size,char *chan,int chan_size)
{
	int result=FALSE;
	const char *tmp;
	const char *res;
	char delim;
	tmp=str;
	if('#'==tmp[0]){
		tmp++;
	}
	res=0;
	delim='.';
	if('"'==tmp[0]){
		delim='"';
		tmp++;
	}
	res=strchr(tmp,delim);
	if(res){
		copy_str(guild,guild_size,tmp,res);
		tmp=res;
	}
	if('"'==tmp[0]){
		tmp++;
	}
	//should be pointing to '.'
	if(res){
		const char *end;
		tmp++;
		if('"'==tmp[0]){
			tmp++;
			end=strchr(tmp,'"');
		}else{
			end=last_nonspace(tmp);
		}
		copy_str(chan,chan_size,tmp,end);
		result=TRUE;
	}
	return result;
}

static int has_whitespace(const char *str)
{
	int result=FALSE;
	int index=0;
	while(1){
		unsigned char a=str[index++];
		if(0==a){
			break;
		}
		if(isspace(a)){
			result=TRUE;
			break;
		}
	}
	return result;
}
static int print_irc_chan(const char *guild,const char *chan,char *out,int out_len)
{
	const char *guild_quotes="";
	const char *chan_quotes="";
	if(has_whitespace(guild)){
		guild_quotes="\"";
	}
	if(has_whitespace(chan)){
		chan_quotes="\"";
	}
	return __snprintf(out,out_len,"#%s%s%s.%s%s%s",guild_quotes,guild,guild_quotes,chan_quotes,chan,chan_quotes);
}

static void post_msg_to_irc(const char *irc_chan,const char *nick,const char *msg)
{
	int i,msg_len;
	const int block_size=350;
	msg_len=strlen(msg);
	for(i=0;i<msg_len;i+=block_size){
		char irc_msg[512]={0};
		const char *chunk;
		int chunk_len;
		chunk=msg+i;
		chunk_len=msg_len-i;
		if(chunk_len>block_size){
			chunk_len=block_size;
		}
		if(chunk_len<=0){
			break;
		}
		//CHAN_MSG chan,nick,msg
		__snprintf(irc_msg,sizeof(irc_msg),"%s %s %s %*s",get_irc_msg_str(CHAN_MSG),irc_chan,nick,chunk_len,chunk);
		push_irc_msg(irc_msg);
	}
}

static void push_irc_msgs(MESSAGE_LIST *mlist,int start_index,int end_index)
{	
	int i;
	for(i=start_index;i<end_index;i++){
		MESSAGE *m;
		if(i>=mlist->count)
			break;
		m=&mlist->m[i];


	}
}

static int process_join_chan(DISCORD_CMD *cmd)
{
	int result=FALSE;
	char guild[80]={0};
	char chan[80]={0};
	char irc_chan[160]={0};
	char *str;
	int i,count;
	CHANNEL *target_chan=0;
	str=cmd->data;
	extract_guild_chan(str,guild,sizeof(guild),chan,sizeof(chan));
	print_irc_chan(guild,chan,irc_chan,sizeof(irc_chan));
	printf("SRC=%s\n",str);
	printf("guild=%s chan=%s\n",guild,chan);
	count=g_guild_list.count;
	for(i=0;i<count;i++){
		GUILD *g;
		int res;
		g=&g_guild_list.guild[i];
		res=stricmp(g->name,guild);
		if(0==res){
			int j,chan_count;
			chan_count=g->channels.count;
			for(j=0;j<chan_count;j++){
				CHANNEL *c;
				c=&g->channels.chan[j];
				res=stricmp(c->name,chan);
				if(0==res){
					target_chan=c;
					result=TRUE;
					break;
				}
			}
		}
	}
	if(result){
		char tmp[256];
		const char *prefix=get_irc_msg_str(OK_JOIN_CHAN);
		__snprintf(tmp,sizeof(tmp),"%s %s",prefix,irc_chan);
		push_irc_msg(tmp);
		printf("OK JOIN CHANNEL: %s\n",irc_chan);
		if(target_chan){
			NICK_LIST *nlist;
			char *name_list=0;
			int name_len=0;
			int started=FALSE;
			nlist=&target_chan->nicks;
			count=nlist->nick_count;
			for(i=0;i<count;i++){
				char *name;
				//353 nick = #n64dev :@APE @AlexAltea @Atomim 
				name=nlist->nick[i];
				if(!started){
					append_printf(&name_list,&name_len,"%s %s :",get_irc_msg_str(NAME_LIST),irc_chan);
					started=TRUE;
				}
				append_printf(&name_list,&name_len,"%s ",name);
				if(name_len>=350){
					push_irc_msg(name_list);
					name_list[0]=0;
					started=FALSE;
				}
			}
			if(name_list){
				if(name_list[0])
					push_irc_msg(name_list);
				free(name_list);
			}
			push_irc_msgs(&target_chan->pin_msgs,0,target_chan->pin_msgs.count);
		}
	}else{
		char tmp[80];
		__snprintf(tmp,sizeof(tmp),"%s %s",get_irc_msg_str(UNKNOWN_CHAN),irc_chan);
		push_irc_msg(tmp);
	}
	return result;
}

static int process_list_chan(DISCORD_CMD *cmd)
{
	int result=FALSE;
	int i,count;
	char tmp[256]={0};
	_snprintf(tmp,sizeof(tmp),"%s",get_irc_msg_str(START_CHAN_LIST));
	push_irc_msg(tmp);
	count=g_guild_list.count;
	for(i=0;i<count;i++){
		int j;
		int chan_count;
		GUILD *guild;
		guild=&g_guild_list.guild[i];
		chan_count=guild->channels.count;
		for(j=0;j<chan_count;j++){
			CHANNEL *chan;
			const char *prefix;
			char irc_chan[160]={0};
			chan=&guild->channels.chan[j];
			prefix=get_irc_msg_str(CHAN_LIST);
			print_irc_chan(guild->name,chan->name,irc_chan,sizeof(irc_chan));
			__snprintf(tmp,sizeof(tmp),"%s %s %i :%s",prefix,irc_chan,chan->nicks.nick_count,chan->topic);
			push_irc_msg(tmp);
		}
	}
	__snprintf(tmp,sizeof(tmp),"%s",get_irc_msg_str(END_CHAN_LIST));
	push_irc_msg(tmp);
	return result;
}

static int process_post_msg(CONNECTION *c,DISCORD_CMD *cmd)
{
	int result=FALSE;
	int i,count;
	char chan_name[160]={0};
	const char *chan_msg;
	int exit=FALSE;
	get_word(cmd->data,chan_name,sizeof(chan_name));
	chan_msg=seek_next_word(cmd->data);
	if(0==chan_msg || 0==chan_name[0]){
		printf("cant find chan data from:%s",cmd->data);
		return result;
	}
	count=g_guild_list.count;
	for(i=0;i<count;i++){
		int j;
		int chan_count;
		GUILD *guild;
		guild=&g_guild_list.guild[i];
		chan_count=guild->channels.count;
		for(j=0;j<chan_count;j++){
			CHANNEL *chan;
			const char *prefix;
			char irc_chan[160]={0};
			chan=&guild->channels.chan[j];
			prefix=get_irc_msg_str(CHAN_LIST);
			print_irc_chan(guild->name,chan->name,irc_chan,sizeof(irc_chan));
			if(0==stricmp(irc_chan,chan_name)){
				char *tmp;
				exit=TRUE;
				if(':'==chan_msg[0]){
					chan_msg++;
				}
				tmp=strdup(chan_msg);
				if(tmp){
					trim_right(tmp);
					post_message(c,chan->id,tmp);
					free(tmp);
				}
				break;
			}
		}
		if(exit){
			break;
		}
	}
	return result;
}

static int process_chan_msg(DISCORD_CMD *cmd)
{
	int result=FALSE;
	char chan_id[40]={0};
	char nick[80]={0};
	char *content=0;
	int content_size=4096;
	const char *str=cmd->data;
	if(0==str){
		return result;
	}
	content=calloc(content_size,1);
	if(0==content){
		return result;
	}
	//chan id, username, content
	get_word(str,chan_id,sizeof(chan_id));
	str=seek_next_word(str);
	get_word(str,nick,sizeof(nick));
	str=seek_next_word(str);
	if(0==str){
		goto EXIT_CHAN_MSG;
	}
	strncpy(content,str,content_size);
	{
		int i,count;
		char irc_chan[160]={0};
		count=g_guild_list.count;
		for(i=0;i<count;i++){
			GUILD *g;
			int j;
			int chan_count;
			g=&g_guild_list.guild[i];
			chan_count=g->channels.count;
			for(j=0;j<chan_count;j++){
				CHANNEL *chan;
				chan=&g->channels.chan[j];
				if(0==stricmp(chan->id,chan_id)){
					print_irc_chan(g->name,chan->name,irc_chan,sizeof(irc_chan));
					break;
				}
			}
			if(irc_chan[0]){
				break;
			}
		}
		post_msg_to_irc(irc_chan,nick,content);
		/*
		content_len=strlen(content);
		for(i=0;i<content_len;i+=block_size){
			char irc_msg[512]={0};
			char *chunk;
			int chunk_len;
			chunk=content+i;
			chunk_len=content_len-i;
			if(chunk_len>block_size){
				chunk_len=block_size;
			}
			if(chunk_len<=0){
				break;
			}
			//CHAN_MSG chan,nick,msg
			__snprintf(irc_msg,sizeof(irc_msg),"%s %s %s %*s",get_irc_msg_str(CHAN_MSG),irc_chan,nick,chunk_len,chunk);
			push_irc_msg(irc_msg);
		}
		*/
	}
EXIT_CHAN_MSG:
	free(content);
	return result;
}

static int process_requests(CONNECTION *c)
{
	int result=FALSE;
	HANDLE hlist[2]={0};
	int hcount=0;
	DWORD res;
	hlist[hcount++]=(HANDLE)c->sock;
	hlist[hcount++]=g_event;
	res=WSAWaitForMultipleEvents(hcount,hlist,FALSE,100,FALSE);
	switch(res){
	case WAIT_OBJECT_0: //socket
		{
			result=TRUE;
			printf("socket event\n");
		}
		break;
	case WAIT_OBJECT_0+1: //req
		while(1)
		{
			DISCORD_CMD cmd={0};
			int res;
			result=TRUE;
			printf("DISCORD COMMAND EVENT\n");
			res=pop_discord_cmd(&cmd);
			if(res){
				switch(cmd.cmd){
				case CMD_JOIN_CHAN:
					process_join_chan(&cmd);
					break;
				case CMD_LIST_CHAN:
					process_list_chan(&cmd);
					break;
				case CMD_GET_MSGS:
					break;
				case CMD_POST_MSG:
					process_post_msg(c,&cmd);
					break;
				case CMD_CHAN_MSG:
					process_chan_msg(&cmd);
					break;
				}
				free_discord_cmd(&cmd);
			}else{
				break;
			}
		}
		break;
	case WAIT_TIMEOUT:
		result=TRUE;
		break;
	default:
		Sleep(100);
		result=FALSE;
		break;
	}
	return result;
}


void discord_thread(void *args)
{
	CONNECTION con={0};
	enum{
		DISC_CONNECT=0,
		DISC_GET_GUILDS,
		DISC_GET_CHANNELS,
		DISC_GET_MESSAGES,
		DISC_GET_GATEWAY,
		DISC_WAIT_CMD,
	};
	int state=DISC_CONNECT;
	printf("discord_thread started\n");
	while(1){
		switch(state){
		case DISC_CONNECT:
			{
				int res;
				close_connection(&con);
				g_gateway[0]=0;
				res=connect_disc(&con);
				if(res){
					printf("found token\n");
					state=DISC_GET_GUILDS;
				}else{
					printf("failed login\n");
					Sleep(5000);
				}
			}
			break;
		case DISC_GET_GUILDS:
			get_guilds(&con,&g_guild_list);
			state=DISC_GET_CHANNELS;
			break;
		case DISC_GET_CHANNELS:
			get_all_channels(&con,&g_guild_list);
			dump_guild_stuff(&g_guild_list);
			state=DISC_GET_MESSAGES;
			break;
		case DISC_GET_MESSAGES:
			get_all_messages(&con,&g_guild_list);
			state=DISC_GET_GATEWAY;
			break;
		case DISC_GET_GATEWAY:
			if(get_gateway(&con)){
				state=DISC_WAIT_CMD;
				trigger_gateway();
			}else{
				close_connection(&con);
				state=DISC_CONNECT;
				Sleep(5000);
			}
			break;
		case DISC_WAIT_CMD:
			{
				int res;
				res=process_requests(&con);
				if(!res){
					Sleep(5000);
					state=DISC_CONNECT;
				}
			}
			break;
		default:
			Sleep(1000);
			break;
		}
	}
}

int do_wait()
{
	while(1){
		int key=getch();
		if(0x1b==key){
			exit(0);
		}
	}
	return 0;
}

int main(int argc,char **argv)
{
	init_mutex();
	g_event=CreateEventA(NULL,FALSE,FALSE,"discord_event");

	_beginthread(&gateway_thread,0,NULL);
	_beginthread(&discord_thread,0,NULL);
	_beginthread(&irc_thread,0,NULL);
	do_wait();
	return 0;
}