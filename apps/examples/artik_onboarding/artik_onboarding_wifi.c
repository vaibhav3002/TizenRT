#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <cJSON.h>
#include <errno.h>
#include <wifi/slsi_wifi_api.h>
#include <apps/netutils/mdnsd.h>
#include <apps/netutils/netlib.h>
#include <artik_module.h>
#include <artik_wifi.h>
#include <artik_network.h>

#include "artik_onboarding.h"

#define NET_IFNAME          "wl1"
#define SOFT_AP_IPADDR      "192.168.10.1"
#define SOFT_AP_NETMASK     "255.255.255.0"
#define SOFT_AP_GW          "192.168.10.1"
#define MDNS_SVC_HOSTNAME   "_http._tcp.local"
#define MDNS_SVC_PORT       80
#define MDNS_SVC_TEXT       "path=/v1.0/artikcloud"

#define WIFI_JOIN_TIMEOUT   30  /* seconds */

/*
 * Security modes below must be sorted
 * by ascending order of preference
 */
enum WifiSecurityMode {
    WIFI_SEC_NONE = 0,
    WIFI_SEC_WEP,
    WIFI_SEC_WPA_PSK,
    WIFI_SEC_WPA2_PSK
};

struct WifiConfig wifi_config;

static artik_wifi_mode_t g_mode = ARTIK_WIFI_MODE_NONE;
static artik_network_dhcp_client_handle g_dhcpc_handle = NULL;
static artik_network_dhcp_server_handle g_dhcps_handle = NULL;
static sem_t g_sem_scan;
static sem_t g_sem_join;
static sem_t g_sem_link;
static uint8_t g_join_result = 0;
static char *g_wifi_scan_results = NULL;
static char g_ap_ssid[32] = "";

void WifiResetConfig()
{
    memset(&wifi_config, 0, sizeof(wifi_config));
}

static void wifi_scan_callback(void *result, void *user_data)
{
    artik_error scan_res = *((artik_error *)result);
    artik_wifi_module *wifi = (artik_wifi_module *)user_data;
    artik_error ret = S_OK;
    artik_wifi_ap *list = NULL;
    int count = 0, i= 0;
    cJSON *json = cJSON_CreateObject();
    cJSON *ap_array = cJSON_CreateArray();
    cJSON *prev = NULL;
    cJSON *ap = NULL;

    if (scan_res != S_OK) {
        printf("Scan request failed (err=%d)\n", scan_res);
        goto exit;
    }

    ret = wifi->get_scan_result(&list, &count);
    if (ret != S_OK) {
        printf("Failed to get scan results (err=%d)\n", ret);
        goto exit;
    }

    /* Browse through the results and create JSON objects */
    for (i=0; i<count; i++) {
        ap = cJSON_CreateObject();
        cJSON_AddStringToObject(ap, "ssid", list[i].name);
        cJSON_AddStringToObject(ap, "bssid", list[i].bssid);
        cJSON_AddStringToObject(ap, "security",
                (list[i].encryption_flags & WIFI_ENCRYPTION_OPEN) ? "Open" : "Secure");
        cJSON_AddNumberToObject(ap, "signal", list[i].signal_level);

        if (i == 0) {
            ap_array->child = ap;
        } else {
            prev->next = ap;
            ap->prev = prev;
        }

        prev = ap;
    }

    cJSON_AddItemToObject(json, "accesspoints", ap_array);
    g_wifi_scan_results = cJSON_PrintUnformatted(json);

    cJSON_Delete(json);
    if (count)
        free(list);

exit:
    sem_post(&g_sem_scan);
}

static void wifi_connect_callback(void *result, void *user_data)
{
    artik_wifi_connection_info *info = (artik_wifi_connection_info *)result;

    if (g_mode == ARTIK_WIFI_MODE_AP) {
        if (info->connected) {
            uint8_t numStations = 0;
            printf("New Station connected\n");
            WiFiIsConnected(&numStations, NULL);
            printf("%d station%s connected\n", numStations, numStations > 1 ? "s" : "");
        } else {
            uint8_t numStations = 0;
            printf("Station disconnected from network\n");
            WiFiIsConnected(&numStations, NULL);
            printf("%d station%s connected\n", numStations, numStations > 1 ? "s" : "");
            usleep(1000*1000);
            sem_post(&g_sem_link);
        }
    } else if (g_mode == ARTIK_WIFI_MODE_STATION) {
        if (info->error != S_OK) {
            printf("WiFi connection error: %d\n", info->error);
            g_join_result = 1;
        } else {
            printf("Connected to Access Point\n");
            g_join_result = info->connected ? 0 : -1;
        }
        sem_post(&g_sem_join);
    }
}

char *WifiScanResult(void)
{
    artik_wifi_module *wifi = (artik_wifi_module *)artik_request_api_module("wifi");
    artik_error ret = S_OK;
    struct timespec timeout;

    if (!wifi) {
        printf("Wifi module is not available\n");
        return NULL;
    }

    sem_init(&g_sem_scan,0,0);

    wifi->set_scan_result_callback(wifi_scan_callback, (void *)wifi);
    ret = wifi->scan_request();
    if (ret != S_OK) {
        printf("Failed to launch scan request (err=%d)\n", ret);
        sem_destroy(&g_sem_scan);
        return NULL;
    }

    clock_gettime(CLOCK_REALTIME, &timeout);
    timeout.tv_sec += 20;
    if (sem_timedwait(&g_sem_scan, &timeout) != -1) {
        if (errno == ETIMEDOUT) {
            printf("Timed while scanning for the network\n");
            sem_destroy(&g_sem_scan);
            return NULL;
        }
    }
    sem_destroy(&g_sem_scan);

    wifi->set_scan_result_callback(NULL, NULL);
    artik_release_api_module(wifi);

    return g_wifi_scan_results;
}

int StartDHCPServer(bool start)
{
    artik_error ret = S_OK;
    artik_network_module *network = (artik_network_module *)
            artik_request_api_module("network");

    if (!network) {
        printf("Network module is not available\n");
        return -1;
    }

    if (start) {
        artik_network_dhcp_server_config config;

        if (g_dhcps_handle) {
            printf("DHCP server is already started\n");
            ret = E_BUSY;
            goto exit;
        }

        config.interface = ARTIK_WIFI;
        ret = network->dhcp_server_start(&g_dhcps_handle, &config);
        if (ret != S_OK) {
            struct in_addr ipaddr;
            ipaddr.s_addr = 0;
            netlib_set_ipv4addr(NET_IFNAME, &ipaddr);
            printf("Failed to start DHCP server\n");
            goto exit;
        }
    } else {
        ret = network->dhcp_server_stop(g_dhcps_handle);
        g_dhcps_handle = NULL;
    }

exit:
    artik_release_api_module(network);
    return (ret == S_OK) ? 0 : -1;
}

int StartDHCPClient(bool start)
{
    artik_error ret = S_OK;
    artik_network_module *network = (artik_network_module *)
            artik_request_api_module("network");

    if (!network) {
        printf("Network module is not available\n");
        return -1;
    }

    if (start) {
        if (g_dhcpc_handle) {
            printf("DHCP client is already started\n");
            ret = E_BUSY;
            goto exit;
        }

        ret = network->dhcp_client_start(&g_dhcpc_handle, ARTIK_WIFI);
        if (ret != S_OK) {
            printf("Failed to start DHCP client (%d)\n", ret);
            g_dhcpc_handle = NULL;
            goto exit;
        }

    } else {
        if (!g_dhcpc_handle) {
            printf("DHCP client is not started\n");
            ret = E_NOT_INITIALIZED;
            goto exit;
        }

        ret = network->dhcp_client_stop(g_dhcpc_handle);
        g_dhcpc_handle = NULL;
    }

exit:
    artik_release_api_module(network);
    return (ret == S_OK) ? 0 : -1;
}

artik_error StartSoftAP(bool start)
{
    artik_error ret = S_OK;
    artik_wifi_module *wifi = (artik_wifi_module *)artik_request_api_module("wifi");

    if (!wifi) {
        printf("Wifi module is not available\n");
        return -1;
    }

    if (start) {
        uint8_t mac_addr[6];

        /* First stop Station mode if set */
        if (g_mode == ARTIK_WIFI_MODE_STATION)
            StartStationConnection(false);

        ret = wifi->init(ARTIK_WIFI_MODE_AP);
        if (ret != S_OK) {
            printf("Failed to initialize WiFi (%d)\n", ret);
            goto exit;
        }

        if (WiFiGetMac(mac_addr) != SLSI_STATUS_SUCCESS) {
            wifi->deinit();
            printf("Failed to get MAC address\n");
            ret = E_WIFI_ERROR;
            goto exit;
        }

        /* Build SSID based on MAC address */
        snprintf(g_ap_ssid, 32, "ARTIK_%02x%02x%02x%02x%02x%02x", mac_addr[0], mac_addr[1],
                mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);

        printf("Starting AP %s\n", g_ap_ssid);

        ret = wifi->start_ap(g_ap_ssid, NULL, 2);
        if (ret != S_OK) {
            wifi->deinit();
            printf("Failed to start Access Point (%d)\n", ret);
            goto exit;
        }

        wifi->set_connect_callback(wifi_connect_callback, (void *)wifi);

        /* Start the DHCP server */
        if (StartDHCPServer(true)) {
            printf("Failed to start DHCP server\n");
            ret = E_WIFI_ERROR;
            goto exit;
        }

        g_mode = ARTIK_WIFI_MODE_AP;
    } else {

        /* Stop the DHCP server */
        if (StartDHCPServer(false)) {
            printf("Failed to stop DHCP server\n");
            ret = E_WIFI_ERROR;
            goto exit;
        }

        wifi->set_connect_callback(NULL, NULL);
        ret = wifi->deinit();
        if (ret != S_OK) {
            printf("Failed to stop WiFi (%d)\n", ret);
            goto exit;
        }

        g_mode = ARTIK_WIFI_MODE_NONE;
    }

exit:
    artik_release_api_module(wifi);
    return ret;
}

static void wifi_find_ap_callback(void *result, void *user_data)
{
    artik_error scan_res = *((artik_error *)result);
    artik_wifi_module *wifi = (artik_wifi_module *)user_data;
    artik_error ret = S_OK;
    artik_wifi_ap *list = NULL;
    int count = 0, i= 0;

    if (scan_res != S_OK) {
        printf("Scan request failed (err=%d)\n", scan_res);
        g_join_result = -1;
        goto error;
    }

    ret = wifi->get_scan_result(&list, &count);
    if (ret != S_OK) {
        printf("Failed to get scan results (err=%d)\n", ret);
        g_join_result = -1;
        goto error;
    }

    if (!count) {
        printf("No Access Points found\n");
        g_join_result = -1;
        goto error;
    }

    /* Browse through the results and look for configured AP */
    for (i=0; i<count; i++) {
        if (!strncmp(list[i].name, wifi_config.ssid, strlen(wifi_config.ssid))) {

            /* Check if AP is secure as expected */
            if (!(list[i].encryption_flags & WIFI_ENCRYPTION_OPEN) && !wifi_config.secure) {
                printf("AP connection is secure but no passphrase was provided\n");
                free(list);
                g_join_result = -1;
                goto error;
            }

            /* Join the Access Point */
            wifi->connect(wifi_config.ssid, wifi_config.secure ? wifi_config.passphrase : NULL,
                    true);
            if (ret != S_OK) {
                printf("Failed to start connection to AP (err=%d)\n", ret);
                free(list);
                g_join_result = -1;
                goto error;
            }

            break;
        }
    }

    free(list);

    return;

 error:
     sem_post(&g_sem_join);
}

artik_error StartStationConnection(bool start)
{
    artik_error ret = S_OK;
    artik_wifi_module *wifi = (artik_wifi_module *)artik_request_api_module("wifi");

    if (!wifi) {
        printf("Wifi module is not available\n");
        return E_NOT_SUPPORTED;
    }

    if (start) {
        struct timespec timeout;

        if (g_mode == ARTIK_WIFI_MODE_STATION) {
            ret = E_BUSY;
            goto exit;
        } else if (g_mode == ARTIK_WIFI_MODE_AP) {
            /* First stop AP mode if set */
            StartSoftAP(false);
        }

        /* Start station mode */
        ret = wifi->init(ARTIK_WIFI_MODE_STATION);
        if (ret != S_OK) {
            printf("Failed to start Wifi station mode (ret=%d)\n", ret);
            goto exit;
        }

        g_mode = ARTIK_WIFI_MODE_STATION;

        /*
         * Scan for configured access point. If found, launch
         * the connection from the callback.
         */
        sem_init(&g_sem_join, 0, 0);
        g_join_result = 0;
        wifi->set_scan_result_callback(wifi_find_ap_callback, (void *)wifi);
        wifi->set_connect_callback(wifi_connect_callback, (void *)wifi);

        ret = wifi->scan_request();
        if (ret != S_OK) {
            printf("Failed to scan access points (%d)\n", ret);
            goto exit;
        }

        /*
         * Wait for a limited amount of time, consider
         * failure if timeout is reached
         */
        clock_gettime(CLOCK_REALTIME, &timeout);
        timeout.tv_sec += WIFI_JOIN_TIMEOUT;
        if (sem_timedwait(&g_sem_join, &timeout) == -1) {
            sem_destroy(&g_sem_join);
            printf("Failure while waiting to join the network\n");
            ret = E_WIFI_ERROR;
            goto exit;
        }

        sem_destroy(&g_sem_join);

        if (g_join_result) {
            printf("Failed to join network (%d)\n", g_join_result);
            ret = E_WIFI_ERROR_ASSOCIATION;
            goto exit;
        }

        usleep(500 * 1000);

        /* Start DHCP client to get an IP address */
        if (StartDHCPClient(true)) {
            ret = E_BUSY;
            goto exit;
        }
    } else {

        if (g_mode != ARTIK_WIFI_MODE_STATION) {
            ret = E_NOT_INITIALIZED;
            goto exit;
        }

        wifi->set_scan_result_callback(NULL, NULL);
        wifi->set_connect_callback(NULL, NULL);
        wifi->disconnect();

        ret = wifi->deinit();
        if (ret != S_OK) {
            printf("Failed to leave WiFi network (%d)\n", ret);
            goto exit;
        }

        StartDHCPClient(false);

        g_mode = SLSI_WIFI_NONE;
    }

exit:
    artik_release_api_module(wifi);
    return ret;
}

void StartMDNSService(bool start)
{
    if (start) {
        char mdns_hostname[64];

        if (strlen(g_ap_ssid)) {
            snprintf(mdns_hostname, sizeof(mdns_hostname) - 1, "%s.local", g_ap_ssid);
        } else {
            strncpy(mdns_hostname, "artik.local", sizeof(mdns_hostname) - 1);
        }

        if (mdnsd_start(mdns_hostname, "wl1")) {
            printf("Failed to start mDNS service\n");
        } else {
            const char *txt[] = {MDNS_SVC_TEXT, NULL};
            mdnsd_register_service(g_ap_ssid, MDNS_SVC_HOSTNAME, MDNS_SVC_PORT, NULL, txt);
        }
    } else {
        mdnsd_stop();
    }
}
