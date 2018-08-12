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

#include <string.h>
#include <iostream>
#include <thread>
#include <cutils/sockets.h>
#include <cutils/jstring.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include "RilClient.h"
#include "Rfx.h"
#include "RtcRilClientController.h"
#include "RfxLog.h"
#include "rfx_properties.h"
#include <telephony/mtk_ril.h>
#include "telephony/librilutilsmtk.h"
/*****************************************************************************
 * Class RfxController
 *****************************************************************************/

#define RFX_LOG_TAG "RilClient"
#define NUM_ELEMS(a)  (sizeof (a) / sizeof (a)[0])

#if RILC_LOG
    #define startRequest           sprintf(printBuf, "(")
    #define closeRequest           sprintf(printBuf, "%s)", printBuf)
    #define printRequest(token, req)           \
                RFX_LOG_D(RFX_LOG_TAG, "[%04d]> %s %s", token, RFX_ID_TO_STR(req), printBuf)

    #define startResponse           sprintf(printBuf, "%s {", printBuf)
    #define closeResponse           sprintf(printBuf, "%s}", printBuf)
    #define printResponse           RFX_LOG_D(RFX_LOG_TAG, "%s", printBuf)

    #define clearPrintBuf           printBuf[0] = 0
    #define removeLastChar          printBuf[strlen(printBuf)-1] = 0
    #define appendPrintBuf(x...)    snprintf(printBuf, PRINTBUF_SIZE, x)
#else
    #define startRequest
    #define closeRequest
    #define printRequest(token, req)
    #define startResponse
    #define closeResponse
    #define printResponse
    #define clearPrintBuf
    #define removeLastChar
    #define appendPrintBuf(x...)
#endif

using namespace std;
using namespace android;

static CommandInfo sCommands[] = {
    #include "ril_client_commands.h"
};

static UnsolResponseInfo sUnsolCommands[] = {
    #include "ril_client_unsol_commands.h"
};

#if RILC_LOG
    static char printBuf[PRINTBUF_SIZE];
#endif

static Mutex sMutex;

RilClient::RilClient(int identity, char* socketName) {
    this -> identity = identity;
    this -> socketName = socketName;
    this -> commandFd = -1;
    this -> listenFd = -1;
    this -> clientState = CLIENT_STATE_UNKNOWN;
    this -> stream = NULL;
    setClientState(CLIENT_INITIALIZING);

    mPendingUrc = (UrcList**)calloc(1, sizeof(UrcList*)*getSimCount());
    if (mPendingUrc == NULL) {
        RFX_LOG_E(RFX_LOG_TAG,"OOM");
        return;
    }
    for (int i = 0; i < getSimCount(); i++) {
        mPendingUrc[i] = NULL;
    }
    RFX_LOG_D(RFX_LOG_TAG, "init done");
}

RilClient::~RilClient() {
    if (mPendingUrc != NULL) {
        for (int i = 0; i < getSimCount(); i++) {
            if (mPendingUrc[i] != NULL) {
                // free each data
                UrcList* urc = mPendingUrc[i];
                UrcList* urcTemp;
                while (urc != NULL) {
                    free(urc->data);
                    urcTemp = urc;
                    urc = urc->pNext;
                    free(urcTemp);
                }
                free(mPendingUrc[i]);
                mPendingUrc[i] = NULL;
            }
        }
        free(mPendingUrc);
    }
}

void RilClient::clientStateCallback() {
    RFX_LOG_D(RFX_LOG_TAG, "Enter callback %s", clientStateToString(clientState));
    switch(clientState) {
        case CLIENT_INITIALIZING:
            handleStateInitializing();
            break;
        case CLIENT_ACTIVE:
            handleStateActive();
            break;
        case CLIENT_DEACTIVE:
            handleStateDeactive();
            break;
        case CLIENT_CLOSED:
            handleStateClosed();
            break;
        default:
            break;
    }
}

void RilClient::handleStateInitializing() {

    int ret;
    char* socketName = this -> socketName;
    struct sockaddr_un my_addr;
    struct sockaddr_un peer_addr;

    if (listenFd < 0) {
        listenFd = android_get_control_socket(socketName);
    }

    /* some trial to manually create socket, will work if permission is added
    if (listenFd < 0) {
        RLOGD("init.rc didnt define, create socket manually");
        //do retry if init.rc didn't define socket
        memset(&my_addr, 0, sizeof(struct sockaddr_un));
        my_addr.sun_family = AF_UNIX;

        char path[256];
        sprintf (path, "/data/%s", socketName);
        RLOGD("socketName is %s", path);
        strncpy(my_addr.sun_path, path,
            sizeof(my_addr.sun_path) - 1);

        listenFd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (listenFd < 0) {
            RLOGD("manually listen fail, closed");
            setClientState(CLIENT_CLOSED);
        }

        int ret = ::bind(listenFd, (struct sockaddr *) &my_addr,
                sizeof(struct sockaddr_un));
        if (ret < 0) {
            RLOGD("bind fail, ret = %d, errno = %d, set state to close", ret, errno);
            listenFd = -1;
            setClientState(CLIENT_CLOSED);
        }
    }
    */

    if (listenFd < 0) {
        RFX_LOG_D(RFX_LOG_TAG, "Failed to get socket %s, %s", socketName, strerror(errno));
        setClientState(CLIENT_CLOSED);
        return;
    }

    RFX_LOG_I(RFX_LOG_TAG, "start listen on fd: %d, soicket name: %s", listenFd, socketName);
    ret = listen(listenFd, 4);
    if (ret < 0) {
        RFX_LOG_E(RFX_LOG_TAG, "Failed to listen on control socket '%d': %s",
             listenFd, strerror(errno));
        setClientState(CLIENT_CLOSED);
        return;
    }

    socklen_t socklen = sizeof (peer_addr);
    commandFd = accept (listenFd,  (sockaddr *) &peer_addr, &socklen);
    RFX_LOG_I(RFX_LOG_TAG, "initialize: commandFd is %d", commandFd);

    if (commandFd < 0 ) {
        RFX_LOG_D(RFX_LOG_TAG, "Error on accept() errno:%d", errno);
        setClientState(CLIENT_CLOSED);
        return;
    }
    RFX_LOG_D(RFX_LOG_TAG, "set client state active");
    setClientState(CLIENT_ACTIVE);
}

#define MAX_COMMAND_BYTES (8 * 1024)
void RilClient::handleStateActive() {
    // send pending URC first
    sendPendedUrcs(commandFd);

    RecordStream *p_rs;
    void *p_record;
    size_t recordlen;
    int ret;

    if (stream == NULL) {
        RFX_LOG_D(RFX_LOG_TAG, "Create new stream first time enter active");
        p_rs = record_stream_new(commandFd, MAX_COMMAND_BYTES);
        stream = p_rs;
    } else {
        RFX_LOG_D(RFX_LOG_TAG, "Already have a stream");
        p_rs = stream;
    }
    RFX_LOG_D(RFX_LOG_TAG, "command Fd active is %d", commandFd);
    while(clientState == CLIENT_ACTIVE) {
        /* loop until EAGAIN/EINTR, end of stream, or other error */
        ret = record_stream_get_next(p_rs, &p_record, &recordlen);

        if (ret == 0 && p_record == NULL) {
            /* end-of-stream */
            break;
        } else if (ret < 0) {
            break;
        } else if (ret == 0) { /* && p_record != NULL */
            processCommands(p_record, recordlen, identity);
        }
    }

    if (ret == 0 || !(errno == EAGAIN || errno == EINTR)) {
        RFX_LOG_D(RFX_LOG_TAG, "socket read: ret: %d, errno: %d", ret, errno);
        /* fatal error or end-of-stream */
        if (ret != 0) {
            RFX_LOG_D(RFX_LOG_TAG, "error on reading command socket errno:%d\n", errno);
        } else {
            RFX_LOG_D(RFX_LOG_TAG, "EOS.  Closing command socket.");
        }
        setClientState(CLIENT_DEACTIVE);
    }  else {
        RFX_LOG_D(RFX_LOG_TAG, "socket read: ret: %d, errno: %d, keep reading", ret, errno);
        setClientState(CLIENT_ACTIVE);
    }
}

void RilClient::handleStateDeactive() {
    if (commandFd != -1) {
        RFX_LOG_D(RFX_LOG_TAG, "clear Fd Command %d", commandFd);
        close(commandFd);
        commandFd = -1;
    } else {
        RFX_LOG_D(RFX_LOG_TAG, "commandFd alread -1");
    }

    if (stream != NULL) {
        RFX_LOG_D(RFX_LOG_TAG, "clear stream because stream exists");
        record_stream_free(stream);
        stream = NULL;
    } else {
        RFX_LOG_D(RFX_LOG_TAG, "stream null here");
    }

    setClientState(CLIENT_INITIALIZING);
}

void RilClient::handleStateClosed() {
    if (commandFd != -1) {
        RFX_LOG_D(RFX_LOG_TAG, "closed: clear Fd Command %d", commandFd);
        close(commandFd);
        commandFd = -1;
    } else {
        RFX_LOG_D(RFX_LOG_TAG, "closed: commandFd alread -1");
    }

    if (stream != NULL) {
        RFX_LOG_D(RFX_LOG_TAG, "closed: clear stream because stream exists");
        record_stream_free(stream);
        stream = NULL;
    } else {
        RFX_LOG_D(RFX_LOG_TAG, "closed: stream null here");
    }
}

void RilClient::setClientState(RilClientState state) {
    if (clientState != state) {
        RFX_LOG_D(RFX_LOG_TAG, "set client state %s with old state %s",
            clientStateToString(state), clientStateToString(clientState));
        clientState = state;
    } else {
        RFX_LOG_D(RFX_LOG_TAG, "client state is already %s", clientStateToString(state));
    }
    activityThread = new StateActivityThread(this);
    activityThread -> run("StateThread");
}

char* RilClient::clientStateToString(RilClientState state) {
    char* ret;
    switch(state) {
        case CLIENT_INITIALIZING:
            ret =  (char *) "CLIENT_INITIALIZING";
            break;
        case CLIENT_ACTIVE:
            ret =  (char *) "CLIENT_ACTIVE";
            break;
        case CLIENT_DEACTIVE:
            ret =  (char *) "CLIENT_DEACTIVE";
            break;
        case CLIENT_CLOSED:
            ret = (char *) "CLIENT_CLOSED";
            break;
        default:
            ret = (char *) "NO_SUCH_STATE";
            break;
    }
    return ret;
}

RilClient::StateActivityThread::StateActivityThread (RilClient* client){
    this -> client = client;
    RFX_LOG_D(RFX_LOG_TAG, "Consctruct Activity thread");
}

RilClient::StateActivityThread::~StateActivityThread() {
    RFX_LOG_D(RFX_LOG_TAG, "Desctruct Activity thread");
}

bool RilClient::StateActivityThread::threadLoop() {
    client -> clientStateCallback();
    return false;
}


void RilClient::processCommands(void *buffer, size_t buflen, int clientId) {
    Parcel p;
    status_t status;
    int32_t request;
    int32_t token;
    RequestInfo *pRI;

    p.setData((uint8_t *) buffer, buflen);
    status = p.readInt32(&request);
    status = p.readInt32 (&token);

    char prop_value[RFX_PROPERTY_VALUE_MAX] = {0};
    rfx_property_get(PROPERTY_3G_SIM, prop_value, "1");
    int capabilitySim = atoi(prop_value) - 1;

    RFX_LOG_D(RFX_LOG_TAG, "enqueue request id %d with token %d for client %d to slot = %d",
            request, token, clientId, capabilitySim);

    pRI = (RequestInfo *)calloc(1, sizeof(RequestInfo));
    if (pRI == NULL) {
        RFX_LOG_E(RFX_LOG_TAG,"OOM");
        return;
    }
    pRI->socket_id = (RIL_SOCKET_ID) capabilitySim;
    pRI->token = token;
    pRI->clientId = (ClientId) clientId;
    for (unsigned int i = 0; i < NUM_ELEMS(sCommands); i++) {
        if (request == sCommands[i].requestNumber) {
            RFX_LOG_D(RFX_LOG_TAG, "find entry! request = %d", request);
            pRI->pCI = &(sCommands[i]);
            pRI->pCI->dispatchFunction(p, pRI);
            return;
        }
    }
    free(pRI);
    RFX_LOG_E(RFX_LOG_TAG, "Didn't find any entry, please check ril_client_commands.h");
}

void RilClient::handleUnsolicited(int slotId, int unsolResponse, void *data,
        size_t datalen, UrcDispatchRule rule) {
    RFX_UNUSED(rule);

    int ret;
    UnsolResponseInfo *pUI = NULL;
    char prop_value[RFX_PROPERTY_VALUE_MAX] = {0};
    rfx_property_get(PROPERTY_3G_SIM, prop_value, "1");
    int capabilitySim = atoi(prop_value) - 1;
    RFX_LOG_D(RFX_LOG_TAG, "capabilitySim = %d", capabilitySim);

    if(capabilitySim != slotId) {
        RFX_LOG_D(RFX_LOG_TAG, "only handle capabilitySim");
        return;
    }

    if (commandFd == -1) {
        RFX_LOG_D(RFX_LOG_TAG, "command Fd not ready here");
        // try to cache URC
        cacheUrc(unsolResponse, data, datalen, rule, slotId);
        return;
    }

    for (unsigned int i = 0; i < NUM_ELEMS(sUnsolCommands); i++) {
        if (unsolResponse == sUnsolCommands[i].requestNumber) {
            pUI = &(sUnsolCommands[i]);
            break;
        }
    }
    if (pUI == NULL) {
        RFX_LOG_E(RFX_LOG_TAG, "didn't find unsolResposnInfo");
        return;
    }

    Parcel p;
    p.writeInt32 (RESPONSE_UNSOLICITED);
    p.writeInt32 (unsolResponse);
    ret = pUI->responseFunction(p, data, datalen);

    if (ret != 0) {
        RFX_LOG_D(RFX_LOG_TAG, "ret = %d, just return", ret);
        return;
    }

    RtcRilClientController::sendResponse(p, commandFd);
}

void RilClient::addHeaderToResponse(Parcel* p) {
    RFX_UNUSED(p);
    RFX_LOG_D(RFX_LOG_TAG, "Add nothing under default behaviour");
    return;
}

void RilClient::requestComplete(RIL_Token token, RIL_Errno e, void *response,
        size_t responselen) {
    if (commandFd < 0) {
        RFX_LOG_D(RFX_LOG_TAG, "command Fd not ready here");
        return;
    }

    int ret;
    size_t errorOffset;
    RequestInfo *pRI = (RequestInfo *) token;

    Parcel p;
    p.writeInt32(RESPONSE_SOLICITED);
    p.writeInt32(pRI->token);
    errorOffset = p.dataPosition();
    p.writeInt32 (e);

    if (response != NULL) {
        // ret = p.write(response, responselen);
        ret = pRI->pCI->responseFunction(p, response, responselen);

        if (ret != 0) {
            RFX_LOG_D(RFX_LOG_TAG, "responseFunction error, ret %d", ret);
            p.setDataPosition(errorOffset);
            p.writeInt32 (ret);
        }
    }

    if (e != RIL_E_SUCCESS) {
        RFX_LOG_D(RFX_LOG_TAG, "fails by %d", e);
    }

    RtcRilClientController::sendResponse(p, commandFd);
    free(pRI);
}

void RilClient::cacheUrc(int unsolResponse, const void *data, size_t datalen,
         UrcDispatchRule rule, int slotId) {
    Mutex::Autolock autoLock(sMutex);
    //Only the URC list we wanted.
    if (!isNeedToCache(unsolResponse)) {
        RFX_LOG_I(RFX_LOG_TAG, "Don't need to cache the request %d", unsolResponse);
        return;
    }
    UrcList* urcCur = mPendingUrc[slotId];
    UrcList* urcPrev = NULL;
    int pendedUrcCount = 0;

    while (urcCur != NULL) {
        RFX_LOG_D(RFX_LOG_TAG, "Pended Vsim URC:%d, slot:%d, :%d",
            pendedUrcCount,
            slotId,
            urcCur->id);
        urcPrev = urcCur;
        urcCur = urcCur->pNext;
        pendedUrcCount++;
    }
    urcCur = (UrcList*)calloc(1, sizeof(UrcList));
    if (urcCur == NULL) {
        RFX_LOG_E(RFX_LOG_TAG,"OOM");
        return;
    }
    if (urcPrev != NULL)
        urcPrev->pNext = urcCur;
    urcCur->pNext = NULL;
    urcCur->id = unsolResponse;
    urcCur->datalen = datalen;
    urcCur->data = (char*)calloc(1, datalen + 1);
    if (urcCur->data == NULL) {
        RFX_LOG_E(RFX_LOG_TAG,"OOM");
        free(urcCur);
        return;
    }
    urcCur->data[datalen] = 0x0;
    memcpy(urcCur->data, data, datalen);
    urcCur->rule = rule;
    if (pendedUrcCount == 0) {
        mPendingUrc[slotId] = urcCur;
    }
    RFX_LOG_D(RFX_LOG_TAG, "[Slot %d] Current pendedVsimUrcCount = %d", slotId, pendedUrcCount + 1);
}

void RilClient::sendUrc(int slotId, UrcList* urcCached) {
    UrcList* urc = urcCached;
    UrcList* urc_temp;
    while (urc != NULL) {
        RFX_LOG_D(RFX_LOG_TAG, "sendVsimPendedUrcs RIL%d, %d", slotId, urc->id);
        handleUnsolicited(slotId, urc->id, urc->data, urc->datalen, urc->rule);
        free(urc->data);
        urc_temp = urc;
        urc = urc->pNext;
        free(urc_temp);
    }
}

void RilClient::sendPendedUrcs(int fdCommand) {
    Mutex::Autolock autoLock(sMutex);
    RFX_LOG_D(RFX_LOG_TAG, "Ready to send pended Vsim URCs, fdCommand:%d", fdCommand);
    if (fdCommand != -1) {
        for (int i = 0; i < getSimCount(); i++) {
            sendUrc(i, mPendingUrc[i]);
            mPendingUrc[i] = NULL;
        }
    }
}

bool RilClient::isNeedToCache(int unsolResponse __unused) {
    return false;
}

static char *
strdupReadString(Parcel &p) {
    size_t stringlen;
    const char16_t *s16;

    s16 = p.readString16Inplace(&stringlen);

    return strndup16to8(s16, stringlen);
}

static void writeStringToParcel(Parcel &p, const char *s) {
    char16_t *s16;
    size_t s16_len;
    s16 = strdup8to16(s, &s16_len);
    p.writeString16(s16, s16_len);
    free(s16);
}

/**
 * Marshall the signalInfoRecord into the parcel if it exists.
 */
static void marshallSignalInfoRecord(Parcel &p,
        RIL_CDMA_SignalInfoRecord &p_signalInfoRecord) {
    p.writeInt32(p_signalInfoRecord.isPresent);
    p.writeInt32(p_signalInfoRecord.signalType);
    p.writeInt32(p_signalInfoRecord.alertPitch);
    p.writeInt32(p_signalInfoRecord.signal);
}

static void
invalidCommandBlock(RequestInfo *pRI) {
    RFX_LOG_E(RFX_LOG_TAG, "invalid command block for token %d request %s",
            pRI->token, RFX_ID_TO_STR(pRI->pCI->requestNumber));
}

void dispatchInts(Parcel &p, RequestInfo *pRI) {
    int32_t count;
    status_t status;
    size_t datalen;
    int *pInts;

    status = p.readInt32 (&count);

    if (status != NO_ERROR || count <= 0) {
        goto invalid;
    }

    datalen = sizeof(int) * count;
    pInts = (int *)calloc(count, sizeof(int));
    if (pInts == NULL) {
        RFX_LOG_E(RFX_LOG_TAG, "Memory allocation failed for request %s",
                RFX_ID_TO_STR(pRI->pCI->requestNumber));
        return;
    }

    for (int i = 0 ; i < count ; i++) {
        int32_t t;

        status = p.readInt32(&t);
        pInts[i] = (int)t;
        if (status != NO_ERROR) {
            free(pInts);
            goto invalid;
        }
   }
   rfx_enqueue_request_message_client(pRI->pCI->requestNumber, pInts, datalen, (void *)pRI,
        pRI->socket_id, pRI->clientId);

#ifdef MEMSET_FREED
    memset(pInts, 0, datalen);
#endif
    free(pInts);
    return;
invalid:
    // free(pRI);
    return;
}

int responseInts(Parcel &p, void *response, size_t responselen) {
    int numInts;

    if (response == NULL && responselen != 0) {
        RFX_LOG_E(RFX_LOG_TAG, "invalid response: NULL");
        return RIL_ERRNO_INVALID_RESPONSE;
    }
    if (responselen % sizeof(int) != 0) {
        RFX_LOG_E(RFX_LOG_TAG, "responseInts: invalid response length %d expected multiple of %d\n",
            (int)responselen, (int)sizeof(int));
        return RIL_ERRNO_INVALID_RESPONSE;
    }

    int *p_int = (int *) response;

    numInts = responselen / sizeof(int);
    p.writeInt32 (numInts);

    /* each int*/
    for (int i = 0 ; i < numInts ; i++) {
        p.writeInt32(p_int[i]);
    }
    return 0;
}

int responseVoid(Parcel &p, void *response, size_t responselen) {
    RFX_UNUSED(p);
    RFX_UNUSED(response);
    RFX_UNUSED(responselen);
    return 0;
}

// For build pass only
void dispatchString(Parcel &p, RequestInfo *pRI) {
    status_t status;
    size_t datalen;
    size_t stringlen;
    char *string8 = NULL;

    string8 = strdupReadString(p);

    startRequest;
    appendPrintBuf("%s%s", printBuf, string8);
    closeRequest;
    printRequest(pRI->token, pRI->pCI->requestNumber);

    rfx_enqueue_request_message_client(pRI->pCI->requestNumber, string8, strlen(string8), pRI,
        pRI->socket_id, pRI->clientId);

#ifdef MEMSET_FREED
    memsetString(string8);
#endif

    free(string8);
    return;
}
// For build pass only
int responseString(Parcel &p, void *response, size_t responselen __unused) {
    /* one string only */
    startResponse;
    appendPrintBuf("%s%s", printBuf, (char*)response);
    closeResponse;

    writeStringToParcel(p, (const char *)response);

    return 0;
}

void dispatchStrings (Parcel &p, RequestInfo *pRI) {
    int32_t countStrings;
    status_t status;
    size_t datalen;
    char **pStrings;

    status = p.readInt32 (&countStrings);

    if (status != NO_ERROR) {
        goto invalid;
    }

    startRequest;
    if (countStrings == 0) {
        // just some non-null pointer
        pStrings = (char **)calloc(1, sizeof(char *));
        if (pStrings == NULL) {
            RFX_LOG_E(RFX_LOG_TAG, "Memory allocation failed for request %s",
                    RFX_ID_TO_STR(pRI->pCI->requestNumber));
            closeRequest;
            return;
        }

        datalen = 0;
    } else if (countStrings < 0) {
        pStrings = NULL;
        datalen = 0;
    } else {
        datalen = sizeof(char *) * countStrings;

        pStrings = (char **)calloc(countStrings, sizeof(char *));
        if (pStrings == NULL) {
            RFX_LOG_E(RFX_LOG_TAG, "Memory allocation failed for request %s",
                    RFX_ID_TO_STR(pRI->pCI->requestNumber));
            closeRequest;
            return;
        }

        for (int i = 0 ; i < countStrings ; i++) {
            pStrings[i] = strdupReadString(p);
            appendPrintBuf("%s%s,", printBuf, pStrings[i]);
        }
    }
    removeLastChar;
    closeRequest;
    printRequest(pRI->token, pRI->pCI->requestNumber);

    rfx_enqueue_request_message_client(pRI->pCI->requestNumber, pStrings, datalen, pRI,
        pRI->socket_id, pRI->clientId);

    if (pStrings != NULL) {
        for (int i = 0 ; i < countStrings ; i++) {
#ifdef MEMSET_FREED
            memsetString (pStrings[i]);
#endif
            free(pStrings[i]);
        }

#ifdef MEMSET_FREED
        memset(pStrings, 0, datalen);
#endif
        free(pStrings);
    }

    return;
invalid:
    invalidCommandBlock(pRI);
    return;
}

void dispatchVoid (Parcel& p, RequestInfo *pRI) {
    RFX_UNUSED(p);
    clearPrintBuf;
    printRequest(pRI->token, pRI->pCI->requestNumber);
    rfx_enqueue_request_message_client(pRI->pCI->requestNumber, NULL, 0, pRI,
        pRI->socket_id, pRI->clientId);
}

void dispatchRaw(Parcel &p, RequestInfo *pRI) {
    int32_t len;
    status_t status;
    const void *data;

    status = p.readInt32(&len);

    if (status != NO_ERROR) {
        goto invalid;
    }

    // The java code writes -1 for null arrays
    if (((int)len) == -1) {
        data = NULL;
        len = 0;
    }

    data = p.readInplace(len);

    startRequest;
    appendPrintBuf("%sraw_size=%d", printBuf, len);
    closeRequest;
    printRequest(pRI->token, pRI->pCI->requestNumber);

    rfx_enqueue_request_message_client(pRI->pCI->requestNumber, const_cast<void *>(data), len, pRI,
        pRI->socket_id, pRI->clientId);

    return;
invalid:
    invalidCommandBlock(pRI);
    return;
}

int responseRaw(Parcel &p, void *response, size_t responselen) {
    if (response == NULL && responselen != 0) {
        RFX_LOG_E(RFX_LOG_TAG, "invalid response: NULL with responselen != 0");
        return RIL_ERRNO_INVALID_RESPONSE;
    }

    // The java code reads -1 size as null byte array
    if (response == NULL) {
        p.writeInt32(-1);
    } else {
        p.writeInt32(responselen);
        p.write(response, responselen);
    }

    return 0;
}

/** response is a char **, pointing to an array of char *'s */
int responseStrings(Parcel &p, void *response, size_t responselen) {
    int numStrings;

    if (response == NULL && responselen != 0) {
        RFX_LOG_E(RFX_LOG_TAG, "invalid response: NULL");
        return RIL_ERRNO_INVALID_RESPONSE;
    }
    if (responselen % sizeof(char *) != 0) {
        RFX_LOG_E(RFX_LOG_TAG, "responseStrings: invalid response length %d expected multiple of %d\n",
                (int)responselen, (int)sizeof(char *));
        return RIL_ERRNO_INVALID_RESPONSE;
    }

    if (response == NULL) {
        p.writeInt32 (0);
    } else {
        char **p_cur = (char **) response;

        numStrings = responselen / sizeof(char *);
        p.writeInt32 (numStrings);

        /* each string*/
        startResponse;
        for (int i = 0 ; i < numStrings ; i++) {
            appendPrintBuf("%s%s,", printBuf, (char*)p_cur[i]);
            writeStringToParcel (p, p_cur[i]);
        }
        removeLastChar;
        closeResponse;
    }
    return 0;
}
