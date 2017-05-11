#include <stdio.h>
#include <string.h>

#include <artik_module.h>

#include "command.h"

static int module_version(int argc, char *argv[]);
static int module_platform(int argc, char *argv[]);
static int module_info(int argc, char *argv[]);
static int module_modules(int argc, char *argv[]);

static struct command module_commands[] = {
	{ "version", "Display the version of the ARTIK SDK", module_version },
	{ "platform", "Display the platform", module_platform },
	{ "info", "Display information about the ARTIK SDK", module_info },
	{ "modules", "Lists all the SDK modules available for this platform", module_modules },
	{ "", "", NULL }
};

static int module_version(int argc, char *argv[])
{
	artik_api_version version;
	artik_get_api_version(&version);
	fprintf(stdout, "ARTIK SDK version: %s\n", version.version);

	return 0;
}

static int module_platform(int argc, char *argv[])
{
	int ret = 0;
	char platname[MAX_PLATFORM_NAME];
	if (artik_get_platform_name(platname) != S_OK) {
		fprintf(stderr, "Failed to get platform name\n");
		usage(argv[1], module_commands);
		ret = -1;
		goto exit;
	}
	fprintf(stdout, "Platform name: %s\n", platname);

exit:
	return ret;
}

static int module_info(int argc, char *argv[])
{
	int ret = 0;
	char *info = artik_get_device_info();
	if (!info) {
		fprintf(stderr, "Failed to get device info\n");
		usage(argv[1], module_commands);
		ret = -1;
		goto exit;
	}
	fprintf(stdout, "Device info: %s\n", info);

exit:
	return ret;
}

static int module_modules(int argc, char *argv[])
{
	int ret = 0;
	artik_api_module *modules = NULL;
	int num_modules = 0;
	int i = 0;

	if (artik_get_available_modules(&modules, &num_modules) != S_OK) {
		fprintf(stderr, "Failed to get available modules\n");
		usage(argv[1], module_commands);
		ret = -1;
		goto exit;
	}

	fprintf(stdout, "Available modules:\n");
	for (i = 0; i < num_modules; i++)
		fprintf(stdout, "\t%d: %s\n", modules[i].id, modules[i].name);

exit:
	return ret;
}


#ifdef CONFIG_BUILD_KERNEL
int main(int argc, FAR char *argv[])
#else
int module_main(int argc, char *argv[])
#endif
{
	return commands_parser(argc, argv, module_commands);
}
