enum{
	START_CHAN_LIST=321,
	CHAN_LIST=322,
	END_CHAN_LIST=323,
	CHAN_MSG=100,
	PRIV_MSG=110,
	OK_JOIN_CHAN=200,
	NAME_LIST=201,
	END_NAME_LIST=202,
	UNKNOWN_CHAN=437,
	CHAN_TOPIC=332,
};

void irc_thread(void *args);
int push_irc_msg(const char *str);
const char *get_irc_msg_str(int code);
