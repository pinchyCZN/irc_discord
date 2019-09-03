#pragma once

#include "mbedtls/config.h"
#include "mbedtls/net.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"

typedef struct{
	mbedtls_ssl_context ssl;
	mbedtls_net_context ctx;
	mbedtls_ssl_config conf;
	mbedtls_entropy_context entropy;
	mbedtls_ctr_drbg_context cts_drbg;
}CONNECTION;


int close_connection(CONNECTION *c);

int open_connection(CONNECTION *c,const char *host,const int port);

int read_line(mbedtls_ssl_context *ssl,char **line,int *line_len,int timeout);

int recv_data(mbedtls_ssl_context *ssl,unsigned char *data,int len);

int recv_any_data(mbedtls_ssl_context *ssl,unsigned char *data,int len);

int send_data(mbedtls_ssl_context *ssl,unsigned char *data,int len);

int drain_response(mbedtls_ssl_context *ssl);

int is_data_avail(CONNECTION *conn,int timeout);