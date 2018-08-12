#define LOG_TAG "AAL-Test"

#define MTK_LOG_ENABLE 1
#include <cutils/log.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include "AALClient.h"


using namespace android;


static const int PREDEFINED_CURVE1[] = {
    0, 1, 10, 100, 1000, 10000, 20000,
    8, 8, 12,  40, 200,   255,   255
};
static const int PREDEFINED_CURVE2[] = {
    0, 16, 32, 50, 100, 140, 180, 240, 300, 600, 1000, 2000, 3000, 4000, 8000, 10000,
    30, 40, 50, 60, 70, 80, 102, 102, 102, 102, 102, 180, 200, 210, 230, 255
};



static void print_usages()
{
    printf(
        "aal-test [parameter] (value)\n"
        "\n"
        "Parameters:\n"
        "    func    2:ESS + 4:DRE\n"
        "    SBS     0 ~ 255 (Smart backlight strength)\n"
        "    SBR     0 ~ 255 (Smart backlight range)\n"
        "    RD      0 ~ 255 (Readability level)\n"
        "    curve   Show curve\n"
        "    setcurve (1/2)   Set curve 1/2\n"
        "    BS      Brighten ramp rate\n"
        "    DS      Darken ramp rate\n"
        "\n"
        "Examples:"
        "    # aal-test func 6    -> Enable DRE + ESS\n"
        "    # aal-test BR 190    -> Change brightness level to 190\n"
        );
}


int main(int argc, char *argv[])
{
    const char *param = "";
    int32_t value = 0;
    int32_t ret = 0;

    if (argc == 1) {
        print_usages();
        return 0;
    } else if (argc == 2) {
    } else if (argc == 3) {
        value = (int32_t)strtol(argv[2], NULL, 0);
        printf("param = %s, value = %d\n", argv[1], value);
    } else {
        printf("Invalid arguments\n");
    }

    ALOGI("AAL client start");
    AALClient &client(AALClient::getInstance());

    param = argv[1];
    if (strcmp(param, "curve") == 0) {
        int32_t len;
        uint32_t serial;
        ret = client.getAdaptField(IAALService::ALI2BLI_CURVE_LENGTH, &len, sizeof(len), &serial);
        if (ret == NO_ERROR) {
            int *curve = new int[len * 2];
            ret = client.getAdaptField(IAALService::ALI2BLI_CURVE, curve, sizeof(int) * len * 2, &serial);
            if (ret == NO_ERROR) {
                printf("Serial = %u\n", serial);
                printf("ALI:");
                for (int i = 0; i < len; i++)
                    printf(" %4d", curve[i]);
                printf("\nBLI:");
                for (int i = 0; i < len; i++)
                    printf(" %4d", curve[len + i]);
            }
            delete [] curve;
            printf("\n");
        } else {
            printf("Error!\n");
        }
    } else if (strcmp(param, "setcurve") == 0) {
        uint32_t serial = 0;
        if (value == 1) {
            ret = client.setAdaptField(IAALService::ALI2BLI_CURVE, (void*)PREDEFINED_CURVE1, sizeof(PREDEFINED_CURVE1), &serial);
        } else if (value == 2) {
            ret = client.setAdaptField(IAALService::ALI2BLI_CURVE, (void*)PREDEFINED_CURVE2, sizeof(PREDEFINED_CURVE2), &serial);
        }
        printf("Serial = %d\n", serial);
    } else if (strcmp(param, "BS") == 0) {
        uint32_t serial = 0;
        if (value > 0) {
            ret = client.setAdaptField(IAALService::BLI_RAMP_RATE_BRIGHTEN, &value, sizeof(value), &serial);
            printf("Set brighten ramp rate = %d, serial = %d\n", value, serial);
        } else {
            ret = client.getAdaptField(IAALService::BLI_RAMP_RATE_BRIGHTEN, &value, sizeof(value), &serial);
            if (ret == NO_ERROR)
                printf("Current brighten ramp rate = %d, serial = %d\n", value, serial);
        }
    } else if (strcmp(param, "DS") == 0) {
        uint32_t serial = 0;
        if (value > 0) {
            ret = client.setAdaptField(IAALService::BLI_RAMP_RATE_DARKEN, &value, sizeof(value), &serial);
            printf("Set darken ramp rate = %d, serial = %d\n", value, serial);
        } else {
            ret = client.getAdaptField(IAALService::BLI_RAMP_RATE_DARKEN, &value, sizeof(value), &serial);
            if (ret == NO_ERROR)
                printf("Current darken ramp rate = %d, serial = %d\n", value, serial);
        }
    } else if (strcmp(param, "func") == 0) {
        ret = client.setFunction(value);
    } else if (strcmp(param, "SBS") == 0) {
        ret = client.setSmartBacklightStrength(value);
    } else if (strcmp(param, "SBR") == 0) {
        ret = client.setSmartBacklightRange(value);
    } else if (strcmp(param, "RD") == 0) {
        ret = client.setReadabilityLevel(value);
    } else {
        printf("Invalid test command.\n");
    }

    printf("ret = %d\n", ret);

    ALOGI("AAL client exit");

    return 0;
}
