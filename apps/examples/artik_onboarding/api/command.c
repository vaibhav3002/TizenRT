#include "command.h"

#include <stdio.h>

void usage(const char* command_base, const struct command *commands)
{
	const struct command *cmd = commands;

	fprintf(stderr, "usage:\n");

	while (cmd->fn) {
		fprintf(stderr, "\t%s %s - %s\n", command_base, cmd->name, cmd->usage);
		cmd++;
	}
}


int commands_parser(int argc, char **argv, const struct command *commands)
{
	const struct command *cmd = commands;

	if (argc < 2) {
		usage(argv[0], commands);
		return -1;
	}

	while(cmd->fn) {
		if (!strcmp(argv[1], cmd->name)) {
			if (cmd->fn) {
				char tname[32];
				snprintf(tname, 32, "%s_command", cmd->name);
				task_create(tname, SCHED_PRIORITY_DEFAULT, 16384, cmd->fn, argv);
			}
			break;
		}

		cmd++;
	}

	if (!cmd->fn) {
		fprintf(stderr, "Unknow command\n");
		usage(argv[0], commands);
		return -1;
	}

	return 0;
}
