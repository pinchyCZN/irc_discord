#include <Windows.h>
#include "network.h"
#include "libstring.h"
#include "config.h"
#include "json.h"
#include "parson.h"

typedef struct{
	int opcode;
	int is_final;
	int len;
	BYTE *data;
}WS_PAYLOAD;
int get_header_size(BYTE *data)
{
	int is_final;
	int is_masked;
	int first_byte;
	int header_size;
	is_final = (data[0] & 0x80) != 0;
	is_masked = (data[1] & 0x80) != 0;
	first_byte = data[1] & 0x7F;
	header_size = 2 + (first_byte == 0x7E ? 2 : 0) + (first_byte == 0x7F ? 8 : 0) + (is_masked ? 4 : 0);
	return header_size;
}
int get_payload_size(BYTE *data)
{
	int result=0;
	int plen;
	UINT64 size=0;
	plen = data[1] & 0x7F;
	switch (plen) {
	case 0x7F:
		size += ((UINT64)data[2]) << 56;
		size += ((UINT64)data[3]) << 48;
		size += ((UINT64)data[4]) << 40;
		size += ((UINT64)data[5]) << 32;
		size += ((UINT64)data[6]) << 24;
		size += ((UINT64)data[7]) << 16;
		size += ((UINT64)data[8]) << 8;
		size += ((UINT64)data[9]);
		break;

	case 0x7E:
		size += ((UINT64)data[2]) << 8;
		size += ((UINT64)data[3]);
		break;

	default:
		size = plen;
	}
	if(size>MAXDWORD){
		size=MAXDWORD;
	}
	result=(DWORD)size;
	return result;
}


int login_gateway(CONNECTION *c)
{
	int result=FALSE;
	ssl_context *ssl;
	int res;
	const char *host;
	int port;
	ssl=&c->ssl;
	//close_connection(c);
	host=g_gateway;
	port=443;
	res=ssl_connect(ssl,0,host,port,(int*)&c->sock);
	if(!res){
		return result;
	}
	{
		char *data=0;
		int data_len=0;
		append_printf(&data,&data_len,"GET https://%s/?encoding=json&v=6 HTTP/1.1\r\n",host);
		append_printf(&data,&data_len,"Host: %s\r\n",host);
		append_printf(&data,&data_len,"Upgrade: websocket\r\n");
		append_printf(&data,&data_len,"Pragma: no-cache\r\n");
		append_printf(&data,&data_len,"Cache-Control: no-cache\r\n");
		append_printf(&data,&data_len,"Connection: Upgrade\r\n");
		append_printf(&data,&data_len,"Sec-WebSocket-Key: KFShSwLlp4E6C7JZc5h4sg==\r\n");
		append_printf(&data,&data_len,"Sec-WebSocket-Version: 13\r\n");
		append_printf(&data,&data_len,"Sec-WebSocket-Extensions: permessage-deflate; client_max_window_bits\r\n");
		append_printf(&data,&data_len,"\r\n");
		if(data){
			res=send_data(ssl,(BYTE*)data,strlen(data));
			if(res){
				int index=0;
				int websocket=FALSE;
				while(1){
					char *line=0;
					int line_len=0;
					res=read_line(ssl,&line,&line_len,g_timeout);
					if(!res){
						break;
					}
					printf("%s",line);
					if(0==index){
						res=get_resp_code(line,line_len);
						if(101!=res){
							drain_response(ssl);
							break;
						}
					}else{
						if(strstri(line,"upgrade")){
							if(strstri(line,"websocket")){
								websocket=TRUE;
							}
						}
					}
					if(line_len>=2){
						if('\r'==line[0] && '\n'==line[1]){
							result=TRUE;
						}
					}
					//end of line processing
					index++;
					if(line){
						free(line);
						line=0;
						line_len=0;
					}
					if(result){
						if(!websocket){
							result=FALSE;
						}
						break;
					}
				}
			}
			free(data);
		}
	}
	if(!result){
		close_connection(c);
	}
	return result;
}



int get_ws_payload(ssl_context *ssl,WS_PAYLOAD *payload)
{
	int result=FALSE;
	BYTE header[32]={0};
	int state=0;
	int fail=FALSE;
	int is_masked=FALSE;
	int header_size=0;
	int payload_size;
	int offset=0;
	while(1){
		int res;
		switch(state){
		case 0: //get header prefix
			memset(header,0, sizeof(header));
			res=recv_data(ssl,header,2);
			if(res>0){
				header_size=get_header_size(header);
				is_masked=(header[1] & 0x80) != 0;
				offset=2;
				state=1;
			}else{
				fail=TRUE;
			}
			break;
		case 1: //get remaining header
			if(header_size>offset){
				res=recv_data(ssl,header+offset,header_size-offset);
				if(res>0){
					offset+=res;
					state=2;
				}else{
					state=0;
					fail=TRUE;
				}
			}else{
				state=2;
			}
			break;
		case 2: //get payload
			payload_size=get_payload_size(header);
			if(payload_size){
				BYTE *tmp;
				tmp=(BYTE*)calloc(payload_size,1);
				if(tmp){
					res=recv_data(ssl,tmp,payload_size);
					if(res){
						state=3;
						payload->data=tmp;
						payload->len=payload_size;
					}else{
						state=0;
						fail=TRUE;
					}
				}else{
					state=0;
					fail=TRUE;
				}
			}else{
				state=3;
				payload->data=0;
				payload->len=0;
			}
			break;
		case 3: //set final params and exit
			payload->opcode=header[0]&0xF;
			payload->is_final=(header[0]&0x80)!=0;
			result=TRUE;
			break;
		}
		if(result){
			dump_hex(header,sizeof(header));
			printf("WS payload is final%i:\n",payload->is_final);
			dump_hex(payload->data,payload->len);
			break;
		}
		if(fail){
			result=FALSE;
			break;
		}
	}
	return result;
}

int append_data(BYTE **data,int *data_len,BYTE *append,int append_len)
{
	int result=FALSE;
	BYTE *tmp;
	int tmp_size;
	int orig_len;
	if(0==append_len)
		return result;
	tmp=*data;
	orig_len=*data_len;
	tmp_size=orig_len+append_len;
	tmp=(BYTE*)realloc(tmp,tmp_size);
	if(tmp){
		memcpy(tmp+orig_len,append,append_len);
		*data=tmp;
		*data_len=tmp_size;
		result=TRUE;
	}
	return result;
}


int send_ws_payload(ssl_context *ssl,int opcode,BYTE *data,int data_len)
{
	int result=FALSE;
	BYTE hdr[16]={0};
	BYTE *tmp;
	int payload_size;
	hdr[0]=0x80|(opcode&0xF);
	hdr[1]=0x80|0x7E;
	hdr[2]=data_len>>8;
	hdr[3]=data_len&0xFF;
	payload_size=8+data_len;
	tmp=(BYTE*)calloc(payload_size,1);
	if(tmp){
		memcpy(tmp,hdr,8);
		memcpy(tmp+8,data,data_len);
		result=send_data(ssl,tmp,payload_size);
		free(tmp);
	}
	return result;
}

int send_identify(ssl_context *ssl)
{
	int result=FALSE;
	char *buf=0;
	int buf_len=0;
	int slen;
	append_printf(&buf,&buf_len,"{\"op\":2,\"d\":");
	append_printf(&buf,&buf_len,"{\"token\":\"%s\",",g_token);
	append_printf(&buf,&buf_len,"\"properties\":{\"os\":\"linux\",\"browser\":\"Chrome\","
		"\"device\":\"0.95.11 alpha build #0 ()\","
		"\"referrer\":\"https:\\/\\/miranda-ng.org\",\"referring_domain\":\"miranda-ng.org\"},"
		"\"compress\":false,\"large_threshold\":250}}");
	if(0==buf){
		return result;
	}
	slen=strlen(buf);
	result=send_ws_payload(ssl,1,(BYTE*)buf,slen);
	printf("sending ident:\n%.*s\n",buf_len,buf);
	return result;
}

int process_payload(CONNECTION *con,BYTE *data,int data_len)
{
	int result=FALSE;
	int opcode=-1;
	char tmp[80]={0};
	ssl_context *ssl;
	int res;
	JSON_Value *root;
	ssl=&con->ssl;
	//{"t":null,"s":null,"op":10,"d":{"heartbeat_interval":41250,"_trace":["[\"gateway-prd-main-g8rb\",{\"micros\":0.0}]"]}}
	printf("%.*s\n",data_len,data);
	root=json_parse_string(data);
	if(0==root){
		printf("Error paring json\n");
		return result;
	}
	//root->v
	switch(opcode){
	case 0: //process incoming command
		break;
	case 9: //session invalidated
		break;
	case 10: //hello
		send_identify(ssl);
		break;
	case 11: //hrtbt ack
		break;
	default:
		printf("unhandled opcode:%i\n",opcode);
		break;
	}
	result=TRUE;
	return result;
}

int send_ping(ssl_context *ssl,BYTE *data,int data_len)
{
	int result=FALSE;
	printf("sending ping\n");
	//send_data(ssl,data,data_len);
	send_ws_payload(ssl,9,data,data_len);
	return result;
}


void gateway_thread(void *args)
{
	CONNECTION con={0};
	enum{
		GW_LOGIN=0,
		GW_WAIT,
	};
	int state=0;
	if(NULL==g_gwevent){
		printf("gateway event not created\n");
		return;
	}
	while(1){
		DWORD res;
		res=WaitForSingleObject(g_gwevent,INFINITE);
		if(WAIT_OBJECT_0==res){
			switch(state){
			case GW_LOGIN:
				if(0==g_gateway[0] && FALSE){
					printf("no gateway\n");
					break;
				}else{
					printf("gateway login\n");
					res=login_gateway(&con);
					if(res){
						BYTE *complete_payload=0;
						int payload_size=0;
						int exit_ws=FALSE;
						int error_count=0;
						while(1){
							WS_PAYLOAD payload={0};
							res=get_ws_payload(&con.ssl,&payload);
							if(!res){
								error_count++;
								if(error_count>5)
									break;
							}
							{
								int opcode;
								opcode=payload.opcode;
								printf("opcode=%i\n",opcode);
								switch(opcode){
								case 0: // text packet
								case 1: // binary packet
								case 2: // continuation
									append_data(&complete_payload,&payload_size,payload.data,payload.len);
									if(payload.is_final){
										null_str(&complete_payload,payload_size);
										process_payload(&con,complete_payload,payload_size);
										free(complete_payload);
										complete_payload=0;
										payload_size=0;
									}
									break;
								case 8: // close
									printf("%.*s\n",payload.len,payload.data);
									SetEvent(g_gwevent);
									state=GW_LOGIN;
									exit_ws=TRUE;
									Sleep(500000);
									break;
								case 9: // ping
									send_ping(&con.ssl,payload.data,payload.len);
									break;
								default:
									printf("unhandled opcode %i\n",opcode);
									break;
								}
								if(payload.data){
									free(payload.data);
								}
								memset(&payload,0,sizeof(payload));
							}
							if(exit_ws){
								break;
							}
						}
						close_connection(&con);
					}
				}
				break;
			}
		}else{
			Sleep(1000);
		}
	}
}
