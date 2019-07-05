#pragma once

#include "polarssl/config.h"
#include "polarssl/net.h"
#include "polarssl/ssl.h"
#include "polarssl/entropy.h"
#include "polarssl/ctr_drbg.h"
#include "polarssl/error.h"

typedef SOCKET			socket_t;

typedef struct{
	socket_t sock;
	ssl_context ssl;
}CONNECTION;


int close_connection(CONNECTION *c);

int ssl_connect(ssl_context *ssl,const int options,const char *host,const int port,int *c_socket);

int read_line(ssl_context *ssl,char **line,int *line_len,int timeout);

int recv_data(ssl_context *ssl,BYTE *data,int len);

int send_data(ssl_context *ssl,BYTE *data,int len);

int drain_response(ssl_context *ssl);
