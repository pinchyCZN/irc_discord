#pragma once

void dump_hex(unsigned char *data,int len);

void *dupe_mem_null(void *mem,int mem_len);

int append_printf(char **buf,int *buf_len,const char *fmt,...);

char *extract_quote(const char *str);

int extract_token(const char *data,char **token);

int trim_trailing_crlf(char *str, int end_index);

int trim_right(char *str);

int get_line_count(char *data,int data_len);

int get_line_offset(char *data,int data_len,int line_num,char **out);

int get_line(char *data,int data_len,int line_num,char *out,int out_len);

int get_resp_code(char *data,int data_len);

const char *strstri(const char *str,const char *substr);

int startswithi(const char *str,const char *substr);

int null_str(unsigned char **data,int data_len);

const char *seek_next_word(const char *str);

int get_word(const char *str,char *out,int out_size);

int __snprintf(char *buf,int buf_len,const char *fmt,...);

void fix_spaced_str(char *str);

void replace_chars(char *str,const char *list,const char *rlist);

const char *seek_next_digit(const char *str);

void time_str_to_systime(const char *str,SYSTEMTIME *time);

void time_str_to_ftime(const char *str,__int64 *val);