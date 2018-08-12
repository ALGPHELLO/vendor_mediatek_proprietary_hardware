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

// MTK fusion include
#include "RfxVoidData.h"
#include "RfxStringData.h"
#include "RfxStringsData.h"
#include "RfxIntsData.h"

// MWI Local include
#include "RmcMobileWifiRequestHandler.h"
#include "RmcMobileWifiInterface.h"

#define RFX_LOG_TAG "RmcMwi"

 // register handler to channel
RFX_IMPLEMENT_HANDLER_CLASS(RmcMobileWifiRequestHandler, RIL_CMD_PROXY_1);

// register request to RfxData
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxStringsData, RfxVoidData, RFX_MSG_REQUEST_SET_WIFI_ENABLED);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxStringsData, RfxVoidData, RFX_MSG_REQUEST_SET_WIFI_ASSOCIATED);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxStringsData, RfxVoidData, RFX_MSG_REQUEST_SET_WIFI_SIGNAL_LEVEL);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxStringsData, RfxVoidData, RFX_MSG_REQUEST_SET_GEO_LOCATION);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxStringsData, RfxVoidData, RFX_MSG_REQUEST_SET_WIFI_IP_ADDRESS);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxStringData, RfxVoidData, RFX_MSG_REQUEST_SET_EMERGENCY_ADDRESS_ID);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxStringsData, RfxVoidData, RFX_MSG_REQUEST_SET_NATT_KEEP_ALIVE_STATUS);

static const int requests[] = {
    RFX_MSG_REQUEST_SET_WIFI_ENABLED,
    RFX_MSG_REQUEST_SET_WIFI_ASSOCIATED,
    RFX_MSG_REQUEST_SET_WIFI_SIGNAL_LEVEL,
    RFX_MSG_REQUEST_SET_GEO_LOCATION,
    RFX_MSG_REQUEST_SET_WIFI_IP_ADDRESS,
    RFX_MSG_REQUEST_SET_EMERGENCY_ADDRESS_ID,
    RFX_MSG_REQUEST_SET_NATT_KEEP_ALIVE_STATUS,
};

RmcMobileWifiRequestHandler::RmcMobileWifiRequestHandler(
    int slot_id, int channel_id) : RfxBaseHandler(slot_id, channel_id) {
    // register to handle request
    registerToHandleRequest(requests, sizeof(requests) / sizeof(int));
}

RmcMobileWifiRequestHandler::~RmcMobileWifiRequestHandler() {
}

void RmcMobileWifiRequestHandler::onHandleTimer() {
}

void RmcMobileWifiRequestHandler::onHandleRequest(const sp<RfxMclMessage>& msg) {
    int requestId = msg->getId();

    switch (requestId) {
        case RFX_MSG_REQUEST_SET_WIFI_ENABLED:
            setWifiEnabled(msg);
            break;
        case RFX_MSG_REQUEST_SET_WIFI_ASSOCIATED:
            setWifiAssociated(msg);
            break;
        case RFX_MSG_REQUEST_SET_WIFI_SIGNAL_LEVEL:
            setWifiSignal(msg);
            break;
        case RFX_MSG_REQUEST_SET_GEO_LOCATION:
            setGeoLocation(msg);
            break;
        case RFX_MSG_REQUEST_SET_WIFI_IP_ADDRESS:
            setWifiIpAddress(msg);
            break;
        case RFX_MSG_REQUEST_SET_EMERGENCY_ADDRESS_ID:
            setEmergencyAddressId(msg);
            break;
        case RFX_MSG_REQUEST_SET_NATT_KEEP_ALIVE_STATUS:
            setNattKeepAliveStatus(msg);
            break;
        default:
            break;
    }
}

void RmcMobileWifiRequestHandler::setWifiEnabled(const sp<RfxMclMessage>& msg) {
    /* AT+EWIFIEN=<ifname>,<enabled>
     * <ifname>: interface name, such as wlan0
     * <enabled>: 0 = disable; 1 = enable
     * <flightModeOn>: 0 = disable; 1 = enable
     */
    char** params = (char**)msg->getData()->getData();
    int dataLen =  msg->getData()->getDataLength() / sizeof(char*);
    logD(RFX_LOG_TAG, "setWifiEnabled dataLen: %d", dataLen);

    char* atCmd = AT_SET_WIFI_ENABLE;
    char* ifname = params[0];
    int enabled = atoi(params[1]);

    if (dataLen == 2) {
        String8 cmd = String8::format("%s=\"%s\",%d", atCmd, ifname, enabled);
        handleCmdWithVoidResponse(msg, cmd);
    } else if (dataLen == 3) {
        int flightModeOn = atoi(params[2]);
        String8 cmd = String8::format("%s=\"%s\",%d,%d", atCmd, ifname, enabled, flightModeOn);
        handleCmdWithVoidResponse(msg, cmd);
    }
}

void RmcMobileWifiRequestHandler::setWifiAssociated(const sp<RfxMclMessage>& msg) {
    /* AT+EWIFIASC=<ifname>,<assoc>,<ssid>,<ap_mac>
     * <ifname>: interface name, such as wlan0
     * <assoc>: 0 = not associated; 1 = associated
     * <ssid>: wifi ap ssid when associated, 0 if assoc = 0
     * <ap_mac>: wifi ap mac addr, 0 if assoc = 0
     */
    char** params = (char**)msg->getData()->getData();

    char* atCmd = AT_SET_WIFI_ASSOCIATED;
    char* ifname = params[0];
    const char* assoc = params[1];
    char* ssid = (atoi(assoc) == 0)? (char*)"0": params[2];
    char* ap_mac = (atoi(assoc) == 0)? (char*)"0": params[3];

    String8 cmd = String8::format("%s=\"%s\",%s,\"%s\",\"%s\"",
                                   atCmd, ifname, assoc, ssid, ap_mac);
    handleCmdWithVoidResponse(msg, cmd);
}

void RmcMobileWifiRequestHandler::setWifiSignal(const sp<RfxMclMessage>& msg) {
    /* AT+EWIFISIGLVL=<ifname>,<rssi>,<snr>
     * <ifname>: interface name, such as wlan0
     * <rssi>: rssi value
     * <snr>: string value
     */
    char** params = (char**)msg->getData()->getData();

    char* atCmd = AT_SET_WIFI_SIGNAL_LEVEL;
    char* ifname = params[0];
    int rssi = atoi(params[1]);
    char* snr = params[2];

    String8 cmd = String8::format("%s=\"%s\",%d,\"%s\"", atCmd, ifname, rssi, snr);
    handleCmdWithVoidResponse(msg, cmd);
}

void RmcMobileWifiRequestHandler::setWifiIpAddress(const sp<RfxMclMessage>& msg) {
    /* AT+EWIFISIGLVL=<ifname>,<ipv4>,<ipv6>
     * <ifname>: interface name, such as wlan0
     * <ipv4>: IPV4 address
     * <ipv6>: IPV6 address
     */
    char** params = (char**)msg->getData()->getData();

    char* atCmd = AT_SET_WIFI_IP_ADDRESS;
    char* ifname = params[0];

    // Google HIDL service changes "" in java as null in cpp
    char* ipv4 = (params[1] == NULL) ? (char*)"" : params[1];
    char* ipv6 = (params[2] == NULL) ? (char*)"" : params[2];

    String8 cmd = String8::format("%s=\"%s\",\"%s\",\"%s\"", atCmd, ifname, ipv4, ipv6);
    handleCmdWithVoidResponse(msg, cmd);
}

void RmcMobileWifiRequestHandler::setGeoLocation(const sp<RfxMclMessage>& msg) {
    /* AT+EIMSGEO=<account_id>,<broadcast_flag>,<latitude>,<longitude>,<accurate>,<method>,<city>,<state>,<zip>,<country>
     * <account_id>: request id, 0~7
     * <broadcast_flag>: 0, 1
     * <latitude>: latitude from GPS, 0 as failed
     * <longitude>: longitude from GPS, 0 as failed
     * <accurate>: accurate from GPS, 0 as failed
     * <method>: Location information from Fwk type, Network or GPS
     * <city>: City
     * <state>: State
     * <zip>: Zip
     * <country>: country
     * <ueWlanMac>: UE Wi-Fi interface mac address
     */
    char** params = (char**)msg->getData()->getData();
    char* atCmd = AT_SET_GEO_LOCATION;
    int dataLen =  msg->getData()->getDataLength() / sizeof(char*);

    logD(RFX_LOG_TAG, "setGeoLocation dataLen: %d", dataLen);

    // Google HIDL service changes "" in java as null in cpp
    char* method = (params[5] == NULL) ? (char*)"" : params[5];
    char* city = (params[6] == NULL) ? (char*)"" : params[6];
    char* state = (params[7] == NULL) ? (char*)"" : params[7];
    char* zip = (params[8] == NULL) ? (char*)"" : params[8];
    char* country = (params[9] == NULL) ? (char*)"" : params[9];

    if (dataLen == 10) {
        String8 cmd = String8::format("%s=%s,%s,%s,%s,%s,\"%s\",\"%s\",\"%s\",\"%s\",\"%s\"", atCmd,
            params[0], params[1], params[2], params[3], params[4],
            method, city, state, zip, country);
        handleCmdWithVoidResponse(msg, cmd);
    } else if (dataLen == 11) {
        char* ueWlanMac = (params[10] == NULL) ? (char*)"" : params[10];
        String8 cmd = String8::format("%s=%s,%s,%s,%s,%s,\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\"", atCmd,
            params[0], params[1], params[2], params[3], params[4],
            method, city, state, zip, country, ueWlanMac);
        handleCmdWithVoidResponse(msg, cmd);
    }
}

void RmcMobileWifiRequestHandler::setEmergencyAddressId(const sp<RfxMclMessage>& msg) {
    /* AT+EIMSAID = <aid>
     * <aid>: Access Id from Settings UI
     */
    char* atCmd = AT_SET_ECC_AID;
    char* aid = (char*)msg->getData()->getData();

    String8 cmd = String8::format("%s=\"%s\"", atCmd, aid);
    handleCmdWithVoidResponse(msg, cmd);
}

void RmcMobileWifiRequestHandler::setNattKeepAliveStatus(const sp<RfxMclMessage>& msg) {
    /* AT+EWIFINATT= <ifname>,<enable>,<src_ip>,<src_port>,<dst_ip>,<dst_port>
     * <ifname>: interface name, such as wlan0
     * <enable>: enabled, 0 = disable; 1 = enabled
     * <src_ip>: source IP
     * <src_port>: source port
     * <dst_ip>: destination IP
     * <dst_port>: destination port
     */
    char** params = (char**)msg->getData()->getData();
    char* atCmd = AT_SET_NATT_KEEP_ALIVE_STATUS;

    String8 cmd = String8::format("%s=\"%s\",%s,\"%s\",%s,\"%s\",%s", atCmd,
        params[0], params[1], params[2], params[3], params[4], params[5]);

    handleCmdWithVoidResponse(msg, cmd);
}
