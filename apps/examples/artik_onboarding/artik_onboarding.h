#ifndef _ARTIK_ONBOARDING_H_
#define _ARTIK_ONBOARDING_H_

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

#endif /* _ARTIK_ONBOARDING_H_ */
