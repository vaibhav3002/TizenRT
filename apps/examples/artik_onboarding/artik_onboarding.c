#include <stdio.h>
#include <sys/stat.h>
#include <tinyara/config.h>
#include <tinyara/fs/fs_utils.h>
#include <shell/tash.h>
#include <artik_error.h>
#include <artik_cloud.h>
#include <artik_wifi.h>
#include <errno.h>

#include "artik_onboarding.h"

static int onboard_main(int argc, char *argv[])
{
    int ret = 0;

    if ((argc > 1) && !strcmp(argv[1], "reset")) {
        ResetConfiguration();
        printf("Onboarding configuration was reset. Reboot the board"
                " to return to onboarding mode\n");
        goto exit;
    }

    if (InitConfiguration() != S_OK) {
        ret = -1;
        goto exit;
    }

    /* If we already have Wifi credentials, try to connect to the hotspot */
    if (strlen(wifi_config.ssid) > 0) {
        if (StartStationConnection(true) == S_OK) {
            /* Check if we have valid ARTIK Cloud credentials */
            if ((strlen(cloud_config.device_id) == AKC_DID_LEN) &&
                (strlen(cloud_config.device_token) == AKC_TOKEN_LEN)) {
                /* Validate if device ID/token pair is valid */
                if (ValidateCloudDevice()) {
                    if (StartCloudWebsocket(true) == S_OK) {
                        printf("ARTIK Cloud connection started\n");
                        goto exit;
                    }
                } else {
                    printf("Device not recognized by ARTIK Cloud, switching back to onboarding mode\n");
                }
            } else {
                printf("Invalid ARTIK Cloud credentials, switching back to onboarding mode\n");
            }
        } else {
            printf("Could not connect to access point, switching back to onboarding mode\n");
        }
    }

    /* If Cloud connection failed, start the onboarding service */
    if (StartSoftAP(true) != S_OK) {
        ret = -1;
        goto exit;
    }

    if (StartWebServer(true, API_SET_WIFI) != S_OK) {
        StartSoftAP(false);
        ret = -1;
        goto exit;
    }

    printf("ARTIK Onboarding Service started\n");

exit:
    return ret;
}

extern int module_main(int argc, char *argv[]);
extern int cloud_main(int argc, char *argv[]);
extern int gpio_main(int argc, char *argv[]);
extern int pwm_main(int argc, char *argv[]);
extern int adc_main(int argc, char *argv[]);
extern int http_main(int argc, char *argv[]);
extern int wifi_main(int argc, char *argv[]);
extern int ws_main(int argc, char *argv[]);
extern int see_main(int argc, char *argv[]);
extern int ntpclient_main(int argc, char *argv[]);

static tash_cmdlist_t atk_cmds[] = {
    {"sdk", module_main, TASH_EXECMD_SYNC},
    {"gpio", gpio_main, TASH_EXECMD_SYNC},
    {"pwm", pwm_main, TASH_EXECMD_SYNC},
    {"adc", adc_main, TASH_EXECMD_SYNC},
    {"cloud", cloud_main, TASH_EXECMD_SYNC},
    {"http", http_main, TASH_EXECMD_SYNC},
    {"wifi", wifi_main, TASH_EXECMD_SYNC},
    {"websocket", ws_main, TASH_EXECMD_SYNC},
    {"see", see_main, TASH_EXECMD_SYNC},
    {"onboard", onboard_main, TASH_EXECMD_SYNC},
    {"ntp", ntpclient_main, TASH_EXECMD_SYNC},
    {NULL, NULL, 0}
};

#ifdef CONFIG_BUILD_KERNEL
int main(int argc, FAR char *argv[])
#else
int artik_onboarding_main(int argc, char *argv[])
#endif
{
#ifdef CONFIG_TASH
    /* add tash command */
    tash_cmdlist_install(atk_cmds);
#endif

    //onboard_main(argc, argv);

    return 0;
}
