
/*
 * customized hint config table
 */

#include <power_cus_types.h>

using namespace vendor::mediatek::hardware::power::V1_1;


struct tCusConfig cusHintConfig[] = {
    {-1, 0, 0, 0, 0, 0},
};

struct tCusConfig cusHintConfigImpl[] = {
   /* example
    * {HINT, COMMAND , 0, 0, 0, 0},
    */
    // MTK_CUS_AUDIO_LATENCY_DL
    {(int)MtkCusPowerHintInternal::MTK_CUS_AUDIO_LATENCY_DL, (int)MtkPowerCmd::CMD_SET_CLUSTER_CPU_CORE_MIN, 0, 2, 0 ,0},
    {(int)MtkCusPowerHintInternal::MTK_CUS_AUDIO_LATENCY_DL, (int)MtkPowerCmd::CMD_SET_SCREEN_OFF_STATE, 1, 0, 0 ,0},

    // MTK_CUS_AUDIO_LATENCY_UL
    {(int)MtkCusPowerHintInternal::MTK_CUS_AUDIO_LATENCY_UL, (int)MtkPowerCmd::CMD_SET_CLUSTER_CPU_CORE_MIN, 0, 2, 0 ,0},
    {(int)MtkCusPowerHintInternal::MTK_CUS_AUDIO_LATENCY_UL, (int)MtkPowerCmd::CMD_SET_SCREEN_OFF_STATE, 1, 0, 0 ,0},

    // MTK_CUS_AUDIO_Power_DL
    {(int)MtkCusPowerHintInternal::MTK_CUS_AUDIO_POWER_DL, (int)MtkPowerCmd::CMD_SET_CLUSTER_CPU_FREQ_MIN, 0, 800000, 0 ,0},
    {(int)MtkCusPowerHintInternal::MTK_CUS_AUDIO_POWER_DL, (int)MtkPowerCmd::CMD_SET_SCREEN_OFF_STATE, 1, 0, 0 ,0},

    {-1, 0, 0, 0, 0, 0},
};
