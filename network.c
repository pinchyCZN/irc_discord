#include <windows.h>
#include "network.h"
#include "config.h"
#include "libstring.h"

static void my_debug(void *ctx,int level,const char *str)
{
	static int DEBUG_LEVEL=5;
	if(level<=DEBUG_LEVEL){
		fprintf((FILE*)ctx,"%s",str);
		fflush((FILE*)ctx);
	}
}


int close_connection(CONNECTION *c)
{
	ssl_context *ssl;
	ssl=&c->ssl;
	if(ssl){
		ssl_close_notify(ssl);
		if(c->sock){
			net_close((int)c->sock);
			c->sock=0;
		}
		ssl_free(ssl);
	}
	return TRUE;
}

int ssl_connect(ssl_context *ssl,const int options,const char *host,const int port,int *c_socket)
{
	int result=FALSE;
	int ret;
	entropy_context entropy;
	ctr_drbg_context ctr_drbg;
	ssl_session ssn;
	int socket=0;
	const char *pers="net_connection";

	memset(&ssn,0,sizeof(ssl_session));
	entropy_init(&entropy);
	if((ret=ctr_drbg_init(&ctr_drbg,entropy_func,&entropy,
		(unsigned char *)pers,strlen(pers)))!=0){
		return result;
	}
	if((ret=net_connect(&socket,host,port))!=0)
		return result;

	if((ret=ssl_init(ssl))!=0){
		net_close(socket);
		return result;
	}
	ssl_set_endpoint(ssl,SSL_IS_CLIENT);
	ssl_set_authmode(ssl,SSL_VERIFY_NONE);

	ssl_set_rng(ssl,ctr_drbg_random,&ctr_drbg);
	ssl_set_dbg(ssl,my_debug,stdout);
	ssl_set_bio(ssl,net_recv,&socket,net_send,&socket);
	c_socket[0]=socket;

	ssl_set_ciphersuites(ssl,ssl_default_ciphersuites);
	ssl_set_session(ssl,1,600,&ssn);
	ssl->read_timeout=30;
	result=TRUE;
	return result;
}

int read_line(ssl_context *ssl,char **line,int *line_len,int timeout)
{
	int result=FALSE;
	BYTE *tmp=0;
	DWORD tick,delta;
	int tmp_size=1024;
	int index=0;
	tick=GetTickCount();
	tmp=(BYTE*)calloc(tmp_size,1);
	if(0==tmp){
		return result;
	}
	while(1){
		int res;
		int avail;
		avail=tmp_size-index;
		if(avail<=0){
			int new_len;
			BYTE *buf;
			new_len=tmp_size+=1024;
			buf=(BYTE*)realloc(tmp,new_len);
			if(buf){
				memset(buf+tmp_size,0,new_len-tmp_size);
				tmp=buf;
				tmp_size=new_len;
			}else{
				break;
			}
		}
		res=ssl_read(ssl,tmp+index,1);
		if(res>0){
			BYTE a;
			a=tmp[index];
			if('\n'==a){
				result=TRUE;
			}
			index+=res;

		}else if(0==res){
			Sleep(10);
		}else{
			break;
		}
		if(result){
			break;
		}
		delta=GetTickCount()-tick;
		if(delta>=(DWORD)timeout){
			break;
		}

	}
	if(result){
		int res;
		res=null_str(&tmp,index);
		if(res){
			*line=(char*)tmp;
			*line_len=index;
		}else{
			result=FALSE;
			free(tmp);
		}
	}
	else{
		free(tmp);
	}
	return result;
}

int recv_data(ssl_context *ssl,BYTE *data,int len)
{
	int result=FALSE;
	DWORD tick,delta;
	int offset=0;
	DWORD timeout=g_timeout;
	tick=GetTickCount();
	while(1){
		int res;
		int amount;
		BYTE *ptr;
		amount=len-offset;
		ptr=data+offset;
		res=ssl_read(ssl,ptr,amount);
		if(res>0){
			offset+=res;
			if(offset>=len){
				result=TRUE;
				break;
			}
		}else if(0==res){
			Sleep(10);
		}else{
			break;
		}
		delta=GetTickCount()-tick;
		if(delta>=timeout){
			break;
		}
	}
	return result;
}

int send_data(ssl_context *ssl,BYTE *data,int len)
{
	int result=FALSE;
	int offset=0;
	DWORD tick,delta;
	DWORD timeout=g_timeout;
	tick=GetTickCount();
	while(1){
		int res;
		dump_hex(data,len);
		res=ssl_write(ssl,data+offset,len-offset);
		if(res>0){
			offset+=res;
			if((len-offset)<=0){
				result=TRUE;
				break;
			}
		}else if(POLARSSL_ERR_NET_WANT_WRITE==res){
			printf("want write\n");
			Sleep(10);
		}else{
			break;
		}
		delta=GetTickCount()-tick;
		if(delta>=timeout){
			break;
		}
	}
	return result;
}

int drain_response(ssl_context *ssl)
{
	BYTE *tmp;
	int tmp_len=0x10000;
	tmp=(BYTE*)malloc(tmp_len);
	if(tmp){
		while(1){
			int res;
			res=ssl_read(ssl,tmp,tmp_len);
			if(res<=0){
				break;
			}
		}
		free(tmp);
	}
	return TRUE;
}

