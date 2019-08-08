enum{
	CMD_JOIN_CHAN=1,
	CMD_LIST_CHAN,
	CMD_GET_MSGS,
	CMD_POST_MSG,
};

int add_discord_cmd(int cmd,const char *cmd_data);
