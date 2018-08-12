/* Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein
 * is confidential and proprietary to MediaTek Inc. and/or its licensors.
 * Without the prior written permission of MediaTek inc. and/or its licensors,
 * any reproduction, modification, use or disclosure of MediaTek Software,
 * and information contained herein, in whole or in part, shall be strictly prohibited.
 */
/* MediaTek Inc. (C) 2016. All rights reserved.
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
#include <gtest/gtest.h>
#include <cutils/properties.h>
#include <string.h>
#include "RfxRilAdapter.h"
#include "Rfx.h"
#include "RtstEnv.h"
#include "RfxBasics.h"
#include "RfxObject.h"
#include "RfxChannelManager.h"
#include "rfx_properties.h"
#include "RfxRootController.h"
#include "RtstHandler.h"
#include <sys/time.h>
#include <telephony/librilutilsmtk.h>

/*****************************************************************************
 * Define
 *****************************************************************************/
#define TAG "RTF"

/*****************************************************************************
 * Static functions
 *****************************************************************************/
#if defined(ANDROID_MULTI_SIM)
static void request(
        int request,
        void *data,
        size_t datalen,
        RIL_Token t,
        RIL_SOCKET_ID socket_id) {
#else
static request(
        int request,
        void *data,
        size_t datalen,
        RIL_Token t) {
    RIL_SOCKET_ID socket_id = RIL_SOCKET_1
#endif
    RFX_LOG_D(TAG, "rfx_enqueue_request_message %d", request);
    if (RtstEnv::get()->isTestMode()) {
        RtstEnv::get()->mockEnqueueMessage(request, data, datalen, t, socket_id);
    } else {
        rfx_enqueue_request_message(request, data, datalen, t, socket_id);
    }
}

/*****************************************************************************
 * Init Delay Timer
 *****************************************************************************/
#define INIT_DELAY_TIME (200)

static timer_t initDelayTimer;
static bool delayDone = false;
static void stopTimer();

static void initDelayCb(sigval_t sig) {
    RFX_UNUSED(sig);
    const int size = 2048;
    unsigned char buf[size] = {0};
    strncpy((char *)buf, "OK", 2);
    RtstEnv::get()->getInitSocket2().write(buf, 2);;
    stopTimer();
    delayDone = true;
}

static void createInitDelayTimer(void) {
    struct sigevent sevp;
    memset(&sevp, 0, sizeof(sevp));
    sevp.sigev_value.sival_int = 0;
    sevp.sigev_notify = SIGEV_THREAD;
    sevp.sigev_notify_function = initDelayCb;

    if(timer_create(CLOCK_MONOTONIC, &sevp, &initDelayTimer) == -1) {
        RFX_LOG_E("RTF", "timer_create  failed reason=[%s]", strerror(errno));
        RFX_ASSERT(0);
    }
}

static void startTimer(int milliseconds) {
    struct itimerspec expire;
    expire.it_interval.tv_sec = 0;
    expire.it_interval.tv_nsec = 0;
    expire.it_value.tv_sec = milliseconds/1000;
    expire.it_value.tv_nsec = (milliseconds%1000)*1000000;
    timer_settime(initDelayTimer, 0, &expire, NULL);
}


static void stopTimer() {
    startTimer(0);
}


/*****************************************************************************
 * class RtstCallback
 *****************************************************************************/
int RtstCallback::handleEvent(int fd, int events, void* data) {
    RFX_UNUSED(data);
    const int size = 2048;
    unsigned char buf[size] = {0};
    RFX_LOG_D(TAG, "fd %d handleEvent", fd);

    if (events == Looper::EVENT_INPUT) {
        static int count = 0;
        RFX_LOG_V(TAG, "fd %d", fd);
        if ((m_env->getMode() == RtstEnv::WORK) && (!delayDone)) {
            RtstFd tempFd(fd);
            memset(buf, 0, 2048);
            int num;
            int slotId = -1;
            if (m_env->isRilSocket1Fd(fd, &slotId)) {
                RFX_LOG_D(TAG, "handle init URC fd %d", fd);
                while (RtstUtils::pollWait(fd, 0) > 0) {
                    tempFd.read(buf, 4);
                    Parcel q;
                    q.setData(buf, 4);
                    q.setDataPosition(0);
                    int size;
                    q.readInt32(&size);
                    num = tempFd.read(buf, size);
                    m_env->addInitRilUrcData(slotId, buf, size);
                }
                tempFd.setFd(-1);
                return 1;
            }
            num = tempFd.read(buf, 2048);
            if (num <= 0 || num >= 2048) {
                return 1;
            }
            buf[num] = '\0';
            m_env->isAtSocket1Fd(fd, &slotId);
            RtstInitAtRsp *p = NULL;
            if (strncmp((const char*)buf, "UT_", strlen("UT_")) == 0) {
                count++;
            } else {
                p = m_env->getInitAtRsp(String8((const char*)buf), slotId);
            }
            if (p == NULL) {
                strncpy((char *)buf, "ERROR\r", 6);
                tempFd.write(buf, 6);
            } else {
                const Vector<String8> &v = p->getAtResponse();
                for (int i = 0; i < (int)v.size(); i++) {
                    tempFd.write((void *)v[i].string(), v[i].length());
                }
            }
            tempFd.setFd(-1);
            RFX_LOG_D(TAG, "count: %d", count);
            RFX_LOG_D(TAG, "target: %d", (getSimCount() * RFX_MAX_CHANNEL_NUM - getSimCount()));
            if (count == (getSimCount() * RFX_MAX_CHANNEL_NUM - getSimCount())) {
                count++;
                RFX_LOG_D(TAG, "AT Init done");
                startTimer(INIT_DELAY_TIME);
            }
            return 1;
        }

        if (m_env->getMode() == RtstEnv::TEST) {
            if (fd == m_env->getRilSocket2(0).getFd()) {
                int num = m_env->getRilSocket2(0).read(buf, 2048);
                Parcel p;
                p.setData(buf, num);
                int id;
                p.readInt32(&id);
                RFX_ASSERT(RIL_REQUEST_CDMA_SMS_BROADCAST_ACTIVATION == id);
                int socket_id;
                p.readInt32(&socket_id);
                int64_t t;
                p.readInt64(&t);
                int value = 100;
                p.readInt32(&value);
                RFX_LOG_D(TAG, "Handle RIL request %d, %d", id, value);

                strncpy((char *)buf, "AT+ECSCB=0\r", 11);
                m_env->getAtSocket2(RIL_CMD_PROXY_1, 0).write((char *)buf, strlen((const char*)buf));
            } else if (fd == m_env->getAtSocket2(RIL_CMD_PROXY_1, 0).getFd()) {
                int num = m_env->getAtSocket2(RIL_CMD_PROXY_1, 0).read(buf, 2048);
                RFX_LOG_D(TAG, "AT response %s", buf);
                Parcel p;
                p.writeInt32(RIL_REQUEST_CDMA_SMS_BROADCAST_ACTIVATION);
                p.writeInt32(RIL_E_SUCCESS);
                RtstUtils::writeStringToParcel(p, "victor");
                Parcel q;
                q.writeInt32(p.dataSize());
                RtstEnv::get()->getRilSocket2(0)
                        .write((void *)q.data(), q.dataSize());
                m_env->getRilSocket2(0).write((void *)p.data(), p.dataSize());
            }
        }
    }
    return 1;
}

/*****************************************************************************
 * class RtstEnv
 *****************************************************************************/
::testing::Environment * const RtstEnv::s_env
    = ::testing::AddGlobalTestEnvironment(new RtstEnv());

void RtstEnv::mockEnqueueMessage(
        int request, void *data, size_t datalen, RIL_Token t, RIL_SOCKET_ID socket_id) {
    RFX_LOG_D(TAG, "mockEnqueueMessage");
    Parcel p;
    p.writeInt32(request);
    p.writeInt32(socket_id);
    p.writeInt64((int64_t)t);
    p.write(data, datalen);
    getRilSocket1((int)socket_id).write((void *)p.data(), p.dataSize());
}


void RtstEnv::initFd(int fd[MAX_SIM_COUNT][RFX_MAX_CHANNEL_NUM]) {
    RFX_LOG_D(TAG, "RtstEnv::initFd");
    for (int i = 0; i < getSimCount(); i++) {
        RtstSocketPair *ril = new RtstSocketPair();
        m_rilSocketPairs.push(ril);
        RtstSocketPair *rilReqAck = new RtstSocketPair();
        m_RilReqAckSocketPairs.push(rilReqAck);

        Vector<RtstSocketPair *> * atPair = new Vector<RtstSocketPair* >();
        for (int j = 0; j < RFX_MAX_CHANNEL_NUM; j++) {
            RtstSocketPair *at = new RtstSocketPair();
            atPair->push(at);
            fd[i][j] = at->getSocket2().getFd();
            RFX_LOG_D(TAG, "RtstEnv::initFd fd[%d][%d]: %d",i,j,fd[i][j]);
        }
        m_atSocketPairs.push(atPair);
    }
}


void RtstEnv::deinitFd() {
    Vector<RtstSocketPair *>::iterator it1;
    for (it1 = m_rilSocketPairs.begin(); it1 != m_rilSocketPairs.end();) {
        delete (*it1);
        it1 = m_rilSocketPairs.erase(it1);
    }
    m_rilSocketPairs.clear();

    Vector<RtstSocketPair *>::iterator it2;
    for (it2 = m_RilReqAckSocketPairs.begin(); it2 != m_RilReqAckSocketPairs.end();) {
        delete (*it2);
        it2 = m_RilReqAckSocketPairs.erase(it2);
    }
    m_RilReqAckSocketPairs.clear();

    Vector<Vector<RtstSocketPair*> *>::iterator it4;
    for (it4 = m_atSocketPairs.begin(); it4 != m_atSocketPairs.end(); it4++) {
        Vector<RtstSocketPair *>::iterator it41;
        for (it41 = (*it4)->begin(); it41 != (*it4)->end(); it41++) {
            delete (*it41);
        }
        (*it4)->clear();
        delete (*it4);
    }
    m_atSocketPairs.clear();
}


void RtstEnv::initForTest() {
    static bool inited = false;
    if (!inited) {
        int fd[MAX_SIM_COUNT][RFX_MAX_CHANNEL_NUM];
        initFd(fd);
        RtstGRil::setOnRequestCallback(request);
    }
}


void RtstEnv::init() {
    static bool inited = false;
    RFX_LOG_D(TAG, "RtstEnv::init");
    if (!inited) {
        setMode(WORK);
        int fd[MAX_SIM_COUNT][RFX_MAX_CHANNEL_NUM];
        initFd(fd);
        /// SET MockFlag, ChannelFd, RilEnv
        RtstMRil::setEmulatorMode();
        RtstMRil::setChannelFd(fd);
        RtstMRil::setRilEnv();
        RtstMRil::setCallbackForStatusUpdate();
        initBootAtRecvThread();
        createInitDelayTimer();
        rfx_init();
        inited = true;
        RtstGRil::setOnRequestCallback(request);
        RtstGRil::setVersion(RIL_VERSION);
        const RtstFd &s = getInitSocket1();
        unsigned char buf[1024];
        int len = s.read(buf, 1024);
        RFX_LOG_D(TAG, "RtstEnv::init Done");
    }
}

void RtstEnv::initBootAtRecvThread() {
    int *fd = new int[getSimCount() * RFX_MAX_CHANNEL_NUM + getSimCount()];
    int count = 0;
    Vector<RtstSocketPair *>::iterator it2;
    for (it2 = m_rilSocketPairs.begin(); it2 != m_rilSocketPairs.end(); it2++) {
        fd[count++] = (*it2)->getSocket1().getFd();
    }
    Vector<Vector<RtstSocketPair*> *>::iterator it4;
    for (it4 = m_atSocketPairs.begin(); it4 != m_atSocketPairs.end(); it4++) {
        Vector<RtstSocketPair *>::iterator it41;
        for (it41 = (*it4)->begin(); it41 != (*it4)->end(); it41++) {
            fd[count] =  (*it41)->getSocket1().getFd();
            RFX_LOG_D(TAG, "RtstEnv::initBootAtRecvThread %d ", fd[count]);
            count++;
        }
    }
    RFX_LOG_D(TAG, "RtstEnv::initBootAtRecvThread total:%d ", count);
    sp<RtstThread> t = RtstThread::create(fd, count);
    t->setCallback(new RtstCallback(this));
    delete[] fd;
}

void RtstEnv::initTestThread() {
    static bool inited = 0;
    if (!inited) {
        int *fd = new int[getSimCount() + getSimCount() * RFX_MAX_CHANNEL_NUM];
        int count = 0;
        Vector<RtstSocketPair *>::iterator it2;
        for (it2 = m_rilSocketPairs.begin(); it2 != m_rilSocketPairs.end(); it2++) {
            fd[count++] = (*it2)->getSocket2().getFd();
        }
        Vector<Vector<RtstSocketPair*> *>::iterator it4;
        for (it4 = m_atSocketPairs.begin(); it4 != m_atSocketPairs.end(); it4++) {
            Vector<RtstSocketPair *>::iterator it41;
            for (it41 = (*it4)->begin(); it41 != (*it4)->end(); it41++) {
                fd[count++] =  (*it41)->getSocket2().getFd();
            }
        }
        sp<RtstThread> t = RtstThread::create(fd, count);
        t->setCallback(new RtstCallback(this));
        inited = true;
        delete[] fd;
    }
}

void RtstEnv::deinit() {
    deinitFd();
}


void RtstEnv::sendRilRequest(int id, int slotId, Parcel &data) {
    RFX_LOG_D(TAG, "sendRilRequest id = %d, slotId = %d", id, slotId);
    RequestInfo *pRI = new RequestInfo();
    static int token = 0;
    pRI->token = token++;
    pRI->pCI = RtstGRil::getCommandInfo(id);
    pRI->p_next = NULL;
    pRI->socket_id = (RIL_SOCKET_ID)slotId;
    pRI->pCI->dispatchFunction(data, pRI);
    pRI->cancelled = 0;
    m_requestInfo.push(pRI);
}


void RtstEnv::sendAtResponse(int channelId, int slotId, const char *rsp) {
    RFX_LOG_D(TAG, "sendAtResponse channelId = %d, slotId = %d, rsp = %s", channelId, slotId, rsp);
    String8 at = String8(rsp) + String8("\r");
    const RtstFd &s = getAtSocket1(channelId, slotId);
    s.write((void *)at.string(), at.length());
}


void RtstEnv::sendUrcString(int channelId, int slotId, const char *urc) {
    RFX_LOG_D(TAG, "sendUrcString slotId = %d, urc = %s", slotId, urc);
    String8 at = String8(urc) + String8("\r");
    const RtstFd &s = getAtSocket1(channelId, slotId);
    s.write((void *)at.string(), at.length());
}


void RtstEnv::setSysProp(const char *key, const char *value) {
    RFX_LOG_D(TAG, "setSysProp key = %s, value = %s", key, value);
    rfx_property_set(key, value);
}


void RtstEnv::setStatus(int slot,  const RfxStatusKeyEnum key, const RfxVariant &value) {
    RFX_LOG_D(TAG, "setStatus slot = %d, key = %d, value = %s", slot, key,
            value.toString().string());
    RtstUtils::setStatusValueForGT(slot, key, value, true);
}


bool RtstEnv::getExpectedAt(int channelId, int slotId, String8 &expectedAt) {
    const RtstFd &s = getAtSocket1(channelId, slotId);
    int ret = RtstUtils::pollWait(s.getFd(), m_timeout);
    if (ret <=0 ) {
        return false;
    }
    char buf[1024];
    int len = s.read(buf, 1024);
    if ((len <= 0) || (len >= 1024)) {
        return false;
    }
    buf[len] = '\0';
    char *p = strchr(buf, '\r');
    if (p == NULL) {
        return false;
    }
    *p = '\0';
    expectedAt.setTo(buf);
    return true;
}

bool RtstEnv::getExpectedRilRsp(int slotId, int &reqId, int &error, Parcel &p) {
    const RtstFd &s = getRilSocket1(slotId);
    int ret = RtstUtils::pollWait(s.getFd(), m_timeout);
    if (ret <=0 ) {
        return false;
    }
    unsigned char buf[1024];
    s.read(buf, 4);
    Parcel q;
    q.setData(buf, 4);
    q.setDataPosition(0);
    int size;
    q.readInt32(&size);
    int len = s.read(buf, size);
    p.setData(buf, len);
    p.setDataPosition(0);
    p.readInt32(&reqId);
    p.readInt32(&error);
    return true;
}

bool RtstEnv::getExpectedRilReqAck(int slotId, int &reqId, Parcel &p) {
    const RtstFd &s = getRilReqAckSocket1(slotId);
    int ret = RtstUtils::pollWait(s.getFd(), m_timeout);
    if (ret <=0 ) {
        return false;
    }
    unsigned char buf[1024];
    int len = s.read(buf, 1024);
    if (len < 0) {
       return false;
    }
    p.setData(buf, len);
    p.setDataPosition(0);
    p.readInt32(&reqId);
    return true;
}

bool RtstEnv::getExpectedRilUrc(int slotId, int &urcId, Parcel &p) {
    const RtstFd &s = getRilSocket1(slotId);
    int ret = RtstUtils::pollWait(s.getFd(), m_timeout);
    if (ret <=0 ) {
        return false;
    }
    unsigned char buf[1024];
    s.read(buf, 4);
    Parcel q;
    q.setData(buf, 4);
    q.setDataPosition(0);
    int size;
    q.readInt32(&size);
    int len = s.read(buf, size);
    p.setData(buf, len);
    p.setDataPosition(0);
    p.readInt32(&urcId);
    return true;
}

bool RtstEnv::getExpectedInitRilUrc(int slotId, int expectedUrc,int &urcId, Parcel &p) {
      if (slotId <0 || slotId >= RTST_MAX_SIM_COUNT) {
          RFX_ASSERT(0);
          return false;
      }
      Vector<Parcel *>::iterator it;
      for (it = m_initUrcData[slotId].begin(); it != m_initUrcData[slotId].end(); it++) {
            (*it)->setDataPosition(0);
            (*it)->readInt32(&urcId);
            if (expectedUrc == urcId) {
                p.setData((*it)->data(), (*it)->dataSize());
                p.setDataPosition(0);
                p.readInt32(&urcId);
                return true;
            }
      }
      return false;
}

bool RtstEnv::getExpectedSysProp(const String8 &key, String8 &value, int delay) {
    if (delay != 0) {
        sleep(delay);
    }
    char str[PROPERTY_VALUE_MAX] = {0};
    rfx_property_get(key.string(), str, "");
    value.setTo(str);
    RFX_LOG_D(TAG, "getExpectedSysProp key = %s, value = %s", key.string(), value.string());
    return true;
}

bool RtstEnv::getExpectedStatus(int slot, const RfxStatusKeyEnum key, RfxVariant &value,
        int delay) {
    bool blocked = false;
    if (RtstUtils::isBlockedStatus(key)) {
        int ret = RtstUtils::pollWait(getStatusSocket1().getFd(), m_timeout);
        if (ret <= 0) {
            return false;
        }
        blocked = true;
        const int len = 1024;
        unsigned char buf[len];
        getStatusSocket1().read(buf, len);
    }
    if ((!blocked) && (delay != 0)) {
        sleep(delay);
    }
    value = RtstUtils::getStatusValue(slot, key);
    RFX_LOG_D(TAG, "getExpectedStatus(slot = %d key = %d, value = %s", slot,
        key, value.toString().string());
    if (blocked) {
        const int len = 1024;
        unsigned char buf[len] = {0};
        strncpy((char *)buf, "STATUS", 6);
        getStatusSocket1().write(buf, 6);
    }
    return true;
}

void RtstEnv::releaseRequestInfo() {
    Vector<RequestInfo *>::iterator it;
    for (it = m_requestInfo.begin(); it != m_requestInfo.end();) {
        delete (*it);
        it = m_requestInfo.erase(it);
    }
}

bool RtstEnv::isRilSocket1Fd(int fd, int *slotId) {
    for (int i = 0; i < getSimCount(); i++) {
        if (fd == getRilSocket1(i).getFd()) {
            if (slotId != NULL) {
                *slotId = i;
            }
            return true;
        }
    }
    return false;
}

bool RtstEnv::isAtSocket1Fd(int fd,int * slotId) {
    for (int i = 0; i < getSimCount(); i++) {
        for (int j = 0; j < RFX_MAX_CHANNEL_NUM; j++) {
            if ((*m_atSocketPairs[i])[j]->getSocket1().getFd() == fd) {
                *slotId = i;
                return true;
            }
        }
    }
    return false;
}

void RtstEnv::addInitRilUrcData(int slotId, unsigned char *buf, int len) {
    Parcel *p = new Parcel();
    RFX_LOG_D(TAG, "addInitRilUrcData %p", p);
    p->setData(buf, len);
    m_initUrcData[slotId].push(p);
}



void RtstEnv::invokeStatusCallback(
    int slot,
    const RfxClassInfo *class_info,
    RfxStatusKeyEnum key,
    RfxVariant oldValue,
    RfxVariant newValue
) {

    sp<RtstMessage> msg = new RtstInvokeStatusCbMsg(
            slot, class_info, key, oldValue, newValue);
    sp<RtstHandler> handler = new RtstHandler(msg);
    RFX_LOG_D(TAG, "RtstEnv::invokeStatusCallback slot = %d, cls = %s, key = %d",
        slot, class_info->getClassName(), key);
    handler->sendMessage();
}

/*****************************************************************************
 * class RtstThread
 *****************************************************************************/
RtstThread::RtstThread() :
    m_looper(NULL),
    m_looperCallback(NULL),
    m_fds(NULL),
    m_fdNum(0) {
}

void RtstThread::setFds(int fds[], int fdNum) {
    m_fds = new int[fdNum];
    m_fdNum = fdNum;
    memcpy(m_fds, fds, fdNum * sizeof(int));
}

void RtstThread::setCallback(sp<LooperCallback> callback) {
    m_looperCallback = callback;
}

sp<RtstThread> RtstThread::create(int fds[], int fdNum) {
    RtstThread *t = new RtstThread();
    if (fdNum > 0) {
        t->setFds(fds, fdNum);
    }
    t->run("RfxTestThread");
    return t;
}


status_t RtstThread::readyToRun() {
    m_looper = Looper::prepare(0);
    for (int i =0 ; i < m_fdNum; i++) {
        m_looper->addFd(m_fds[i],
            Looper::POLL_CALLBACK,
            Looper::EVENT_INPUT,
            m_looperCallback,
            NULL);
    }
    return android::NO_ERROR;
}


bool RtstThread::threadLoop() {
    m_looper->pollAll(-1);
    return true;
}


sp<Looper> RtstThread::getLooper() {
    return m_looper;
}
