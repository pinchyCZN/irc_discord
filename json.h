#pragma once

enum{
	JSON_UNKNOWN=0,
	JSON_VALUE,
	JSON_NODE
};
typedef struct{
	char *key;
	int key_len;
	int type;
	union{
		BYTE *value;
		void *ptr;
	};
	int value_len;
	void *next;
}JSON;

int get_json_value_str(const char *str,int str_len,const char *key,const char **out,int *out_len);
int get_json_value(JSON *json,const char *key,char *out,int out_size);

int parse_json(BYTE *str,int str_len, JSON **json);
