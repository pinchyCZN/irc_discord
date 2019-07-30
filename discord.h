enum{
	CMD_JOIN_CHAN=1,
	CMD_GET_MSGS,
	CMD_POST_MSG,
};

int add_discord_cmd(int cmd,char *cmd_data);
