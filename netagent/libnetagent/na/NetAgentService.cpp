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
#include "NetAgentService.h"
#include "NetlinkEventHandler.h"

#define NA_LOG_TAG "NetAgentService"
#define UNUSED(x) ((void)(x))

/*****************************************************************************
 * Class NetAgentService
 *****************************************************************************/
pthread_mutex_t NetAgentService::sInitMutex = PTHREAD_MUTEX_INITIALIZER;
NetAgentService* NetAgentService::sInstance = NULL;

const char* NetAgentService::CCMNI_IFNAME_CCMNI = "ccmni";

NetAgentService::NetAgentService() {
    init();
}

void NetAgentService::init() {
    mReaderThread = 0;
    mEventThread = 0;
    sock_fd = 0;
    sock6_fd = 0;
    m_pNetAgentIoObj = NULL;
    m_pNetAgentReqInfo = NULL;
    mRouteSock = 0;
    m_pRouteHandler = NULL;
    pthread_mutex_init(&mDispatchMutex, NULL);
    pthread_cond_init(&mDispatchCond, NULL);
    m_lTransIntfId.clear();

    // Initialize NetAgent IO Socket.
    NA_INIT(m_pNetAgentIoObj);
    if (m_pNetAgentIoObj != NULL) {
        startEventLoop();
        startReaderLoop();
        startNetlinkEventHandler();
        // TODO: Enable it when modem is ready for AT+EPDNHOCFG and AT+EPDN
        syncCapabilityToModem();
    } else {
        NA_LOG_E("[%s] init NetAgent io socket fail", __FUNCTION__);
    }
}

NetAgentService::~NetAgentService() {
    if (NA_DEINIT(m_pNetAgentIoObj) != NETAGENT_IO_RET_SUCCESS ) {
        NA_LOG_E("[%s] deinit NetAgent io socket fail", __FUNCTION__);
    }

    NetAgentReqInfo *pTmp = NULL;
    while (m_pNetAgentReqInfo != NULL) {
       pTmp = m_pNetAgentReqInfo;
       m_pNetAgentReqInfo = m_pNetAgentReqInfo->pNext;
       freeNetAgentCmdObj(pTmp);
       FREEIF(pTmp);
    }

    if (m_pRouteHandler != NULL) {
        if (m_pRouteHandler->stop() < 0) {
            NA_LOG_E("[%s] Unable to stop route NetlinkEventHandler: %s",
                    __FUNCTION__, strerror(errno));
        }
        delete m_pRouteHandler;
        m_pRouteHandler = NULL;
    }

    sInstance = NULL;
    m_lTransIntfId.clear();
}

NetAgentService* NetAgentService::getInstance() {
    if (sInstance != NULL) {
        return sInstance;
    }
    return NULL;
}

bool NetAgentService::createNetAgentService() {
    pthread_mutex_lock(&sInitMutex);
    if (sInstance == NULL) {
        sInstance = new NetAgentService();
        if (sInstance == NULL) {
            NA_LOG_E("[%s] new NetAgentService fail", __FUNCTION__);
            pthread_mutex_unlock(&sInitMutex);
            return false;
        } else if (sInstance->m_pNetAgentIoObj == NULL) {
            delete sInstance;
            sInstance = NULL;
            pthread_mutex_unlock(&sInitMutex);
            return false;
        }
    }
    pthread_mutex_unlock(&sInitMutex);
    return true;
}

void NetAgentService::startEventLoop(void) {
    int ret;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    ret = pthread_create(&mEventThread, &attr, NetAgentService::eventThreadStart, this);

    if (ret != 0) {
        NA_LOG_E("[%s] failed to create event thread ret:%d", __FUNCTION__, ret);
    } else {
        NA_LOG_D("[%s] create event thread OK ret:%d, mEventThread:%ld",
                __FUNCTION__, ret, mEventThread);
    }
}

void *NetAgentService::eventThreadStart(void *arg) {
    NetAgentService *me = reinterpret_cast<NetAgentService *>(arg);
    me->runEventLoop();
    return NULL;
}

void NetAgentService::runEventLoop() {
    while (1) {
        NetAgentReqInfo *pReq = NULL;

        pthread_mutex_lock(&mDispatchMutex);
        pReq = dequeueReqInfo();
        if (pReq != NULL) {
            pthread_mutex_unlock(&mDispatchMutex);
            handleEvent(pReq);
            FREEIF(pReq);
        } else {
            pthread_cond_wait(&mDispatchCond, &mDispatchMutex);
            pthread_mutex_unlock(&mDispatchMutex);
        }
    }
}

void NetAgentService::startReaderLoop(void) {
    int ret;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    ret = pthread_create(&mReaderThread, &attr, NetAgentService::readerThreadStart, this);

    if (ret != 0) {
        NA_LOG_E("[%s] failed to create reader thread ret:%d", __FUNCTION__, ret);
    } else {
        NA_LOG_D("[%s] create reader thread OK ret:%d, mReaderThread:%ld",
                __FUNCTION__, ret, mReaderThread);
    }
}

void *NetAgentService::readerThreadStart(void *arg) {
    NetAgentService *me = reinterpret_cast<NetAgentService *>(arg);
    me->runReaderLoop();
    return NULL;
}

void NetAgentService::runReaderLoop() {
    while (1) {
        void *pNetAgentCmdObj = NULL;

        // Receive URC reported from DDM.
        NA_CMD_RECV(m_pNetAgentIoObj, pNetAgentCmdObj);
        if (pNetAgentCmdObj != NULL) {
            enqueueReqInfo(pNetAgentCmdObj, REQUEST_TYPE_DDM);
        } else {
            NA_LOG_E("[%s] recv urc fail", __FUNCTION__);
        }
    }
}

void NetAgentService::handleEvent(NetAgentReqInfo* pReqInfo) {
    switch (pReqInfo->cmdType) {
        case NETAGENT_IO_CMD_IFUP:
            configureNetworkInterface(pReqInfo, ENABLE);
            break;
        case NETAGENT_IO_CMD_IFDOWN:
            configureNetworkInterface(pReqInfo, DISABLE);
            break;
        case NETAGENT_IO_CMD_IFCHG:
            configureNetworkInterface(pReqInfo, UPDATE);
            break;
        case NETAGENT_IO_CMD_IPUPDATE:
            updateIpv6GlobalAddress(pReqInfo);
            break;
        case NETAGENT_IO_CMD_RA:
            notifyNoRA(pReqInfo);
            break;
        case NETAGENT_IO_CMD_IFSTATE:
            configureNetworkTransmitState(pReqInfo);
            break;
        case NETAGENT_IO_CMD_SETMTU:
            configureMTUSize(pReqInfo);
            break;
        case NETAGENT_IO_CMD_SYNC_CAPABILITY:
            setCapabilityToModem(pReqInfo);
            break;
        case NETAGENT_IO_CMD_PDNHO:
            handlePdnHandoverControl(pReqInfo);
            break;
        case NETAGENT_IO_CMD_IPCHG:
            updatePdnHandoverAddr(pReqInfo);
        break;
        default:
            break;
    }
    freeNetAgentCmdObj(pReqInfo);
}

NetAgentReqInfo *NetAgentService::createNetAgentReqInfo(void* obj, REQUEST_TYPE reqType, NA_CMD cmd) {
    NetAgentReqInfo* pNewReqInfo = NULL;
    pNewReqInfo = (NetAgentReqInfo *)calloc(1, sizeof(NetAgentReqInfo));
    if (pNewReqInfo == NULL) {
        NA_LOG_E("[%s] can't allocate NetAgentReqInfo", __FUNCTION__);
        return NULL;
    }

    pNewReqInfo->pNext = NULL;
    pNewReqInfo->pNetAgentCmdObj = obj;
    pNewReqInfo->reqType = reqType;
    pNewReqInfo->cmdType = cmd;
    return pNewReqInfo;
}

void NetAgentService::enqueueReqInfo(void* obj, REQUEST_TYPE reqType) {
    NetAgentReqInfo *pNew = NULL;
    NetAgentReqInfo *pCurrent = NULL;
    NA_CMD cmd;

    if (getCommand(obj, reqType, &cmd) < 0) {
        NA_LOG_E("[%s] get command fail", __FUNCTION__);
        return;
    }

    pNew = createNetAgentReqInfo(obj, reqType, cmd);
    if (pNew == NULL) {
        NA_LOG_E("[%s] create NetAgentReqInfo fail", __FUNCTION__);
        return;
    }

    pthread_mutex_lock(&mDispatchMutex);
    if (m_pNetAgentReqInfo == NULL) { /* No pending */
        m_pNetAgentReqInfo = pNew;
        pthread_cond_broadcast(&mDispatchCond);
    } else {
        pCurrent = m_pNetAgentReqInfo;
        while(pCurrent != NULL) {
            if (pCurrent->pNext == NULL) {
                pCurrent->pNext = pNew;
                break;
            }
            pCurrent = pCurrent->pNext;
        }
    }
    pthread_mutex_unlock(&mDispatchMutex);
}

NetAgentReqInfo *NetAgentService::dequeueReqInfo() {
    NetAgentReqInfo *pCurrent = m_pNetAgentReqInfo;

    if (pCurrent != NULL) {
        m_pNetAgentReqInfo = pCurrent->pNext;
    }
    return pCurrent;
}

void NetAgentService::syncCapabilityToModem() {
    NA_LOG_D("[%s]", __FUNCTION__);

    NetEventReqInfo *pNetEventObj = (NetEventReqInfo *)calloc(1, sizeof(NetEventReqInfo));

    if (pNetEventObj == NULL) {
        NA_LOG_E("[%s] can't allocate rild event obj", __FUNCTION__);
        return;
    }
    pNetEventObj->cmd = NETAGENT_IO_CMD_SYNC_CAPABILITY;
    enqueueReqInfo(pNetEventObj, REQUEST_TYPE_NETAGENT);
}

void NetAgentService::setCapabilityToModem(NetAgentReqInfo* pReqInfo) {
    UNUSED(pReqInfo);
    void *pNetAgentCmdObj = NA_CMD_SYNC_CAPABILITY_ALLOC();
    if (NA_CMD_SEND(m_pNetAgentIoObj, pNetAgentCmdObj) != NETAGENT_IO_RET_SUCCESS) {
        NA_LOG_E("[%s] fail", __FUNCTION__);
    }
    NA_CMD_FREE(pNetAgentCmdObj);
}

void NetAgentService::setNwIntfDown(const char *interfaceName) {
    ifc_reset_connections(interfaceName, RESET_ALL_ADDRESSES);
    ifc_remove_default_route(interfaceName);
    ifc_disable(interfaceName);
}

void NetAgentService::nwIntfIoctlInit() {
    if (sock_fd > 0) {
        close(sock_fd);
    }

    sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd < 0) {
        NA_LOG_E("[%s] couldn't create IP socket: errno=%d", __FUNCTION__, errno);
    }

    if (sock6_fd > 0) {
        close(sock6_fd);
    }

    sock6_fd = socket(AF_INET6, SOCK_DGRAM, 0);
    if (sock6_fd < 0) {
        sock6_fd = -errno;    /* save errno for later */
        NA_LOG_E("[%s] couldn't create IPv6 socket: errno=%d", __FUNCTION__, errno);
    }
}

void NetAgentService::nwIntfIoctlDeInit() {
    close(sock_fd);
    close(sock6_fd);
    sock_fd = 0;
    sock6_fd = 0;
}

/* For setting IFF_UP: nwIntfSetFlags(s, &ifr, IFF_UP, 0) */
/* For setting IFF_DOWN: nwIntfSetFlags(s, &ifr, 0, IFF_UP) */
void NetAgentService::nwIntfSetFlags(int s, struct ifreq *ifr, int set, int clr) {
    int ret = 0;

    ret = ioctl(s, SIOCGIFFLAGS, ifr);
    if (ret < 0) {
        NA_LOG_E("[%s] error in set SIOCGIFFLAGS:%d - %d:%s",
                __FUNCTION__, ret, errno, strerror(errno));
        return;
    }

    ifr->ifr_flags = (ifr->ifr_flags & (~clr)) | set;
    ret = ioctl(s, SIOCSIFFLAGS, ifr);
    if (ret < 0) {
        NA_LOG_E("[%s] error in set SIOCSIFFLAGS:%d - %d:%s",
                __FUNCTION__, ret, errno, strerror(errno));
    }
}

inline void NetAgentService::nwIntfInitSockAddrIn(struct sockaddr_in *sin, const char *addr) {
    sin->sin_family = AF_INET;
    sin->sin_port = 0;
    sin->sin_addr.s_addr = inet_addr(addr);
}

void NetAgentService::nwIntfSetAddr(int s, struct ifreq *ifr, const char *addr) {
    int ret = 0;

    NA_LOG_D("[%s] configure IPv4 adress : %s", __FUNCTION__, addr);
    nwIntfInitSockAddrIn((struct sockaddr_in *) &ifr->ifr_addr, addr);
    ret = ioctl(s, SIOCSIFADDR, ifr);
    if (ret < 0) {
        NA_LOG_E("[%s] error in set SIOCSIFADDR:%d - %d:%s",
                __FUNCTION__, ret, errno, strerror(errno));
    }
}

void NetAgentService::nwIntfSetIpv6Addr(int s, struct ifreq *ifr, const char *addr) {
    struct in6_ifreq ifreq6;
    int ret = 0;

    NA_LOG_D("[%s] configure IPv6 adress : %s", __FUNCTION__, addr);
    ret = ioctl(s, SIOCGIFINDEX, ifr);
    if (ret < 0) {
        NA_LOG_E("[%s] error in set SIOCGIFINDEX:%d - %d:%s",
                __FUNCTION__, ret, errno, strerror(errno));
        return;
    }

    // ret: -1, error occurs, ret: 0, invalid address, ret: 1, success;
    ret = inet_pton(AF_INET6, addr, &ifreq6.ifr6_addr);
    if (ret <= 0) {
        NA_LOG_E("[%s] ipv6 address: %s, inet_pton ret: %d", __FUNCTION__, addr, ret);
        return;
    }
    ifreq6.ifr6_prefixlen = 64;
    ifreq6.ifr6_ifindex = ifr->ifr_ifindex;

    ret = ioctl(s, SIOCSIFADDR, &ifreq6);
    if (ret < 0) {
        NA_LOG_E("[%s] error in set SIOCSIFADDR:%d - %d:%s",
                __FUNCTION__, ret, errno, strerror(errno));
    }
}

const char* NetAgentService::getCcmniInterfaceName() {
    // FIXME: To get ccmni interface name by slot ID.
    return getCcmniInterfaceName(0);
}

// Get CCMNI interface name by slot ID.
// TODO: the slot index should be used for specific projects such as DSDA.
const char* NetAgentService::getCcmniInterfaceName(int rid) {
    UNUSED(rid);
    return CCMNI_IFNAME_CCMNI;
}

//Configure the IP address to the CCMNI interface.
void NetAgentService::configureNetworkInterface(NetAgentReqInfo* pReqInfo, STATUS config) {
    struct ifreq ifr;
    unsigned int interfaceId = 0;
    NA_ADDR_TYPE addrType;
    NA_IFST state;
    char addressV4[MAX_IPV4_ADDRESS_LENGTH] = {0};
    char addressV6[MAX_IPV6_ADDRESS_LENGTH] = {0};
    char *reason = NULL;

    NA_GET_IFST_STATE(config, state);

    if (NA_GET_IF_ID(pReqInfo->pNetAgentCmdObj, &interfaceId) != NETAGENT_IO_RET_SUCCESS) {
        NA_LOG_E("[%s] fail to get interface id", __FUNCTION__);
        return;
    }

    if (config == ENABLE) {
        NA_LOG_D("[%s] push transIntfId %d to the list", __FUNCTION__, interfaceId);
        m_lTransIntfId.push_back(interfaceId);
    }
    interfaceId %= TRANSACTION_ID_OFFSET;

    if (NA_GET_ADDR_TYPE(pReqInfo->pNetAgentCmdObj, &addrType) != NETAGENT_IO_RET_SUCCESS) {
        NA_LOG_E("[%s] fail to get addr type", __FUNCTION__);
        return;
    }

    if (config == UPDATE) {
        if (NA_GET_IP_CHANGE_REASON(pReqInfo->pNetAgentCmdObj, &reason) != NETAGENT_IO_RET_SUCCESS) {
            NA_LOG_E("[%s] fail to get IP change reason", __FUNCTION__);
            reason = NULL;
        }

        NA_LOG_I("[%s] update interface %d, addr type : %s(%d), ip change reason: %s", __FUNCTION__,
                interfaceId, addrTypeToString(addrType), addrType, reason != NULL ? reason : "");
    } else {
        NA_LOG_D("[%s] interface %d to %s, addr type : %s(%d)", __FUNCTION__,
                interfaceId, config ? "UP" : "DOWN", addrTypeToString(addrType), addrType);
    }

    memset(&ifr, 0, sizeof(struct ifreq));
    snprintf(ifr.ifr_name, IFNAMSIZ, "%s%d", getCcmniInterfaceName(), interfaceId);

    nwIntfIoctlInit();

    if (config == ENABLE || config == UPDATE) {
        if (config == ENABLE) {
            // set the network interface down first before up
            // to prevent from unknown exception causing not close related
            // dev file description
            NA_LOG_D("[%s] set network interface down before up", __FUNCTION__);
            setNwIntfDown(ifr.ifr_name);
        } else {
            NA_LOG_D("[%s] reset connections", __FUNCTION__);
            ifc_reset_connections(ifr.ifr_name, addrType);
        }

        switch (addrType) {
            case NETAGENT_IO_ADDR_TYPE_IPv4:
                getIpv4Address(pReqInfo->pNetAgentCmdObj, addressV4);
                break;
            case NETAGENT_IO_ADDR_TYPE_IPv6:
                getIpv6Address(pReqInfo->pNetAgentCmdObj, addressV6);
                break;
            case NETAGENT_IO_ADDR_TYPE_IPv4v6:
                getIpv4v6Address(pReqInfo->pNetAgentCmdObj, addressV4, addressV6);
                break;
            default:
                NA_LOG_E("[%s] get addr type fail", __FUNCTION__);
                break;
        }

        if (strlen(addressV4) > 0) {
            if (config == ENABLE) {
                nwIntfSetFlags(sock_fd, &ifr, IFF_UP, 0);
            }
            nwIntfSetAddr(sock_fd, &ifr, addressV4);
        }

        if (strlen(addressV6) > 0) {
            if (config == ENABLE) {
                configureRSTimes(interfaceId);
                nwIntfSetFlags(sock6_fd, &ifr, IFF_UP, 0);
            }
            nwIntfSetIpv6Addr(sock6_fd, &ifr, addressV6);
        }
    } else {
        setNwTxqState(interfaceId, 1);
        setNwIntfDown(ifr.ifr_name);
    }

    nwIntfIoctlDeInit();

    if (config == UPDATE) {
        // Send ipupdate confirm to DDM.
        if (strlen(addressV4) > 0) {
            if (addrType == NETAGENT_IO_ADDR_TYPE_IPv4
                    || addrType == NETAGENT_IO_ADDR_TYPE_IPv4v6) {
                unsigned int addrV4;
                if (NA_GET_ADDR_V4(pReqInfo->pNetAgentCmdObj, &addrV4) == NETAGENT_IO_RET_SUCCESS) {
                    confirmIpUpdate(interfaceId,
                            NETAGENT_IO_ADDR_TYPE_IPv4,
                            &addrV4,
                            INVALID_IPV6_PREFIX_LENGTH);
                } else {
                    NA_LOG_E("[%s] error occurs when get addressV4", __FUNCTION__);
                }
            } else {
                NA_LOG_E("[%s] not to confirm ipupdate for invalid addrType", __FUNCTION__);
            }
        } else {
            NA_LOG_E("[%s] not to confirm ipupdate for wrong address", __FUNCTION__);
        }
    } else {
        // Interface up/down done confirm with DDM.
        confirmInterfaceState(interfaceId, state, addrType);
    }

    if (config == DISABLE) {
        NA_LOG_D("[%s] remove transIntfId %d from the list and last ReqInfo", __FUNCTION__,
                getTransIntfId(interfaceId));
        m_lTransIntfId.remove(getTransIntfId(interfaceId));
        if (m_pRouteHandler != NULL) {
            m_pRouteHandler->removeLastReqInfo(interfaceId);
        }
        clearPdnHandoverInfo(interfaceId);
    }
}

void NetAgentService::configureMTUSize(NetAgentReqInfo* pReqInfo) {
    struct ifreq ifr;
    unsigned int interfaceId = 0;
    unsigned int mtuSize;
    char mtu[MAX_MTU_SIZE_LENGTH] = {0};
    char *cmd;

    if (NA_GET_IF_ID(pReqInfo->pNetAgentCmdObj, &interfaceId) != NETAGENT_IO_RET_SUCCESS) {
        NA_LOG_E("[%s] fail to get interface id", __FUNCTION__);
        return;
    }
    NA_LOG_D("[%s] TransIntfId %d request to set mtu size", __FUNCTION__, interfaceId);

    interfaceId %= TRANSACTION_ID_OFFSET;
    if (NA_GET_MTU_SIZE(pReqInfo->pNetAgentCmdObj, &mtuSize) != NETAGENT_IO_RET_SUCCESS) {
        NA_LOG_E("[%s] fail to get mtu size", __FUNCTION__);
        return;
    }

    memset(&ifr, 0, sizeof(struct ifreq));
    sprintf(ifr.ifr_name, "%s%d", getCcmniInterfaceName(), interfaceId);

    sprintf(mtu, "%d", mtuSize);
    NA_LOG_D("[%s] get mtu size %d from URC", __FUNCTION__, mtuSize);

    asprintf(&cmd, "ifconfig %s mtu %s", ifr.ifr_name, mtu);
    system(cmd);
    free(cmd);
}

void NetAgentService::configureRSTimes(int interfaceId) {
    char *cmd = NULL;
    asprintf(&cmd, "echo 2 > /proc/sys/net/ipv6/conf/%s%d/router_solicitations",
            getCcmniInterfaceName(), interfaceId);
    if (cmd != NULL) {
        NA_LOG_D("[%s] cmd = %s", __FUNCTION__, cmd);
        system(cmd);
        free(cmd);
    } else {
        NA_LOG_E("[%s] cmd is NULL", __FUNCTION__);
    }
}

void NetAgentService::updateIpv6GlobalAddress(NetAgentReqInfo* pReqInfo) {
    char address[INET6_ADDRSTRLEN] = "";
    int ipv6PrefixLength = INVALID_IPV6_PREFIX_LENGTH;
    unsigned int addrV6[4];
    unsigned int interfaceId = INVALID_INTERFACE_ID;
    ACTION action = ACTION_UNKNOWN;

    if (m_pRouteHandler == NULL) {
        NA_LOG_E("[%s] m_pRouteHandler is NULL", __FUNCTION__);
        return;
    }

    if (m_pRouteHandler->getAddress(pReqInfo->pNetAgentCmdObj, address) != NETLINK_RET_SUCCESS) {
        NA_LOG_E("[%s] fail to get address", __FUNCTION__);
        return;
    }

    if (m_pRouteHandler->getIpv6PrefixLength(pReqInfo->pNetAgentCmdObj, &ipv6PrefixLength) != NETLINK_RET_SUCCESS) {
        NA_LOG_E("[%s] fail to get ipv6PrefixLength", __FUNCTION__);
        return;
    }

    if (m_pRouteHandler->getInterfaceId(pReqInfo->pNetAgentCmdObj, &interfaceId) != NETLINK_RET_SUCCESS) {
        NA_LOG_E("[%s] fail to get interfaceId", __FUNCTION__);
        return;
    }

    if (m_pRouteHandler->getAction(pReqInfo->pNetAgentCmdObj, &action) != NETLINK_RET_SUCCESS) {
        NA_LOG_E("[%s] fail to get action", __FUNCTION__);
        return;
    }

    if (isIpv6Global(address)) {
        if (m_pRouteHandler->hasLastReqInfoChanged(pReqInfo->pNetAgentCmdObj) == NETLINK_RET_REQ_INFO_NO_CHANGED) {
            NA_LOG_I("[%s] pReqInfo is not changed, no need to notify DDM", __FUNCTION__);
            return;
        }

        if (action == ACTION_ADDR_REMOVED) {
            if (isNeedNotifyIPv6RemovedToModem(interfaceId, address) == false) {
                NA_LOG_I("[%s] Don't notify ho source ip be removed to modem", __FUNCTION__);
                return;
            }
            memset(&address, 0, sizeof(address));
            strncpy(address, NULL_IPV6_ADDRESS, strlen(NULL_IPV6_ADDRESS));
            ipv6PrefixLength = INVALID_IPV6_PREFIX_LENGTH;
        }

        if (convertIpv6ToBinary(addrV6, address) < 0) {
            NA_LOG_E("[%s] fail to convert ipv6 address to binary", __FUNCTION__);
            return;
        }

        confirmIpUpdate(interfaceId, NETAGENT_IO_ADDR_TYPE_IPv6, addrV6, ipv6PrefixLength);

        if (m_pRouteHandler->setLastReqInfo(pReqInfo->pNetAgentCmdObj) != NETLINK_RET_SUCCESS) {
            NA_LOG_E("[%s] fail to set last pReqInfo", __FUNCTION__);
        }
    }
}

void NetAgentService::notifyNoRA(NetAgentReqInfo* pReqInfo) {
    unsigned int interfaceId = INVALID_INTERFACE_ID;
    NA_RA flag;

    if (m_pRouteHandler == NULL) {
        NA_LOG_E("[%s] m_pRouteHandler is NULL", __FUNCTION__);
        return;
    }

    if (m_pRouteHandler->hasLastReqInfoChanged(pReqInfo->pNetAgentCmdObj) == NETLINK_RET_REQ_INFO_NO_CHANGED) {
        NA_LOG_I("[%s] pReqInfo is not changed, no need to notify DDM", __FUNCTION__);
        return;
    }

    if (m_pRouteHandler->getInterfaceId(pReqInfo->pNetAgentCmdObj, &interfaceId) != NETLINK_RET_SUCCESS) {
        NA_LOG_E("[%s] fail to get interfaceId", __FUNCTION__);
        return;
    }

    if (m_pRouteHandler->getFlag(pReqInfo->pNetAgentCmdObj, &flag) != NETLINK_RET_SUCCESS) {
        NA_LOG_E("[%s] fail to get flag", __FUNCTION__);
        return;
    }

    confirmNoRA(interfaceId, flag);

    if (m_pRouteHandler->setLastReqInfo(pReqInfo->pNetAgentCmdObj) != NETLINK_RET_SUCCESS) {
        NA_LOG_E("[%s] fail to set last pReqInfo", __FUNCTION__);
    }
}

void NetAgentService::handlePdnHandoverControl(NetAgentReqInfo* pReqInfo) {
    unsigned int tranId = INVALID_INTERFACE_ID;
    unsigned int interfaceId = INVALID_INTERFACE_ID;
    char addressV4[MAX_IPV4_ADDRESS_LENGTH] = {0};
    char addressV6[MAX_IPV6_ADDRESS_LENGTH] = {0};
    NA_PDN_HO_INFO hoInfo;

    if (NA_GET_IF_ID(pReqInfo->pNetAgentCmdObj, &tranId) != NETAGENT_IO_RET_SUCCESS) {
        NA_LOG_E("[%s] fail to get interface id", __FUNCTION__);
        return;
    }
    interfaceId = tranId % TRANSACTION_ID_OFFSET;

    if (NA_GET_PDN_HO_INFO(pReqInfo->pNetAgentCmdObj, &hoInfo) != NETAGENT_IO_RET_SUCCESS) {
        NA_LOG_E("[%s] fail to get handover info", __FUNCTION__);
        return;
    }

    NA_ADDR_TYPE addrType = hoInfo.addr_type;
    if (NETAGENT_IO_HO_STATE_START == hoInfo.hostate) {
        NA_LOG_D("[%s] tid: %d, hostate: %s, result: %s, src_ran: %s, tgt_ran: %s",
                __FUNCTION__, tranId, hoStateToString(hoInfo.hostate),
                hoResultToString(hoInfo.is_succ), ranTypeToString(hoInfo.src_ran),
                ranTypeToString(hoInfo.tgt_ran));
        if (NETAGENT_IO_HO_RESULT_SUCCESS == hoInfo.is_succ) {
            switch (addrType) {
                case NETAGENT_IO_ADDR_TYPE_IPv4:
                    getIpv4Address(pReqInfo->pNetAgentCmdObj, addressV4);
                    break;
                case NETAGENT_IO_ADDR_TYPE_IPv6:
                    getIpv6Address(pReqInfo->pNetAgentCmdObj, addressV6);
                    break;
                case NETAGENT_IO_ADDR_TYPE_IPv4v6:
                    getIpv4v6Address(pReqInfo->pNetAgentCmdObj, addressV4, addressV6);
                    break;
                default:
                    // No address
                    break;
            }
            recordPdnHandoverInfo(interfaceId, addrType, addressV4, addressV6);
        }

    } else if (NETAGENT_IO_HO_STATE_STOP == hoInfo.hostate) {
        bool needFlushIpsecPolicy =
                hoInfo.is_succ == NETAGENT_IO_HO_RESULT_SUCCESS &&
                hoInfo.src_ran == NETAGENT_IO_HO_RAN_WIFI &&
                hoInfo.tgt_ran == NETAGENT_IO_HO_RAN_MOBILE;

        NA_LOG_D("[%s] tid: %d, hostate: %s, result: %s, src_ran: %s, tgt_ran: %s, flush_ipsec: %d",
                __FUNCTION__, tranId, hoStateToString(hoInfo.hostate),
                hoResultToString(hoInfo.is_succ), ranTypeToString(hoInfo.src_ran),
                ranTypeToString(hoInfo.tgt_ran), needFlushIpsecPolicy);

        if (needFlushIpsecPolicy) {
            clearIpsec(interfaceId);
        }

        confirmPdnHandoverControl(tranId);
    }
}

NetAgentPdnInfo *NetAgentService::recordPdnHandoverInfo(
        unsigned int interfaceId, NA_ADDR_TYPE addrType, char *addressV4, char *addressV6) {

    NetAgentPdnInfo *pPdnSrcInfo = getPdnHandoverInfo(interfaceId);
    if (pPdnSrcInfo == NULL) {
        pPdnSrcInfo = (NetAgentPdnInfo *)calloc(1, sizeof(NetAgentPdnInfo));
        if (pPdnSrcInfo == NULL) {
            NA_LOG_E("[%s] can't allocate NetAgentPdnInfo", __FUNCTION__);
            return NULL;
        }
    }

    pPdnSrcInfo->interfaceId = interfaceId;
    pPdnSrcInfo->addrType = addrType;

    switch (addrType) {
        case NETAGENT_IO_ADDR_TYPE_IPv4:
            memcpy(pPdnSrcInfo->addressV4, addressV4, MAX_IPV4_ADDRESS_LENGTH);
            NA_LOG_D("[%s] interfaceId: %d, addrType: %s, addressV4: %s",
                    __FUNCTION__, interfaceId, addrTypeToString(addrType), addressV4);
            break;
        case NETAGENT_IO_ADDR_TYPE_IPv6:
            memcpy(pPdnSrcInfo->addressV6, addressV6, MAX_IPV6_ADDRESS_LENGTH);
            NA_LOG_D("[%s] interfaceId: %d, addrType: %s, addressV6: %s",
                    __FUNCTION__, interfaceId, addrTypeToString(addrType), addressV4);
            break;
        case NETAGENT_IO_ADDR_TYPE_IPv4v6:
            memcpy(pPdnSrcInfo->addressV4, addressV4, MAX_IPV4_ADDRESS_LENGTH);
            memcpy(pPdnSrcInfo->addressV6, addressV6, MAX_IPV6_ADDRESS_LENGTH);
            NA_LOG_D("[%s] interfaceId: %d, addrType: %s, addressV4: %s, addressV6: %s",
                    __FUNCTION__, interfaceId, addrTypeToString(addrType), addressV4, addressV6);
            break;
        default:
            // No address, shall not go to here.
            break;
    }
    m_pdnHoInfoMap[interfaceId] = pPdnSrcInfo;

    return pPdnSrcInfo;
}

NetAgentPdnInfo *NetAgentService::getPdnHandoverInfo(unsigned int interfaceId) {
    if (m_pdnHoInfoMap.count(interfaceId) > 0) {
        return m_pdnHoInfoMap[interfaceId];
    }
    return NULL;
}

bool NetAgentService::clearPdnHandoverInfo(unsigned int interfaceId) {
    if (m_pdnHoInfoMap.count(interfaceId) > 0) {
        NetAgentPdnInfo *pPdnSrcInfo = m_pdnHoInfoMap[interfaceId];
        m_pdnHoInfoMap.erase(interfaceId);
        if (pPdnSrcInfo != NULL) {
            free(pPdnSrcInfo);
        }
        return true;
    }

    return false;
}

void NetAgentService::clearIpsec(unsigned int interfaceId) {
    NetAgentPdnInfo *pPdnSrcInfo = getPdnHandoverInfo(interfaceId);
    if (pPdnSrcInfo != NULL) {
        switch (pPdnSrcInfo->addrType) {
            case NETAGENT_IO_ADDR_TYPE_IPv4:
                NA_FLUSH_IPSEC_POLICY(pPdnSrcInfo->addressV4, NETAGENT_IO_ADDR_TYPE_IPv4);
                break;
            case NETAGENT_IO_ADDR_TYPE_IPv6:
                NA_FLUSH_IPSEC_POLICY(pPdnSrcInfo->addressV6, NETAGENT_IO_ADDR_TYPE_IPv6);
                break;
            case NETAGENT_IO_ADDR_TYPE_IPv4v6:
                NA_FLUSH_IPSEC_POLICY(pPdnSrcInfo->addressV4, NETAGENT_IO_ADDR_TYPE_IPv4);
                NA_FLUSH_IPSEC_POLICY(pPdnSrcInfo->addressV6, NETAGENT_IO_ADDR_TYPE_IPv6);
                break;
            default:
                // No address
                break;
        }
    } else {
        NA_LOG_E("[%s] Can't find NetAgentPdnInfo for tid: %d", __FUNCTION__, interfaceId);
    }
}

void NetAgentService::updatePdnHandoverAddr(NetAgentReqInfo* pReqInfo) {
    struct ifreq ifr;
    unsigned int interfaceId = INVALID_INTERFACE_ID;
    NA_ADDR_TYPE addrType;
    char addressV4[MAX_IPV4_ADDRESS_LENGTH] = {0};
    char addressV6[MAX_IPV6_ADDRESS_LENGTH] = {0};
    char *reason = NULL;
    unsigned int addrV4;
    unsigned int addrV6[4];

    if (NA_GET_IF_ID(pReqInfo->pNetAgentCmdObj, &interfaceId) != NETAGENT_IO_RET_SUCCESS) {
        NA_LOG_E("[%s] fail to get interface id", __FUNCTION__);
        return;
    }
    interfaceId %= TRANSACTION_ID_OFFSET;

    if (NA_GET_ADDR_TYPE(pReqInfo->pNetAgentCmdObj, &addrType) != NETAGENT_IO_RET_SUCCESS) {
        NA_LOG_E("[%s] fail to get addr type", __FUNCTION__);
        return;
    }

    if (NA_GET_IP_CHANGE_REASON(pReqInfo->pNetAgentCmdObj, &reason) != NETAGENT_IO_RET_SUCCESS) {
        NA_LOG_E("[%s] fail to get IP change reason", __FUNCTION__);
        reason = NULL;
    }

    NA_LOG_I("[%s] update interface %d, addr type : %s(%d), reason: %s", __FUNCTION__,
                interfaceId, addrTypeToString(addrType), addrType, reason != NULL ? reason : "");

    memset(&ifr, 0, sizeof(struct ifreq));
    snprintf(ifr.ifr_name, IFNAMSIZ, "%s%d", getCcmniInterfaceName(), interfaceId);

    // add new interface address into kernel
    switch (addrType) {
        case NETAGENT_IO_ADDR_TYPE_IPv4:
            getIpv4Address(pReqInfo->pNetAgentCmdObj, addressV4);
            ifc_add_address(ifr.ifr_name, addressV4, IPV4_REFIX_LENGTH);
            NA_LOG_D("[%s] add addressV4: %s", __FUNCTION__, addressV4);
            if (NA_GET_ADDR_V4(pReqInfo->pNetAgentCmdObj, &addrV4) != NETAGENT_IO_RET_SUCCESS) {
                NA_LOG_I("[%s] fail to get addrV4", __FUNCTION__);
            }
            break;
        case NETAGENT_IO_ADDR_TYPE_IPv6:
            getIpv6Address(pReqInfo->pNetAgentCmdObj, addressV6);
            ifc_add_address(ifr.ifr_name, addressV6, IPV6_REFIX_LENGTH);
            NA_LOG_D("[%s] add addressV6: %s", __FUNCTION__, addressV6);
            if (NA_GET_ADDR_V6(pReqInfo->pNetAgentCmdObj, addrV6) != NETAGENT_IO_RET_SUCCESS) {
                NA_LOG_I("[%s] fail to get addrV4", __FUNCTION__);
            }
            break;
        case NETAGENT_IO_ADDR_TYPE_IPv4v6:
            getIpv4v6Address(pReqInfo->pNetAgentCmdObj, addressV4, addressV6);
            ifc_add_address(ifr.ifr_name, addressV4, IPV4_REFIX_LENGTH);
            ifc_add_address(ifr.ifr_name, addressV6, IPV6_REFIX_LENGTH);
            NA_LOG_D("[%s] add addressV4: %s, addressV6: %s", __FUNCTION__, addressV4, addressV6);
            if (NA_GET_ADDR_V4(pReqInfo->pNetAgentCmdObj, &addrV4) != NETAGENT_IO_RET_SUCCESS) {
                NA_LOG_I("[%s] fail to get addrV4", __FUNCTION__);
            }
            if (NA_GET_ADDR_V6(pReqInfo->pNetAgentCmdObj, addrV6) != NETAGENT_IO_RET_SUCCESS) {
                NA_LOG_I("[%s] fail to get addrV4", __FUNCTION__);
            }
            break;
        default:
            NA_LOG_E("[%s] get addr type fail", __FUNCTION__);
            break;
    }

    NetAgentPdnInfo *pPdnSrcInfo = getPdnHandoverInfo(interfaceId);
    if (pPdnSrcInfo == NULL) {
        NA_LOG_E("[%s] Can't find NetAgentPdnInfo for tid: %d", __FUNCTION__, interfaceId);
        return;
    }

    // del old interface address into kernel
    switch (pPdnSrcInfo->addrType) {
        case NETAGENT_IO_ADDR_TYPE_IPv4:
            ifc_del_address(ifr.ifr_name, pPdnSrcInfo->addressV4, IPV4_REFIX_LENGTH);
            NA_LOG_D("[%s] remove addressV4: %s", __FUNCTION__, pPdnSrcInfo->addressV4);
            break;
        case NETAGENT_IO_ADDR_TYPE_IPv6:
            ifc_del_address(ifr.ifr_name, pPdnSrcInfo->addressV6, IPV6_REFIX_LENGTH);
            NA_LOG_D("[%s] remove addressV6: %s", __FUNCTION__, pPdnSrcInfo->addressV6);
            break;
        case NETAGENT_IO_ADDR_TYPE_IPv4v6:
            ifc_del_address(ifr.ifr_name, pPdnSrcInfo->addressV4, IPV4_REFIX_LENGTH);
            ifc_del_address(ifr.ifr_name, pPdnSrcInfo->addressV6, IPV6_REFIX_LENGTH);
            NA_LOG_D("[%s] remove addressV4: %s, addressV6: %s",
                    __FUNCTION__, pPdnSrcInfo->addressV4, pPdnSrcInfo->addressV6);
            break;
        default:
            // No address, shall not go to here.
            break;
    }

    //send comfirm to modem
    switch (addrType) {
        case NETAGENT_IO_ADDR_TYPE_IPv4:
        case NETAGENT_IO_ADDR_TYPE_IPv4v6:
            confirmIpUpdate(interfaceId,
                    NETAGENT_IO_ADDR_TYPE_IPv4,
                    &addrV4,
                    INVALID_IPV6_PREFIX_LENGTH);
            break;
        default:
            // No address, shall not go to here.
            break;
    }
}

/**
When IPv6 address is removed from kernel via netlink notification,
AP doesn't need to relay it to modem if delAddr is the same address
of interface before handover.

@param interfaceId for ccmni
@param delAddr IPv6 address be removed from kernel
*/
bool NetAgentService::isNeedNotifyIPv6RemovedToModem(unsigned int interfaceId, char* delAddr) {

    unsigned int addrV6[4];
    NetAgentPdnInfo *pPdnSrcInfo = getPdnHandoverInfo(interfaceId);

    if (pPdnSrcInfo == NULL) {
        NA_LOG_E("[%s] can not found PdnSrcInfo", __FUNCTION__);
        return true;
    }

    switch (pPdnSrcInfo->addrType) {
        /**
        adjust the format of IP address to be consistent and comparable
        EX 2001:2001:0:0:1::11 -> 2001:0000:0000:0001:0000:0000:0000:0011
        */
        case NETAGENT_IO_ADDR_TYPE_IPv6:
        case NETAGENT_IO_ADDR_TYPE_IPv4v6:
            if (convertIpv6ToBinary(addrV6, delAddr) < 0) {
                NA_LOG_E("[%s] fail to convert ipv6 address to binary", __FUNCTION__);
                return true;
             }

            if (convertIpv6ToString(delAddr, addrV6) < 0) {
                NA_LOG_E("[%s] error occurs when converting ipv6 to string", __FUNCTION__);
                return true;
            }

            NA_LOG_I("[%s] compare PdnSrcInfo IPv6:%s, Netlink removed IPv6:%s", __FUNCTION__, pPdnSrcInfo->addressV6, delAddr);
            if (strncmp(delAddr, pPdnSrcInfo->addressV6, strlen(delAddr)) == 0) {
                return false;
            }
        break;
    }
    return true;
}

void NetAgentService::confirmInterfaceState(unsigned int interfaceId, NA_IFST state, NA_ADDR_TYPE addrType) {
    void *pNetAgentCmdObj = 0;

    int nTransIntfId = getTransIntfId(interfaceId);
    if (INVALID_TRANS_INTF_ID != nTransIntfId) {
        pNetAgentCmdObj = NA_CMD_IFST_ALLOC(nTransIntfId, state, addrType);
        if (NA_CMD_SEND(m_pNetAgentIoObj, pNetAgentCmdObj) != NETAGENT_IO_RET_SUCCESS) {
            NA_LOG_E("[%s] send Ifst confirm fail", __FUNCTION__);
        }
        NA_CMD_FREE(pNetAgentCmdObj);
    } else {
        NA_LOG_I("[%s] ignore to send Ifst confirm", __FUNCTION__);
    }
}

void NetAgentService::confirmIpUpdate(unsigned int interfaceId, NA_ADDR_TYPE addrType, unsigned int* addr, int ipv6PrefixLength) {
    void *pNetAgentCmdObj = 0;

    int nTransIntfId = getTransIntfId(interfaceId);
    if (INVALID_TRANS_INTF_ID != nTransIntfId) {
        pNetAgentCmdObj = NA_CMD_IPUPDATE_ALLOC(nTransIntfId, addrType, addr, ipv6PrefixLength);
        if (NA_CMD_SEND(m_pNetAgentIoObj, pNetAgentCmdObj) != NETAGENT_IO_RET_SUCCESS) {
            NA_LOG_E("[%s] send IpUpdate confirm fail", __FUNCTION__);
        }
        NA_CMD_FREE(pNetAgentCmdObj);
    } else {
        NA_LOG_D("[%s] ignore to send ip update event", __FUNCTION__);
    }
}

void NetAgentService::confirmNoRA(unsigned int interfaceId, NA_RA flag) {
    void *pNetAgentCmdObj = 0;

    int nTransIntfId = getTransIntfId(interfaceId);
    if (INVALID_TRANS_INTF_ID != nTransIntfId) {
        pNetAgentCmdObj = NA_CMD_RA_ALLOC(nTransIntfId, flag);
        if (NA_CMD_SEND(m_pNetAgentIoObj, pNetAgentCmdObj) != NETAGENT_IO_RET_SUCCESS) {
            NA_LOG_E("[%s] send NoRA confirm fail", __FUNCTION__);
        }
        NA_CMD_FREE(pNetAgentCmdObj);
    } else {
        NA_LOG_I("[%s] ignore to send no RA event", __FUNCTION__);
    }
}

void NetAgentService::confirmPdnHandoverControl(unsigned int tranId) {
    NA_LOG_D("[%s] tranId %d", __FUNCTION__, tranId);

    void *pNetAgentCmdObj = NA_CMD_PDNHO_ALLOC(tranId);
    if (NA_CMD_SEND(m_pNetAgentIoObj, pNetAgentCmdObj) != NETAGENT_IO_RET_SUCCESS) {
        NA_LOG_E("[%s] send PDN handover confirm fail", __FUNCTION__);
    }
    NA_CMD_FREE(pNetAgentCmdObj);
}

void NetAgentService::startNetlinkEventHandler(void) {
    if ((m_pRouteHandler = setupSocket(&mRouteSock, NETLINK_ROUTE,
                                      RTMGRP_IPV6_IFADDR | RTMGRP_IPV6_PREFIX,
                                      NetlinkListener::NETLINK_FORMAT_BINARY)) == NULL) {
        NA_LOG_E("[%s] setup socket fail", __FUNCTION__);
    }
}

NetlinkEventHandler *NetAgentService::setupSocket(int *sock, int netlinkFamily,
        int groups, int format) {

    struct sockaddr_nl nladdr;
    int sz = 64 * 1024;
    int on = 1;

    memset(&nladdr, 0, sizeof(nladdr));
    nladdr.nl_family = AF_NETLINK;
    nladdr.nl_pid = getpid();
    nladdr.nl_groups = groups;

    if ((*sock = socket(PF_NETLINK, SOCK_DGRAM | SOCK_CLOEXEC, netlinkFamily)) < 0) {
        NA_LOG_E("[%s] Unable to create netlink socket: %s", __FUNCTION__, strerror(errno));
        return NULL;
    }

    if (setsockopt(*sock, SOL_SOCKET, SO_RCVBUFFORCE, &sz, sizeof(sz)) < 0) {
        NA_LOG_E("[%s] Unable to set uevent socket SO_RCVBUFFORCE option: %s",
                __FUNCTION__, strerror(errno));
        close(*sock);
        return NULL;
    }

    if (setsockopt(*sock, SOL_SOCKET, SO_PASSCRED, &on, sizeof(on)) < 0) {
        NA_LOG_E("[%s] Unable to set uevent socket SO_PASSCRED option: %s",
                __FUNCTION__, strerror(errno));
        close(*sock);
        return NULL;
    }

    if (bind(*sock, (struct sockaddr *) &nladdr, sizeof(nladdr)) < 0) {
        NA_LOG_E("[%s] Unable to bind netlink socket: %s", __FUNCTION__, strerror(errno));
        close(*sock);
        return NULL;
    }

    NetlinkEventHandler *handler = new NetlinkEventHandler(this, *sock, format);
    if (handler == NULL) {
        NA_LOG_E("[%s] new NetlinkEventHandler fail", __FUNCTION__);
        close(*sock);
        return NULL;
    }

    if (handler->start() < 0) {
        NA_LOG_E("[%s] Unable to start NetlinkEventHandler: %s", __FUNCTION__, strerror(errno));
        delete handler;
        handler = NULL;
        close(*sock);
        return NULL;
    }

    return handler;
}

bool NetAgentService::isIpv6Global(const char *ipv6Addr) {
    if (ipv6Addr) {
        struct sockaddr_in6 sa;
        int ret = 0;

        if (strncasecmp("FE80", ipv6Addr, strlen("FE80")) == 0) {
            NA_LOG_I("[%s] not global", __FUNCTION__);
            return false;
        }

        // ret: -1, error occurs, ret: 0, invalid address, ret: 1, success;
        ret = inet_pton(AF_INET6, ipv6Addr, &(sa.sin6_addr));
        if (ret <= 0) {
            NA_LOG_E("[%s] ipv6 address: %s, inet_pton ret: %d", __FUNCTION__, ipv6Addr, ret);
            return false;
        }

        if (IN6_IS_ADDR_MULTICAST(&sa.sin6_addr)) {
            NA_LOG_I("[%s] multi-cast", __FUNCTION__);
            if (IN6_IS_ADDR_MC_GLOBAL(&sa.sin6_addr)) {
                NA_LOG_D("[%s] global", __FUNCTION__);
                return true;
            } else {
                NA_LOG_I("[%s] not global", __FUNCTION__);
            }
        } else {
            if (IN6_IS_ADDR_LINKLOCAL(&sa.sin6_addr)) {
                NA_LOG_I("[%s] link-local", __FUNCTION__);
            } else if (IN6_IS_ADDR_SITELOCAL(&sa.sin6_addr)) {
                NA_LOG_I("[%s] site-local", __FUNCTION__);
            } else if (IN6_IS_ADDR_V4MAPPED(&sa.sin6_addr)) {
                NA_LOG_I("[%s] v4mapped", __FUNCTION__);
            } else if (IN6_IS_ADDR_V4COMPAT(&sa.sin6_addr)) {
                NA_LOG_I("[%s] v4compat", __FUNCTION__);
            } else if (IN6_IS_ADDR_LOOPBACK(&sa.sin6_addr)) {
                NA_LOG_I("[%s] host", __FUNCTION__);
            } else if (IN6_IS_ADDR_UNSPECIFIED(&sa.sin6_addr)) {
                NA_LOG_I("[%s] unspecified", __FUNCTION__);
            } else if (_IN6_IS_ULA(&sa.sin6_addr)) {
                NA_LOG_D("[%s] uni-local", __FUNCTION__);
                return true;
            } else {
                NA_LOG_D("[%s] global", __FUNCTION__);
                return true;
            }
        }
    } else {
        NA_LOG_E("[%s] input ipv6 address is null!!", __FUNCTION__);
    }
    return false;
}

int NetAgentService::getCommand(void* obj, REQUEST_TYPE reqType, NA_CMD *cmd) {
    if (reqType == REQUEST_TYPE_DDM) {
        if (NA_CMD_TYPE(obj, cmd) != NETAGENT_IO_RET_SUCCESS) {
            NA_LOG_E("[%s] get %s command fail", __FUNCTION__, reqTypeToString(reqType));
            return -1;
        }
    } else if (reqType == REQUEST_TYPE_NETLINK) {
        if (m_pRouteHandler->getCommandType(obj, cmd) != NETLINK_RET_SUCCESS) {
            NA_LOG_E("[%s] get %s command fail", __FUNCTION__, reqTypeToString(reqType));
            return -1;
        }
    } else if (reqType == REQUEST_TYPE_NETAGENT) {
        NetEventReqInfo *pNetEventReqInfo = (NetEventReqInfo *)obj;
        *cmd = pNetEventReqInfo->cmd;
    } else {
        NA_LOG_E("[%s] request is %s(%d)", __FUNCTION__, reqTypeToString(reqType), reqType);
        return -1;
    }
    return 0;
}

void NetAgentService::getIpv4Address(void *obj, char *addressV4) {
    unsigned int addrV4;
    if (NA_GET_ADDR_V4(obj, &addrV4) == NETAGENT_IO_RET_SUCCESS) {
        if (addrV4 != 0) {
            if (convertIpv4ToString(addressV4, &addrV4) < 0) {
                NA_LOG_E("[%s] error occurs when converting ipv4 to string", __FUNCTION__);
            }
        } else {
            NA_LOG_I("[%s] IPv4 address lost after IRAT", __FUNCTION__);
        }
    } else {
        NA_LOG_E("[%s] error occurs when parsing addressV4", __FUNCTION__);
    }
}

void NetAgentService::getIpv6Address(void *obj, char *addressV6) {
    unsigned int addrV6[4];
    if (NA_GET_ADDR_V6(obj, addrV6) == NETAGENT_IO_RET_SUCCESS) {
        if (!(addrV6[0] == 0 && addrV6[1] == 0 && addrV6[2] == 0 && addrV6[3] == 0)) {
            if (convertIpv6ToString(addressV6, addrV6) < 0) {
                NA_LOG_E("[%s] error occurs when converting ipv6 to string", __FUNCTION__);
            }
        } else {
            NA_LOG_I("[%s] IPv6 address lost after IRAT", __FUNCTION__);
        }
    } else {
        NA_LOG_E("[%s] error occurs when parsing addressV6", __FUNCTION__);
    }
}

void NetAgentService::getIpv4v6Address(void *obj, char *addressV4, char *addressV6) {
    getIpv4Address(obj, addressV4);
    getIpv6Address(obj, addressV6);
}

int NetAgentService::convertIpv6ToBinary(unsigned int *output, char *input) {
    int ret = 1;
    struct in6_addr v6Address;
    memset(&v6Address, 0, sizeof(v6Address));
    // ret: -1, error occurs, ret: 0, invalid address, ret: 1, success;
    ret = inet_pton(AF_INET6, input, &v6Address);
    if (ret >= 0) {
        memcpy(output, &v6Address, 16);
        return 0;
    }
    return -1;
}

int NetAgentService::convertIpv4ToString(char *output, unsigned int *input) {
    unsigned char *address = reinterpret_cast<unsigned char *>(input);
    if (output == NULL || address == NULL) {
        NA_LOG_E("[%s] null occurs on output = %s or addressV4 = %s", __FUNCTION__, output, address);
        return -1;
    }
    sprintf(output, "%d.%d.%d.%d", *address, *(address+1), *(address+2), *(address+3));
    return 0;
}

int NetAgentService::convertIpv6ToString(char *output, unsigned int *input) {
    unsigned char *address = reinterpret_cast<unsigned char *>(input);
    if (output == NULL || address == NULL) {
        NA_LOG_E("[%s] null occurs on output = %s or addressV6 = %s", __FUNCTION__, output, address);
        return -1;
    }
    sprintf(output, "%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X",
            *address, *(address+1), *(address+2), *(address+3),
            *(address+4), *(address+5), *(address+6), *(address+7),
            *(address+8), *(address+9), *(address+10), *(address+11),
            *(address+12), *(address+13), *(address+14), *(address+15));
    return 0;
}

void NetAgentService::freeNetAgentCmdObj(NetAgentReqInfo *pReqInfo) {
    if (pReqInfo->reqType == REQUEST_TYPE_DDM) {
        NA_CMD_FREE(pReqInfo->pNetAgentCmdObj);
    } else if (pReqInfo->reqType == REQUEST_TYPE_NETLINK) {
        m_pRouteHandler->freeNetlinkEventObj(pReqInfo->pNetAgentCmdObj);
    } else if (pReqInfo->reqType == REQUEST_TYPE_NETAGENT) {
        FREEIF(pReqInfo->pNetAgentCmdObj);
    }
}

void NetAgentService::setNetworkTransmitState(int state, int transIntfId,
        const sp<NetActionBase>& action) {
    if (isTransIntfIdMatched(transIntfId)) {
        NetEventReqInfo *pNetEventObj = (NetEventReqInfo *)calloc(1, sizeof(NetEventReqInfo));
        if (pNetEventObj == NULL) {
            NA_LOG_E("[%s] can't allocate rild event obj", __FUNCTION__);
            action->ack(false);
            return;
        }

        pNetEventObj->cmd = NETAGENT_IO_CMD_IFSTATE;
        pNetEventObj->action = action;
        pNetEventObj->parameter.snts.state = state;
        pNetEventObj->parameter.snts.interfaceId = transIntfId % TRANSACTION_ID_OFFSET;

        enqueueReqInfo(pNetEventObj, REQUEST_TYPE_NETAGENT);
    } else {
        action->ack(false);
    }
}

void NetAgentService::configureNetworkTransmitState(NetAgentReqInfo* pReqInfo) {
    NetEventReqInfo *pNetEventObj = (NetEventReqInfo *)pReqInfo->pNetAgentCmdObj;
    setNwTxqState(pNetEventObj->parameter.snts.interfaceId, pNetEventObj->parameter.snts.state);
    pNetEventObj->action->ack(true);
}

void NetAgentService::setNwTxqState(int interfaceId, int state) {
    struct ifreq ifr;

    // Tclose timer(sec.) << 16 | 0 : stop uplink data transfer with Tclose timer
    // 1 : start uplink data transfer
    memset(&ifr, 0, sizeof(struct ifreq));
    snprintf(ifr.ifr_name, IFNAMSIZ, "%s", getNetworkInterfaceName(interfaceId));
    ifc_set_txq_state(ifr.ifr_name, state);
}

char* NetAgentService::getNetworkInterfaceName(int interfaceId) {
    char* ret = NULL;
    ret = ccci_get_node_name(static_cast<CCCI_USER>(USR_NET_0 + interfaceId), MD_SYS1);
    return ret;
}

bool NetAgentService::isTransIntfIdMatched(int transIntfId) {
    for (std::list<int>::iterator it = m_lTransIntfId.begin(); it != m_lTransIntfId.end(); ++it) {
        if (*it == transIntfId) {
            return true;
        }
    }
    NA_LOG_I("[%s] transIntfId %d is not matched", __FUNCTION__, transIntfId);
    return false;
}

int NetAgentService::getTransIntfId(int interfaceId) {
    if (interfaceId == INVALID_INTERFACE_ID) {
        NA_LOG_E("[%s] invalid interfaceId", __FUNCTION__);
        return INVALID_TRANS_INTF_ID;
    }

    for (std::list<int>::iterator it = m_lTransIntfId.begin(); it != m_lTransIntfId.end(); it++) {
        if (((*it) % TRANSACTION_ID_OFFSET) == interfaceId) {
            return *it;
        }
    }
    return INVALID_TRANS_INTF_ID;
}

const char *NetAgentService::cmdToString(NA_CMD cmd) {
    switch (cmd) {
        case NETAGENT_IO_CMD_IFST: return "IFST";
        case NETAGENT_IO_CMD_RA: return "RA";
        case NETAGENT_IO_CMD_IPUPDATE: return "IPUPDATE";
        case NETAGENT_IO_CMD_IFUP: return "IFUP";
        case NETAGENT_IO_CMD_IFDOWN: return "IFDOWN";
        case NETAGENT_IO_CMD_IFCHG: return "IFCHG";
        case NETAGENT_IO_CMD_IFSTATE: return "IFSTATE";
        case NETAGENT_IO_CMD_SETMTU: return "SETMTU";
        case NETAGENT_IO_CMD_SYNC_CAPABILITY: return "SYNCCAP";
        case NETAGENT_IO_CMD_PDNHO: return "PDNHO";
        case NETAGENT_IO_CMD_IPCHG: return "IPCHG";
        default: return "UNKNOWN";
    }
}

const char *NetAgentService::addrTypeToString(NA_ADDR_TYPE addrType) {
    switch (addrType) {
        case NETAGENT_IO_ADDR_TYPE_IPv4: return "IPV4";
        case NETAGENT_IO_ADDR_TYPE_IPv6: return "IPV6";
        case NETAGENT_IO_ADDR_TYPE_IPv4v6: return "IPV4V6";
        default: return "UNKNOWN";
    }
}

const char *NetAgentService::reqTypeToString(REQUEST_TYPE reqType) {
    switch (reqType) {
        case REQUEST_TYPE_DDM: return "DDM";
        case REQUEST_TYPE_NETLINK: return "NETLINK";
        case REQUEST_TYPE_NETAGENT: return "NETAGENT";
        default: return "UNKNOWN";
    }
}

const char *NetAgentService::ranTypeToString(NA_RAN_TYPE ranType) {
    switch (ranType) {
        case NETAGENT_IO_HO_RAN_MOBILE: return "MOBILE";
        case NETAGENT_IO_HO_RAN_WIFI: return "WIFI";
        default: return "UNKNOWN";
    }
}

const char *NetAgentService::hoStateToString(int state) {
    switch (state) {
        case 0: return "START";
        case 1: return "STOP";
        default: return "UNKNOWN";
    }
}

const char *NetAgentService::hoResultToString(int result) {
    switch (result) {
        case 0: return "FAIL";
        case 1: return "SUCCESS";
        default: return "UNKNOWN";
    }
}

// Test mode start.
void NetAgentService::setTransactionInterfaceId(int transIntfId) {
    NA_LOG_D("[%s] transIntfId = %d", __FUNCTION__, transIntfId);
    if (transIntfId == INVALID_TRANS_INTF_ID) {
        return;
    }
    m_lTransIntfId.push_back(transIntfId);
}

void NetAgentService::removeTransactionInterfaceId(int transIntfId) {
    NA_LOG_D("[%s] transIntfId = %d", __FUNCTION__, transIntfId);
    if (transIntfId == INVALID_TRANS_INTF_ID) {
        return;
    }
    m_lTransIntfId.remove(transIntfId);
}

void NetAgentService::removeAllTransactionInterfaceId() {
    NA_LOG_D("[%s] X", __FUNCTION__);
    m_lTransIntfId.clear();
}
// Test mode end.
