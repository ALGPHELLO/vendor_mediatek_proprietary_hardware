 /*
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein
 * is confidential and proprietary to MediaTek Inc. and/or its licensors.
 * Without the prior written permission of MediaTek inc. and/or its licensors,
 * any reproduction, modification, use or disclosure of MediaTek Software,
 * and information contained herein, in whole or in part, shall be strictly prohibited.
 *
 * MediaTek Inc. (C) 2010. All rights reserved.
 *
 * BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
 * THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
 * RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER ON
 * AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
 * NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
 * SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
 * SUPPLIED WITH THE MEDIATEK SOFTWARE, AND RECEIVER AGREES TO LOOK ONLY TO SUCH
 * THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. RECEIVER EXPRESSLY ACKNOWLEDGES
 * THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES
 * CONTAINED IN MEDIATEK SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK
 * SOFTWARE RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
 * STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND
 * CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
 * AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE,
 * OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY RECEIVER TO
 * MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
 *
 * The following software/firmware and/or related documentation ("MediaTek Software")
 * have been modified by MediaTek Inc. All revisions are subject to any receiver's
 * applicable license agreements with MediaTek Inc.
 */

/*****************************************************************************
 * Include
 *****************************************************************************/
#include "RfxRilUtils.h"
#include "RfxStatusManager.h"
#include "RfxRootController.h"
#include "RfxMclMessage.h"
#include "RfxMessage.h"
#include "RfxDispatchThread.h"
#include "telephony/librilutilsmtk.h"
#include "utils/String8.h"
#ifdef HAVE_AEE_FEATURE
#include "aee.h"
#endif

#define RFX_LOG_TAG "RfxRilUtils"

#define PROP_NAME_OPERATOR_ID_SIM1 "persist.radio.sim.opid"
#define PROP_NAME_OPERATOR_ID_SIM2 "persist.radio.sim.opid_1"
#define PROP_NAME_OPERATOR_ID_SIM3 "persist.radio.sim.opid_2"
#define PROP_NAME_OPERATOR_ID_SIM4 "persist.radio.sim.opid_3"

int RfxRilUtils::m_multiSIMConfig = -1;
int RfxRilUtils::m_isEngLoad = -1;
int RfxRilUtils::m_isChipTest = -1;
int RfxRilUtils::m_isInternalLoad = -1;
int RfxRilUtils::m_isUserLoad = -1;
int RfxRilUtils::mIsC2kSupport = -1;
int RfxRilUtils::mIsLteSupport = -1;
int RfxRilUtils::mIsImsSupport = -1;
int RfxRilUtils::mIsMultiIms = -1;

/// M: add for op09 volte setting @{
int RfxRilUtils::mIsOp09 = -1;
int RfxRilUtils::mIsCtVolteSupport = -1;
/// @}

RilRunMode RfxRilUtils::m_rilRunMode = RilRunMode::RIL_RUN_MODE_NORMORL;
STATUSCALLBACK RfxRilUtils::s_statusCallback = NULL;


enum MultiSIMConfig {
    MSIM_MODE_SS,
    MSIM_MODE_DSDS,
    MSIM_MODE_DSDA,
    MSIM_MODE_TSTS,
    MSIM_MODE_QSQS,
    MSIM_MODE_NUM
};

void RfxRilUtils::readMultiSIMConfig(void) {
    if (RfxRilUtils::m_multiSIMConfig == -1) {
        char property_value[RFX_PROPERTY_VALUE_MAX];
        memset(property_value, 0, sizeof(property_value));
        rfx_property_get("persist.radio.multisim.config", property_value, NULL);
        if (strcmp(property_value, "ss") == 0) {
            RfxRilUtils::m_multiSIMConfig = MSIM_MODE_SS;
        } else if (strcmp(property_value, "dsds") == 0) {
            RfxRilUtils::m_multiSIMConfig = MSIM_MODE_DSDS;
        } else if (strcmp(property_value, "dsda") == 0) {
            RfxRilUtils::m_multiSIMConfig = MSIM_MODE_DSDA;
        } else if (strcmp(property_value, "tsts") == 0) {
            RfxRilUtils::m_multiSIMConfig = MSIM_MODE_TSTS;
        } else if (strcmp(property_value, "qsqs") == 0) {
            RfxRilUtils::m_multiSIMConfig = MSIM_MODE_QSQS;
        } else {
            RFX_LOG_E(RFX_LOG_TAG, "unknown msim mode property (%s)", property_value);
        }
        RFX_LOG_D(RFX_LOG_TAG, "msim config: %d", RfxRilUtils::m_multiSIMConfig);
    }
}

int RfxRilUtils::getSimCount() {
    int count = 2;
    RfxRilUtils::readMultiSIMConfig();
    if (RfxRilUtils::m_multiSIMConfig == -1) {
        RFX_LOG_E(RFX_LOG_TAG, "getSimCount fail. default return 2. (unknown msim config: %d)",
                RfxRilUtils::m_multiSIMConfig);
        count = 2;
    } else if (RfxRilUtils::m_multiSIMConfig == MSIM_MODE_SS) {
        count = 1;
    } else if (RfxRilUtils::m_multiSIMConfig == MSIM_MODE_DSDS
            || RfxRilUtils::m_multiSIMConfig == MSIM_MODE_DSDA) {
        count = 2;
    } else if (RfxRilUtils::m_multiSIMConfig == MSIM_MODE_TSTS) {
        count = 3;
    } else if (RfxRilUtils::m_multiSIMConfig == MSIM_MODE_QSQS) {
        count = 4;
    }
    return count;
}

int RfxRilUtils::isEngLoad() {
    if (RfxRilUtils::m_isEngLoad == -1) {
        char property_value[RFX_PROPERTY_VALUE_MAX] = { 0 };
        rfx_property_get("ro.build.type", property_value, "");
        RfxRilUtils::m_isEngLoad = (strcmp("eng", property_value) == 0);
    }
    return RfxRilUtils::m_isEngLoad;
}

bool RfxRilUtils::isChipTestMode() {
    if (RfxRilUtils::m_isChipTest == -1) {
        char prop_val[RFX_PROPERTY_VALUE_MAX] = {0};
        rfx_property_get("persist.chiptest.enable", prop_val, "0");
        RfxRilUtils::m_isChipTest = atoi(prop_val);
    }
    return RfxRilUtils::m_isChipTest? true: false;
}

int RfxRilUtils::isUserLoad() {
    if (RfxRilUtils::m_isUserLoad == -1) {
        char property_value_emulation[RFX_PROPERTY_VALUE_MAX] = { 0 };
        char property_value[RFX_PROPERTY_VALUE_MAX] = { 0 };
        rfx_property_get("ril.emulation.userload", property_value_emulation, "0");
        if(strcmp("1", property_value_emulation) == 0) {
            return 1;
        }
        rfx_property_get("ro.build.type", property_value, "");
        RfxRilUtils::m_isUserLoad = (strcmp("user", property_value) == 0);
    }
    return RfxRilUtils::m_isUserLoad;
}

int RfxRilUtils::isC2kSupport() {
    if (RfxRilUtils::mIsC2kSupport == -1) {
        char tempstr[RFX_PROPERTY_VALUE_MAX];
        memset(tempstr, 0, sizeof(tempstr));
        rfx_property_get("ro.boot.opt_c2k_support", tempstr, "0");
        RfxRilUtils::mIsC2kSupport = atoi(tempstr);
    }
    return RfxRilUtils::mIsC2kSupport;
}

int RfxRilUtils::isLteSupport() {
    if (RfxRilUtils::mIsLteSupport == -1) {
        char tempstr[RFX_PROPERTY_VALUE_MAX];
        memset(tempstr, 0, sizeof(tempstr));
        rfx_property_get("ro.boot.opt_lte_support", tempstr, "0");
        RfxRilUtils::mIsLteSupport = atoi(tempstr);
    }
    return RfxRilUtils::mIsLteSupport;
}

int RfxRilUtils::isImsSupport() {
    if (RfxRilUtils::mIsImsSupport == -1) {
        char tempstr[RFX_PROPERTY_VALUE_MAX];
        memset(tempstr, 0, sizeof(tempstr));
        rfx_property_get("persist.mtk_ims_support", tempstr, "0");
        RfxRilUtils::mIsImsSupport = atoi(tempstr);
    }
    return RfxRilUtils::mIsImsSupport;
}

int RfxRilUtils::isMultipleImsSupport() {
    if (RfxRilUtils::mIsMultiIms == -1) {
        char tempstr[RFX_PROPERTY_VALUE_MAX] = { 0 };
        rfx_property_get("persist.mtk_mims_support", tempstr, "0");
        RfxRilUtils::mIsMultiIms = atoi(tempstr);
    }
    return (RfxRilUtils::mIsMultiIms > 1) ? 1 : 0;
}

int RfxRilUtils::triggerCCCIIoctlEx(int request, int *param) {
    int ret_ioctl_val = -1;
    int ccci_sys_fd = -1;
    char dev_node[32] = {0};
    int enableMd1 = 0, enableMd2 = 0;
    char prop_value[RFX_PROPERTY_VALUE_MAX] = { 0 };

    rfx_property_get("ro.boot.opt_md1_support", prop_value, "0");
    enableMd1 = atoi(prop_value);

#if defined(PURE_AP_USE_EXTERNAL_MODEM)
    RFX_LOG_D(RFX_LOG_TAG, "Open CCCI MD1 ioctl port[%s]",CCCI_MD1_POWER_IOCTL_PORT);
    ccci_sys_fd = open(CCCI_MD1_POWER_IOCTL_PORT, O_RDWR);
#else
    snprintf(dev_node, 32, "%s", ccci_get_node_name(USR_RILD_IOCTL, MD_SYS1));
    RFX_LOG_D(RFX_LOG_TAG, "MD1/SYS1 IOCTL [%s, %d]", dev_node, request);
    ccci_sys_fd = open(dev_node, O_RDWR | O_NONBLOCK);
#endif

    if (ccci_sys_fd < 0) {
        RFX_LOG_D(RFX_LOG_TAG, "Open CCCI ioctl port failed [%d]", ccci_sys_fd);
        return -1;
    }

#if defined(PURE_AP_USE_EXTERNAL_MODEM)
    if(request == CCCI_IOC_ENTER_DEEP_FLIGHT) {
        int pid = findPid("gsm0710muxd");
        RFX_LOG_D(RFX_LOG_TAG, "MUXD pid=%d",pid);
        if(pid != -1) kill(pid,SIGUSR2);
        RFX_LOG_D(RFX_LOG_TAG, "send SIGUSR2 to MUXD done");
        sleepMsec(100);    // make sure MUXD have enough time to close channel and FD
    }
#endif

    ret_ioctl_val = ioctl(ccci_sys_fd, request, param);
    if (ret_ioctl_val < 0) {
        RFX_LOG_E(RFX_LOG_TAG, "CCCI ioctl result: ret_val=%d, request=%d, param=%d",
                ret_ioctl_val, request, *param);
    } else {
        RFX_LOG_D(RFX_LOG_TAG, "CCCI ioctl result: ret_val=%d, request=%d, param=%d",
                ret_ioctl_val, request, *param);
    }

    close(ccci_sys_fd);
    return ret_ioctl_val;
}

int RfxRilUtils::triggerCCCIIoctl(int request) {
    int param = -1;
    int ret_ioctl_val;

    ret_ioctl_val = triggerCCCIIoctlEx(request, &param);

    return ret_ioctl_val;
}


RilRunMode RfxRilUtils::getRilRunMode() {
    return m_rilRunMode;
}

void RfxRilUtils::setRilRunMode(RilRunMode mode) {
    m_rilRunMode = mode;
}

void RfxRilUtils::setStatusValueForGT(int slotId, const RfxStatusKeyEnum key, const RfxVariant &value) {
    RFX_LOG_D(RFX_LOG_TAG, "setStatusValueForGT, updateValueMdComm, slot_id = %d, key = %s, value = %s", slotId,
            RfxStatusManager::getKeyString(key), value.toString().string());
    sp<RfxMessage> msg = RfxMessage::obtainStatusSync(slotId, key, value, false, false, true);
    RFX_OBJ_GET_INSTANCE(RfxRilAdapter)->requestToMcl(msg);

    RFX_LOG_D(RFX_LOG_TAG, "setStatusValueForGT, updateValueToTelCore, slot_id = %d, key = %s, value = %s", slotId,
            RfxStatusManager::getKeyString(key), value.toString().string());
    sp<RfxMclMessage> msgToTcl = RfxMclMessage::obtainStatusSync(slotId, key, value, false, false, true);
    RfxDispatchThread::enqueueStatusSyncMessage(msgToTcl);
}

void RfxRilUtils::setStatusCallbackForGT(STATUSCALLBACK statusCallback) {
    s_statusCallback = statusCallback;
}

void RfxRilUtils::updateStatusToGT(int slotId, const RfxStatusKeyEnum key, const RfxVariant &value) {
    RFX_LOG_E(RFX_LOG_TAG, "updateStatusToGT");
    if (s_statusCallback != NULL) {
        RFX_LOG_E(RFX_LOG_TAG, "updateStatusToGT is not null");
        s_statusCallback(slotId, key, value);
    } else {
        RFX_LOG_E(RFX_LOG_TAG, "updateStatusToGT is NULL");
    }
}

/// M: add for op09 volte setting @{
bool RfxRilUtils::isOp09() {
    if (mIsOp09 == -1) {
        char optrstr[PROPERTY_VALUE_MAX] = { 0 };
        property_get("persist.operator.optr", optrstr, "");
        // RLOGD("isOp09(): optr = %s", optrstr);
        if (strncmp(optrstr, "OP09", 4) == 0) {
            mIsOp09 = 1;
        } else {
            mIsOp09 = 0;
        }
    }
    return (mIsOp09 == 1);
}

bool RfxRilUtils::isCtVolteSupport() {
    if (mIsCtVolteSupport == -1) {
        char ctstr[PROPERTY_VALUE_MAX] = { 0 };
        property_get("persist.mtk_ct_volte_support", ctstr, "");
        if (strcmp(ctstr, "1") == 0) {
            mIsCtVolteSupport = 1;
        } else {
            mIsCtVolteSupport = 0;
        }
    }
    return (mIsCtVolteSupport == 1);
}
/// @}

int RfxRilUtils::getMajorSim() {
    char tmp[RFX_PROPERTY_VALUE_MAX] = { 0 };
    int simId = 0;

    rfx_property_get("persist.radio.simswitch", tmp, "1");
    simId = atoi(tmp);
    RFX_LOG_D(RFX_LOG_TAG, "getMajorSim, simId=%d", simId);
    return simId;
}

void RfxRilUtils::printLog(int level, String8 tag, String8 log, int slot) {
    switch (level) {
        case VERBOSE:
            RFX_LOG_V(tag.string(), "[%d] %s", slot, log.string());
            break;
        case DEBUG:
            RFX_LOG_D(tag.string(), "[%d] %s", slot, log.string());
            break;
        case INFO:
            RFX_LOG_I(tag.string(), "[%d] %s", slot, log.string());
            break;
        case WARN:
            RFX_LOG_W(tag.string(), "[%d] %s", slot, log.string());
            break;
        case ERROR:
            RFX_LOG_E(tag.string(), "[%d] %s", slot, log.string());
            break;
        default:
            RFX_LOG_E(tag.string(), "undefine log level: %s", log.string());
    }
}

bool RfxRilUtils::isInLogReductionList(int reqId) {
    const int logReductionRequest[] = {
        RFX_MSG_REQUEST_SIM_IO,
        RFX_MSG_REQUEST_READ_EMAIL_ENTRY,
        RFX_MSG_REQUEST_READ_SNE_ENTRY,
        RFX_MSG_REQUEST_READ_ANR_ENTRY,
        RFX_MSG_REQUEST_READ_UPB_GRP,
        RFX_MSG_REQUEST_QUERY_PHB_STORAGE_INFO,
        RFX_MSG_REQUEST_WRITE_PHB_ENTRY,
        RFX_MSG_REQUEST_READ_PHB_ENTRY,
        RFX_MSG_REQUEST_QUERY_UPB_CAPABILITY,
        RFX_MSG_REQUEST_EDIT_UPB_ENTRY,
        RFX_MSG_REQUEST_DELETE_UPB_ENTRY,
        RFX_MSG_REQUEST_READ_UPB_GAS_LIST,
        RFX_MSG_REQUEST_WRITE_UPB_GRP,
        RFX_MSG_REQUEST_QUERY_UPB_AVAILABLE,
        RFX_MSG_REQUEST_READ_UPB_AAS_LIST,
        RFX_MSG_REQUEST_GSM_SMS_BROADCAST_ACTIVATION,
        RFX_MSG_REQUEST_CDMA_SMS_BROADCAST_ACTIVATION,
        RFX_MSG_REQUEST_DEACTIVATE_DATA_CALL,
        RFX_MSG_REQUEST_SYNC_DATA_SETTINGS_TO_MD,
        RFX_MSG_REQUEST_ALLOW_DATA,
        RFX_MSG_REQUEST_SETUP_DATA_CALL,
        RFX_MSG_REQUEST_RESET_MD_DATA_RETRY_COUNT,
    };

    size_t count = sizeof(logReductionRequest)/sizeof(int);
    for (size_t i = 0; i < count; i++) {
        if (reqId == logReductionRequest[i]) {
            return true;
        }
    }
    return false;
}

int RfxRilUtils::handleAee(const char *modem_warning, const char *modem_version) {
#ifdef HAVE_AEE_FEATURE
    return aee_modem_warning("Modem Warning", NULL, DB_OPT_DUMMY_DUMP, modem_warning,
            modem_version);
#else
    RFX_UNUSED(modem_warning);
    RFX_UNUSED(modem_version);
    LOGD("[handleOemUnsolicited]HAVE_AEE_FEATURE is not defined");
    return 1;
#endif
}

// External SIM [Start]
#define PROPERTY_MODEM_VSIM_CAPABILITYY "gsm.modem.vsim.capability"
#define MODEM_VSIM_CAPABILITYY_EANBLE 0x01
#define MODEM_VSIM_CAPABILITYY_HOTSWAP 0x02

int RfxRilUtils::isExternalSimSupport() {
    char property_value[RFX_PROPERTY_VALUE_MAX] = { 0 };
    rfx_property_get("ro.mtk_external_sim_support", property_value, "0");
    return atoi(property_value);
}

int RfxRilUtils::isExternalSimOnlySlot(int slot) {
    char property_value[RFX_PROPERTY_VALUE_MAX] = { 0 };
    rfx_property_get("ro.mtk_external_sim_only_slots", property_value, "0");
    int supported = atoi(property_value) & (1 << slot);

    RFX_LOG_D(RFX_LOG_TAG, "[isExternalSimOnlySlot] vsimOnlySlots:%d, supported:%d",
            atoi(property_value), supported);
    return ((supported > 0) ? 1 : 0);
}

int RfxRilUtils::isPersistExternalSimDisabled() {
    char property_value[RFX_PROPERTY_VALUE_MAX] = { 0 };
    rfx_property_get("ro.mtk_persist_vsim_disabled", property_value, "0");
    return atoi(property_value);
}

int RfxRilUtils::isNonDsdaRemoteSupport() {
    char property_value[RFX_PROPERTY_VALUE_MAX] = { 0 };
    rfx_property_get("ro.mtk_non_dsda_rsim_support", property_value, "0");
    return atoi(property_value);
}

int RfxRilUtils::isSwitchVsimWithHotSwap() {
    int enabled = 0;
    char vsim_hotswap[RFX_PROPERTY_VALUE_MAX] = {0};

    for (int index = 0; index < RfxRilUtils::getSimCount(); index++) {
        getMSimProperty(index, (char*)PROPERTY_MODEM_VSIM_CAPABILITYY, vsim_hotswap);
        if ((atoi(vsim_hotswap) & MODEM_VSIM_CAPABILITYY_HOTSWAP) > 1) {
            enabled = 1;
            break;
        }
    }

    RFX_LOG_D(RFX_LOG_TAG, "[VSIM] isSwitchVsimWithHotSwap: %d.", enabled);

    return enabled;
}

int RfxRilUtils::isVsimEnabledBySlot(int slot) {
    int enabled = 0;
    char vsim_enabled_prop[RFX_PROPERTY_VALUE_MAX] = {0};
    char vsim_inserted_prop[RFX_PROPERTY_VALUE_MAX] = {0};

    getMSimProperty(slot, (char*)"gsm.external.sim.enabled", vsim_enabled_prop);
    getMSimProperty(slot, (char*)"gsm.external.sim.inserted", vsim_inserted_prop);

    if ((atoi(vsim_enabled_prop) > 0 && atoi(vsim_inserted_prop) > 0) || isExternalSimOnlySlot(slot)) {
        enabled = 1;
    }

    RFX_LOG_D(RFX_LOG_TAG, "[VSIM] isVsimEnabled slot:%d is %d.", slot, enabled);

    return enabled;
}

bool RfxRilUtils::isVsimEnabled() {
    bool enabled = false;

    for (int index = 0; index < RfxRilUtils::getSimCount(); index++) {
        if (1 == isVsimEnabledBySlot(index)) {
            enabled = true;
            break;
        }
    }
    RFX_LOG_D(RFX_LOG_TAG, "[VSIM] isVsimEnabled=%d", enabled);
    return enabled;
}
// External SIM [End]

bool RfxRilUtils::isTplusWSupport() {
    char tmp[RFX_PROPERTY_VALUE_MAX] = { 0 };
    rfx_property_get("ril.simswitch.tpluswsupport", tmp, "0");
    RFX_LOG_D(RFX_LOG_TAG, "tpluswsupport=%s", tmp);
    return (atoi(tmp) != 0);
}

int RfxRilUtils::getKeep3GMode() {
    char tmp[RFX_PROPERTY_VALUE_MAX] = { 0 };
    rfx_property_get("ril.nw.worldmode.keep_3g_mode", tmp, "0");
    RFX_LOG_D(RFX_LOG_TAG, "keep_3g_mode=%s", tmp);
    return (atoi(tmp));
}

bool RfxRilUtils::isWfcEnable(int slotId) {
    char wfcSupport[RFX_PROPERTY_VALUE_MAX] = { 0 };
    char wfcEnable[PROPERTY_VALUE_MAX] = { 0 };

    rfx_property_get("persist.mtk_wfc_support", wfcSupport, "0");
    rfx_property_get("persist.mtk.wfc.enable", wfcEnable, "0");
    RFX_LOG_D(RFX_LOG_TAG, "isWfcEnable(), slotId: %d, wfcSupport: %s, wfcEnable %s",
            slotId, wfcSupport, wfcEnable);

    if (atoi(wfcSupport) == 1) {
        /* wfcEnable is a bitmask for VoLTE/ViLTE/WFC. Maximun sim is 4.
         * ex: 0001 means SIM 1 enable, 0101 means SIM 1 and SIM 3 enable
         */
        if (RfxRilUtils::isMultipleImsSupport() == 1) {
            /* Check if the "current SIM's WFC"" has been switch on */
            if (((atoi(wfcEnable) >> slotId) & 0x01) == 1) {
                return true;
            }
        } else {
            /* Only need to check main slot */
            if (atoi(wfcEnable) == 1) {
                return true;
            }
        }
    }
    return false;
}

bool RfxRilUtils::isDigitsSupport() {
    char digitsSupport[RFX_PROPERTY_VALUE_MAX] = { 0 };
    rfx_property_get("persist.mtk_digits_support", digitsSupport, "0");
    RFX_LOG_D(RFX_LOG_TAG, "isDigitsSupported=%s", digitsSupport);
    return (atoi(digitsSupport) == 1);
}

int RfxRilUtils::getOperatorId(int simId) {
    String8 key;
    char prop_val[RFX_PROPERTY_VALUE_MAX] = {0};
    switch (simId) {
        case 0:
            key = String8(PROP_NAME_OPERATOR_ID_SIM1);
            break;
        case 1:
            key = String8(PROP_NAME_OPERATOR_ID_SIM2);
            break;
        case 2:
            key = String8(PROP_NAME_OPERATOR_ID_SIM3);
            break;
        case 3:
            key = String8(PROP_NAME_OPERATOR_ID_SIM4);
            break;
        default:
            key = String8(PROP_NAME_OPERATOR_ID_SIM1);
    }
    rfx_property_get(key, prop_val, "0");
    int opId = atoi(prop_val);
    LOGD("getOperatorId: %d", opId);
    return opId;
}
