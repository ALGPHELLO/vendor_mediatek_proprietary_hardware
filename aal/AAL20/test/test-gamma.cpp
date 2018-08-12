#define LOG_TAG "GAMMA-Test"

#define MTK_LOG_ENABLE 1
#include <stdint.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <cutils/log.h>
#include <ddp_drv.h>
#include <ddp_gamma.h>


int main(int argc, char *argv[])
{
    int ret = 0;
    int arg = 0;
    int drvID;
    int value;
    float exp;

    if (argc < 2)
    {
        printf("gamma-test <exp>\n");
        return 0;
    }

    drvID = open("/dev/mtk_disp_mgr", O_RDONLY, 0);

    if (drvID > 0) {
        exp = atof(argv[1]);

        DISP_GAMMA_LUT_T *gammaLut = new DISP_GAMMA_LUT_T;

        gammaLut->hw_id = DISP_GAMMA0;
        for (int i = 0; i < DISP_GAMMA_LUT_SIZE; i++)
        {
            value = round(pow((float)i/(float)DISP_GAMMA_LUT_SIZE, 1.0f/exp) * 1024.0);
            if (value > 1023)
                value = 1023;

            gammaLut->lut[i] = GAMMA_ENTRY(value, value, value);

            if ((i & 0xf) == 0) {
                ALOGD("gamma[%d] = 0x%x, value = %d\n", i, gammaLut->lut[i], value);
            }
        }

        ioctl(drvID, DISP_IOCTL_SET_GAMMALUT, gammaLut);

        delete gammaLut;
    } else {
        printf("Fail to open dev node\n");
    }

    return 0;
}
