enum{
	CMD_JOIN_CHAN=1,
	CMD_LIST_CHAN,
	CMD_GET_MSGS,
	CMD_POST_MSG,
	CMD_CHAN_MSG,
	CMD_GET_NAMES,
	CMD_CREATE_CHAN,
	CMD_RESUME,
	CMD_PART,
	CMD_INVITE_USE,
	CMD_GUILD_LEAVE,
	CMD_TEST,
};

int add_discord_cmd(int cmd,const char *cmd_data);
