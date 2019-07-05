#include <windows.h>
#include <stdio.h>

#include "json.h"
#pragma warning(disable:4996)

int get_json_value_str(const char *str,int str_len,const char *key,const char **out,int *out_len)
{
	int result=FALSE;
	int state=0;
	int sub_state=0;
	int key_match=0;
	int level=0;
	int i;
	const char *val_start=0;
	int val_len=0;
	for(i=0;i<str_len;i++){
		char a;
		const char *ptr;
		ptr=str+i;
		a=ptr[0];

		switch(state){
		case 0:
			if('{'==a){
				state=1;
			}else if(!isspace(a)){
				state=99;
			}
			break;
		case 1: //check for start of string
			switch(sub_state){
			default:
			case 0: //check start of key
				if('"'==a){
					state=2;
				}else if('}'==a){
					level--;
				}else if(!isspace(a)){
					state=99;
				}
				break;
			case 1: //check for value or next level
				if('{'==a){
					sub_state=0;
					level++;
				}else if('"'==a){
					state=2;
				}else if(!isspace(a)){
					state=99;
				}
				break;
			}
			break;
		case 2: //string start
			val_start=ptr;
			state=3;
			break;
		case 3: //check end of string
			if('"'==a){
				char b=str[i-1];
				if(b!='/'){ //is the real end
					int res;
					SIZE_T p1,p2;
					p2=(SIZE_T)ptr;
					p1=(SIZE_T)val_start;
					p1=p2-p1;
					val_len=(int)p1;
					switch(sub_state){
					case 0: //checking key
						res=strncmp(val_start,key,val_len);
						if(0==res){
							key_match=1;
						}
						state=4;
						break;
					case 1: //checking value
						if(key_match){
							*out=val_start;
							*out_len=val_len;
							result=TRUE;
						}
						sub_state=0;
						state=5;
						break;
					}
				}
			}
			break;
		case 4: //find colon
			if(':'==a){
				sub_state=1;
				state=1;
			}else if(!isspace(a)){
				state=99;
			}
			break;
		case 5: //find comma or next brace
			if(','==a){
				state=1;
			}else if(!isspace(a)){
				state=99;
			}
			break;
		default:
			break;
		}
		if(state>=99){
			printf("parse error\n");
			break;
		}
		if(result){
			break;
		}
	}
	return result;
}


int advance_node(JSON **json)
{
	int result=FALSE;
	JSON *tmp;
	tmp=(JSON*)calloc(sizeof(JSON),1);
	if(tmp){
		(*json)->next=tmp;
		*json=tmp;
		result=TRUE;
	}
	return result;
}

int parse_json(BYTE *str,int str_len, JSON **json)
{
	int result=FALSE;
	int i,state=0;
	JSON *node,*start_node;
	int end_found=FALSE;
	int block_depth=0;
	int is_quoted=FALSE;
	node=(JSON*)calloc(sizeof(JSON),1);
	start_node=node;
	for(i=0;i<str_len;i++){
		BYTE a;
		int is_space=FALSE;
		int is_quote=FALSE;
		int is_comma=FALSE;
		a=str[i];
		//printf("char=%c state=%i\n",a,state);
		is_space=isspace(a);
		if('\"'==a)
			is_quote=TRUE;
		if(','==a)
			is_comma=TRUE;
		switch(state){
		case 0:
			if(is_space)
				continue;
			if('{'==a){
				state=1;
			}else{
				state=99;
			}
			break;
		case 1: //start of key
			if(is_quote){
				node->key=(char*)str+i+1;
				state=2;
			}else if(!is_space){
				state=99;
			}
			break;
		case 2: //start of key text
			if(is_quote){
				char *ptr=(char*)str+i;
				char b=str[i-1];
				if('\\'==b){
					continue;
				}
				node->key_len=ptr-node->key;
				state=3;
			}
			break;
		case 3: //search for colon
			if(':'==a){
				state=4;
			}else if(!is_space){
				state=99;
			}
			break;
		case 4: //search for start of value
			if(is_quote){
				node->value=str+i+1;
				node->type=JSON_VALUE;
				state=5;
				is_quoted=TRUE;
			}
			else if(!is_space){
				is_quoted=FALSE;
				if('{'==a){
					int res=0;
					JSON *tmp=0;
					node->type=JSON_NODE;
					res=parse_json(str+i,str_len-i,&tmp);
					node->ptr=tmp;
					state=40;
					block_depth=0;
				}else if('['==a){
					node->type=JSON_NODE;
					state=30;
					block_depth=0;
				}else{
					node->value=str+i;
					state=5;
				}
			}
			break;
		case 5: //search for end of value
			if(is_quoted){
				if(is_quote){
					char b=str[i-1];
					if('\\'==b){
						continue;
					}
					state=6;
				}
			}else if(is_space || is_comma){
				state=6;
				if(is_comma){
					state=1;
				}
			}else if('}'==a){
				state=50; //ride out the end
				end_found=TRUE;
			}
			if(state!=5){
				char *ptr;
				ptr=(char*)str+i;
				node->value_len=ptr-(char*)node->value;
				advance_node(&node);
			}
			break;
		case 6: //look for comma or end of node
			if(','==a){
				//add node
				JSON *tmp;
				tmp=(JSON*)calloc(sizeof(JSON),1);
				if(tmp){
					node->next=tmp;
					node=tmp;
				}else{
					state=100;
				}
				state=1;
			}else if('}'==a){
				state=50;
				end_found=TRUE;
			}

			break;
		case 30: //list block [
			if('['==a){
				block_depth++;
			}else if(']'==a){
				block_depth--;
			}
			if(block_depth<0){
				block_depth=0;
				state=6;
			}
			break;
		case 40: //continue thru sub node
			if('{'==a){
				block_depth++;
			}else if('}'==a){
				block_depth--;
			}
			if(block_depth<0){
				block_depth=0;
				state=6;

			}
			break;
		case 50: //other shit
				 //wait till end
			break;
		case 99:
			break;
		default:
			break;
		}
		if(state>=99){
			printf("error parsing\n");
			break;
		}
		if(end_found){
			result=TRUE;
			break;
		}

	}
	*json=start_node;
	return result;
}
int free_json(JSON *json)
{
	JSON *node=json;
	while(node){
		void *tmp;
		tmp=node->next;
		if(node->ptr)
			free_json((JSON*)node->ptr);
		free(node);
		node=(JSON*)tmp;
	}
	return 0;
}

int get_json_value(JSON *json,const char *key,char *out,int out_size)
{
	int result=FALSE;
	JSON *node=json;
	int key_len=strlen(key);
	while(node){
		if(node->key && key_len==node->key_len){
			int res;
			res=strncmp(node->key,key,key_len);
			if(0==res){
				int len;
				len=node->value_len;
				if(len>out_size){
					len=out_size;
				}
				strncpy(out,(const char*)node->value,len);
				if(out_size>0)
					out[out_size-1]=0;
				result=TRUE;
				break;
			}
		}
		node=(JSON*)node->next;
	}
	return result;
}
