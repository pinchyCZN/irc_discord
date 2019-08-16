#pragma once

static int do_wait()
{
	while(1){
		int key=getch();
		if(0x1b==key){
			exit(0);
		}
	}
	return 0;
}


static int test_time()
{
	__int64 val;
	const char *tmp="2019-07-12T18:23:29.016000+00:00";
	time_str_to_ftime(tmp,&val);
	printf("%I64X\n",val);
}

static void do_discord_test(CONNECTION *conn)
{
	int i,count;
	count=g_guild_list.count;
	printf(">>>doing discord test<<<\n");
	for(i=0;i<count;i++){
		GUILD *g;
		g=&g_guild_list.guild[i];
		//#Turok_Speedrunning.general
		if(strstr(g->name,"Turok_Speedrunning")){
			int j;
			for(j=0;j<g->channels.count;j++){
				CHANNEL *c;
				c=&g->channels.chan[j];
				if(strstr(c->name,"general")){
					const char *chan_id=c->id;
					MESSAGE_LIST mlist={0};
					printf(">>>getting messages<<<\n");
					get_messages(conn,&mlist,chan_id,100,0,NULL,0);
					sort_messages(&mlist);
					printf("count:%i\n",mlist.count);
					dump_message_list(&mlist,"==");
					remove_all_msg(&mlist);
					break;
				}
			}
			break;
		}
	}
}

static int test_msg_stuff()
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
}
static int test_func()
{
	int x;
	x=isspace(0);
	printf("%i\n",x);
	do_wait();
	exit(0);
}

