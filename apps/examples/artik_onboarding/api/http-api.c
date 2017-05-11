#include <stdio.h>
#include <string.h>

#include <artik_module.h>
#include <artik_http.h>

#include "command.h"

static int http_get(int argc, char **argv);
static int http_post(int argc, char **argv);
static int http_put(int argc, char **argv);
static int http_delete(int argc, char **argv);

const struct command http_commands[] = {
	{ "get", "get <url>", http_get},
	{ "post", "post <url> <body>", http_post},
	{ "put", "put <url> <body>", http_put},
	{ "delete", "delete <url>", http_delete },
	{ "", "", NULL }
};

static int http_get(int argc, char **argv)
{
	artik_http_module *http = (artik_http_module *)artik_request_api_module("http");
	artik_error ret = S_OK;
	char *response = NULL;
	int status = 0;
	artik_http_headers headers;
	artik_http_header_field fields[] = {
		{"Connect", "close"},
		{"User-Agent", "Artik browser"},
		{"Accept-Language", "en-US,en;q=0.8"},
	};

	if (argc < 4) {
		fprintf(stderr, "Wrong number of arguments");
		goto exit;
	}

	headers.fields = fields;
	headers.num_fields = sizeof(fields) / sizeof(fields[0]);

	printf("uri = %s\n", argv[3]);
	ret = http->get(argv[3], &headers, &response, &status, NULL);
	if (ret != S_OK) {
		fprintf(stderr, "Failed to get %s (err:%s)\n", argv[3], error_msg(ret));
		goto exit;
	}

	if (response) {
		fprintf(stderr, "HTTP %d - %s\n", status, response);
		free(response);
	}

exit:
	artik_release_api_module(http);
	return (ret == S_OK);
}

static int http_post(int argc, char **argv)
{
	artik_http_module *http = (artik_http_module *)artik_request_api_module("http");
	artik_error ret = S_OK;
	char *response = NULL;
	int status = 0;
	artik_http_headers headers;
	artik_http_header_field fields[] = {
		{"Connect", "close"},
		{"User-Agent", "Artik browser"},
		{"Accept-Language", "en-US,en;q=0.8"},
	};

	if (argc < 5) {
		fprintf(stderr, "Wrong number of arguments");
		goto exit;
	}

	headers.fields = fields;
	headers.num_fields = sizeof(fields) / sizeof(fields[0]);

	ret = http->post(argv[3], &headers, argv[4], &response, &status, NULL);
	if (ret != S_OK) {
		fprintf(stderr, "Failed to get %s (err:%s)\n", argv[3], error_msg(ret));
		goto exit;
	}

	if (response) {
		fprintf(stderr, "HTTP %d - %s\n", status, response);
		free(response);
	}

exit:
	artik_release_api_module(http);
	return (ret == S_OK);
}

static int http_put(int argc, char **argv)
{
	artik_http_module *http = (artik_http_module *)artik_request_api_module("http");
	artik_error ret = S_OK;
	char *response = NULL;
	int status = 0;
	artik_http_headers headers;
	artik_http_header_field fields[] = {
		{"Connect", "close"},
		{"User-Agent", "Artik browser"},
		{"Accept-Language", "en-US,en;q=0.8"},
	};

	if (argc < 4) {
		fprintf(stderr, "Wrong number of arguments");
		goto exit;
	}

	headers.fields = fields;
	headers.num_fields = sizeof(fields) / sizeof(fields[0]);

	ret = http->put(argv[3], &headers, argv[4], &response, &status, NULL);
	if (ret != S_OK) {
		fprintf(stderr, "Failed to get %s (err:%s)\n", argv[3], error_msg(ret));
		goto exit;
	}

	if (response) {
		fprintf(stderr, "HTTP %d - %s\n", status, response);
		free(response);
	}

exit:
	artik_release_api_module(http);
	return (ret == S_OK);
}

static int http_delete(int argc, char **argv)
{
	artik_http_module *http = (artik_http_module *)artik_request_api_module("http");
	artik_error ret = S_OK;
	char *response = NULL;
	int status = 0;
	artik_http_headers headers;
	artik_http_header_field fields[] = {
		{"Connect", "close"},
		{"User-Agent", "Artik browser"},
		{"Accept-Language", "en-US,en;q=0.8"},
	};

	if (argc < 3) {
		fprintf(stderr, "Wrong number of arguments");
		goto exit;
	}

	headers.fields = fields;
	headers.num_fields = sizeof(fields) / sizeof(fields[0]);

	ret = http->del(argv[3], &headers, &response, &status, NULL);
	if (ret != S_OK) {
		fprintf(stderr, "Failed to get %s (err:%s)\n", argv[3], error_msg(ret));
		goto exit;
	}

	if (response) {
		fprintf(stderr, "HTTP %d - %s\n", status, response);
		free(response);
	}

exit:
	artik_release_api_module(http);
	return (ret == S_OK);
}

#ifdef CONFIG_BUILD_KERNEL
int main(int argc, FAR char *argv[])
#else
int http_main(int argc, char *argv[])
#endif
{
	return commands_parser(argc, argv, http_commands);
}
