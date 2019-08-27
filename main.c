#define CINTERFACE
#include <winsock2.h>
#include <Windows.h>
#include <process.h>
#include <stdio.h>
#include <conio.h>
#include <WinInet.h>
#include <string.h>

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
	__int64 ftime;
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
typedef struct{
	char *id;
	char *recipient;
	char *recipient_id;
	MESSAGE_LIST msgs;
}DM_CHAN;
typedef struct{
	DM_CHAN *dm_chan;
	int count;
}DM_LIST;

static GUILD_LIST g_guild_list={0};
static DM_LIST g_dm_list={0};

static int g_enable_dbgprint=FALSE;
static void DBGPRINT(const char *fmt,...)
{
	va_list ap;
	if(!g_enable_dbgprint)
		return;
	va_start(ap,fmt);
	vprintf(fmt,ap);
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
		remove_all_msg(&tmp->pin_msgs);
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
static int remove_all_dm_chans(DM_LIST *dlist)
{
	int i,count;
	count=dlist->count;
	for(i=0;i<count;i++){
		DM_CHAN *tmp;
		tmp=&dlist->dm_chan[i];
		remove_all_msg(&tmp->msgs);
		free(tmp->recipient);
		free(tmp->recipient_id);
		free(tmp->id);
	}
	free(dlist->dm_chan);
	dlist->dm_chan=0;
	dlist->count=0;
	return TRUE;
}
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
static int add_dm_chan(DM_LIST *dlist,DM_CHAN *chan)
{
	int result=FALSE;
	int count,index;
	DM_CHAN *ptr;
	int size;
	char *recipient,*recipient_id,*id;
	recipient=strdup(chan->recipient);
	recipient_id=strdup(chan->recipient_id);
	id=strdup(chan->id);
	if(0==recipient || 0==recipient_id || 0==id){
		goto ERROR_ADD_CHAN;
	}
	count=dlist->count;
	index=count;
	count++;
	size=sizeof(DM_CHAN)*count;
	ptr=realloc(dlist->dm_chan,size);
	if(ptr){
		DM_CHAN *tmp;
		tmp=&ptr[index];
		memset(tmp,0,sizeof(DM_CHAN));
		memcpy(&tmp->msgs,&chan->msgs,sizeof(tmp->msgs));
		tmp->id=id;
		tmp->recipient=recipient;
		tmp->recipient_id=recipient_id;
		dlist->dm_chan=ptr;
		dlist->count=count;
		result=TRUE;
	}
ERROR_ADD_CHAN:
	if(!result){
		free(recipient);free(recipient_id);free(id);
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

static int my_strcmp(const void *arg1,const void *arg2)
{
	char **a,**b;
	a=(char**)arg1;
	b=(char**)arg2;
	int res=stricmp(a[0],b[0]);
	return res;
}
static int merge_new_nicks(NICK_LIST *nlist,MESSAGE_LIST *mlist)
{
	int result=0;
	int i,count;
	qsort(nlist->nick,nlist->nick_count,sizeof(char*),&my_strcmp);
	count=mlist->count;
	for(i=0;i<count;i++){
		MESSAGE *m;
		char *str;
		if(nlist->nick_count>=1000)
			break;
		m=&mlist->m[i];
		str=bsearch(&m->author,nlist->nick,nlist->nick_count,sizeof(char*),&my_strcmp);
		if(0==str){
			printf("adding nick:%s\n",m->author);
			add_nick(nlist,m->author);
			qsort(nlist->nick,nlist->nick_count,sizeof(char**),&my_strcmp);
			result++;
		}
	}
	qsort(nlist->nick,nlist->nick_count,sizeof(char**),&my_strcmp);
	return result;
}

static int add_message(MESSAGE_LIST *mlist,MESSAGE *msg)
{
	int result=FALSE;
	int index,count;
	MESSAGE *ptr;
	char *author,*auth_id,*id,*content,*timestamp;
	int size;
	__int64 ftime;
	author=strdup(msg->author);
	auth_id=strdup(msg->auth_id);
	id=strdup(msg->id);
	content=strdup(msg->msg);
	timestamp=strdup(msg->timestamp);
	ftime=msg->ftime;
	if(0==ftime)
		time_str_to_ftime(timestamp,&ftime);
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
		tmp->ftime=ftime;
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

static void remove_some_messages(MESSAGE_LIST *mlist,int target_amount)
{
	int i,count;
	MESSAGE_LIST new_list={0};
	int center,delta;
	int start,end;
	count=mlist->count;
	if(count<target_amount)
		return;
	center=count/2;
	delta=(count-target_amount)/2;
	start=center-delta;
	end=center+delta;
	for(i=0;i<count;i++){
		if(i<start || i>end){
			MESSAGE *m=&mlist->m[i];
			add_message(&new_list,m);
		}
	}
	remove_all_msg(mlist);
	*mlist=new_list;
}


static int compare_message(const void *arg1,const void *arg2)
{
	int result;
	__int64 val1,val2;
	MESSAGE *a,*b;
	a=(MESSAGE*)arg1;
	b=(MESSAGE*)arg2;
	val1=a->ftime;
	val2=b->ftime;
	result=1;
	if(val1 < val2)
		result=-1; //-1 is first argument is less than the second
	else if(val1==val2)
		result=0;
	return result;
}
// sort from oldest to newest
static void sort_messages(MESSAGE_LIST *mlist)
{
	qsort(mlist->m,mlist->count,sizeof(MESSAGE),&compare_message);
}

static int compare_guilds(const void *arg1,const void *arg2)
{
	const GUILD *g1,*g2;
	int res;
	g1=(GUILD*)arg1;
	g2=(GUILD*)arg2;
	res=stricmp(g1->name,g2->name);
	return res;
}
static void sort_guilds(GUILD_LIST *glist)
{
	qsort(glist->guild,glist->count,sizeof(GUILD),&compare_guilds);
}
static int compare_channels(const void *arg1,const void *arg2)
{
	const CHANNEL *c1,*c2;
	int res;
	c1=(CHANNEL*)arg1;
	c2=(CHANNEL*)arg2;
	res=stricmp(c1->name,c2->name);
	return res;
}
static void sort_channels(CHANNEL_LIST *clist)
{
	qsort(clist->chan,clist->count,sizeof(CHANNEL),&compare_channels);
}
static int compare_dm_channels(const void *arg1,const void *arg2)
{
	const DM_CHAN *d1,*d2;
	d1=(DM_CHAN*)arg1;
	d2=(DM_CHAN*)arg2;
	return stricmp(d1->recipient,d2->recipient);
}
static void sort_dm_channels(DM_LIST *dmlist)
{
	qsort(dmlist->dm_chan,dmlist->count,sizeof(DM_CHAN),&compare_dm_channels);
}

static int get_channel_obj(const char *guild,const char *channel,CHANNEL **chan_obj)
{
	int result=FALSE;
	int i,count;
	count=g_guild_list.count;
	for(i=0;i<count;i++){
		GUILD *g;
		g=&g_guild_list.guild[i];
		if(0==stricmp(g->name,guild)){
			int j,chan_count;
			g=&g_guild_list.guild[i];
			chan_count=g->channels.count;
			for(j=0;j<chan_count;j++){
				CHANNEL *c;
				c=&g->channels.chan[j];
				if(0==stricmp(c->name,channel)){
					*chan_obj=c;
					result=TRUE;
					break;
				}
			}
		}
		if(result)
			break;
	}
	return result;
}
static int find_user_id(const char *nick,char *uid,int uid_len)
{
	int result=FALSE;
	int i,count;
	const char *found=0;
	count=g_guild_list.count;
	for(i=0;i<count;i++){
		int j,chan_count;
		GUILD *g=&g_guild_list.guild[i];
		chan_count=g->channels.count;
		for(j=0;j<chan_count;j++){
			int k,msg_count;
			CHANNEL *chan;
			chan=&g->channels.chan[j];
			msg_count=chan->msgs.count;
			for(k=0;k<msg_count;k++){
				MESSAGE *m;
				m=&chan->msgs.m[k];
				if(0==stricmp(m->author,nick)){
					found=m->auth_id;
					goto FOUND_ID;
				}
			}
		}
	}
FOUND_ID:
	if(found){
		__snprintf(uid,uid_len,"%s",found);
		result=TRUE;
	}
	return result;
}

static int is_chunked(const char *str)
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
		char *ptr;
		char *end;
		char *content=0;
		int failed=FALSE;
		content_len=0;
		ptr=content_start;
		end=data+data_len;
		while(1){
			char *next;
			int chunk_len;
			chunk_len=strtoul(ptr,0,16);
			if(0==chunk_len){
				break;
			}
			next=strchr(ptr,'\n');
			if(0==next){
				failed=TRUE;
				break;
			}
			next++;
			if((next+chunk_len)>=end){
				failed=TRUE;
				break;
			}
			append_printf(&content,&content_len,"%.*s",chunk_len,next);
			ptr=next+chunk_len+2; //account for CRLF
		}
		if((!failed) && content){
			int len;
			len=strlen(content);
			if((content_start+len+1)<=end){
				strncpy(content_start,content,len);
				if(len>0)
					content_start[len]=0;
				*out=content_start;
				*out_len=len;
				result=TRUE;
			}
		}
		free(content);
	}
	return result;
}

static int is_resp_complete(char *data,int data_len)
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
						else{
							if(0==content_len){
								result=TRUE;
							}
							state=2;
						}
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


// allocates and adds null terminator but keeps resp_len as original
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
				if(0==n){
					Sleep(10);
				}
			}else{
				DBGPRINT("ERROR ssl read:%i\n",n);
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

static char *g_last_http_error=0;
static void reset_last_error()
{
	if(g_last_http_error){
		free(g_last_http_error);
		g_last_http_error=0;
	}
}
static void save_last_error(char *resp,int resp_len)
{
	int res;
	char *content=0;
	int content_len=0;
	if(0==resp)
		return;
	res=get_content(resp,resp_len,&content,&content_len);
	if(res){
		JSON_Value *root;
		root=json_parse_string(content);
		if(json_value_get_type(root)==JSONObject){
			JSON_Object *obj;
			JSON_Value *val;
			int code;
			const char *msg;
			int error_len=0;
			obj=json_value_get_object(root);
			val=json_object_get_value(obj,"code");
			code=(int)json_value_get_number(val);
			val=json_object_get_value(obj,"message");
			msg=json_value_get_string(val);
			reset_last_error();
			if(msg)
				append_printf(&g_last_http_error,&error_len,"code:%i msg:%s",code,msg);
			else
				append_printf(&g_last_http_error,&error_len,"%.80s",content);
		}
		json_value_free(root);
	}else{
		char line[80]={0};
		int error_len=0;
		reset_last_error();
		get_line(resp,resp_len,0,line,sizeof(line));
		append_printf(&g_last_http_error,&error_len,"%.80s",line);
	}
}

//allocates resp_content
static int do_http_req(CONNECTION *c,const char *req,char **resp_content,int *resp_content_len)
{
	int result=FALSE;
	ssl_context *ssl;
	int msg_len;
	char *resp=0;
	int resp_len=0;
	int res;
	reset_last_error();
	ssl=&c->ssl;
	drain_response(ssl);
	msg_len=(int)strlen(req);
	res=ssl_write(ssl,(BYTE*)req,msg_len);
	if(res<0){
		WSASetLastError(WSAECONNRESET);
		return result;
	}
	res=get_response(c,&resp,&resp_len);
	if(res){
		DBGPRINT("response:%s\n",resp);
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
			printf("REQ:\n%s|\n---\n",req);
			printf("RESP:\n%.*s|\n---\n",resp_len,resp);
			save_last_error(resp,resp_len);
		}
	}else{
		printf("failed to get response\n");
	}
	if(resp){
		free(resp);
	}
	return result;
}

static int connect_disc(CONNECTION *c)
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


static int get_gateway(CONNECTION *c)
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

static int get_guilds(CONNECTION *c,GUILD_LIST *glist)
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
	sort_guilds(glist);
	return result;

}

static get_me_user_name(CONNECTION *c,char *uname,int uname_len)
{
	int result=FALSE;
	char *data=0;
	int data_len=0;
	append_printf(&data,&data_len,"GET /api/v6/users/@me HTTP/1.1\r\n");
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
			if(json_value_get_type(root)==JSONObject){
				JSON_Object *obj;
				JSON_Value *val;
				const char *str;
				obj=json_value_get_object(root);
				val=json_object_get_value(obj,"username");
				str=json_value_get_string(val);
				if(str){
					//printf("USERNAME:%s\n",str);
					strncpy(uname,str,uname_len);
					if(uname_len){
						uname[uname_len-1]=0;
					}
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
static int add_json_message_obj(JSON_Object *msg,MESSAGE_LIST *mlist)
{
	int result=FALSE;
	const char *id;
	char *content;
	const char *timestamp;
	char *auth;
	const char *auth_id;
	if(0==msg || 0==mlist)
		return result;
	id=json_object_get_string(msg,"id");
	content=strdup(json_object_get_string(msg,"content"));
	timestamp=json_object_get_string(msg,"timestamp");
	auth=strdup(json_object_dotget_string(msg,"author.username"));
	auth_id=json_object_dotget_string(msg,"author.id");
	fix_spaced_str(auth);
	//replace_chars(content,"\t\n\r","  ");
	if(id && content && timestamp && auth){
		MESSAGE msg={0};
		if(0==auth)
			auth="unknown";
		if(0==auth_id)
			auth_id="unknown";
		msg.author=auth;
		msg.auth_id=(char*)auth_id;
		msg.id=(char*)id;
		msg.msg=(char*)content;
		msg.timestamp=(char*)timestamp;
		time_str_to_ftime(timestamp,&msg.ftime);
		if(add_message(mlist,&msg)){
			result=TRUE;
		}else{
			printf("Failed to add msg %s\n",id);
		}
	}
	free(auth);
	free(content);
	return result;
}

//position: -1=before,0=around,1=after
static int get_messages(CONNECTION *c,MESSAGE_LIST *mlist,const char *chan_id,unsigned int limit,int position,const char *pos_id,int pinned)
{
	int result=FALSE;
	char *data=0;
	int data_len=0;
	if(limit>100)
		limit=100;
	else if(0==limit)
		limit=1;
	if(pinned){
		append_printf(&data,&data_len,"GET /api/v6/channels/%s/pins",chan_id);
	}else{
		append_printf(&data,&data_len,"GET /api/v6/channels/%s/messages",chan_id);
		append_printf(&data,&data_len,"?limit=%u",limit);
		if(pos_id && pos_id[0]!=0){
			if(position<0)
				append_printf(&data,&data_len,"&before=%s",pos_id);
			else if(0==position)
				append_printf(&data,&data_len,"&around=%s",pos_id);
			else
				append_printf(&data,&data_len,"&after=%s",pos_id);
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
					msg=json_array_get_object(msgs,i);
					add_json_message_obj(msg,mlist);
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
		char *content=0;
		int content_len=0;
		int res;
		//printf("REQ:%s\n",data);
		//printf("---\n");
		res=do_http_req(c,data,&content,&content_len);
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
						replace_chars(topic,"\t\n\r","  ");
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
			printf("failed to get response\n");
		}
		if(content){
			free(content);
		}
		free(data);
	}
	sort_channels(clist);
	return result;
}
static int get_all_channels(CONNECTION *c,GUILD_LIST *glist)
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
static int get_all_dm_channels(CONNECTION *c,DM_LIST *dlist)
{
	int result=FALSE;
	char *data=0;
	int data_len=0;
	remove_all_dm_chans(dlist);
	append_printf(&data,&data_len,"GET /api/v6/users/@me/channels HTTP/1.1\r\n");
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
				JSON_Array *chans;
				int i,count;
				chans=json_value_get_array(root);
				count=json_array_get_count(chans);
				for(i=0;i<count;i++){
					JSON_Object *chan;
					double x;
					chan=json_array_get_object(chans,i);
					x=json_object_get_number(chan,"type");
					if(1==x){ //DM channels only
						char *id;
						id=strdup(json_object_get_string(chan,"id"));
						if(id){
							JSON_Array *recips;
							recips=json_object_get_array(chan,"recipients");
							if(recips){
								int recip_count;
								recip_count=json_array_get_count(recips);
								if(recip_count){
									JSON_Object *recip;
									recip=json_array_get_object(recips,0);
									if(recip){
										char *uname,*uname_id;
										uname=strdup(json_object_get_string(recip,"username"));
										uname_id=strdup(json_object_get_string(recip,"id"));
										if(uname && uname_id){
											DM_CHAN dm_chan={0};
											fix_spaced_str(uname);
											dm_chan.id=id;
											dm_chan.recipient=uname;
											dm_chan.recipient_id=uname_id;
											add_dm_chan(dlist,&dm_chan);
										}
										free(uname);free(uname_id);
									}
								}
							}

						}
						free(id);
					}
				}
			}
			json_value_free(root);
			result=TRUE;
		}else{
			printf("failed to get response\n");
		}
		if(content){
			free(content);
		}
		free(data);
	}
	sort_dm_channels(dlist);
	return result;
}
static int create_dm_channel(CONNECTION *c,const char *uid,DM_LIST *dlist)
{
	int result=FALSE;
	char *data=0;
	int data_len=0;
	char *json=0;
	int json_len=0;
	append_printf(&data,&data_len,"POST /api/v6/users/@me/channels HTTP/1.1\r\n");
	append_printf(&data,&data_len,"Host: discordapp.com:443\r\n");
	append_printf(&data,&data_len,"Accept-Encoding: identity\r\n");
	append_printf(&data,&data_len,"Connection: Keep-Alive\r\n");
	append_printf(&data,&data_len,"Content-Type: application/json\r\n");
	append_printf(&data,&data_len,"Authorization: %s\r\n",g_token);
	append_printf(&json,&json_len,"{%\"recipient_id\":\"%s\"}",uid);
	if(data && json){
		char *content=0;
		int content_len=0;
		int res;
		append_printf(&data,&data_len,"Content-Length: %u\r\n\r\n",strlen(json));
		append_printf(&data,&data_len,"%s",json);
		res=do_http_req(c,data,&content,&content_len);
		if(res){
			/*
			{
			"last_message_id": "614101661539106828",
			"type": 1,
			"id": "612020557151469578",
			"recipients": [
			{
			"username": "mr test name",
			"discriminator": "1808",
			"id": "612016992551043072",
			"avatar": null
			}
			]
			}
			*/
			JSON_Value *root;
			root=json_parse_string(content);
			if(json_value_get_type(root)==JSONObject){
				JSON_Object *obj;
				JSON_Value *val;
				char *id=0;
				obj=json_value_get_object(root);
				val=json_object_get_value(obj,"id");
				if(val){
					id=strdup(json_value_get_string(val));
				}
				if(id){
					JSON_Array *recips;
					recips=json_object_get_array(obj,"recipients");
					if(recips){
						int recip_count;
						recip_count=json_array_get_count(recips);
						if(recip_count){
							JSON_Object *recip;
							recip=json_array_get_object(recips,0);
							if(recip){
								char *uname,*uname_id;
								uname=strdup(json_object_get_string(recip,"username"));
								uname_id=strdup(json_object_get_string(recip,"id"));
								if(uname && uname_id){
									DM_CHAN dm_chan={0};
									fix_spaced_str(uname);
									dm_chan.id=id;
									dm_chan.recipient=uname;
									dm_chan.recipient_id=uname_id;
									result=add_dm_chan(dlist,&dm_chan);
								}
								free(uname);free(uname_id);
							}
						}
					}
					free(id);
				}
			}
		}
		free(content);
	}
	free(data);
	free(json);
	return result;
}

static int post_message(CONNECTION *c,const char *chan_id,const char *msg,MESSAGE_LIST *mlist)
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
		char *content=0;
		int content_len=0;
		int res;
		res=do_http_req(c,data,&content,&content_len);
		if(res){
			JSON_Value *root;
			root=json_parse_string(content);
			if(json_value_get_type(root)==JSONObject){
				JSON_Object *obj;
				obj=json_value_get_object(root);
				add_json_message_obj(obj,mlist);
			}
			json_value_free(root);
			result=TRUE;
		}
		if(content)
			free(content);
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
		printf("%s<%s> %s %s %s\n",prefix,msg->author,msg->timestamp,msg->id,msg->msg);
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
static DISCORD_CMD *g_cmd_list=0;

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
		}else{
			free(tmp);
		}
	}
	return result;
}

static int pop_discord_cmd(DISCORD_CMD *cmd)
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
	int index=0;
	const char *str=msg;
	if(0==str)
		return;
	while(1){
		char irc_msg[512]={0};
		const char *chunk;
		int chunk_len;
		chunk=str;
		chunk_len=get_str_chunk(str,350,100);
		if(0==chunk_len)
			break;
		str+=chunk_len;
		//CHAN_MSG chan,nick,msg
		__snprintf(irc_msg,sizeof(irc_msg),"%s %s %s %.*s",get_irc_msg_str(CHAN_MSG),irc_chan,nick,chunk_len,chunk);
		push_irc_msg(irc_msg);
	}
}

static void post_priv_msg_to_irc(const char *nick,const char *msg)
{
	int index=0;
	const char *str=msg;
	if(0==str)
		return;
	while(1){
		char irc_msg[512]={0};
		const char *chunk;
		int chunk_len;
		chunk=str;
		chunk_len=get_str_chunk(str,350,100);
		if(0==chunk_len)
			break;
		str+=chunk_len;
		//nick,msg
		__snprintf(irc_msg,sizeof(irc_msg),"%s %s %.*s",get_irc_msg_str(PRIV_MSG),nick,chunk_len,chunk);
		push_irc_msg(irc_msg);
	}
}

static void post_irc_server_msg(const char *fmt,...)
{
	va_list ap;
	char msg[512]={0};
	char tmp[512]={0};
	if(0==fmt)
		return;
	va_start(ap,fmt);
	_vsnprintf(tmp,sizeof(tmp),fmt,ap);
	tmp[sizeof(tmp)-1]=0;
	__snprintf(msg,sizeof(msg),"%s %s",get_irc_msg_str(SERVER_INFO),tmp);
	push_irc_msg(msg);
}

static void push_irc_msgs(MESSAGE_LIST *mlist,int start_index,int end_index,const char *irc_chan,const char *prefix,int flags)
{	
	int i;
	SYSTEMTIME time={0};
	int time_init=FALSE;
	int msg_counter=0;
	for(i=start_index;i<end_index;i++){
		MESSAGE *m;
		const char *nick;
		char *msg=0;
		int msg_len=0;
		if(i>=mlist->count)
			break;
		m=&mlist->m[i];
		nick=m->author;
		if(flags){
			int post_time=FALSE;
			if(!time_init){
				time_str_to_systime(m->timestamp,&time);
				time_init=TRUE;
			}
			if(i==start_index){
				post_time=TRUE;
			}else if(i==(end_index-1)){
				if(msg_counter>15)
					post_time=TRUE;
			}
			{
				SYSTEMTIME tmp_time={0};
				time_str_to_systime(m->timestamp,&tmp_time);
				if(tmp_time.wDay!=time.wDay){
					time=tmp_time;
					post_time=TRUE;
				}
			}
			if(post_time){
				char tmp[80]={0};
				__snprintf(tmp,sizeof(tmp),"%.19s",m->timestamp);
				post_msg_to_irc(irc_chan,"--------",tmp);
				msg_counter=0;
			}
		}
		if(m->msg && m->msg[0])
			append_printf(&msg,&msg_len,"%s%s",prefix,m->msg);
		else
			append_printf(&msg,&msg_len,"<attachment>");
		if(msg){
			post_msg_to_irc(irc_chan,nick,msg);
			free(msg);
			msg_counter++;
		}
	}
}

static void push_priv_irc_msgs(MESSAGE_LIST *mlist,const char *nick,int flags)
{
	int i,count;
	SYSTEMTIME time={0};
	int time_init=FALSE;
	int msg_counter=0;
	count=mlist->count;
	for(i=0;i<count;i++){
		MESSAGE *m=&mlist->m[i];
		char *msg=0;
		int msg_len=0;
		if(flags){
			SYSTEMTIME tmp_time={0};
			int post_time=FALSE;
			if(!time_init){
				time_str_to_systime(m->timestamp,&time);
				time_init=TRUE;
			}
			time_str_to_systime(m->timestamp,&tmp_time);
			if(tmp_time.wDay!=time.wDay){
				time=tmp_time;
				post_time=TRUE;
			}
			if(0==i){
				post_time=TRUE;
			}else if((count-1)==i){
				if(msg_counter>15)
					post_time=TRUE;
			}
			if(post_time){
				char tmp[80]={0};
				__snprintf(tmp,sizeof(tmp),"---[%.19s]",m->timestamp);
				post_priv_msg_to_irc(nick,tmp);
				msg_counter=0;
			}
		}
		if(m->msg && m->msg[0])
			append_printf(&msg,&msg_len,"<%s> %s",m->author,m->msg);
		else
			append_printf(&msg,&msg_len,"<attachment>");
		if(msg){
			post_priv_msg_to_irc(nick,msg);
			free(msg);
			msg_counter++;
		}
	}
}

static int push_irc_nick_list(NICK_LIST *nlist,const char *irc_chan)
{
	char *name_list=0;
	int name_len=0;
	int started=FALSE;
	int i,count;
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
		name_list=0;
		name_len=0;
	}
	if(count){
		char *tmp=0;
		int tmp_len=0;
		append_printf(&tmp,&tmp_len,"%s %s",get_irc_msg_str(END_NAME_LIST),irc_chan);
		push_irc_msg(tmp);
		free(tmp);
	}
	return count;
}

static int get_channel_obj_from_irc_chan(const char *irc_chan,CHANNEL **chan_obj)
{
	int result=FALSE;
	char guild[80]={0};
	char chan[80]={0};
	result=extract_guild_chan(irc_chan,guild,sizeof(guild),chan,sizeof(chan));
	if(result){
		result=get_channel_obj(guild,chan,chan_obj);
	}
	return result;
}
static int get_dm_chan_from_uname(const char *uname,DM_CHAN **dm_chan_obj)
{
	int result=FALSE;
	int i,count;
	count=g_dm_list.count;
	for(i=0;i<count;i++){
		DM_CHAN *tmp;
		tmp=&g_dm_list.dm_chan[i];
		if(0==stricmp(tmp->recipient,uname)){
			result=TRUE;
			*dm_chan_obj=tmp;
			break;
		}
	}
	return result;
}

static int process_list_chan(CONNECTION *conn,DISCORD_CMD *cmd)
{
	int result=FALSE;
	int i,count;
	char tmp[256]={0};
	int list_dm_chans=FALSE;
	if(cmd->data){
		unsigned char a=cmd->data[0];
		if(a && (!isspace(a))){
			a=tolower(a);
			if('p'==a || 'd'==a){
				get_all_dm_channels(conn,&g_dm_list);
				list_dm_chans=TRUE;
			}else{
				get_guilds(conn,&g_guild_list);
				get_all_channels(conn,&g_guild_list);
			}
		}
	}
	if(list_dm_chans){
		count=g_dm_list.count;
		for(i=0;i<count;i++){
			DM_CHAN *dm_chan=&g_dm_list.dm_chan[i];
			post_irc_server_msg("DM: %s id: %s chanid: %s",dm_chan->recipient,dm_chan->recipient_id,dm_chan->id);
			result=TRUE;
		}
	}else{
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
	}
	return result;
}

static int process_post_msg(CONNECTION *c,DISCORD_CMD *cmd)
{
	int result=FALSE;
	char chan_name[160]={0};
	const char *chan_msg;
	int found_chan=FALSE;
	CHANNEL *chan=0;
	get_word(cmd->data,chan_name,sizeof(chan_name));
	chan_msg=seek_next_word(cmd->data);
	if(0==chan_msg || 0==chan_name[0]){
		post_irc_server_msg("ERROR unable to extract channel name");	
		printf("cant find chan data from:%s",cmd->data);
		return result;
	}
	result=get_channel_obj_from_irc_chan(chan_name,&chan);
	if(result){
		char *tmp;
		result=FALSE;
		if(':'==chan_msg[0]){
			chan_msg++;
		}
		tmp=strdup(chan_msg);
		if(tmp){
			trim_right(tmp);
			result=post_message(c,chan->id,tmp,&chan->msgs);
			sort_messages(&chan->msgs);
			free(tmp);
		}
		if(!result){
			char *err=0;
			int err_len=0;
			append_printf(&err,&err_len,"Failed to post message to: %s [%s]",chan_name,g_last_http_error);
			post_msg_to_irc(chan_name,"ERROR",err);
			post_irc_server_msg("ERROR: %s",err);
			free(err);
		}
	}else{
		DM_CHAN *dm_chan=0;
		result=get_dm_chan_from_uname(chan_name,&dm_chan);
		if(!result){
			char uid[40]={0};
			if(find_user_id(chan_name,uid,sizeof(uid))){
				if(create_dm_channel(c,uid,&g_dm_list))
					result=get_dm_chan_from_uname(chan_name,&dm_chan);
			}
		}
		if(result){
			char *tmp;
			result=FALSE;
			if(':'==chan_msg[0]){
				chan_msg++;
			}
			tmp=strdup(chan_msg);
			if(tmp){
				trim_right(tmp);
				result=post_message(c,dm_chan->id,tmp,&dm_chan->msgs);
				sort_messages(&dm_chan->msgs);
				free(tmp);
			}
			if(!result){
				char *err=0;
				int err_len=0;
				append_printf(&err,&err_len,"Failed to post message to: %s [%s]",chan_name,g_last_http_error);
				post_msg_to_irc(chan_name,"ERROR",err);
				post_irc_server_msg("ERROR: %s",err);
				free(err);
			}
		}else{
			post_irc_server_msg("ERROR unable to find channel %s",chan_name);
		}
	}
	return result;
}

static int add_new_message(MESSAGE_LIST *mlist,MESSAGE *msg)
{
	int result=FALSE;
	int i,count;
	count=mlist->count;
	for(i=0;i<count;i++){
		MESSAGE *tmp=&mlist->m[i];
		if(0==stricmp(tmp->id,msg->id)){
			char *content;
			content=strdup(msg->msg);
			if(content){
				free(tmp->msg);
				tmp->msg=content;
			}
			return result;
		}
	}
	result=add_message(mlist,msg);
	sort_messages(mlist);
	return result;
}

static int process_chan_msg(DISCORD_CMD *cmd,const char *uname)
{
	int result=FALSE;
	char chan_id[40]={0};
	char nick[80]={0};
	char nick_id[40]={0};
	char timestamp[40]={0};
	char *content=0;
	int content_size=4096;
	MESSAGE_LIST *msg_list=0;
	const char *str=cmd->data;
	if(0==str){
		return result;
	}
	content=calloc(content_size,1);
	if(0==content){
		return result;
	}
	//chan id,uname_id,username,timestamp,content
	get_word(str,chan_id,sizeof(chan_id));
	str=seek_next_word(str);
	get_word(str,nick_id,sizeof(nick_id));
	str=seek_next_word(str);
	get_word(str,nick,sizeof(nick));
	str=seek_next_word(str);
	get_word(str,timestamp,sizeof(timestamp));
	str=seek_next_word(str);
	if(0==str){
		goto EXIT_CHAN_MSG;
	}
	strncpy(content,str,content_size);
	if(0==stricmp(uname,nick)){
		goto EXIT_CHAN_MSG;
	}
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
				CHANNEL *chan=&g->channels.chan[j];
				if(0==stricmp(chan->id,chan_id)){
					print_irc_chan(g->name,chan->name,irc_chan,sizeof(irc_chan));
					msg_list=&chan->msgs;
					result=TRUE;
					break;
				}
			}
			if(irc_chan[0]){
				break;
			}
		}
		if(result){
			post_msg_to_irc(irc_chan,nick,content);
			goto ADD_NEW_MSG;
		}
	}
	if(!result){
		int i,count;
		count=g_dm_list.count;
		for(i=0;i<count;i++){
			DM_CHAN *dm_chan=&g_dm_list.dm_chan[i];
			if(0==stricmp(dm_chan->id,chan_id)){
				msg_list=&dm_chan->msgs;
				result=TRUE;
				break;
			}
		}
		if(result){
			post_priv_msg_to_irc(nick,content);
		}
	}
ADD_NEW_MSG:
	if(result && msg_list){
		MESSAGE msg={0};
		msg.id=chan_id;
		msg.author=nick;
		msg.auth_id=nick_id;
		msg.timestamp=timestamp;
		time_str_to_ftime(timestamp,&msg.ftime);
		msg.msg=content;
		add_new_message(msg_list,&msg);
	}
EXIT_CHAN_MSG:
	free(content);
	return result;
}

//get the message that is closest to the given time
static int get_nearest_message_id(MESSAGE_LIST *mlist,__int64 ftime,int direction,const char **msg_id)
{
	int result=FALSE;
	int i,count;
	int found=-1;
	count=mlist->count;
	if(direction>=0){ //after,around
		for(i=0;i<count;i++){ //low to high
			MESSAGE *m=&mlist->m[i];
			if(ftime >= m->ftime){
				found=i;
			}else{
				break;
			}
		}
	}else{
		for(i=count-1;i>=0;i--){ //high to low
			MESSAGE *m=&mlist->m[i];
			if(ftime <= m->ftime){
				found=i;
			}else{
				break;
			}
		}
	}
	if(found>=0){
		*msg_id=mlist->m[found].id;
		result=TRUE;
	}else{
		if(count){
			MESSAGE *start_msg,*end_msg;
			start_msg=&mlist->m[0];
			end_msg=&mlist->m[mlist->count-1];
			if(start_msg->ftime > ftime){
				if(direction<=0){
					//time is below the lowest
					*msg_id=start_msg->id;
					result=TRUE;
				}
			}else if(end_msg->ftime < ftime){
				if(direction>=0){
					//time is above the highest
					*msg_id=end_msg->id;
					result=TRUE;
				}
			}else if(start_msg->ftime == ftime){
				*msg_id=start_msg->id;
				result=TRUE;
			}else if(end_msg->ftime == ftime){
				*msg_id=end_msg->id;
				result=TRUE;
			}
		}
	}
	return result;
}

//check if the msg_id is in our list, if we have enuf in the range and copy to out
int have_msg_range(MESSAGE_LIST *mlist,const char *msg_id,int direction,int range,MESSAGE_LIST *out)
{
	int result=FALSE;
	int i,count;
	int index=-1;
	if(0==msg_id)
		return result;
	count=mlist->count;
	for(i=0;i<count;i++){
		MESSAGE *m=&mlist->m[i];
		if(0==stricmp(m->id,msg_id)){
			index=i;
			break;
		}
	}
	if(index>=0){
		if(direction<0){
			index-=range;
		}else if(0==direction){
			index-=range/2;
		}else{
			index=index+1;
		}
		if(index>=0 && (index+range)<=count){
			for(i=0;i<range;i++){
				int mindex=i+index;
				MESSAGE *m=&mlist->m[mindex];
				add_message(out,m);
				result=TRUE;
			}
		}
	}
	return result;
}

static int binsearch_compare_message(const void *arg1,const void *arg2)
{
	int result;
	MESSAGE *a,*b;
	a=(MESSAGE*)arg1;
	b=(MESSAGE*)arg2;
	result=1;
	if(a->ftime < b->ftime)
		result=-1;
	else if(a->ftime == b->ftime){
		result=0;
	}
	return result;
}

static void add_generic_msg(MESSAGE_LIST *mlist,char *author,const char *fmt,...)
{
	int len;
	va_list ap;
	char *tmp=0;
	int tmp_len=0;
	MESSAGE msg={0};
	va_start(ap,fmt);
	len=_vsnprintf(0,0,fmt,ap);
	if(len<=0)
		return;
	tmp_len=len+1;
	tmp=calloc(tmp_len,1);
	if(0==tmp)
		return;
	_vsnprintf(tmp,tmp_len,fmt,ap);
	tmp[tmp_len-1]=0;
	msg.author=author;
	msg.auth_id="";
	msg.id="";
	msg.timestamp="2000-01-01";
	msg.msg=tmp;
	add_message(mlist,&msg);
	free(tmp);
}

static void process_get_msgs(CONNECTION *conn,DISCORD_CMD *cmd)
{
	char chan_name[160]={0};
	const char *str;
	//chan_name before timestamp count
	//before=-1,around=0,after=1,limit params from msg ID
	int direction=0; //around
	int limit=100;
	CHANNEL *_chan=0;
	DM_CHAN *dm_chan=0;
	MESSAGE_LIST *chan_mlist=0;
	const char *chan_id=0;
	const char *msg_id=0;
	MESSAGE_LIST mlist={0};
	int get_pinned=FALSE;
	str=cmd->data;
	get_word(str,chan_name,sizeof(chan_name));
	printf(">>>process get msgs:%s\n",chan_name);
	if(!get_channel_obj_from_irc_chan(chan_name,&_chan)){
		if(!get_dm_chan_from_uname(chan_name,&dm_chan)){
			char *tmp=0;
			int tmp_len=0;
			append_printf(&tmp,&tmp_len,"%s %s",get_irc_msg_str(UNKNOWN_CHAN),chan_name);
			push_irc_msg(tmp);
			free(tmp);
			return;
		}
	}
	if(_chan){
		chan_mlist=&_chan->msgs;
		chan_id=_chan->id;
	}else if(dm_chan){
		chan_mlist=&dm_chan->msgs;
		chan_id=dm_chan->id;
	}else{
		return;
	}
	sort_messages(chan_mlist);
	str=seek_next_word(str);
	if(str){
		unsigned char a=str[0];
		if(isalpha(a)){
			if(startswithi(str,"before"))
				direction=-1;
			else if(startswithi(str,"after"))
				direction=1;
			else if(startswithi(str,"pin"))
				get_pinned=TRUE;
			else if(startswithi(str,"info")){
				char *tmp=0;
				int tmp_len=0;
				if(chan_mlist->count)
					append_printf(&tmp,&tmp_len,"%.19s -> %.19s (%u)",chan_mlist->m[0].timestamp,chan_mlist->m[chan_mlist->count-1].timestamp,chan_mlist->count);
				else
					append_printf(&tmp,&tmp_len,"NO MESSAGES AVAILABLE");
				if(tmp){
					if(_chan)
						post_msg_to_irc(chan_name,"MSGINFO",tmp);
					else
						post_priv_msg_to_irc(chan_name,tmp);
					post_irc_server_msg("%s",tmp);
					free(tmp);
				}
				return;
			}else if(startswithi(str,"find")){
				int i,count;
				int found=0;
				MESSAGE_LIST *ml;
				str=seek_next_word(str);
				if(0==str)
					return;
				if(strlen(str)<2)
					return;
				ml=chan_mlist;
				count=ml->count;
				for(i=0;i<count;i++){
					MESSAGE *m=&ml->m[i];
					if(strstri(m->msg,str)){
						found++;
						if(found>20)
							break;
						add_generic_msg(&mlist,"=======","%s",m->timestamp);
						add_message(&mlist,m);
					}
				}
				if(found){
					if(_chan)
						push_irc_msgs(&mlist,0,mlist.count,chan_name,"",0);
					else
						push_priv_irc_msgs(&mlist,chan_name,0);
				}
				remove_all_msg(&mlist);
				return;
			}
			str=seek_next_word(str);
		}
		if(str){
			a=str[0];
			if(isdigit(a)){
				int val=atoi(str);
				if(strchr(str,'-') || val>=2000){
					__int64 ftime=0;
					time_str_to_ftime(str,&ftime);
					get_nearest_message_id(chan_mlist,ftime,direction,&msg_id);
				}else{
					limit=atoi(str);
				}
			}else{
				if(chan_mlist->count)
					msg_id=chan_mlist->m[chan_mlist->count-1].id;
			}
			str=seek_next_word(str);
			if(str){
				a=str[0];
				if(isdigit(a)){
					limit=atoi(str);
				}
			}
		}else{
			__int64 ftime;
			ftime=get_current_ftime();
			get_nearest_message_id(chan_mlist,ftime,direction,&msg_id);
		}
	}
	if(get_pinned){
		if(_chan)
			push_irc_msgs(&_chan->pin_msgs,0,_chan->pin_msgs.count,chan_name,"PINNED: ",TRUE);
		return;
	}
	sort_messages(chan_mlist);
	if(have_msg_range(chan_mlist,msg_id,direction,limit,&mlist)){
		printf(">>>have messages in range: limit:%i direction:%i msg_id:%s\n",limit,direction,msg_id);
	}else{
		printf(">>>calling get messages params: limit:%i direction:%i msg_id:%s\n",limit,direction,msg_id);
		get_messages(conn,&mlist,chan_id,limit,direction,msg_id,FALSE);
	}
	printf(">>message count:%i\n",mlist.count);
	if(0==mlist.count){
		if(g_last_http_error){
			if(_chan)
				post_msg_to_irc(chan_name,"MSGINFO",g_last_http_error);
			else
				post_priv_msg_to_irc(chan_name,g_last_http_error);
			post_irc_server_msg("ERROR:%s",g_last_http_error);
		}
		return;
	}
	sort_messages(&mlist);
	{
		int i,count;
		if(_chan)
			post_msg_to_irc(chan_name,"MSG_BLOCK_BEGIN","<============================>");
		else
			post_priv_msg_to_irc(chan_name,"<============================>");
		count=mlist.count;
		for(i=0;i<count;i++){
			MESSAGE *m;
			void *ptr;
			m=&mlist.m[i];
			ptr=bsearch((void*)m,chan_mlist->m,chan_mlist->count,sizeof(MESSAGE),&binsearch_compare_message);
			if(0==ptr){
				if(chan_mlist->count>=50000){
					remove_some_messages(chan_mlist,30000);
				}
				printf(">>adding message:%s %I64X\n",m->timestamp,m->ftime);
				add_message(chan_mlist,m);
				sort_messages(chan_mlist);
			}
		}
		if(_chan){
			push_irc_msgs(&mlist,0,mlist.count,chan_name,"",TRUE);
			merge_new_nicks(&_chan->nicks,&mlist);
			push_irc_nick_list(&_chan->nicks,chan_name);
		}else{
			push_priv_irc_msgs(&mlist,chan_name,TRUE);
		}
		remove_all_msg(&mlist);
	}
}

static int process_join_chan(CONNECTION *conn,DISCORD_CMD *cmd)
{
	int result=FALSE;
	char guild[80]={0};
	char chan[80]={0};
	char irc_chan[160]={0};
	char *str;
	CHANNEL *target_chan=0;
	str=cmd->data;
	extract_guild_chan(str,guild,sizeof(guild),chan,sizeof(chan));
	print_irc_chan(guild,chan,irc_chan,sizeof(irc_chan));
	printf("SRC=%s\n",str);
	printf("guild=%s chan=%s\n",guild,chan);
	result=get_channel_obj_from_irc_chan(irc_chan,&target_chan);
	if(result){
		char tmp[256];
		const char *prefix=get_irc_msg_str(OK_JOIN_CHAN);
		__snprintf(tmp,sizeof(tmp),"%s %s",prefix,irc_chan);
		push_irc_msg(tmp);
		printf("OK JOIN CHANNEL: %s\n",irc_chan);
		{
			char *topic=0;
			int topic_len;
			push_irc_nick_list(&target_chan->nicks,irc_chan);
			append_printf(&topic,&topic_len,"%s %s %s",get_irc_msg_str(CHAN_TOPIC),irc_chan,target_chan->topic);
			push_irc_msg(topic);
			free(topic);
			push_irc_msgs(&target_chan->pin_msgs,0,target_chan->pin_msgs.count,irc_chan,"PINNED: ",TRUE);
		}
		{
			DISCORD_CMD cmd_msg={0};
			cmd_msg.cmd=CMD_JOIN_CHAN;
			cmd_msg.data=irc_chan;
			process_get_msgs(conn,&cmd_msg);
		}
	}else{
		char tmp[80];
		__snprintf(tmp,sizeof(tmp),"%s %s",get_irc_msg_str(UNKNOWN_CHAN),irc_chan);
		push_irc_msg(tmp);
	}
	return result;
}

static void process_get_names(DISCORD_CMD *cmd)
{
	char irc_chan[160]={0};
	CHANNEL *chan;
	get_word(cmd->data,irc_chan,sizeof(irc_chan));
	if(0==irc_chan[0])
		return;
	if(!get_channel_obj_from_irc_chan(irc_chan,&chan)){
		char *tmp=0;
		int tmp_len=0;
		append_printf(&tmp,&tmp_len,"%s %s",get_irc_msg_str(UNKNOWN_CHAN),irc_chan);
		push_irc_msg(tmp);
		free(tmp);
		return;
	}
	push_irc_nick_list(&chan->nicks,irc_chan);
}

static void process_create_chan(DISCORD_CMD *cmd)
{
	const char *str;
	char chan_id[40]={0};
	char nick[80]={0};
	char uid[40]={0};
	DM_CHAN dm_chan={0};
	//chan id,uname,uid
	str=cmd->data;
	printf("adding DM channel:%s\n",str);
	get_word(str,chan_id,sizeof(chan_id));
	str=seek_next_word(str);
	get_word(str,nick,sizeof(nick));
	str=seek_next_word(str);
	get_word(str,uid,sizeof(uid));
	if(!(chan_id[0] && nick[0] && uid[0])){
		return;
	}
	dm_chan.id=chan_id;
	dm_chan.recipient=nick;
	dm_chan.recipient_id=uid;
	add_dm_chan(&g_dm_list,&dm_chan);
}

static void process_part_chan(DISCORD_CMD *cmd)
{
	int res;
	const char *chan_name;
	CHANNEL *chan;
	chan_name=cmd->data;
	if(0==chan_name)
		return;
	res=get_channel_obj_from_irc_chan(chan_name,&chan);
	if(!res)
		return;
	remove_all_msg(&chan->msgs);
}

static void process_resume(CONNECTION *conn,DISCORD_CMD *cmd)
{
	int i,count;
	int total=0;
	count=g_guild_list.count;
	for(i=0;i<count;i++){
		int j,chan_count;
		GUILD *g=&g_guild_list.guild[i];
		chan_count=g->channels.count;
		for(j=0;j<chan_count;j++){
			CHANNEL *c=&g->channels.chan[j];
			if(c->msgs.count){
				MESSAGE *m;
				int index;
				const char *msg_id;
				index=c->msgs.count-1;
				m=&c->msgs.m[index];
				msg_id=m->id;
				{
					DISCORD_CMD msg_cmd={0};
					char tmp[256]={0};
					msg_cmd.cmd=CMD_GET_MSGS;
					__snprintf(tmp,sizeof(tmp),"#%s.%s after %s",g->name,c->name,m->timestamp);
					msg_cmd.data=tmp;
					reset_last_error();
					process_get_msgs(conn,&msg_cmd);
					if(g_last_http_error)
						total=100;
				}
				total++;
				if(total>10)
					break;
			}
		}
		if(total>10)
			break;
	}
}

static void process_invite_use(CONNECTION *c,DISCORD_CMD *cmd)
{
	char *data=0;
	int data_len=0;
	char code[80]={0};
	char *ptr;
	get_word(cmd->data,code,sizeof(code));
	ptr=strrchr(code,'/');
	if(ptr){
		__snprintf(code,sizeof(code),"%s",ptr+1);
	}
	append_printf(&data,&data_len,"POST /api/v6/invite/%s HTTP/1.1\r\n",code);
	append_printf(&data,&data_len,"Host: discordapp.com:443\r\n");
	append_printf(&data,&data_len,"Accept-Encoding: identity\r\n");
	append_printf(&data,&data_len,"Connection: Keep-Alive\r\n");
	append_printf(&data,&data_len,"Content-Type: application/json\r\n");
	append_printf(&data,&data_len,"Authorization: %s\r\n",g_token);
	append_printf(&data,&data_len,"Content-Length: 0\r\n\r\n");
	if(data){
		char *content=0;
		int content_len=0;
		int res;
		res=do_http_req(c,data,&content,&content_len);
		if(res){
			post_irc_server_msg("Successfully joined guild");
		}else{
			post_irc_server_msg("Failed to join guild:%s",g_last_http_error);
		}
		free(content);
		free(data);
	}
}

static void process_guild_leave(CONNECTION *c,DISCORD_CMD *cmd)
{
	const char *str=cmd->data;
	char irc_chan[160]={0};
	char guild[80]={0};
	const char *guild_id=0;
	int is_dm=FALSE;
	int i,count;
	if(0==str || 0==str[0])
		return;
	get_word(str,irc_chan,sizeof(irc_chan));
	if('#'==irc_chan[0]){
		char channel[80]={0};
		int res;
		res=extract_guild_chan(irc_chan,guild,sizeof(guild),channel,sizeof(channel));
		if(!res){
			__snprintf(guild,sizeof(guild),"%s",irc_chan+1);
		}
	}else{
		__snprintf(guild,sizeof(guild),"%s",irc_chan);
	}
	count=g_guild_list.count;
	for(i=0;i<count;i++){
		GUILD *g=&g_guild_list.guild[i];
		if(0==stricmp(g->name,guild)){
			guild_id=g->id;
			break;
		}
	}
	if(0==guild_id){
		count=g_dm_list.count;
		for(i=0;i<count;i++){
			DM_CHAN *dm=&g_dm_list.dm_chan[i];
			if(0==stricmp(dm->recipient,guild)){
				guild_id=dm->id;
				is_dm=TRUE;
				break;
			}
		}
	}
	if(0==guild_id){
		post_irc_server_msg("Error finding guild:%s",guild);
	}else{
		char *data=0;
		int data_len=0;
		if(is_dm)
			append_printf(&data,&data_len,"DELETE /api/v6/channels/%s HTTP/1.1\r\n",guild_id);
		else
			append_printf(&data,&data_len,"DELETE /api/v6/users/@me/guilds/%s HTTP/1.1\r\n",guild_id);
		append_printf(&data,&data_len,"Host: discordapp.com:443\r\n");
		append_printf(&data,&data_len,"Accept-Encoding: identity\r\n");
		append_printf(&data,&data_len,"Connection: Keep-Alive\r\n");
		append_printf(&data,&data_len,"Content-Type: application/json\r\n");
		append_printf(&data,&data_len,"Authorization: %s\r\n",g_token);
		append_printf(&data,&data_len,"Content-Length: 0\r\n\r\n");
		if(data){
			char *content=0;
			int content_len=0;
			int res;
			res=do_http_req(c,data,&content,&content_len);
			if(res)
				post_irc_server_msg("Successfully parted channel %s %s",irc_chan,guild_id);
			else{
				if(g_last_http_error){
					if(strstr(g_last_http_error,"204"))
						post_irc_server_msg("Successfully parted guild");
					else
						post_irc_server_msg("discord server response:%s",g_last_http_error);
				}else
					post_irc_server_msg("Unknown error leaving guild");
			}
			free(content);
			free(data);
		}
	}
}

#include "test_main.h"

static int process_requests(CONNECTION *c,const char *uname)
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
			res=pop_discord_cmd(&cmd);
			if(res){
				switch(cmd.cmd){
				case CMD_JOIN_CHAN:
					process_join_chan(c,&cmd);
					break;
				case CMD_LIST_CHAN:
					process_list_chan(c,&cmd);
					break;
				case CMD_GET_MSGS:
					process_get_msgs(c,&cmd);
					break;
				case CMD_POST_MSG:
					process_post_msg(c,&cmd);
					break;
				case CMD_CHAN_MSG:
					process_chan_msg(&cmd,uname);
					break;
				case CMD_GET_NAMES:
					process_get_names(&cmd);
					break;
				case CMD_CREATE_CHAN:
					process_create_chan(&cmd);
					break;
				case CMD_PART:
					process_part_chan(&cmd);
					break;
				case CMD_RESUME:
					process_resume(c,&cmd);
					break;
				case CMD_INVITE_USE:
					process_invite_use(c,&cmd);
					break;
				case CMD_GUILD_LEAVE:
					process_guild_leave(c,&cmd);
					break;
				case CMD_TEST:
					//do_discord_test(c);
					break;
				}
				free_discord_cmd(&cmd);
			}else{
				break;
			}
		}
		break;
	case WAIT_TIMEOUT:
		{
			static DWORD tick=0;
			DWORD delta;
			if(0==tick){
				tick=GetTickCount();
			}
			delta=GetTickCount()-tick;
			if(delta>40000){
				char tmp[40]={0};
				//printf("main thread heartbeat\n");
				get_me_user_name(c,tmp,sizeof(tmp));
				tick=GetTickCount();
			}
			result=TRUE;
		}
		break;
	default:
		Sleep(100);
		result=FALSE;
		break;
	}
	return result;
}


static void discord_thread(void *args)
{
	CONNECTION con={0};
	enum{
		DISC_CONNECT=0,
		DISC_GET_GUILDS,
		DISC_GET_CHANNELS,
		DISC_GET_DM_CHANNELS,
		DISC_GET_MESSAGES,
		DISC_GET_GATEWAY,
		DISC_WAIT_CMD,
	};
	int state=DISC_CONNECT;
	char user_name[40]={0};
	printf("discord_thread started\n");
	while(1){
		switch(state){
		case DISC_CONNECT:
			{
				int res;
				WSASetLastError(0);
				close_connection(&con);
				g_gateway[0]=0;
				res=connect_disc(&con);
				if(res){
					printf("found token\n");
					state=DISC_GET_GUILDS;
				}else{
					printf("failed login:%s\n",g_last_http_error);
					Sleep(5000);
				}
			}
			break;
		case DISC_GET_GUILDS:
			get_me_user_name(&con,user_name,sizeof(user_name));
			get_guilds(&con,&g_guild_list);
			state=DISC_GET_CHANNELS;
			break;
		case DISC_GET_CHANNELS:
			get_all_channels(&con,&g_guild_list);
			dump_guild_stuff(&g_guild_list);
			state=DISC_GET_DM_CHANNELS;
			break;
		case DISC_GET_DM_CHANNELS:
			get_all_dm_channels(&con,&g_dm_list);
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
				res=process_requests(&con,user_name);
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
		{
			int error;
			error=WSAGetLastError();
			if(WSAECONNRESET==error){
				printf("ERROR: socket error detected\n");
				state=DISC_CONNECT;
				Sleep(5000);
			}
		}
	}
}

int main(int argc,char **argv)
{
	//test_func();
	init_mutex();
	g_event=CreateEventA(NULL,FALSE,FALSE,"discord_event");
	_beginthread(&gateway_thread,0,NULL);
	_beginthread(&discord_thread,0,NULL);
	_beginthread(&irc_thread,0,NULL);
	do_wait();
	return 0;
}