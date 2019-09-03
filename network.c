#include <windows.h>
#include "network.h"
#include "config.h"
#include "libstring.h"

#pragma warning(disable:4996)

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
	mbedtls_net_free(&c->ctx);
	mbedtls_ssl_free(&c->ssl);
	mbedtls_ssl_config_free(&c->conf);
	mbedtls_ctr_drbg_free(&c->cts_drbg);
	mbedtls_entropy_free(&c->entropy);
	return TRUE;
}

int open_connection(CONNECTION *c,const char *host,const int port)
{
	int result=FALSE;
	mbedtls_ssl_context *ssl;
	mbedtls_ssl_config *conf;
	mbedtls_net_context *ctx;
	mbedtls_entropy_context *entropy;
	mbedtls_ctr_drbg_context *ctr_drbg;
	const char *per="BLAH123";
	char port_str[40]={0};
	int res;
	ssl=&c->ssl;
	conf=&c->conf;
	ctx=&c->ctx;
	entropy=&c->entropy;
	ctr_drbg=&c->cts_drbg;
	mbedtls_net_init(ctx);
	mbedtls_ssl_init(ssl);
	mbedtls_ssl_config_init(conf);
	mbedtls_entropy_init(entropy);
	if(0!=mbedtls_ctr_drbg_seed(ctr_drbg,mbedtls_entropy_func,entropy,per,strlen(per))){
		goto ERROR_OPEN;
	}
	if(0!=mbedtls_ssl_config_defaults(conf,MBEDTLS_SSL_IS_CLIENT,MBEDTLS_SSL_TRANSPORT_STREAM,MBEDTLS_SSL_PRESET_DEFAULT)){
		goto ERROR_OPEN;
	}
	mbedtls_ssl_conf_authmode(conf,MBEDTLS_SSL_VERIFY_NONE);
	mbedtls_ssl_conf_rng(conf,mbedtls_ctr_drbg_random,ctr_drbg);
	if(0!=mbedtls_ssl_setup(ssl,conf)){
		goto ERROR_OPEN;
	}
	mbedtls_ssl_set_bio(ssl,ctx,mbedtls_net_send,mbedtls_net_recv,NULL);
	_snprintf(port_str,sizeof(port_str),"%u",port);
	res=mbedtls_net_connect(ctx,host,port_str,MBEDTLS_NET_PROTO_TCP);
	if(0==res){
		res=mbedtls_ssl_handshake(ssl);
		if(0==res)
			result=TRUE;
	}
ERROR_OPEN:
	if(!result){
		close_connection(c);
	}
	return result;
}

static int must_close(const int error)
{
	int i,count;
	const int list[]={
		MBEDTLS_ERR_SSL_WANT_READ,
		MBEDTLS_ERR_SSL_WANT_WRITE,
		MBEDTLS_ERR_SSL_ASYNC_IN_PROGRESS,
		MBEDTLS_ERR_SSL_CRYPTO_IN_PROGRESS,
	};
	count=sizeof(list)/sizeof(list[0]);
	for(i=0;i<count;i++){
		if(list[i]==error)
			return FALSE;
	}
	return TRUE;
}

int read_line(mbedtls_ssl_context *ssl,char **line,int *line_len,int timeout)
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
		res=mbedtls_ssl_read(ssl,tmp+index,1);
		if(res>0){
			BYTE a;
			a=tmp[index];
			if('\n'==a){
				result=TRUE;
			}
			index+=res;

		}else if(0==res){
			Sleep(100);
		}else{
			if(must_close(res)){
				printf("ERROR:must close after read line\n");
				WSASetLastError(WSAECONNRESET);
			}
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

int recv_data(mbedtls_ssl_context *ssl,unsigned char *data,int len)
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
		res=mbedtls_ssl_read(ssl,ptr,amount);
		if(res>0){
			offset+=res;
			if(offset>=len){
				result=TRUE;
				break;
			}
		}else if(0==res){
			Sleep(100);
		}else{
			if(must_close(res)){
				printf("ERROR:must close after read data\n");
				WSASetLastError(WSAECONNRESET);
			}
			break;
		}
		delta=GetTickCount()-tick;
		if(delta>=timeout){
			break;
		}
	}
	return result;
}

//return amount read, -1 if error
int recv_any_data(mbedtls_ssl_context *ssl,unsigned char *data,int len)
{
	int result=0;
	int res;
	res=mbedtls_ssl_read(ssl,data,len);
	if(res>=0){
		result=res;
	}else{
		if(must_close(res)){
			printf("must close after read any data\n");
			WSASetLastError(WSAECONNRESET);
			result=-1;
		}
	}
	return result;
}

int send_data(mbedtls_ssl_context *ssl,unsigned char *data,int len)
{
	int result=FALSE;
	int offset=0;
	DWORD tick,delta;
	DWORD timeout=g_timeout;
	tick=GetTickCount();
	while(1){
		int res;
		//dump_hex(data,len);
		res=mbedtls_ssl_write(ssl,data+offset,len-offset);
		if(res>0){
			offset+=res;
			if((len-offset)<=0){
				result=TRUE;
				break;
			}
		}else if(0==res){
			Sleep(50);
		}else{
			if(must_close(res)){
				printf("ERROR:must close after send\n");
				WSASetLastError(WSAECONNRESET);
			}
			break;
		}
		delta=GetTickCount()-tick;
		if(delta>=timeout){
			break;
		}
	}
	return result;
}

int drain_response(mbedtls_ssl_context *ssl)
{
	BYTE *tmp;
	int tmp_len=0x10000;
	int avail;
	avail=mbedtls_ssl_get_bytes_avail(ssl);
	if(avail<=0){
		return FALSE;
	}
	tmp=(BYTE*)malloc(tmp_len);
	if(tmp){
		int total=0;
		while(1){
			int res;
			int amount=tmp_len;
			if(amount>avail)
				amount=avail;
			res=mbedtls_ssl_read(ssl,tmp,amount);
			if(res<=0){
				if(must_close(res)){
					printf("ERROR:must close after drain\n");
					WSASetLastError(WSAECONNRESET);
				}
				break;
			}
			avail-=res;
			if(avail<=0)
				break;
		}
		free(tmp);
	}
	return TRUE;
}

int is_data_avail(CONNECTION *conn,int timeout)
{
	int result=FALSE;
	int res;
	res=mbedtls_ssl_get_bytes_avail(&conn->ssl);
	if(res)
		return TRUE;
	res=mbedtls_net_poll(&conn->ctx,MBEDTLS_NET_POLL_READ,timeout);
	if(res>=0){
		res=res&MBEDTLS_NET_POLL_READ;
		if(res)
			result=TRUE;
	}
	return result;
}
