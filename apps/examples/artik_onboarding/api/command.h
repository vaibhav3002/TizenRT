#ifndef _ARTIK_COMMAND_H_
#define _ARTIK_COMMAND_H

typedef int (*command_fn)(int argc, char *argv[]);

struct command {
	const char name[16];
	const char usage[128];
	command_fn fn;
};

int commands_parser(int argc, char **argv, const struct command *commands);
void usage(const char* command_base, const struct command *commands);

#endif /* _ARTIK_COMMAND_H */
