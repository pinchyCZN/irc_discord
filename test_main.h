#pragma once


static int test_time()
{
	__int64 val;
	const char *tmp="2019-07-12T18:23:29.016000+00:00";
	time_str_to_ftime(tmp,&val);
	printf("%I64X\n",val);
}

static int test_func()
{
	int i;
	MESSAGE_LIST mlist={0};
	for(i=0;i<1000;i++){
		MESSAGE m;
		char *tmp=0;
		int tmp_len=0;
		m.author=strdup("author");
		m.auth_id=strdup("auth_id");
		m.ftime=i+10000;
		append_printf(&tmp,&tmp_len,"ID%05i",i);
		m.id=tmp;
		tmp=0;
		append_printf(&tmp,&tmp_len,"MSG%i",i);
		m.msg=tmp;
		m.timestamp=strdup("timestamp");
		add_message(&mlist,&m);
	}
	remove_some_messages(&mlist,800);
	for(i=0;i<mlist.count;i++){
		MESSAGE *m=&mlist.m[i];
		printf("%s %s\n",m->id,m->msg);
	}
	printf("count=%i\n",mlist.count);
	do_wait();
	exit(0);
}

