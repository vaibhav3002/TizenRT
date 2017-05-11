#include <stdio.h>
#include <string.h>

#include <artik_module.h>
#include <artik_cloud.h>

#include "command.h"

#define	FAIL_AND_EXIT(x)		{ fprintf(stderr, x); usage(argv[1], cloud_commands); ret = -1; goto exit; }

static int device_command(int argc, char *argv[]);
static int devices_command(int argc, char *argv[]);
static int message_command(int argc, char *argv[]);
static int connect_command(int argc, char *argv[]);
static int disconnect_command(int argc, char *argv[]);
static int send_command(int argc, char *argv[]);
static int sdr_command(int argc, char *argv[]);

static artik_websocket_handle ws_handle = NULL;

const struct command cloud_commands[] = {
		{ "device", "device <token> <device id> [<properties>]", device_command },
		{ "devices", "<token> <user id> [<count> <offset> <properties>]", devices_command },
		{ "message", "<token> <device id> <message>", message_command },
		{ "connect", "<token> <device id> [use_se]", connect_command },
		{ "disconnect", "", disconnect_command },
		{ "send", "<message>", send_command },
		{ "sdr", "start|status|complete <dtid> <vdid>|<regid>|<regid> <nonce>", sdr_command },
		{ "", "", NULL }
};

static void websocket_rx_callback(void *user_data, void *result)
{
    if (result) {
        fprintf(stderr, "RX: %s\n", (char*)result);
        free(result);
    }
}

static int device_command(int argc, char *argv[])
{
	artik_cloud_module *cloud = (artik_cloud_module *)artik_request_api_module("cloud");
	int ret = 0;
	char *response = NULL;
    artik_error err = S_OK;
    bool properties = false;

    if (!cloud) {
    	fprintf(stderr, "Failed to request cloud module\n");
    	return -1;
    }

    /* Check number of arguments */
    if (argc < 5) FAIL_AND_EXIT("Wrong number of arguments\n")

    /* Parse optional arguments */
    if (argc > 5) {
        properties = (atoi(argv[5]) > 0);
    }

    err = cloud->get_device(argv[3], argv[4], properties, &response);
    if (err != S_OK) FAIL_AND_EXIT("Failed to get user devices\n")

    if (response) {
        fprintf(stdout, "Response: %s\n", response);
        free(response);
    }

exit:
	if (cloud)
		artik_release_api_module(cloud);

	return ret;
}

static int devices_command(int argc, char *argv[])
{
	artik_cloud_module *cloud = (artik_cloud_module *)artik_request_api_module("cloud");
	int ret = 0;
	char *response = NULL;
	artik_error err = S_OK;
	int count = 10;
	bool properties = false;
	int offset = 0;

    if (!cloud) {
    	fprintf(stderr, "Failed to request cloud module\n");
    	return -1;
    }

	/* Check number of arguments */
	if (argc < 5) FAIL_AND_EXIT("Wrong number of arguments\n")

	/* Parse optional arguments */
	if (argc > 5) {
		count = atoi(argv[5]);
		if (argc > 6) {
			offset = atoi(argv[6]);
			if (argc > 7)
				properties = (atoi(argv[7]) > 0);
		}
	}

	err = cloud->get_user_devices(argv[3], count, properties, 0, argv[4], &response);
	if (err != S_OK) FAIL_AND_EXIT("Failed to get user devices\n")

	if (response) {
		fprintf(stdout, "Response: %s\n", response);
		free(response);
	}

exit:
	if (cloud)
		artik_release_api_module(cloud);

	return ret;
}

static int message_command(int argc, char *argv[])
{
	artik_cloud_module *cloud = (artik_cloud_module *)artik_request_api_module("cloud");
	int ret = 0;
    char *response = NULL;
	artik_error err = S_OK;

    if (!cloud) {
    	fprintf(stderr, "Failed to request cloud module\n");
    	return -1;
    }

    /* Check number of arguments */
	if (argc < 6) FAIL_AND_EXIT("Wrong number of arguments\n")

    err = cloud->send_message(argv[3], argv[4], argv[5], &response);
    if (err != S_OK) FAIL_AND_EXIT("Failed to send message\n")

	if (response) {
		fprintf(stdout, "Response: %s\n", response);
		free(response);
	}

exit:
	if (cloud)
		artik_release_api_module(cloud);

	return ret;
}

static int connect_command(int argc, char *argv[])
{
	int ret = 0;
	artik_error err = S_OK;
	artik_cloud_module *cloud = (artik_cloud_module *)artik_request_api_module("cloud");
	bool use_se = false;

    /* Check number of arguments */
    if (argc < 5) {
		fprintf(stderr, "Wrong number of arguments\n");
		ret = -1;
		goto exit;
	}

    if ((argc == 6) && !strncmp(argv[5], "use_se", strlen("use_se")))
        use_se = true;

    if (!cloud) {
    	fprintf(stderr, "Failed to request cloud module\n");
		ret = -1;
		goto exit;
    }

    err = cloud->websocket_open_stream(&ws_handle, argv[3], argv[4], use_se);
    if (err != S_OK) {
    	fprintf(stderr, "Failed to connect websocket\n");
    	ws_handle = NULL;
		ret = -1;
    	goto exit;
    }

    err = cloud->websocket_set_receive_callback(ws_handle, websocket_rx_callback, NULL);
    if (err != S_OK) {
    	fprintf(stderr, "Failed to set websocket receive callback\n");
    	ws_handle = NULL;
		ret = -1;
        goto exit;
    }

exit:
	if (cloud)
		artik_release_api_module(cloud);

    return ret;
}

static int disconnect_command(int argc, char *argv[])
{
	int ret = 0;
	artik_cloud_module *cloud = (artik_cloud_module *)artik_request_api_module("cloud");

    if (!cloud) {
    	fprintf(stderr, "Failed to request cloud module\n");
		return -1;
    }

    if (!ws_handle) {
        fprintf(stderr, "Websocket to cloud is not connected\n");
        goto exit;
    }

    cloud->websocket_close_stream(ws_handle);

exit:
	artik_release_api_module(cloud);

    return ret;
}

static int send_command(int argc, char *argv[])
{
	int ret = 0;
	artik_cloud_module *cloud = (artik_cloud_module *)artik_request_api_module("cloud");
    artik_error err = S_OK;

    if (!cloud) {
    	fprintf(stderr, "Failed to request cloud module\n");
		return -1;
    }

    if (!ws_handle) {
        fprintf(stderr, "Websocket to cloud is not connected\n");
        goto exit;
    }

    /* Check number of arguments */
    if (argc < 4) FAIL_AND_EXIT("Wrong number of arguments\n")

	fprintf(stderr, "Sending %s\n", argv[3]);

    err = cloud->websocket_send_message(ws_handle, argv[3]);
    if (err != S_OK) FAIL_AND_EXIT("Failed to send message to cloud through websocket\n")

exit:
	artik_release_api_module(cloud);

    return ret;
}

static int sdr_command(int argc, char *argv[])
{
	int ret = 0;
	artik_cloud_module *cloud = NULL;

    if (!strcmp(argv[3], "start")) {
        char *response = NULL;

        if (argc < 6) FAIL_AND_EXIT("Wrong number of arguments\n")

        cloud = (artik_cloud_module *)artik_request_api_module("cloud");
        if (!cloud) FAIL_AND_EXIT("Failed to request cloud module\n")

        cloud->sdr_start_registration(argv[4], argv[5], &response);

        if (response) {
            fprintf(stdout, "Response: %s\n", response);
            free(response);
        }
    } else if (!strcmp(argv[3], "status")) {
        char *response = NULL;

        if (argc < 5) FAIL_AND_EXIT("Wrong number of arguments\n")

        cloud = (artik_cloud_module *)artik_request_api_module("cloud");
        if (!cloud) FAIL_AND_EXIT("Failed to request cloud module\n")

        cloud->sdr_registration_status(argv[4], &response);

        if (response) {
            fprintf(stdout, "Response: %s\n", response);
            free(response);
        }
    } else if (!strcmp(argv[3], "complete")) {
        char *response = NULL;

        if (argc < 6) FAIL_AND_EXIT("Wrong number of arguments\n")

        cloud = (artik_cloud_module *)artik_request_api_module("cloud");
        if (!cloud) FAIL_AND_EXIT("Failed to request cloud module\n")

        cloud->sdr_complete_registration(argv[4], argv[5], &response);

        if (response) {
            fprintf(stdout, "Response: %s\n", response);
            free(response);
        }
    } else {
    	fprintf(stdout, "Unknown command: sdr %s\n", argv[3]);
        usage(argv[1], cloud_commands);
        ret = -1;
        goto exit;
    }

exit:
	if (cloud)
		artik_release_api_module(cloud);

	return ret;
}

#ifdef CONFIG_BUILD_KERNEL
int main(int argc, FAR char *argv[])
#else
int cloud_main(int argc, char *argv[])
#endif
{
    return commands_parser(argc, argv, cloud_commands);
}
