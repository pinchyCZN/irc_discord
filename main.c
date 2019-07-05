#define CINTERFACE
#include <Windows.h>
#include <process.h>
#include <stdio.h>
#include <conio.h>
#include <WinInet.h>


#include "libstring.h"
#include "json.h"
#include "network.h"
#include "config.h"
#include "gateway.h"

#pragma warning(disable:4996)



int is_chunked(const char *str)
{
	if(strstri(str,"chunked")){
		return TRUE;
	}
	return FALSE;
}

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
	printf("is_resp_complete:\n%.*s\n",data_len,data);
	printf("---\n");
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
			if(n>0){
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
			}else{
				Sleep(10);
			}
			delta=GetTickCount()-tick;
			if(delta>5000000){
				break;
			}
		}
		free(tmp);
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
		int msg_len;
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
			//printf("%s\n---\n",data);
			msg_len=(int)strlen(data);
			ssl_write(ssl,(BYTE*)data,msg_len);
			res=get_response(c,&resp,&resp_len);
			if(res){
				res=get_resp_code(resp,resp_len);
				if(200==res){
					char *content=0;
					int content_len=0;
					printf("RESP:\n%.*s\n",resp_len,resp);
					res=get_content(resp,resp_len,&content,&content_len);
					if(res){
						char *token=0;
						content[content_len-1]=0;
						res=extract_token(content,&token);
						if(res){
							strncpy(g_token,token,sizeof(g_token));
							result=TRUE;
						}
					}
				}else{
					char *content=0;
					int content_len=0;
					printf("invalid resp code %i\n",res);
					res=get_content(resp,resp_len,&content,&content_len);
					if(res){
						printf("%.*s\n",content_len,content);
					}else{
						printf("%.*s\n",resp_len,resp);
					}
				}
			}else{
				printf("failed to get response\n");
			}
			if(resp){
				free(resp);
			}
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
	append_printf(&data,&data_len,"Content-Type: application/json\r\n");
	append_printf(&data,&data_len,"Authorization: %s\r\n\r\n",g_token);
	if(data){
		ssl_context *ssl;
		int msg_len;
		char *resp=0;
		int resp_len=0;
		int res;
		printf("%s\n",data);
		printf("---\n");
		ssl=&c->ssl;
		msg_len=strlen(data);
		ssl_write(ssl,(BYTE*)data,msg_len);
		res=get_response(c,&resp,&resp_len);
		if(res){
			char *content;
			int content_len=0;
			printf("gateway resp:\n%.*s\n",resp_len,resp);
			res=get_content(resp,resp_len,&content,&content_len);
			if(res){
				const char *val=0;
				int val_length=0;
				//{"url": "wss://gateway.discord.gg"}
				res=get_json_value_str(content,content_len,"url",&val,&val_length);
				if(res){
					int dst_len=sizeof(g_gateway);
					const char *prefix="wss://";
					int plen=strlen(prefix);
					res=strncmp(val,prefix,plen);
					if(0==res){
						val_length-=plen;
						val+=plen;
						if(val_length<0)
							val_length=0;
					}
					if(val_length>dst_len)
						val_length=dst_len;
					strncpy(g_gateway,val,val_length);
					g_gateway[sizeof(g_gateway)-1]=0;
					result=TRUE;
				}
				
				printf("%s\n",content);
			}
		}else{
			printf("failed to get response\n");
		}
		free(data);

	}
	return result;
}

int do_at_me(CONNECTION *c)
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
		ssl_context *ssl;
		int msg_len;
		char *resp=0;
		int resp_len=0;
		int res;
		printf("%s\n",data);
		printf("---\n");
		ssl=&c->ssl;
		msg_len=strlen(data);
		ssl_write(ssl,(BYTE*)data,msg_len);
		res=get_response(c,&resp,&resp_len);
		if(res){
			char *content;
			int content_len=0;
			printf("AT ME RESP:\n%.*s\n",resp_len,resp);
			res=get_content(resp,resp_len,&content,&content_len);
			if(res){
				printf("%.*s\n",content_len,content);
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

int do_at_me_settings(CONNECTION *c)
{
	int result=FALSE;
	char *data=0;
	int data_len=0;
	append_printf(&data,&data_len,"GET /api/v6/users/@me/settings HTTP/1.1\r\n");
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
		printf("%s\n",data);
		printf("---\n");
		ssl=&c->ssl;
		msg_len=strlen(data);
		ssl_write(ssl,(BYTE*)data,msg_len);
		res=get_response(c,&resp,&resp_len);
		if(res){
			char *content;
			int content_len=0;
			printf("settings RESP:\n%.*s\n",resp_len,resp);
			res=get_content(resp,resp_len,&content,&content_len);
			if(res){
				printf("%.*s\n",content_len,content);
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



void discord_thread(void *args)
{
	CONNECTION con={0};
	enum{
		DISC_CONNECT=0,
		DISC_AT_ME,
		DISC_AT_ME_SETTINGS,
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
					state=DISC_AT_ME;
				}else{
					printf("failed login\n");
					Sleep(5000);
				}
			}
			break;
		case DISC_AT_ME:
			if(do_at_me(&con)){
				state=DISC_AT_ME_SETTINGS;
			}else{
				state=DISC_GET_GATEWAY;
			}
			break;
		case DISC_AT_ME_SETTINGS:
			do_at_me_settings(&con);
			state=DISC_GET_GATEWAY;
			break;
		case DISC_GET_GATEWAY:
			if(get_gateway(&con)){
				state=DISC_WAIT_CMD;
				SetEvent(g_gwevent);
			}else{
				close_connection(&con);
				state=DISC_CONNECT;
				Sleep(5000);
			}
			break;
		case DISC_WAIT_CMD:
			Sleep(1000);
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
int print_json(JSON *json)
{
	while(json){
		if(json->type==JSON_NODE && json->ptr){
			printf("%.*s\n",json->key_len,json->key);
			print_json((JSON*)json->ptr);
		}
		else
			printf("key=%.*s value=%.*s\n",json->key_len,json->key,json->value_len,json->value);
		json=(JSON*)json->next;
	}
	return 0;
}

int main(int argc,char **argv)
{
	InitializeCriticalSection(&g_mutex);
	g_gwevent=CreateEventA(NULL,FALSE,FALSE,"GatewayEvent");
	_beginthread(&gateway_thread,0,NULL);
	_beginthread(&discord_thread,0,NULL);
	do_wait();
	return 0;
}