#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <artik_module.h>
#include <artik_error.h>
#include <artik_cloud.h>
#include <artik_gpio.h>
#include <artik_platform.h>
#include <artik_security.h>
#include <cJSON.h>

#include "artik_onboarding.h"

#define AKC_DEFAULT_DTID    "dt2d93bdb9c8fa446eb4a35544e66150f7"
#define LED_ID              ARTIK_A053_XGPIO20

#define RESP_OK_TPL         "{\"error\":false,\"reason\":\"none\"}"
#define RESP_ERROR_TPL      "{\"error\":true,\"reason\":\"%s\"}"
#define RESP_INVALID_TPL    "{\"error\":true,\"reason\":\"Invalid response from the cloud\"}"
#define RESP_UNAVAIL_TPL    "{\"error\":true,\"reason\":\"Cloud module is not available\"}"
#define RESP_CERT_SN_TPL    "{\"error\":true,\"reason\":\"Serial number could not be found\"}"
#define RESP_PIN_TPL        "{\"error\":false,\"reason\":\"none\",\"pin\":\"%s\"}"

struct ArtikCloudConfig cloud_config;

static artik_websocket_handle g_ws_handle = NULL;

void CloudResetConfig(void)
{
    memset(&cloud_config, 0, sizeof(cloud_config));
    strncpy(cloud_config.device_id, "null", AKC_DID_LEN);
    strncpy(cloud_config.device_token, "null", AKC_TOKEN_LEN);
    strncpy(cloud_config.device_type_id, AKC_DEFAULT_DTID, AKC_DTID_LEN);
}

static void set_led_state(bool state)
{
    artik_gpio_config config;
    artik_gpio_handle handle;
    artik_gpio_module *gpio = (artik_gpio_module *)artik_request_api_module("gpio");
    char name[16] = "";

    if (!gpio)
        return;

    memset(&config, 0, sizeof(config));
    config.dir = GPIO_OUT;
    config.id = LED_ID;
    snprintf(name, 16, "gpio%d", config.id);
    config.name = name;

    if (gpio->request(&handle, &config) != S_OK)
        return;

    gpio->write(handle, state ? 1 : 0);
    gpio->release(handle);

    artik_release_api_module(gpio);
}

static void cloud_websocket_rx_cb(void *user_data, void *result)
{
    cJSON *msg, *error, *code, *type, *data, *actions, *action;

    if (!result)
        return;

    /* Parse JSON and look for actions */
    msg = cJSON_Parse((const char *)result);
    if (!msg)
        goto exit;

    /* Check if error */
    error = cJSON_GetObjectItem(msg, "error");
    if (error && (error->type == cJSON_Object)) {
        code = cJSON_GetObjectItem(error, "code");
        if (code && (code->type == cJSON_Number)) {
            data = cJSON_GetObjectItem(error, "message");
            if (data && (data->type == cJSON_String)) {
                printf("Websocket error %d - %s\n", code->valueint,
                        data->valuestring);
                if (code->valueint == 404) {
                    /*
                     * Device no longer exists, going back
                     * to onboarding service.
                     */
                    ResetConfiguration();
                    StartCloudWebsocket(false);
                }
            }
        }

        goto error;
    }

    type = cJSON_GetObjectItem(msg, "type");
    if (!type || (type->type != cJSON_String))
        goto error;

    if (strncmp(type->valuestring, "action", strlen("action")))
        goto error;

    data = cJSON_GetObjectItem(msg, "data");
    if (!data || (data->type != cJSON_Object))
        goto error;

    actions = cJSON_GetObjectItem(data, "actions");
    if (!actions || (actions->type != cJSON_Array))
        goto error;

    /* Browse through actions */
    for (action = (actions)->child; action != NULL; action = action->next) {
        cJSON *name;

        if (action->type != cJSON_Object)
            continue;

        name = cJSON_GetObjectItem(action, "name");

        if (!name || (name->type != cJSON_String))
            continue;

        if(!strncmp(name->valuestring, "setOn", strlen("setOn"))) {
            fprintf(stderr, "CLOUD ACTION: SetOn\n");
            SendMessageToCloud("{\"state\":true}");
            set_led_state(true);
        } else if(!strncmp(name->valuestring, "setOff", strlen("setOff"))) {
            fprintf(stderr, "CLOUD ACTION: SetOff\n");
            SendMessageToCloud("{\"state\":false}");
            set_led_state(false);
        }
    }

error:
    cJSON_Delete(msg);
exit:
    free(result);
}

static pthread_addr_t websocket_start_cb(void *arg)
{
    artik_cloud_module *cloud = (artik_cloud_module*)artik_request_api_module("cloud");
    artik_error ret = S_OK;

    if (!cloud) {
        printf("Cloud module is not available\n");
        return NULL;
    }

    ret = cloud->websocket_open_stream(&g_ws_handle, cloud_config.device_token,
            cloud_config.device_id, true);
    if (ret != S_OK) {
        printf("Failed to open websocket to cloud (err=%d)\n", ret);
        goto exit;
    }

    cloud->websocket_set_receive_callback(g_ws_handle, cloud_websocket_rx_cb, NULL);

exit:
    artik_release_api_module(cloud);
    return NULL;
}

static pthread_addr_t websocket_stop_cb(void *arg)
{
    int ret = 0;
    artik_cloud_module *cloud = (artik_cloud_module*)artik_request_api_module("cloud");

    if (!cloud) {
        printf("Cloud module is not available\n");
        ret = E_NOT_SUPPORTED;
        goto exit;
    }

    cloud->websocket_set_receive_callback(g_ws_handle, NULL, NULL);

    ret = cloud->websocket_close_stream(g_ws_handle);
    if (ret != S_OK) {
        artik_release_api_module(cloud);
        printf("Failed to close websocket to cloud (err=%d)\n", ret);
        goto exit;
    }

    g_ws_handle = NULL;
    artik_release_api_module(cloud);

 exit:
    return NULL;
}

artik_error StartCloudWebsocket(bool start)
{
    artik_error ret = S_OK;

    if (start) {
        static pthread_t tid;
        pthread_attr_t attr;
        int status;
        struct sched_param sparam;

        if (g_ws_handle) {
            printf("Websocket is already open, close it first\n");
            goto exit;
        }

        pthread_attr_init(&attr);
        sparam.sched_priority = 100;
        status = pthread_attr_setschedparam(&attr, &sparam);
        status = pthread_attr_setschedpolicy(&attr, SCHED_RR);
        status = pthread_attr_setstacksize(&attr, 1024 * 8);
        status = pthread_create(&tid, &attr, websocket_start_cb, NULL);
        if (status) {
            printf("Failed to create thread for websocket\n");
            goto exit;
        }

        pthread_setname_np(tid, "cloud-websocket");
        pthread_join(tid, NULL);
    } else {
        static pthread_t tid;
        pthread_attr_t attr;
        int status;
        struct sched_param sparam;

        if (!g_ws_handle) {
            printf("Websocket is not open, cannot close it\n");
            goto exit;
        }

        pthread_attr_init(&attr);
        sparam.sched_priority = 100;
        status = pthread_attr_setschedparam(&attr, &sparam);
        status = pthread_attr_setschedpolicy(&attr, SCHED_RR);
        status = pthread_attr_setstacksize(&attr, 1024 * 8);
        status = pthread_create(&tid, &attr, websocket_stop_cb, NULL);
        if (status) {
            printf("Failed to create thread for closing websocket\n");
            goto exit;
        }
    }

exit:
    return ret;
}

artik_error SendMessageToCloud(char *message)
{
    artik_error ret = S_OK;
    artik_cloud_module *cloud = (artik_cloud_module*)artik_request_api_module("cloud");

    if (!cloud) {
        printf("Cloud module is not available\n");
        return E_NOT_SUPPORTED;
    }

    if (!g_ws_handle) {
        printf("Websocket is not open, cannot send message\n");
        goto exit;
    }

    ret = cloud->websocket_send_message(g_ws_handle, message);
    if (ret != S_OK) {
        printf("Failed to send message to cloud (err=%d)\n", ret);
        goto exit;
    }

exit:
    artik_release_api_module(cloud);
    return ret;
}

static pthread_addr_t get_device_cb(void *arg)
{
    artik_error ret = S_OK;
    char *response = NULL;
    int res = 0;
    char url[128];
    char bearer[128];
    artik_http_module *http = (artik_http_module *)artik_request_api_module("http");
    artik_ssl_config ssl_config;
    artik_http_headers headers;
    artik_http_header_field fields[] = {
        {"Authorization", NULL},
        {"Content-Type", "application/json"},
    };

    if (!http) {
        printf("HTTP module is not available\n");
        return NULL;
    }

    headers.fields = fields;
    headers.num_fields = sizeof(fields) / sizeof(fields[0]);

    /* Build up authorization header */
    snprintf(bearer, 128, "Bearer %s", cloud_config.device_token);
    fields[0].data = bearer;

    /* Build up url with parameters */
    snprintf(url, 256, "https://s-api.artik.cloud/v1.1/devices/%s?properties=false",
            cloud_config.device_id);

    /* Prepare the SSL configuration */
    memset(&ssl_config, 0, sizeof(ssl_config));
    ssl_config.use_se = true;

    ret = http->get(url, &headers, &response, NULL, &ssl_config);
    if (ret != S_OK) {
        printf("Failed to call get_device secure Cloud API (err=%d)\n", ret);
        goto exit;
    }

    if (response) {
        cJSON *resp, *data;

        /* Consider the device is valid only if "data" JSON object is returned */
        resp = cJSON_Parse(response);
        if (resp) {
            data = cJSON_GetObjectItem(resp, "data");
            if (data && (data->type == cJSON_Object))
                res = 1;
        }

        free(response);
    } else {
        printf("Get device: failed to get response (%d)\n", res);
    }

exit:
    artik_release_api_module(http);
    pthread_exit((void *)res);

    return NULL;
}

bool ValidateCloudDevice(void)
{
    int res = 0;
    static pthread_t tid;
    pthread_attr_t attr;
    int status;
    struct sched_param sparam;

    pthread_attr_init(&attr);
    sparam.sched_priority = 100;
    status = pthread_attr_setschedparam(&attr, &sparam);
    status = pthread_attr_setschedpolicy(&attr, SCHED_RR);
    status = pthread_attr_setstacksize(&attr, 1024 * 8);
    status = pthread_create(&tid, &attr, get_device_cb, NULL);
    if (status) {
        printf("Failed to create thread for validating cloud device\n");
        goto exit;
    }

    pthread_join(tid, (void**)&res);

exit:
    return (res != 0);
}

static pthread_addr_t start_sdr_registration_cb(void *arg)
{
    int status = 200;
    char **resp = (char**)arg;
    char *response = NULL;
    cJSON *jresp = NULL;
    unsigned char serial_num[ARTIK_CERT_SN_MAXLEN];
    unsigned int sn_len = ARTIK_CERT_SN_MAXLEN;
    char vdid[AKC_VDID_LEN + 1] = "";
    int i = 0;
    artik_cloud_module *cloud = (artik_cloud_module *)artik_request_api_module("cloud");
    artik_security_module *security = (artik_security_module *)artik_request_api_module("security");
    artik_security_handle handle;
    artik_error ret = S_OK;

    if (!cloud || !security) {
        status = 500;
        *resp = strdup(RESP_UNAVAIL_TPL);
        goto exit;
    }

    /* Generate the vendor ID based on the certificate serial number */
    security->request(&handle);
    ret = security->get_certificate_sn(handle, serial_num, &sn_len);
    if ((ret != S_OK) || !sn_len) {
        strcpy(vdid, "vdid_default1234");
    } else {
        for (i=0; i<sn_len; i++) {
            char tmp[3];
            snprintf(tmp, 3, "%02x", serial_num[i]);
            tmp[2] = '\0';
            strncat(vdid, tmp, 2);
        }
    }

    security->release(handle);

    /* Start SDR registration to ARTIK Cloud */
    ret = cloud->sdr_start_registration(cloud_config.device_type_id, vdid, &response);
    if (ret != S_OK) {
        if (response) {
            cJSON *error, *message;

            /* We got an error from the cloud, parse the JSON to extract the error message */
            jresp = cJSON_Parse(response);
            if (jresp) {
                error = cJSON_GetObjectItem(jresp, "error");
                if (error && (error->type == cJSON_Object)) {
                    message = cJSON_GetObjectItem(error, "message");
                    if (message && (message->type == cJSON_String)) {
                        int len = strlen(RESP_ERROR_TPL) + strlen(message->valuestring);
                        *resp = zalloc(len + 1);
                        if (*resp == NULL)
                            goto exit;
                        snprintf(*resp, len, RESP_ERROR_TPL, message->valuestring);
                        goto exit;
                    }
                }
            }
        }
    } else {
        if (response) {
            cJSON *data, *rid, *pin, *nonce;

            /* It's all good. Parse PIN, registration ID and nonce for later use */
            jresp = cJSON_Parse(response);
            if (jresp) {
                data = cJSON_GetObjectItem(jresp, "data");
                if (data) {
                    rid = cJSON_GetObjectItem(data, "rid");
                    if (rid && (rid->type == cJSON_String)) {
                        strncpy(cloud_config.reg_id, rid->valuestring, AKC_REG_ID_LEN);
                        nonce = cJSON_GetObjectItem(data, "nonce");
                        if (nonce && (nonce->type == cJSON_String)) {
                            strncpy(cloud_config.reg_nonce, nonce->valuestring, AKC_REG_ID_LEN);
                            pin = cJSON_GetObjectItem(data, "pin");
                            if (pin && (pin->type == cJSON_String)) {
                                int len = strlen(RESP_PIN_TPL) + strlen(pin->valuestring);
                                *resp = zalloc(len + 1);
                                if (*resp == NULL)
                                    goto exit;
                                snprintf(*resp, len, RESP_PIN_TPL, pin->valuestring);
                                goto exit;
                            }
                        }
                    }
                }
            }
        }
    }

    /* Fallback here if we could not parse everything we were looking for */
    status = 500;
    *resp = strdup(RESP_INVALID_TPL);

 exit:

    pthread_exit((void *)status);

    if (jresp)
        cJSON_Delete(jresp);

    if (response)
        free(response);

    if (cloud)
        artik_release_api_module(cloud);

    return NULL;
}

int StartSDRRegistration(char **resp)
{
    int status = 0;
    static pthread_t tid;
    pthread_attr_t attr;
    struct sched_param sparam;

    pthread_attr_init(&attr);
    sparam.sched_priority = 100;
    status = pthread_attr_setschedparam(&attr, &sparam);
    status = pthread_attr_setschedpolicy(&attr, SCHED_RR);
    status = pthread_attr_setstacksize(&attr, 1024 * 8);
    status = pthread_create(&tid, &attr, start_sdr_registration_cb, (void *)resp);
    if (status)
        goto exit;

    pthread_join(tid, (void**)&status);

exit:
    return status;
}

static pthread_addr_t complete_sdr_registration_cb(void *arg)
{
    int status = 200;
    char **resp = (char**)arg;
    char *response = NULL;
    cJSON *jresp = NULL;
    artik_cloud_module *cloud = (artik_cloud_module *)artik_request_api_module("cloud");
    artik_error ret = S_OK;

    if (!cloud) {
        status = 500;
        *resp = strdup(RESP_UNAVAIL_TPL);
        goto exit;
    }

    /* Start SDR registration to ARTIK Cloud */
    ret = cloud->sdr_complete_registration(cloud_config.reg_id, cloud_config.reg_nonce, &response);
    if (ret != S_OK) {
        if (response) {
            cJSON *error, *message;

            /* We got an error from the cloud, parse the JSON to extract the error message */
            jresp = cJSON_Parse(response);
            if (jresp) {
                error = cJSON_GetObjectItem(jresp, "error");
                if (error && (error->type == cJSON_Object)) {
                    message = cJSON_GetObjectItem(error, "message");
                    if (message && (message->type == cJSON_String)) {
                        int len = strlen(RESP_ERROR_TPL) + strlen(message->valuestring);
                        status = 400;
                        *resp = zalloc(len + 1);
                        if (*resp == NULL)
                            goto exit;
                        snprintf(*resp, len, RESP_ERROR_TPL, message->valuestring);
                        goto exit;
                    }
                }
            }
        }
    } else {
        if (response) {
            cJSON *data, *token, *did;

            /* It's all good. Parse Device token and ID for later use */
            jresp = cJSON_Parse(response);
            if (jresp) {
                data = cJSON_GetObjectItem(jresp, "data");
                if (data) {
                    token = cJSON_GetObjectItem(data, "accessToken");
                    if (token && (token->type == cJSON_String)) {
                        strncpy(cloud_config.device_token, token->valuestring, AKC_TOKEN_LEN);
                        did = cJSON_GetObjectItem(data, "did");
                        if (did && (did->type == cJSON_String)) {
                            strncpy(cloud_config.device_id, did->valuestring, AKC_DID_LEN);
                            *resp = strdup(RESP_OK_TPL);
                            goto exit;
                        }
                    }
                }
            }
        }
    }

    /* Fallback here if we could not parse everything we were looking for */
    status = 500;
    *resp = strdup(RESP_INVALID_TPL);

 exit:

    pthread_exit((void *)status);

    if (jresp)
        cJSON_Delete(jresp);

    if (response)
        free(response);

    if (cloud)
        artik_release_api_module(cloud);

    return NULL;
}

int CompleteSDRRegistration(char **resp)
{
    int status = 0;
    static pthread_t tid;
    pthread_attr_t attr;
    struct sched_param sparam;

    pthread_attr_init(&attr);
    sparam.sched_priority = 100;
    status = pthread_attr_setschedparam(&attr, &sparam);
    status = pthread_attr_setschedpolicy(&attr, SCHED_RR);
    status = pthread_attr_setstacksize(&attr, 1024 * 8);
    status = pthread_create(&tid, &attr, complete_sdr_registration_cb, (void *)resp);
    if (status)
        goto exit;

    pthread_join(tid, (void**)&status);

exit:
    return status;
}


