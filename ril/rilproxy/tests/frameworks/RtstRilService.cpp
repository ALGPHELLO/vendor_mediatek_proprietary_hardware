#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wswitch"
#pragma GCC diagnostic ignored "-Wformat"
#pragma GCC diagnostic ignored "-Wsign-compare"
#include "../../libril/ril_service.cpp"

class RadioResponseDefault: public IRadioResponse {
public:
    Return<void> getIccCardStatusResponse(const RadioResponseInfo& info, const CardStatus& cardStatus) { return Status::ok();}
    Return<void> supplyIccPinForAppResponse(const RadioResponseInfo& info, int32_t remainingRetries) { return Status::ok();}
    Return<void> supplyIccPukForAppResponse(const RadioResponseInfo& info, int32_t remainingRetries) { return Status::ok();}
    Return<void> supplyIccPin2ForAppResponse(const RadioResponseInfo& info, int32_t remainingRetries) { return Status::ok();}
    Return<void> supplyIccPuk2ForAppResponse(const RadioResponseInfo& info, int32_t remainingRetries) { return Status::ok();}
    Return<void> changeIccPinForAppResponse(const RadioResponseInfo& info, int32_t remainingRetries) { return Status::ok();}
    Return<void> changeIccPin2ForAppResponse(const RadioResponseInfo& info, int32_t remainingRetries) { return Status::ok();}
    Return<void> supplyNetworkDepersonalizationResponse(const RadioResponseInfo& info, int32_t remainingRetries) { return Status::ok();}
    Return<void> getCurrentCallsResponse(const RadioResponseInfo& info, const hidl_vec<Call>& calls) { return Status::ok();}
    Return<void> dialResponse(const RadioResponseInfo& info) { return Status::ok();}
    Return<void> getIMSIForAppResponse(const RadioResponseInfo& info, const hidl_string& imsi) { return Status::ok();}
    Return<void> hangupConnectionResponse(const RadioResponseInfo& info) { return Status::ok();}
    Return<void> hangupWaitingOrBackgroundResponse(const RadioResponseInfo& info) { return Status::ok();}
    Return<void> hangupForegroundResumeBackgroundResponse(const RadioResponseInfo& info) { return Status::ok();}
    Return<void> switchWaitingOrHoldingAndActiveResponse(const RadioResponseInfo& info) { return Status::ok();}
    Return<void> conferenceResponse(const RadioResponseInfo& info) { return Status::ok();}
    Return<void> rejectCallResponse(const RadioResponseInfo& info) { return Status::ok();}
    Return<void> getLastCallFailCauseResponse(const RadioResponseInfo& info, const LastCallFailCauseInfo& failCauseinfo) { return Status::ok();}
    Return<void> getSignalStrengthResponse(const RadioResponseInfo& info, const SignalStrength& sigStrength) { return Status::ok();}
    Return<void> getVoiceRegistrationStateResponse(const RadioResponseInfo& info, const VoiceRegStateResult& voiceRegResponse) { return Status::ok();}
    Return<void> getDataRegistrationStateResponse(const RadioResponseInfo& info, const DataRegStateResult& dataRegResponse) { return Status::ok();}
    Return<void> getOperatorResponse(const RadioResponseInfo& info, const  hidl_string& longName, const  hidl_string& shortName, const  hidl_string& numeric) { return Status::ok();}
    Return<void> setRadioPowerResponse(const RadioResponseInfo& info) { return Status::ok();}
    Return<void> sendDtmfResponse(const RadioResponseInfo& info) { return Status::ok();}
    Return<void> sendSmsResponse(const RadioResponseInfo& info, const SendSmsResult& sms) { return Status::ok();}
    Return<void> sendSMSExpectMoreResponse(const RadioResponseInfo& info, const SendSmsResult& sms) { return Status::ok();}
    Return<void> setupDataCallResponse(const RadioResponseInfo& info, const SetupDataCallResult& dcResponse) { return Status::ok();}
    Return<void> iccIOForAppResponse(const RadioResponseInfo& info, const IccIoResult& iccIo) { return Status::ok();}
    Return<void> sendUssdResponse(const RadioResponseInfo& info) { return Status::ok();}
    Return<void> cancelPendingUssdResponse(const RadioResponseInfo& info) { return Status::ok();}
    Return<void> getClirResponse(const RadioResponseInfo& info, int32_t n, int32_t m) { return Status::ok();}
    Return<void> setClirResponse(const RadioResponseInfo& info) { return Status::ok();}
    Return<void> getCallForwardStatusResponse(const RadioResponseInfo& info, const  hidl_vec<CallForwardInfo>& callForwardInfos) { return Status::ok();}
    Return<void> setCallForwardResponse(const RadioResponseInfo& info) { return Status::ok();}
    Return<void> getCallWaitingResponse(const RadioResponseInfo& info, bool enable, int32_t serviceClass) { return Status::ok();}
    Return<void> setCallWaitingResponse(const RadioResponseInfo& info) { return Status::ok();}
    Return<void> acknowledgeLastIncomingGsmSmsResponse(const RadioResponseInfo& info) { return Status::ok();}
    Return<void> acceptCallResponse(const RadioResponseInfo& info) { return Status::ok();}
    Return<void> deactivateDataCallResponse(const RadioResponseInfo& info) { return Status::ok();}
    Return<void> getFacilityLockForAppResponse(const RadioResponseInfo& info, int32_t response) { return Status::ok();}
    Return<void> setFacilityLockForAppResponse(const RadioResponseInfo& info, int32_t retry) { return Status::ok();}
    Return<void> setBarringPasswordResponse(const RadioResponseInfo& info) { return Status::ok();}
    Return<void> getNetworkSelectionModeResponse(const RadioResponseInfo& info, bool manual) { return Status::ok();}
    Return<void> setNetworkSelectionModeAutomaticResponse(const RadioResponseInfo& info) { return Status::ok();}
    Return<void> setNetworkSelectionModeManualResponse(const RadioResponseInfo& info) { return Status::ok();}
    Return<void> getAvailableNetworksResponse(const RadioResponseInfo& info, const  hidl_vec<OperatorInfo>& networkInfos) { return Status::ok();}
    Return<void> startDtmfResponse(const RadioResponseInfo& info) { return Status::ok();}
    Return<void> stopDtmfResponse(const RadioResponseInfo& info) { return Status::ok();}
    Return<void> getBasebandVersionResponse(const RadioResponseInfo& info, const  hidl_string& version) { return Status::ok();}
    Return<void> separateConnectionResponse(const RadioResponseInfo& info) { return Status::ok();}
    Return<void> setMuteResponse(const RadioResponseInfo& info) { return Status::ok();}
    Return<void> getMuteResponse(const RadioResponseInfo& info, bool enable) { return Status::ok();}
    Return<void> getClipResponse(const RadioResponseInfo& info, ClipStatus status) { return Status::ok();}
    Return<void> getDataCallListResponse(const RadioResponseInfo& info, const  hidl_vec<SetupDataCallResult>& dcResponse) { return Status::ok();}
    Return<void> sendOemRilRequestRawResponse(const RadioResponseInfo& info, const  hidl_vec<uint8_t>& data) { return Status::ok();}
    Return<void> sendOemRilRequestStringsResponse(const RadioResponseInfo& info, const  hidl_vec< hidl_string>& data) { return Status::ok();}
    Return<void> sendScreenStateResponse(const RadioResponseInfo& info) { return Status::ok();}
    Return<void> setSuppServiceNotificationsResponse(const RadioResponseInfo& info) { return Status::ok();}
    Return<void> writeSmsToSimResponse(const RadioResponseInfo& info, int32_t index) { return Status::ok();}
    Return<void> deleteSmsOnSimResponse(const RadioResponseInfo& info) { return Status::ok();}
    Return<void> setBandModeResponse(const RadioResponseInfo& info) { return Status::ok();}
    Return<void> getAvailableBandModesResponse(const RadioResponseInfo& info, const  hidl_vec<RadioBandMode>& bandModes) { return Status::ok();}
    Return<void> sendEnvelopeResponse(const RadioResponseInfo& info, const  hidl_string& commandResponse) { return Status::ok();}
    Return<void> sendTerminalResponseToSimResponse(const RadioResponseInfo& info) { return Status::ok();}
    Return<void> handleStkCallSetupRequestFromSimResponse(const RadioResponseInfo& info) { return Status::ok();}
    Return<void> explicitCallTransferResponse(const RadioResponseInfo& info) { return Status::ok();}
    Return<void> setPreferredNetworkTypeResponse(const RadioResponseInfo& info) { return Status::ok();}
    Return<void> getPreferredNetworkTypeResponse(const RadioResponseInfo& info, PreferredNetworkType nwType) { return Status::ok();}
    Return<void> getNeighboringCidsResponse(const RadioResponseInfo& info, const  hidl_vec<NeighboringCell>& cells) { return Status::ok();}
    Return<void> setLocationUpdatesResponse(const RadioResponseInfo& info) { return Status::ok();}
    Return<void> setCdmaSubscriptionSourceResponse(const RadioResponseInfo& info) { return Status::ok();}
    Return<void> setCdmaRoamingPreferenceResponse(const RadioResponseInfo& info) { return Status::ok();}
    Return<void> getCdmaRoamingPreferenceResponse(const RadioResponseInfo& info, CdmaRoamingType type) { return Status::ok();}
    Return<void> setTTYModeResponse(const RadioResponseInfo& info) { return Status::ok();}
    Return<void> getTTYModeResponse(const RadioResponseInfo& info, TtyMode mode) { return Status::ok();}
    Return<void> setPreferredVoicePrivacyResponse(const RadioResponseInfo& info) { return Status::ok();}
    Return<void> getPreferredVoicePrivacyResponse(const RadioResponseInfo& info, bool enable) { return Status::ok();}
    Return<void> sendCDMAFeatureCodeResponse(const RadioResponseInfo& info) { return Status::ok();}
    Return<void> sendBurstDtmfResponse(const RadioResponseInfo& info) { return Status::ok();}
    Return<void> sendCdmaSmsResponse(const RadioResponseInfo& info, const SendSmsResult& sms) { return Status::ok();}
    Return<void> acknowledgeLastIncomingCdmaSmsResponse(const RadioResponseInfo& info) { return Status::ok();}
    Return<void> getGsmBroadcastConfigResponse(const RadioResponseInfo& info, const  hidl_vec<GsmBroadcastSmsConfigInfo>& configs) { return Status::ok();}
    Return<void> setGsmBroadcastConfigResponse(const RadioResponseInfo& info) { return Status::ok();}
    Return<void> setGsmBroadcastActivationResponse(const RadioResponseInfo& info) { return Status::ok();}
    Return<void> getCdmaBroadcastConfigResponse(const RadioResponseInfo& info, const  hidl_vec<CdmaBroadcastSmsConfigInfo>& configs) { return Status::ok();}
    Return<void> setCdmaBroadcastConfigResponse(const RadioResponseInfo& info) { return Status::ok();}
    Return<void> setCdmaBroadcastActivationResponse(const RadioResponseInfo& info) { return Status::ok();}
    Return<void> getCDMASubscriptionResponse(const RadioResponseInfo& info, const  hidl_string& mdn, const  hidl_string& hSid, const  hidl_string& hNid, const  hidl_string& min, const  hidl_string& prl) { return Status::ok();}
    Return<void> writeSmsToRuimResponse(const RadioResponseInfo& info, uint32_t index) { return Status::ok();}
    Return<void> deleteSmsOnRuimResponse(const RadioResponseInfo& info) { return Status::ok();}
    Return<void> getDeviceIdentityResponse(const RadioResponseInfo& info, const  hidl_string& imei, const  hidl_string& imeisv, const  hidl_string& esn, const  hidl_string& meid) { return Status::ok();}
    Return<void> exitEmergencyCallbackModeResponse(const RadioResponseInfo& info) { return Status::ok();}
    Return<void> getSmscAddressResponse(const RadioResponseInfo& info, const  hidl_string& smsc) { return Status::ok();}
    Return<void> setSmscAddressResponse(const RadioResponseInfo& info) { return Status::ok();}
    Return<void> reportSmsMemoryStatusResponse(const RadioResponseInfo& info) { return Status::ok();}
    Return<void> reportStkServiceIsRunningResponse(const RadioResponseInfo& info) { return Status::ok();}
    Return<void> getCdmaSubscriptionSourceResponse(const RadioResponseInfo& info, CdmaSubscriptionSource source) { return Status::ok();}
    Return<void> requestIsimAuthenticationResponse(const RadioResponseInfo& info, const  hidl_string& response) { return Status::ok();}
    Return<void> acknowledgeIncomingGsmSmsWithPduResponse(const RadioResponseInfo& info) { return Status::ok();}
    Return<void> sendEnvelopeWithStatusResponse(const RadioResponseInfo& info, const IccIoResult& iccIo) { return Status::ok();}
    Return<void> getVoiceRadioTechnologyResponse(const RadioResponseInfo& info, RadioTechnology rat) { return Status::ok();}
    Return<void> getCellInfoListResponse(const RadioResponseInfo& info, const  hidl_vec<CellInfo>& cellInfo) { return Status::ok();}
    Return<void> setCellInfoListRateResponse(const RadioResponseInfo& info) { return Status::ok();}
    Return<void> setInitialAttachApnResponse(const RadioResponseInfo& info) { return Status::ok();}
    Return<void> getImsRegistrationStateResponse(const RadioResponseInfo& info, bool isRegistered, RadioTechnologyFamily ratFamily) { return Status::ok();}
    Return<void> sendImsSmsResponse(const RadioResponseInfo& info, const SendSmsResult& sms) { return Status::ok();}
    Return<void> iccTransmitApduBasicChannelResponse(const RadioResponseInfo& info, const IccIoResult& result) { return Status::ok();}
    Return<void> iccOpenLogicalChannelResponse(const RadioResponseInfo& info, int32_t channelId, const  hidl_vec<int8_t>& selectResponse) { return Status::ok();}
    Return<void> iccCloseLogicalChannelResponse(const RadioResponseInfo& info) { return Status::ok();}
    Return<void> iccTransmitApduLogicalChannelResponse(const RadioResponseInfo& info, const IccIoResult& result) { return Status::ok();}
    Return<void> nvReadItemResponse(const RadioResponseInfo& info, const  hidl_string& result) { return Status::ok();}
    Return<void> nvWriteItemResponse(const RadioResponseInfo& info) { return Status::ok();}
    Return<void> nvWriteCdmaPrlResponse(const RadioResponseInfo& info) { return Status::ok();}
    Return<void> nvResetConfigResponse(const RadioResponseInfo& info) { return Status::ok();}
    Return<void> setUiccSubscriptionResponse(const RadioResponseInfo& info) { return Status::ok();}
    Return<void> setDataAllowedResponse(const RadioResponseInfo& info) { return Status::ok();}
    Return<void> getHardwareConfigResponse(const RadioResponseInfo& info, const  hidl_vec<HardwareConfig>& config) { return Status::ok();}
    Return<void> requestIccSimAuthenticationResponse(const RadioResponseInfo& info, const IccIoResult& result) { return Status::ok();}
    Return<void> setDataProfileResponse(const RadioResponseInfo& info) { return Status::ok();}
    Return<void> requestShutdownResponse(const RadioResponseInfo& info) { return Status::ok();}
    Return<void> getRadioCapabilityResponse(const RadioResponseInfo& info, const RadioCapability& rc) { return Status::ok();}
    Return<void> setRadioCapabilityResponse(const RadioResponseInfo& info, const RadioCapability& rc) { return Status::ok();}
    Return<void> startLceServiceResponse(const RadioResponseInfo& info, const LceStatusInfo& statusInfo) { return Status::ok();}
    Return<void> stopLceServiceResponse(const RadioResponseInfo& info, const LceStatusInfo& statusInfo) { return Status::ok();}
    Return<void> pullLceDataResponse(const RadioResponseInfo& info, const LceDataInfo& lceInfo) { return Status::ok();}
    Return<void> getModemActivityInfoResponse(const RadioResponseInfo& info, const ActivityStatsInfo& activityInfo) { return Status::ok();}
    Return<void> setAllowedCarriersResponse(const RadioResponseInfo& info, int32_t numAllowed) { return Status::ok();}
    Return<void> getAllowedCarriersResponse(const RadioResponseInfo& info, bool allAllowed, const CarrierRestrictions& carriers) { return Status::ok();}
    Return<void> sendDeviceStateResponse(const RadioResponseInfo &info) { return Status::ok();}
    Return<void> setIndicationFilterResponse(const RadioResponseInfo &info) { return Status::ok();}
    Return<void> setSimCardPowerResponse(const RadioResponseInfo &info) { return Status::ok();}
    Return<void> acknowledgeRequest(int32_t serial) { return Status::ok();}
};

using android::Parcel;
#include <cutils/jstring.h>

extern void writeRILSocket2(int slot, Parcel &p);
static void writeStringToParcel(Parcel &p, const char *s) {
    char16_t *s16;
    size_t s16_len;
    s16 = strdup8to16(s, &s16_len);
    p.writeString16(s16, s16_len);
    free(s16);
}


class RadioResponseImpl : public RadioResponseDefault {
public:
    Return<void> setCdmaBroadcastActivationResponse(const RadioResponseInfo& info) {
        Parcel p;
        p.writeInt32(RIL_REQUEST_CDMA_SMS_BROADCAST_ACTIVATION);
        p.writeInt32((int32_t)info.error);
        writeRILSocket2(m_slotId, p);

        return Status::ok();
    }

    Return<void> deleteSmsOnRuimResponse(const RadioResponseInfo& info) {
        Parcel p;
        p.writeInt32(RIL_REQUEST_CDMA_DELETE_SMS_ON_RUIM);
        p.writeInt32((int32_t)info.error);
        writeRILSocket2(m_slotId, p);

        return Status::ok();
    }

    Return<void> acknowledgeLastIncomingCdmaSmsResponse(const RadioResponseInfo& info) {
        Parcel p;
        p.writeInt32(RIL_REQUEST_CDMA_SMS_ACKNOWLEDGE);
        p.writeInt32((int32_t)info.error);
        writeRILSocket2(m_slotId, p);
        return Status::ok();
    }

    Return<void> setCdmaBroadcastConfigResponse(const RadioResponseInfo& info) {
        Parcel p;
        p.writeInt32(RIL_REQUEST_CDMA_SET_BROADCAST_SMS_CONFIG);
        p.writeInt32((int32_t)info.error);
        writeRILSocket2(m_slotId, p);
        return Status::ok();
    }

    Return<void> getCdmaBroadcastConfigResponse(const RadioResponseInfo& info,
            const hidl_vec<CdmaBroadcastSmsConfigInfo>& configs) {
        Parcel p;
        p.writeInt32(RIL_REQUEST_CDMA_GET_BROADCAST_SMS_CONFIG);
        p.writeInt32((int32_t)info.error);
        p.writeInt32(configs.size());
        for (int i = 0; i < configs.size(); i++) {
            p.writeInt32(configs[i].serviceCategory);
            p.writeInt32(configs[i].language);
            p.writeInt32(configs[i].selected ? 1 : 0);
        }
        writeRILSocket2(m_slotId, p);
        return Status::ok();
    }

    Return<void> sendCdmaSmsResponse(const RadioResponseInfo& info, const SendSmsResult& sms) {
        Parcel p;
        p.writeInt32(RIL_REQUEST_CDMA_SEND_SMS);
        p.writeInt32((int32_t)info.error);
        p.writeInt32(sms.messageRef);
        if (sms.ackPDU.empty()) {
            writeStringToParcel(p, NULL);
        } else {
            writeStringToParcel(p, sms.ackPDU.c_str());
        }
        p.writeInt32(sms.errorCode);
        writeRILSocket2(m_slotId, p);
        return Status::ok();
    }

    Return<void> writeSmsToRuimResponse(const RadioResponseInfo& info, uint32_t index) {
        Parcel p;
        p.writeInt32(RIL_REQUEST_CDMA_WRITE_SMS_TO_RUIM);
        p.writeInt32((int32_t)info.error);
        p.writeInt32(1);
        p.writeInt32(index);
        writeRILSocket2(m_slotId, p);
        return Status::ok();
    }

    Return<void> reportSmsMemoryStatusResponse(const RadioResponseInfo& info) {
        Parcel p;
        p.writeInt32(RIL_REQUEST_REPORT_SMS_MEMORY_STATUS);
        p.writeInt32((int32_t)info.error);
        writeRILSocket2(m_slotId, p);
        return Status::ok();
    }

    Return<void> sendImsSmsResponse(const RadioResponseInfo& info, const SendSmsResult& sms) {
        Parcel p;
        p.writeInt32(RIL_REQUEST_IMS_SEND_SMS);
        p.writeInt32((int32_t)info.error);
        //if ((int)info.error == 0) { // changes, N VS O
            p.writeInt32(sms.messageRef);
            if (sms.ackPDU.empty()) {
                writeStringToParcel(p, NULL);
            } else {
                writeStringToParcel(p, sms.ackPDU.c_str());
            }
            p.writeInt32(sms.errorCode);
        //}
        writeRILSocket2(m_slotId, p);
        return Status::ok();
    }

    Return<void> getSmscAddressResponse(const RadioResponseInfo& info, const  hidl_string& smsc) {
        Parcel p;
        p.writeInt32(RIL_REQUEST_GET_SMSC_ADDRESS);
        p.writeInt32((int32_t)info.error);
        if ((int)info.error == 0) {
            writeStringToParcel(p, smsc.c_str());
        }
        writeRILSocket2(m_slotId, p);
        return Status::ok();
    }

    Return<void> setSmscAddressResponse(const RadioResponseInfo& info) {
        Parcel p;
        p.writeInt32(RIL_REQUEST_SET_SMSC_ADDRESS);
        p.writeInt32((int32_t)info.error);
        writeRILSocket2(m_slotId, p);
        return Status::ok();
    }


    void setSlot(int slot) {
        m_slotId = slot;
    }
    int m_slotId;
};

class RadioIndicationDefault: public IRadioIndication {
public:
    Return<void> radioStateChanged(RadioIndicationType type, RadioState radioState) { return Status::ok(); }
    Return<void> callStateChanged(RadioIndicationType type) { return Status::ok(); }
    Return<void> networkStateChanged(RadioIndicationType type) { return Status::ok(); }
    Return<void> newSms(RadioIndicationType type, const hidl_vec<uint8_t>& pdu) { return Status::ok(); }
    Return<void> newSmsStatusReport(RadioIndicationType type, const hidl_vec<uint8_t>& pdu) { return Status::ok(); }
    Return<void> newSmsOnSim(RadioIndicationType type, int32_t recordNumber) { return Status::ok(); }
    Return<void> onUssd(RadioIndicationType type, UssdModeType modeType, const hidl_string& msg) { return Status::ok(); }
    Return<void> nitzTimeReceived(RadioIndicationType type, const hidl_string& nitzTime, uint64_t receivedTime) { return Status::ok(); }
    Return<void> currentSignalStrength(RadioIndicationType type, const SignalStrength& signalStrength) { return Status::ok(); }
    Return<void> dataCallListChanged(RadioIndicationType type, const hidl_vec<SetupDataCallResult>& dcList) { return Status::ok(); }
    Return<void> suppSvcNotify(RadioIndicationType type, const SuppSvcNotification& suppSvc) { return Status::ok(); }
    Return<void> stkSessionEnd(RadioIndicationType type) { return Status::ok(); }
    Return<void> stkProactiveCommand(RadioIndicationType type, const hidl_string& cmd) { return Status::ok(); }
    Return<void> stkEventNotify(RadioIndicationType type, const hidl_string& cmd) { return Status::ok(); }
    Return<void> stkCallSetup(RadioIndicationType type, int64_t timeout) { return Status::ok(); }
    Return<void> simSmsStorageFull(RadioIndicationType type) { return Status::ok(); }
    Return<void> simRefresh(RadioIndicationType type, const SimRefreshResult& refreshResult) { return Status::ok(); }
    Return<void> callRing(RadioIndicationType type, bool isGsm, const CdmaSignalInfoRecord& record) { return Status::ok(); }
    Return<void> simStatusChanged(RadioIndicationType type) { return Status::ok(); }
    Return<void> cdmaNewSms(RadioIndicationType type, const CdmaSmsMessage& msg) { return Status::ok(); }
    Return<void> newBroadcastSms(RadioIndicationType type, const hidl_vec<uint8_t>& data) { return Status::ok(); }
    Return<void> cdmaRuimSmsStorageFull(RadioIndicationType type) { return Status::ok(); }
    Return<void> restrictedStateChanged(RadioIndicationType type, PhoneRestrictedState state) { return Status::ok(); }
    Return<void> enterEmergencyCallbackMode(RadioIndicationType type) { return Status::ok(); }
    Return<void> cdmaCallWaiting(RadioIndicationType type, const CdmaCallWaiting& callWaitingRecord) { return Status::ok(); }
    Return<void> cdmaOtaProvisionStatus(RadioIndicationType type, CdmaOtaProvisionStatus status) { return Status::ok(); }
    Return<void> cdmaInfoRec(RadioIndicationType type, const CdmaInformationRecords& records) { return Status::ok(); }
    Return<void> oemHookRaw(RadioIndicationType type, const hidl_vec<uint8_t>& data) { return Status::ok(); }
    Return<void> indicateRingbackTone(RadioIndicationType type, bool start) { return Status::ok(); }
    Return<void> resendIncallMute(RadioIndicationType type) { return Status::ok(); }
    Return<void> cdmaSubscriptionSourceChanged(RadioIndicationType type, CdmaSubscriptionSource cdmaSource) { return Status::ok(); }
    Return<void> cdmaPrlChanged(RadioIndicationType type, int32_t version) { return Status::ok(); }
    Return<void> exitEmergencyCallbackMode(RadioIndicationType type) { return Status::ok(); }
    Return<void> rilConnected(RadioIndicationType type) { return Status::ok(); }
    Return<void> voiceRadioTechChanged(RadioIndicationType type, RadioTechnology rat) { return Status::ok(); }
    Return<void> cellInfoList(RadioIndicationType type, const hidl_vec<CellInfo>& records) { return Status::ok(); }
    Return<void> imsNetworkStateChanged(RadioIndicationType type) { return Status::ok(); }
    Return<void> subscriptionStatusChanged(RadioIndicationType type, bool activate) { return Status::ok(); }
    Return<void> srvccStateNotify(RadioIndicationType type, SrvccState state) { return Status::ok(); }
    Return<void> hardwareConfigChanged(RadioIndicationType type, const hidl_vec<HardwareConfig>& configs) { return Status::ok(); }
    Return<void> radioCapabilityIndication(RadioIndicationType type, const RadioCapability& rc) { return Status::ok(); }
    Return<void> onSupplementaryServiceIndication(RadioIndicationType type, const StkCcUnsolSsResult& ss) { return Status::ok(); }
    Return<void> stkCallControlAlphaNotify(RadioIndicationType type, const hidl_string& alpha) { return Status::ok(); }
    Return<void> lceData(RadioIndicationType type, const LceDataInfo& lce) { return Status::ok(); }
    Return<void> pcoData(RadioIndicationType type, const PcoDataInfo& pco) { return Status::ok(); }
    Return<void> modemReset(RadioIndicationType type, const hidl_string& reason) { return Status::ok(); }
};


class RadioIndicationImpl : public RadioIndicationDefault {
public:
    Return<void> cdmaRuimSmsStorageFull(RadioIndicationType type) {
        Parcel p;
        p.writeInt32(RIL_UNSOL_CDMA_RUIM_SMS_STORAGE_FULL);
        writeRILSocket2(m_slotId, p);
        return Status::ok();
    }
    Return<void> cdmaNewSms(RadioIndicationType type, const CdmaSmsMessage& msg) {
        Parcel p;
        p.writeInt32(RIL_UNSOL_RESPONSE_CDMA_NEW_SMS);
        p.writeInt32(msg.teleserviceId);
        p.writeInt32(msg.isServicePresent ? 1 : 0);
        p.writeInt32(msg.serviceCategory);
        p.writeInt32((int)msg.address.digitMode);
        p.writeInt32((int)msg.address.numberMode);
        p.writeInt32((int)msg.address.numberType);
        p.writeInt32((int)msg.address.numberPlan);
        p.writeInt32(msg.address.digits.size());
        for(int i = 0 ; i < msg.address.digits.size(); i++) {
            p.writeInt32(msg.address.digits[i]);
        }

        p.writeInt32((int)msg.subAddress.subaddressType);
        p.writeInt32(msg.subAddress.odd ? 1 : 0);
        p.writeInt32(msg.subAddress.digits.size());
        for(int i = 0 ; i < msg.subAddress.digits.size(); i ++) {
            p.writeInt32(msg.subAddress.digits[i]);
        }
        p.writeInt32(msg.bearerData.size());
        for(int i = 0 ; i < msg.bearerData.size(); i++) {
           p.writeInt32(msg.bearerData[i]);
        }

        writeRILSocket2(m_slotId, p);

        return Status::ok();
    }

public:
    void setSlot(int slot) {
        m_slotId = slot;
    }
    int m_slotId;
};


extern "C" void RIL_createRadioService(RIL_RadioFunctions *callbacks, CommandInfo *commands) {
    int simCount = 1;
    #if (SIM_COUNT >= 2)
    simCount = SIM_COUNT;
    #endif
    for (int i = 0; i < simCount; i++) {
        radioService[i] = new RadioImpl;
        radioService[i]->mSlotId = i;
        sp<RadioResponseImpl> rr = new RadioResponseImpl();
        rr->setSlot(i);
        sp<RadioIndicationImpl> ri = new RadioIndicationImpl();
        ri->setSlot(i);
        radioService[i]->setResponseFunctions(rr, ri);
    }
    s_vendorFunctions = callbacks;
    s_commands = commands;
}

const sp<RadioImpl>& RIL_getRadioService(int slot) {
    return radioService[slot];
}


int g_serial = 1;
void setCdmaBroadcastActivation(int slot, bool activate) {
    RIL_getRadioService(slot)->setCdmaBroadcastActivation(g_serial++, activate);
}

void deleteSmsOnRuim(int slot, int index) {
    RIL_getRadioService(slot)->deleteSmsOnRuim(g_serial++, index);
}

void acknowledgeLastIncomingCdmaSms(int slot, int errorClass, int causeCode) {
    CdmaSmsAck smsAck;
    smsAck.errorClass = (CdmaSmsErrorClass)errorClass;
    smsAck.smsCauseCode = causeCode;
    RIL_getRadioService(slot)->acknowledgeLastIncomingCdmaSms(g_serial++, smsAck);
}

void setCdmaBroadcastConfig(int slot, int config[], int num) {
    hidl_vec<CdmaBroadcastSmsConfigInfo> configInfo;
    configInfo.resize(num);
    for (int i = 0; i < num; i++) {
        configInfo[i].serviceCategory = config[i * 3];
        configInfo[i].language = config[i * 3 + 1];
        configInfo[i].selected = (config[i * 3 + 2] == 1);
    }
    RIL_getRadioService(slot)->setCdmaBroadcastConfig(g_serial++, configInfo);
}

void getCdmaBroadcastConfig(int slot) {
    RIL_getRadioService(slot)->getCdmaBroadcastConfig(g_serial++);
}

void setCdmaSms(
        CdmaSmsMessage &sms,
        int teleserviceId,
        bool isServicePresent,
        int serviceCategory,
        const RIL_CDMA_SMS_Address &addr,
        const RIL_CDMA_SMS_Subaddress &subAddr,
        int uBearerDataLen,
        unsigned char bearerData[]) {
    sms.teleserviceId = teleserviceId;
    sms.isServicePresent = isServicePresent;
    sms.serviceCategory = serviceCategory;
    sms.address.digitMode =
            (android::hardware::radio::V1_0::CdmaSmsDigitMode)addr.digit_mode;
    sms.address.numberMode =
            (android::hardware::radio::V1_0::CdmaSmsNumberMode)addr.number_mode;
    sms.address.numberType =
            (android::hardware::radio::V1_0::CdmaSmsNumberType)addr.number_type;
    sms.address.numberPlan =
            (android::hardware::radio::V1_0::CdmaSmsNumberPlan)addr.number_plan;

    int digitLimit = MIN((addr.number_of_digits), RIL_CDMA_SMS_ADDRESS_MAX);
    sms.address.digits.setToExternal((unsigned char *)addr.digits, digitLimit);

    sms.subAddress.subaddressType = (android::hardware::radio::V1_0::CdmaSmsSubaddressType)
            subAddr.subaddressType;
    sms.subAddress.odd = subAddr.odd;

    digitLimit= MIN((subAddr.number_of_digits), RIL_CDMA_SMS_SUBADDRESS_MAX);
    sms.subAddress.digits.setToExternal((unsigned char *)subAddr.digits, digitLimit);

    digitLimit = MIN((uBearerDataLen), RIL_CDMA_SMS_BEARER_DATA_MAX);
    sms.bearerData.setToExternal((unsigned char *)bearerData, digitLimit);
}

void sendCdmaSms(
    int slot,
    int teleserviceId,
    bool isServicePresent,
    int serviceCategory,
    const RIL_CDMA_SMS_Address &addr,
    const RIL_CDMA_SMS_Subaddress &subAddr,
    int uBearerDataLen,
    unsigned char bearerData[]) {
    CdmaSmsMessage sms;
    setCdmaSms(sms, teleserviceId, isServicePresent, serviceCategory,
            addr, subAddr, uBearerDataLen, bearerData);
    RIL_getRadioService(slot)->sendCdmaSms(g_serial++, sms);
}


void writeSmsToRuim(
    int slot,
    int status,
    int teleserviceId,
    bool isServicePresent,
    int serviceCategory,
    const RIL_CDMA_SMS_Address &addr,
    const RIL_CDMA_SMS_Subaddress &subAddr,
    int uBearerDataLen,
    unsigned char bearerData[]) {
    CdmaSmsWriteArgs args;
    args.status = (CdmaSmsWriteArgsStatus)status;
    setCdmaSms(args.message, teleserviceId, isServicePresent, serviceCategory,
            addr, subAddr, uBearerDataLen, bearerData);
    RIL_getRadioService(slot)->writeSmsToRuim(g_serial++, args);
}


void reportSmsMemoryStatus(int slot, bool available) {
    RIL_getRadioService(slot)->reportSmsMemoryStatus(g_serial++, available);
}

void send3gppSmsOverIms(
    int slot,
    bool retry,
    int messageRef,
    const char *smscPdu,
    const char* pdu) {
    ImsSmsMessage message;
    message.tech = (android::hardware::radio::V1_0::RadioTechnologyFamily)1;
    message.retry = false;
    message.messageRef = messageRef;
    convertCharPtrToHidlString(smscPdu);
    message.gsmMessage.resize(1);
    message.gsmMessage[0].smscPdu = convertCharPtrToHidlString(smscPdu);
    message.gsmMessage[0].pdu = convertCharPtrToHidlString(pdu);
    RIL_getRadioService(slot)->sendImsSms(g_serial++, message);
}

void send3gpp2SmsOverIms(
    int slot,
    bool retry,
    int messageRef,
    int teleserviceId,
    bool isServicePresent,
    int serviceCategory,
    const RIL_CDMA_SMS_Address &addr,
    const RIL_CDMA_SMS_Subaddress &subAddr,
    int uBearerDataLen,
    unsigned char bearerData[]) {
    ImsSmsMessage message;
    message.tech = (android::hardware::radio::V1_0::RadioTechnologyFamily)2;
    message.retry = false;
    message.messageRef = messageRef;
    message.cdmaMessage.resize(1);
    setCdmaSms(message.cdmaMessage[0], teleserviceId, isServicePresent, serviceCategory,
            addr, subAddr, uBearerDataLen, bearerData);
    RIL_getRadioService(slot)->sendImsSms(g_serial++, message);
}

void setSmscAddress(int slot, const char* smsc) {
    RIL_getRadioService(slot)->setSmscAddress(g_serial++, convertCharPtrToHidlString(smsc));
}

void getSmscAddress(int slot) {
    RIL_getRadioService(slot)->getSmscAddress(g_serial++);
}

