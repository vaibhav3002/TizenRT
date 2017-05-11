#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <artik_error.h>

#include "artik_onboarding.h"

static char config_file[16] = "/mnt/config";

static void PrintConfig(void)
{
    printf("Wifi:\n");
    printf("\tssid: %s\n", wifi_config.ssid);
    printf("\tpassphrase: %s\n", wifi_config.passphrase);
    printf("\tsecure: %s\n", wifi_config.secure ? "true" : "false");

    printf("Cloud:\n");
    printf("\tdevice_id: %s\n", cloud_config.device_id);
    printf("\tdevice_token: %s\n", cloud_config.device_token);
    printf("\tdevice_type_id: %s\n", cloud_config.device_type_id);
}

artik_error InitConfiguration(void)
{
    struct stat st;
    int fd = 0;
    artik_error err = S_OK;

    stat(config_file, &st);

    if (st.st_size != (sizeof(wifi_config) + sizeof(cloud_config))) {
        printf("Invalid configuration, creating default one\n");

        err = ResetConfiguration();
    } else {

        fd = open(config_file, O_RDONLY);
        if (fd == -1) {
            printf("Unable to open configuration file (errno=%d)\n", errno);
            err = E_ACCESS_DENIED;
            goto exit;
        }

        lseek(fd, 0, SEEK_SET);
        read(fd, (void *)&wifi_config, sizeof(wifi_config));
        read(fd, (void *)&cloud_config, sizeof(cloud_config));

        close(fd);
    }

exit:
    //PrintConfig();

    return err;
}

artik_error SaveConfiguration(void)
{
    int ret = 0;
    int fd = 0;

    fd = open(config_file, O_WRONLY|O_CREAT);
    if (fd == -1) {
        printf("Unable to open configuration file (errno=%d)\n", errno);
        return E_ACCESS_DENIED;
    }

    lseek(fd, 0, SEEK_SET);
    ret = write(fd, (const void *)&wifi_config, sizeof(wifi_config));
    if (ret != sizeof(wifi_config))
        printf("Failed to write wifi config (%d - errno=%d)\n", ret, errno);
    ret = write(fd, (const void *)&cloud_config, sizeof(cloud_config));
    if (ret != sizeof(cloud_config))
        printf("Failed to write wifi config (%d - errno=%d)\n", ret, errno);

    close(fd);

    return S_OK;
}

artik_error ResetConfiguration(void)
{
    WifiResetConfig();
    CloudResetConfig();

    return SaveConfiguration();
}
