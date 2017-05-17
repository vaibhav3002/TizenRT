#ifndef _ARTIK_ONBOARDING_H_
#define _ARTIK_ONBOARDING_H_

/*
 * Service states
 */
enum ServiceState {
    STATE_IDLE,
    STATE_ONBOARDING,
    STATE_CONNECTED
};

extern enum ServiceState current_service_state;

/*
 * WiFi related exports
 */
#define SSID_MAX_LEN            64
#define PASSPHRASE_MAX_LEN      128

struct WifiConfig {
    char ssid[SSID_MAX_LEN];
    char passphrase[PASSPHRASE_MAX_LEN];
    bool secure;
};

extern struct WifiConfig wifi_config;

char *WifiScanResult(void);
void WifiResetConfig(void);
artik_error StartSoftAP(bool start);
artik_error StartStationConnection(bool start);
void StartMDNSService(bool start);

/*
 * ARTIK Cloud related exports
 */
#define AKC_DID_LEN         32
#define AKC_TOKEN_LEN       32
#define AKC_DTID_LEN        34
#define AKC_REG_ID_LEN      32
#define AKC_REG_NONCE_LEN   32
#define AKC_VDID_LEN        32

struct ArtikCloudConfig {
    char device_id[AKC_DID_LEN + 1];
    char device_token[AKC_TOKEN_LEN + 1];
    char device_type_id[AKC_DTID_LEN + 1];
    char reg_id[AKC_REG_ID_LEN + 1];
    char reg_nonce[AKC_REG_NONCE_LEN + 1];
};

extern struct ArtikCloudConfig cloud_config;

void CloudResetConfig(void);
artik_error StartCloudWebsocket(bool start);
artik_error SendMessageToCloud(char *message);
bool ValidateCloudDevice(void);
int StartSDRRegistration(char **resp);
int CompleteSDRRegistration(char **resp);

/*
 * Web server related exports
 */
enum ApiSet {
    API_SET_WIFI = (1 << 0),
    API_SET_CLOUD = (1 << 1)
};

artik_error StartWebServer(bool, enum ApiSet);

/*
 * Configuration Web server related exports
 */
artik_error InitConfiguration(void);
artik_error SaveConfiguration(void);
artik_error ResetConfiguration(void);

#define API_ERROR_OK              "0"
#define API_ERROR_INVALID_JSON    "100"
#define API_ERROR_INTERNAL        "102"
#define API_ERROR_INVALID_PARAMS  "106"
#define API_ERROR_INVALID_UUID    "108"
#define API_ERROR_MISSING_PARAM   "109"
#define API_ERROR_COMMUNICATION   "111"
#define API_ERROR_CLOUD_BASE      50000

#define RESP_ERROR_OK             "{\"error\":false," \
                                   "\"error_code\":0," \
                                   "\"reason\":\"none\"}"

#define RESP_ERROR(code,reason)     "{\"error\":true," \
                                     "\"error_code\":"code"," \
                                     "\"reason\":\""reason"\"}"

#define RESP_ERROR_EXTRA(code, reason, extra) "{\"error\":false," \
                                               "\"error_code\":"code"," \
                                               "\"reason\":\""reason"\"," \
                                               extra"}"

#endif /* _ARTIK_ONBOARDING_H_ */
