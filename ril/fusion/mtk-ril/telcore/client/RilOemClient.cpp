/* Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein
 * is confidential and proprietary to MediaTek Inc. and/or its licensors.
 * Without the prior written permission of MediaTek inc. and/or its licensors,
 * any reproduction, modification, use or disclosure of MediaTek Software,
 * and information contained herein, in whole or in part, shall be strictly prohibited.
 *
 * MediaTek Inc. (C) 2016. All rights reserved.
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
#include "RilOemClient.h"
#include "RfxLog.h"
#include <sys/types.h>
#include <sys/socket.h>
#include "rfx_properties.h"
#include "RfxMessageId.h"
#include "RfxRilUtils.h"
#include "Rfx.h"

/*****************************************************************************
 * Class RfxController
 *****************************************************************************/

#define RFX_LOG_TAG "RilOemClient"
#define OEM_MAX_PARA_LENGTH 768

static CommandInfo sCommands[] = {
    #include "ril_client_commands.h"
};

static UnsolResponseInfo sUnsolCommands[] = {
    #include "ril_client_unsol_commands.h"
};
int RilOemClient::mVtFd = -1;
bool RilOemClient::bVtExist = false;
#define RIL_OEM_SOCKET_COUNT 32
#define RIL_OEM_SOCKET_LENGTH 1024

RilOemClient::RilOemClient(int identity, char* socketName) : RilClient(identity, socketName) {
    RFX_LOG_D(RFX_LOG_TAG, "Init Oem client");
}

RilOemClient::~RilOemClient() {

}

void RilOemClient::handleStateActive() {
    int number = 0;
    char **args;

    if (recv(commandFd, &number, sizeof(int), 0) != sizeof(int)) {
        RFX_LOG_E(RFX_LOG_TAG, "error reading on socket (number)");
        setClientState(CLIENT_DEACTIVE); /* or should set to init state*/
        return;
    }

    RFX_LOG_I(RFX_LOG_TAG, "NUMBER:%d", number);
    if (number < 0 || number > RIL_OEM_SOCKET_COUNT) {
        RFX_LOG_E(RFX_LOG_TAG, "Not allow negative number or more than limitation");
        setClientState(CLIENT_DEACTIVE);
        return;
    }
    args = (char **) calloc(1, sizeof(char*) * number);
    if (args == NULL) {
        RFX_LOG_E(RFX_LOG_TAG,"OOM");
        return;
    }

    for (int i = 0; i < number; i++) {
        unsigned int len;
        if (recv(commandFd, &len, sizeof(int), 0) != sizeof(int)) {
            RFX_LOG_E(RFX_LOG_TAG, "error reading on socket (Args)");
            freeArgs(args, number);
            setClientState(CLIENT_DEACTIVE); /* or should set to init state*/
            return;
        }
        // +1 for null-term

        if (len > RIL_OEM_SOCKET_LENGTH) {
            RFX_LOG_E(RFX_LOG_TAG, "lenght of args is less than 0 or more than limitation");
            freeArgs(args, number);
            setClientState(CLIENT_DEACTIVE);
            return;
        }
        RFX_LOG_I(RFX_LOG_TAG, "arg len:%u", len);
        args[i] = (char *) calloc(1, (sizeof(char) * len) + 1);
        if (args == NULL) {
            RFX_LOG_E(RFX_LOG_TAG,"OOM");
            return;
        }
        if (recv(commandFd, args[i], sizeof(char) * len, 0)
                != (int)(sizeof(char) * len)) {
            RFX_LOG_E(RFX_LOG_TAG, "error reading on socket: Args[%d] \n", i);
            freeArgs(args, number);
            setClientState(CLIENT_DEACTIVE); /* or should set to init state*/
            return;
        }
        char *buf = args[i];
        buf[len] = 0;

        RFX_LOG_I(RFX_LOG_TAG, "ARGS[%d]:%s", i, buf);
    }

    if (0 > handleSpecialRequestWithArgs(number, args)) {
        RFX_LOG_I(RFX_LOG_TAG, "Oem port: SpecialRequest not support");
        setClientState(CLIENT_DEACTIVE); /* or should set to init state*/
    }
    freeArgs(args, number);
}

void RilOemClient::handleStateDeactive() {
    if (stream != NULL) {
        RFX_LOG_D(RFX_LOG_TAG, "clear stream because stream exists");
        record_stream_free(stream);
        stream = NULL;
    } else {
        RFX_LOG_D(RFX_LOG_TAG, "stream null here");
    }

    setClientState(CLIENT_INITIALIZING);
}

void RilOemClient::handleStateClosed() {
    if (stream != NULL) {
        RFX_LOG_D(RFX_LOG_TAG, "closed: clear stream because stream exists");
        record_stream_free(stream);
        stream = NULL;
    } else {
        RFX_LOG_D(RFX_LOG_TAG, "closed: stream null here");
    }
}

void RilOemClient::requestComplete(RIL_Token token, RIL_Errno e, void *response,
        size_t responselen) {
    RFX_UNUSED(responselen);
    RequestInfo *info = (RequestInfo *) token;
    if (RFX_MSG_REQUEST_QUERY_MODEM_THERMAL == info->request) {
        String8 strResult;
        RFX_LOG_I(RFX_LOG_TAG, "request for THERMAL returned");
        if(RIL_E_SUCCESS == e){
            strResult = String8((char*) response);
        } else {
            strResult = String8((char*) "ERROR");
        }

        if(mThermalFd >= 0){
            RFX_LOG_I(RFX_LOG_TAG, "mThermalFd is valid strResult is %s", strResult.string());

            size_t len = strResult.size();
            ssize_t ret = send(mThermalFd, strResult, len, MSG_NOSIGNAL);
            if (ret != (ssize_t) len) {
                RFX_LOG_I(RFX_LOG_TAG, "lose data when send response.");
            }
        } else {
            RFX_LOG_I(RFX_LOG_TAG, "mThermalFd is < 0");
        }
    } else if (RFX_MSG_REQUEST_RESET_RADIO == info->request) {
        if (commandFd >= 0) {
            close(commandFd);
        }
    }
    setClientState(CLIENT_DEACTIVE); /* or should set to init state*/
    if(info) {
        free(info);
    }
}

void RilOemClient::handleUnsolicited(int slotId, int unsolResponse, void *data,
        size_t datalen, UrcDispatchRule rule) {
    RFX_UNUSED(slotId);
    RFX_UNUSED(unsolResponse);
    RFX_UNUSED(data);
    RFX_UNUSED(datalen);
    RFX_UNUSED(rule);
}

int RilOemClient::handleSpecialRequestWithArgs(int argCount, char** args) {
    char *cmd;
    char orgArgs[OEM_MAX_PARA_LENGTH] = {0};
    RfxAtLine *line;
    int err = 0;

    if (1 == argCount) {
        strncpy(orgArgs, args[0], OEM_MAX_PARA_LENGTH-1);
        line = new RfxAtLine(args[0], NULL);
        cmd = line->atTokNextstr(&err);
        if (err < 0) {
            RFX_LOG_E(RFX_LOG_TAG, "invalid command");
            delete line;
            return FAILURE;
        }

        if (strcmp(cmd, (char *) "THERMAL") == 0) {
            if (mThermalFd >= 0) {
                close(mThermalFd);
            }
            mThermalFd = commandFd;
            commandFd = -1;
            executeThermal(orgArgs);
            delete line;
            return SUCCESS;
        /// M: CC: For 3G VT only @{
        } else if (strcmp(cmd, (char *) "VT") == 0) {
#ifdef MTK_VT3G324M_SUPPORT
            if(mVtFd > 0) {
                close(mVtFd); // close previous fd, avoid fd leak
                if (bVtExist) {
                    executeHangupAll();
                }
            }
            mVtFd = commandFd;
#else
            RFX_LOG_E(RFX_LOG_TAG, "3G VT not supported");
#endif
            setClientState(CLIENT_DEACTIVE); /* or should set to init state*/
            delete line;
            return SUCCESS;
        /// @}
        } else if (strcmp(cmd, "MDTM_TOG") == 0) {
            executeShutDownByThermal(orgArgs);
            delete line;
            return SUCCESS;
        } else {
            delete line;
        }
    }
    RFX_LOG_E(RFX_LOG_TAG, "Invalid request");
    return FAILURE;
}

void RilOemClient::executeThermal(char *arg) {
    RFX_LOG_I(RFX_LOG_TAG, "executeThermal");
    char *cmd;
    int err = 0, slotId = 0;
    Parcel p;

    RfxAtLine *line = new RfxAtLine(arg, NULL);
    cmd = line->atTokNextstr(&err);
    slotId = line->atTokNextint(&err);
    int mainSlotId = RfxRilUtils::getMajorSim() - 1;
    RFX_LOG_D(RFX_LOG_TAG, "Thermal line = %s, cmd:%s, slotId:%d, targetSim: %d",
            arg, cmd, slotId, mainSlotId);

    RequestInfo *pRI = (RequestInfo *)calloc(1, sizeof(RequestInfo));
    if (pRI == NULL) {
        RFX_LOG_E(RFX_LOG_TAG,"OOM");
        delete line;
        return;
    }
    pRI->socket_id = (RIL_SOCKET_ID) mainSlotId;
    pRI->token = 0xffffffff;
    pRI->clientId = (ClientId) CLIENT_ID_OEM;
    pRI->request = RFX_MSG_REQUEST_QUERY_MODEM_THERMAL;

    RFX_LOG_I(RFX_LOG_TAG, "arg : %s", line->getCurrentLine());
    rfx_enqueue_request_message_client(RFX_MSG_REQUEST_QUERY_MODEM_THERMAL,
            (void *) line->getCurrentLine(), strlen(line->getCurrentLine()), pRI,
            (RIL_SOCKET_ID) mainSlotId, CLIENT_ID_OEM);
    delete line;
}

/// M: CC: For 3G VT only @{
void RilOemClient::executeHangupAll() {
    int mainSlotId = RfxRilUtils::getMajorSim() - 1;
    RIL_SOCKET_ID socId = (RIL_SOCKET_ID) mainSlotId;

    RequestInfo *pRI = (RequestInfo *)calloc(1, sizeof(RequestInfo));
    RFX_ASSERT(pRI != NULL);
    pRI->token = 0xffffffff;        // token is not used in this context
    pRI->socket_id = (RIL_SOCKET_ID) mainSlotId;
    pRI->request = RFX_MSG_REQUEST_HANGUP_ALL;

    rfx_enqueue_request_message(RFX_MSG_REQUEST_HANGUP_ALL, NULL, 0, pRI, socId);
}

void RilOemClient::updateToVtService(RIL_VT_MsgType type, RIL_VT_MsgParams param) {
#ifdef MTK_VT3G324M_SUPPORT
    int ret;
    int paramLen =0;

    switch (type) {
        case MSG_ID_WRAP_3GVT_RIL_CONFIG_INIT_IND:
        case MSG_ID_WRAP_3GVT_RIL_CONFIG_UPDATE_IND:
            bVtExist = true;
            break;
        case MSG_ID_WRAP_3GVT_RIL_CONFIG_DEINIT_IND:
            bVtExist = false;
            break;
        default:
            break;
    }

    if (mVtFd > 0) {
        RFX_LOG_D(RFX_LOG_TAG, "[VT] s_VT_fd is valid");

        // msgType is defined as int in VT Service, so just passing the value.
        int msgType = (int) type;
        ret = send(mVtFd, (const void *) &msgType, sizeof(int), 0);
        RFX_LOG_D(RFX_LOG_TAG, "[VT] send msgType ret = %d", ret);
        if (sizeof(int) != ret) {
            RFX_UNUSED(param);
            goto failed;
        }

        paramLen = sizeof(RIL_VT_MsgParams);
        ret = send(mVtFd, (const void *)&paramLen, sizeof(paramLen), 0);
        RFX_LOG_D(RFX_LOG_TAG, "[VT] send paramLen ret = %d", ret);
        if (sizeof(paramLen) != ret) {
            RFX_UNUSED(param);
            goto failed;
        }

        ret = send(mVtFd, (const void *) &param, paramLen, 0);
        RFX_LOG_D(RFX_LOG_TAG, "[VT] send msgParam ret = %d", ret);
        if (paramLen != ret) {
            goto failed;
        }
    } else {
        RFX_LOG_D(RFX_LOG_TAG, "[VT] s_VT_fd is < 0");
    }
    return;
failed:
    RFX_LOG_D(RFX_LOG_TAG, "[VT] s_VT_fd send fail");
#else
    RFX_UNUSED(type);
    RFX_UNUSED(param);
    RFX_LOG_E(RFX_LOG_TAG, "3G VT not supported");
#endif
}
/// @}

void RilOemClient::freeArgs(char** args, int number) {
    for (int i = 0; i < number; i++) {
        if (args[i]) {
            free(args[i]);
        }
    }
    if (args) {
        free(args);
    }
}

void RilOemClient::executeShutDownByThermal(char* arg) {
    RFX_LOG_I(RFX_LOG_TAG, "executeShutDownByThermal");
    char *cmd;
    int err = 0, modemOn = 0;
    Parcel p;

    RfxAtLine *line = new RfxAtLine(arg, NULL);
    cmd = line->atTokNextstr(&err);
    modemOn = line->atTokNextint(&err);
    int mainSlotId = RfxRilUtils::getMajorSim() - 1;
    RFX_LOG_D(RFX_LOG_TAG, "Thermal line = %s, cmd:%s, modemOn:%d, targetSim: %d",
            arg, cmd, modemOn, mainSlotId);

    RequestInfo *pRI = (RequestInfo *)calloc(1, sizeof(RequestInfo));
    if (pRI == NULL) {
        RFX_LOG_E(RFX_LOG_TAG,"OOM");
        delete line;
        return;
    }
    pRI->socket_id = (RIL_SOCKET_ID) mainSlotId;
    pRI->token = 0xffffffff;
    pRI->clientId = (ClientId) CLIENT_ID_OEM;
    if (modemOn) {
        pRI->request = RFX_MSG_REQUEST_MODEM_POWERON;
    } else {
        pRI->request = RFX_MSG_REQUEST_MODEM_POWEROFF;
    }

    RFX_LOG_I(RFX_LOG_TAG, "arg : %s", line->getCurrentLine());
    rfx_enqueue_request_message_client(pRI->request, NULL, 0, pRI,
            (RIL_SOCKET_ID) mainSlotId, CLIENT_ID_OEM);
    delete line;
}
