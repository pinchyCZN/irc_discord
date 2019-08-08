#include <windows.h>
#include <stdio.h>

#pragma warning(disable:4996)

void dump_hex(unsigned char *data,int len)
{
	int i;
	int width=16;
	for(i=0;i<len;i+=width){
		int j,index;
		int count=0;
		printf("%08X: ",i);
		for(j=0;j<width;j++){
			BYTE a,sep=' ';
			index=i+j;
			if(index>=len){
				for(;j<width;j++)
					printf("   ");
				break;
			}
			count++;
			if(count>=4 && j!=(width-1)){
				sep='-';count=0;
			}
			a=data[index];
			printf("%02X%c",a,sep);
		}
		printf(" ");
		for(j=0;j<width;j++){
			BYTE a;
			index=i+j;
			if(index>=len)
				break;
			a=data[index];
			if(a<' ' || a>0x7E)
				a='.';
			printf("%c",a);
		}
		printf("\n");
	}
}

void *dupe_mem_null(void *mem,int mem_len)
{
	void *result=0;
	void *tmp;
	int tmp_len=mem_len+1;
	tmp=calloc(tmp_len,1);
	if(tmp){
		memcpy(tmp,mem,mem_len);
		result=tmp;
	}
	return tmp;
}

int append_printf(char **buf,int *buf_len,const char *fmt,...)
{
	int result=FALSE;
	int res;
	va_list ap;
	va_start(ap,fmt);
	res=_vsnprintf(0,0,fmt,ap);
	if(res>0){
		char *tmp=0;
		int tmp_size=0;
		int current_len=0;
		int start=0;
		tmp=*buf;
		if(tmp){
			current_len=strlen(tmp);
		}
		tmp_size=current_len+res+1;
		start=current_len;
		tmp=(char*)realloc(tmp,tmp_size);
		if(tmp){
			_vsnprintf(tmp+start,tmp_size-start,fmt,ap);
			tmp[tmp_size-1]=0;
			*buf=tmp;
			*buf_len=tmp_size;
			result=TRUE;
		}
	}
	return result;
}

char *extract_quote(const char *str)
{
	char *result=0;
	int index=0;
	int state=0;
	const char *start=0;
	const char *end=0;
	while(1){
		const char *ptr;
		char a;
		ptr=str+index;
		a=ptr[0];
		if(0==a)
			break;
		switch(state){
		case 0:
			if('\"'==a){
				start=ptr+1;
				state=1;
			}
			break;
		case 1:
			if('\"'==a){
				end=ptr;
				state=2;
			}
			break;
		}
		if(2==state){
			char *tmp;
			SIZE_T a,b,tmp_len;
			a=(SIZE_T)start;
			b=(SIZE_T)end;
			tmp_len=(b-a)+1;
			tmp=(char*)calloc(tmp_len,1);
			if(tmp){
				strncpy(tmp,start,tmp_len);
				if(tmp_len>0)
					tmp[tmp_len-1]=0;
				result=tmp;
			}
			break;
		}
		index++;
	}
	return result;
}

int extract_token(const char *data,char **token)
{
	//{"token": "NDE1ODgwNzQ0Mjc2MzI4NDU4.XQZhhQ.AKraCLiXSvIet8090qFeJd4-Af4"}
	int result=FALSE;
	const char *ptr;
	int index=0;
	ptr=strchr(data,':');
	if(ptr){
		char *tmp=extract_quote(ptr);
		if(tmp){
			result=TRUE;
			*token=tmp;
		}
	}
	return result;
}

int trim_trailing_crlf(char *str, int end_index)
{
	int i;
	for(i=end_index;i>=0;i--){
		char a;
		a=str[i];
		if('\r'==a || '\n'==a){
			str[i]=0;
		}else{
			break;
		}
	}
	return TRUE;
}
int trim_right(char *str)
{
	int i,len;
	len=strlen(str);
	for(i=len-1;i>=0;i--){
		unsigned char a;
		a=str[i];
		if(isspace(a)){
			str[i]=0;
		}else{
			break;
		}
	}
	return TRUE;
}

int get_line_count(char *data,int data_len)
{
	int result=0;
	int index=0;
	while(1){
		char a;
		if(index>=data_len){
			if(index>0){
				result++;
			}
			break;
		}
		a=data[index++];
		if(0==a){
			result++;
			break;
		}
		if('\n'==a){
			result++;
		}
	}
	return result;
}
int get_line_offset(char *data,int data_len,int line_num,char **out)
{
	int result=FALSE;
	int index=0;
	int lnum=0;
	while(1){
		char a;
		if(index>=data_len){
			break;
		}
		a=data[index++];
		if(0==a){
			break;
		}
		if('\n'==a){
			lnum++;
		}
		if(lnum==line_num){
			*out=data+index;
			result=TRUE;
			break;
		}
	}
	return result;
}
int get_line(char *data,int data_len,int line_num,char *out,int out_len)
{
	int result=FALSE;
	int lnum=0;
	int pos=0;
	int out_index=0;
	while(1){
		char a;
		if(pos>=data_len){
			break;
		}
		a=data[pos++];
		if(lnum==line_num){
			result=TRUE;
			if(out_index>=out_len){
				break;
			}
			if(0==a || '\n'==a){
				if(out_index>0){
					trim_trailing_crlf(out,out_index-1);
				}
				out[out_index++]=0;
				break;
			}
			out[out_index++]=a;
		}
		if('\n'==a){
			lnum++;
		}
		if(0==a){
			break;
		}
	}
	if(out && out_len){
		out[out_len-1]=0;
	}
	return result;
}

int get_resp_code(char *data,int data_len)
{
	int result=0;
	int res;
	char line[512]={0};
	res=get_line(data,data_len,0,line,sizeof(line));
	if(res){
		const char *HTTP="HTTP/";
		const int HTTP_LEN=strlen(HTTP);
		res=strncmp(line,HTTP,HTTP_LEN);
		if(0==res){
			char *tmp;
			tmp=strchr(line,' ');
			if(tmp){
				int status;
				tmp++;
				status=atoi(tmp);
				result=status;
			}
		}

	}
	return result;
}

const char *strstri(const char *str,const char *substr)
{
	const char *result=0;
	int index1=0,index2=0;
	while(1){
		char a,b;
		a=str[index1];
		b=substr[index2];
		a=tolower(a);
		b=tolower(b);
		if(a==b || 0==b){
			if(0==b){
				result=str+index1-index2;
				break;
			}
			index2++;
		}else{
			index2=0;
		}
		if(0==a)
			break;
		index1++;
	}
	return result;
}

int startswithi(const char *str,const char *substr)
{
	int result=FALSE;
	int index1=0,index2=0;
	while(1){
		char a,b;
		a=str[index1];
		b=substr[index2];
		a=tolower(a);
		b=tolower(b);
		if(a==b || 0==b){
			if(0==b){
				result=TRUE;
				break;
			}
			index2++;
		}else{
			break;
		}
		if(0==a)
			break;
		index1++;
	}
	return result;
}

int null_str(unsigned char **data,int data_len)
{
	int result=FALSE;
	BYTE *tmp;
	int tmp_len;
	tmp_len=data_len+1;
	tmp=*data;
	tmp=(BYTE*)realloc(tmp,tmp_len);
	if(tmp){
		*data=tmp;
		tmp[tmp_len-1]=0;
		result=TRUE;
	}
	return result;
}

const char *seek_next_word(const char *str)
{
	const char *result=0;
	int index=0;
	int state=0;
	if(0==str){
		return result;
	}
	while(1){
		unsigned char a=str[index];
		if(0==a){
			break;
		}
		if(0==state){
			if(isspace(a)){
				state=1;
			}
		}else{
			if(!isspace(a)){
				result=str+index;
				break;
			}
		}
		index++;
	}
	return result;
}

int get_word(char *str,char *out,int out_size)
{
	int result=FALSE;
	int index=0;
	int out_index=0;
	int state=0;
	if(0==str || 0==out){
		return result;
	}
	while(1){
		int store_char=FALSE;
		int exit=FALSE;
		unsigned char a=str[index++];
		if(0==a){
			store_char=TRUE;
			exit=TRUE;
		}
		if(0==state){
			if(!isspace(a)){
				store_char=TRUE;
				state=1;
			}
		}else if(1==state){
			if(isspace(a)){
				a=0;
				store_char=TRUE;
				exit=TRUE;
			}else{
				store_char=TRUE;
			}
		}else{
			exit=TRUE;
		}
		if(store_char){
			if(out_index>=out_size){
				break;
			}
			out[out_index++]=a;
			result=TRUE;
		}
		if(exit){
			break;
		}
	}
	if(out_size){
		out[out_size-1]=0;
	}
	return result;
}

int __snprintf(char *buf,int buf_len,const char *fmt,...)
{
	int result=0;
	va_list ap;
	va_start(ap,fmt);
	result=_vsnprintf(buf,buf_len,fmt,ap);
	if(buf_len){
		buf[buf_len-1]=0;
	}
	if(result<0){
		result=0;
	}
	return result;
}

void fix_spaced_str(char *str)
{
	int index=0;
	while(1){
		unsigned char a=str[index];
		if(0==a){
			break;
		}
		if(isspace(a)){
			str[index]='_';
		}
		index++;
	}
}