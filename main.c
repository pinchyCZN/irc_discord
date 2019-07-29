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
	char *name;
	char *topic;
	char *id;
	MESSAGE_LIST msgs;
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
	printf("REQ:%s\n---\n",req);
	res=get_response(c,&resp,&resp_len);
	if(res){
		printf("RESP:\n%.*s\n",resp_len,resp);
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

static int get_messages(CONNECTION *c,MESSAGE_LIST *mlist,const char *chan_id,int count,const char *before_id)
{
	int result=FALSE;
	char *data=0;
	int data_len=0;
	append_printf(&data,&data_len,"GET /api/v6/channels/%s/messages",chan_id);
	append_printf(&data,&data_len,"?limit=%u",count);
	if(before_id && before_id[0]!=0){
		append_printf(&data,&data_len,"&before=",before_id);
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
		printf("REQ:%s\n",data);
		printf("---\n");
		ssl=&c->ssl;
		msg_len=strlen(data);
		ssl_write(ssl,(BYTE*)data,msg_len);
		res=get_response(c,&resp,&resp_len);
		if(res){
			char *content;
			int content_len=0;
			printf("RESP:\n%.*s\n",resp_len,resp);
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
enum{
	CMD_JOIN_CHAN=1,
	CMD_GET_MSGS,
	CMD_POST_MSG,
};
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
int add_discord_cmd(DISCORD_CMD *cmd)
{
	int result=FALSE;
	DISCORD_CMD *tmp;
	tmp=calloc(1,sizeof(DISCORD_CMD));
	if(tmp){
		char *str;
		tmp->cmd=cmd->cmd;
		str=strdup(cmd->data);
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
		return result;
	}
	cmd->cmd=g_cmd_list->cmd;
	cmd->data=g_cmd_list->data;
	cmd->next=0;
	cmd->prev=0;
	g_cmd_list=g_cmd_list->next;
	g_cmd_list->prev=0;
	result=TRUE;
	leave_mutex();
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
	res=WSAWaitForMultipleEvents(hcount,hlist,FALSE,1000,FALSE);
	if(WAIT_OBJECT_0==res){
	}
	Sleep(100);
	result=TRUE;
	return result;
}


void discord_thread(void *args)
{
	CONNECTION con={0};
	enum{
		DISC_CONNECT=0,
		DISC_GET_GUILDS,
		DISC_GET_CHANNELS,
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