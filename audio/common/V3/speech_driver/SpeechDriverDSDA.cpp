#include "SpeechDriverDSDA.h"

#include <AudioLock.h>

#include "SpeechDriverLAD.h"


#include <utils/threads.h>
#include <cutils/properties.h>

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "SpeechDriverDSDA"
#endif

namespace android {

dsda_proposal_t SpeechDriverDSDA::GetDSDAProposalType() {
    char property_value[PROPERTY_VALUE_MAX];
    property_get("persist.af.dsda_proposal_type", property_value, "0");
    int dsda_proposal_type = atoi(property_value);
    ALOGD("%s(), persist.af.dsda_proposal_type = %d", __FUNCTION__, dsda_proposal_type);

    if (dsda_proposal_type == 1) {
        ALOGD("%s(), force set DSDA_PROPOSAL_1", __FUNCTION__);
        return DSDA_PROPOSAL_1;
    } else if (dsda_proposal_type == 2) {
        ALOGD("%s(), force set DSDA_PROPOSAL_2", __FUNCTION__);
        return DSDA_PROPOSAL_2;
    } else {
#ifdef MTK_INT_MD_SPE_FOR_EXT_MD
        ALOGD("%s(), DSDA_PROPOSAL_2", __FUNCTION__);
        return DSDA_PROPOSAL_2;
#else
        ALOGD("%s(), DSDA_PROPOSAL_1", __FUNCTION__);
        return DSDA_PROPOSAL_1;
#endif
    }
}
/*==============================================================================
 *                     Constructor / Destructor / Init / Deinit
 *============================================================================*/

SpeechDriverDSDA::SpeechDriverDSDA(modem_index_t modem_index) {
    ALOGD("%s(), modem_index = %d", __FUNCTION__, modem_index);

    pSpeechDriverInternal = SpeechDriverLAD::GetInstance(MODEM_1);
    pSpeechDriverExternal = SpeechDriverLAD::GetInstance(MODEM_EXTERNAL);

    // force disable external modem speech enhancement, which will be done in internal modem
    pSpeechDriverExternal->SetForceDisableSpeechEnhancement(true);
}

SpeechDriverDSDA::~SpeechDriverDSDA() {
    ALOGD("%s()", __FUNCTION__);

    pSpeechDriverInternal = NULL;
    pSpeechDriverExternal = NULL;
}


/*==============================================================================
 *                     Singleton Pattern
 *============================================================================*/

SpeechDriverDSDA *SpeechDriverDSDA::mDSDA = NULL;

SpeechDriverInterface *SpeechDriverDSDA::GetInstance(modem_index_t modem_index) {
    static AudioLock mGetInstanceLock;
    AL_AUTOLOCK(mGetInstanceLock);

    SpeechDriverInterface *pSpeechDriver = NULL;

    const dsda_proposal_t dsda_proposal_type = GetDSDAProposalType();
    if (dsda_proposal_type == DSDA_PROPOSAL_1) { // proposal I
        pSpeechDriver = SpeechDriverLAD::GetInstance(modem_index);
    } else if (dsda_proposal_type == DSDA_PROPOSAL_2) { // proposal II
        if (mDSDA == NULL) {
            mDSDA = new SpeechDriverDSDA(modem_index);
        }

        pSpeechDriver = mDSDA;
    }

    ASSERT(pSpeechDriver != NULL);
    return pSpeechDriver;
}


/*==============================================================================
 *                     Speech Control
 *============================================================================*/

status_t SpeechDriverDSDA::SetSpeechMode(const audio_devices_t input_device, const audio_devices_t output_device) {
    ALOGD("%s()", __FUNCTION__);

    pSpeechDriverExternal->SetSpeechEnhancement(false);

    pSpeechDriverInternal->SetSpeechMode(input_device, output_device);
    pSpeechDriverExternal->SetSpeechMode(input_device, output_device);

    return NO_ERROR;
}

status_t SpeechDriverDSDA::setMDVolumeIndex(int stream, int device, int index) {
    ALOGD("+%s() stream= %d, device = 0x%x, index =%d", __FUNCTION__, stream, device, index);
    return INVALID_OPERATION;
}

status_t SpeechDriverDSDA::SpeechOn() {
    ALOGD("%s()", __FUNCTION__);

    CheckApSideModemStatusAllOffOrDie();
    SetApSideModemStatus(SPEECH_STATUS_MASK);

    pSpeechDriverInternal->SpeechRouterOn(); // TODO
    pSpeechDriverExternal->SpeechOn();

    return NO_ERROR;
}

status_t SpeechDriverDSDA::SpeechOff() {
    ALOGD("%s()", __FUNCTION__);

    ResetApSideModemStatus(SPEECH_STATUS_MASK);
    CheckApSideModemStatusAllOffOrDie();


    pSpeechDriverInternal->SpeechRouterOff(); // TODO
    pSpeechDriverExternal->SpeechOff();

    return NO_ERROR;
}

status_t SpeechDriverDSDA::VideoTelephonyOn() {
    ALOGE("%s()", __FUNCTION__);
    CheckApSideModemStatusAllOffOrDie();
    SetApSideModemStatus(VT_STATUS_MASK);
    return INVALID_OPERATION;
}

status_t SpeechDriverDSDA::VideoTelephonyOff() {
    ALOGE("%s()", __FUNCTION__);
    ResetApSideModemStatus(VT_STATUS_MASK);
    CheckApSideModemStatusAllOffOrDie();
    return INVALID_OPERATION;
}

status_t SpeechDriverDSDA::SpeechRouterOn() { // should not call this
    ALOGE("%s()", __FUNCTION__);
    CheckApSideModemStatusAllOffOrDie();
    SetApSideModemStatus(SPEECH_ROUTER_STATUS_MASK);
    return INVALID_OPERATION;
}

status_t SpeechDriverDSDA::SpeechRouterOff() { // should not call this
    ALOGE("%s()", __FUNCTION__);
    ResetApSideModemStatus(SPEECH_ROUTER_STATUS_MASK);
    CheckApSideModemStatusAllOffOrDie();
    return INVALID_OPERATION;
}

/*==============================================================================
 *                     Recording Control
 *============================================================================*/

status_t SpeechDriverDSDA::RecordOn() {
    ALOGD("%s()", __FUNCTION__);
    SetApSideModemStatus(RECORD_STATUS_MASK);
    return pSpeechDriverInternal->RecordOn();
}

status_t SpeechDriverDSDA::RecordOn(record_type_t type_record __unused) {
    ALOGD("%s()", __FUNCTION__);
    SetApSideModemStatus(RECORD_STATUS_MASK);
    return pSpeechDriverInternal->RecordOn();
}

status_t SpeechDriverDSDA::RecordOff() {
    ALOGD("%s()", __FUNCTION__);
    ResetApSideModemStatus(RECORD_STATUS_MASK);
    return pSpeechDriverInternal->RecordOff();
}

status_t SpeechDriverDSDA::RecordOff(record_type_t type_record __unused) {
    ALOGD("%s()", __FUNCTION__);
    ResetApSideModemStatus(RECORD_STATUS_MASK);
    return pSpeechDriverInternal->RecordOff();
}

status_t SpeechDriverDSDA::SetPcmRecordType(record_type_t type_record) {
    ALOGD("%s()", __FUNCTION__);
    return pSpeechDriverInternal->SetPcmRecordType(type_record);
}

status_t SpeechDriverDSDA::VoiceMemoRecordOn() {
    ALOGD("%s()", __FUNCTION__);
    SetApSideModemStatus(VM_RECORD_STATUS_MASK);
    return pSpeechDriverInternal->VoiceMemoRecordOn(); // TODO
}

status_t SpeechDriverDSDA::VoiceMemoRecordOff() {
    ALOGD("%s()", __FUNCTION__);
    ResetApSideModemStatus(VM_RECORD_STATUS_MASK);
    return pSpeechDriverInternal->VoiceMemoRecordOff(); // TODO
}

uint16_t SpeechDriverDSDA::GetRecordSampleRate() const {
    ALOGD("%s()", __FUNCTION__);
    return pSpeechDriverInternal->GetRecordSampleRate();
}

uint16_t SpeechDriverDSDA::GetRecordChannelNumber() const {
    ALOGD("%s()", __FUNCTION__);
    return pSpeechDriverInternal->GetRecordChannelNumber();
}


/*==============================================================================
 *                     Background Sound
 *============================================================================*/

status_t SpeechDriverDSDA::BGSoundOn() {
    ALOGD("%s()", __FUNCTION__);
    SetApSideModemStatus(BGS_STATUS_MASK);
    return pSpeechDriverInternal->BGSoundOn();
}

status_t SpeechDriverDSDA::BGSoundConfig(uint8_t ul_gain, uint8_t dl_gain) {
    ALOGD("%s(), ul_gain = 0x%x, dl_gain = 0x%x", __FUNCTION__, ul_gain, dl_gain);
    return pSpeechDriverInternal->BGSoundConfig(ul_gain, dl_gain);
}

status_t SpeechDriverDSDA::BGSoundOff() {
    ALOGD("%s()", __FUNCTION__);
    ResetApSideModemStatus(BGS_STATUS_MASK);
    return pSpeechDriverInternal->BGSoundOff();
}

/*==============================================================================
*                     PCM 2 Way
*============================================================================*/

status_t SpeechDriverDSDA::PCM2WayOn(const bool wideband_on __unused) {
    ALOGE("%s()", __FUNCTION__);
    SetApSideModemStatus(P2W_STATUS_MASK);
    return INVALID_OPERATION;
}

status_t SpeechDriverDSDA::PCM2WayOff() {
    ALOGE("%s()", __FUNCTION__);
    ResetApSideModemStatus(P2W_STATUS_MASK);
    return INVALID_OPERATION;
}



/*==============================================================================
 *                     TTY-CTM Control
 *============================================================================*/
status_t SpeechDriverDSDA::TtyCtmOn(tty_mode_t ttyMode) {
    ALOGE("%s(), ttyMode = %d", __FUNCTION__, ttyMode);
    SetApSideModemStatus(TTY_STATUS_MASK);
    return NO_ERROR;
}

status_t SpeechDriverDSDA::TtyCtmOff() {
    ALOGE("%s()", __FUNCTION__);
    ResetApSideModemStatus(TTY_STATUS_MASK);
    return NO_ERROR;
}

status_t SpeechDriverDSDA::TtyCtmDebugOn(bool tty_debug_flag) {
    ALOGE("%s(), tty_debug_flag = %d", __FUNCTION__, tty_debug_flag);
    return NO_ERROR;
}

/*==============================================================================
 *                     Acoustic Loopback
 *============================================================================*/

status_t SpeechDriverDSDA::SetAcousticLoopback(bool loopback_on) {
    ALOGD("%s(), loopback_on = %d", __FUNCTION__, loopback_on);

    if (loopback_on == true) {
        CheckApSideModemStatusAllOffOrDie();
        SetApSideModemStatus(LOOPBACK_STATUS_MASK);

        pSpeechDriverInternal->SpeechRouterOn();
        pSpeechDriverExternal->SetAcousticLoopback(true);
    } else {
        ResetApSideModemStatus(LOOPBACK_STATUS_MASK);
        CheckApSideModemStatusAllOffOrDie();

        pSpeechDriverInternal->SpeechRouterOff();
        pSpeechDriverExternal->SetAcousticLoopback(false);
    }

    return NO_ERROR;
}

status_t SpeechDriverDSDA::SetAcousticLoopbackBtCodec(bool enable_codec) {
    ALOGD("%s(), enable_codec = %d", __FUNCTION__, enable_codec);
    return pSpeechDriverExternal->SetAcousticLoopbackBtCodec(enable_codec);
}

status_t SpeechDriverDSDA::SetAcousticLoopbackDelayFrames(int32_t delay_frames) {
    ALOGD("%s(), delay_frames = %d", __FUNCTION__, delay_frames);
    return pSpeechDriverExternal->SetAcousticLoopbackDelayFrames(delay_frames);
}

/*==============================================================================
 *                     Volume Control
 *============================================================================*/

status_t SpeechDriverDSDA::SetDownlinkGain(int16_t gain) {
    ALOGD("%s(), gain = 0x%x, old mDownlinkGain = 0x%x", __FUNCTION__, gain, mDownlinkGain);
    return pSpeechDriverInternal->SetDownlinkGain(gain);
}

status_t SpeechDriverDSDA::SetEnh1DownlinkGain(int16_t gain) {
    ALOGD("%s(), gain = 0x%x, old SetEnh1DownlinkGain = 0x%x", __FUNCTION__, gain, mDownlinkenh1Gain);
    return pSpeechDriverInternal->SetEnh1DownlinkGain(gain);
}

status_t SpeechDriverDSDA::SetUplinkGain(int16_t gain) {
    ALOGD("%s(), gain = 0x%x, old mUplinkGain = 0x%x", __FUNCTION__, gain, mUplinkGain);
    return pSpeechDriverInternal->SetUplinkGain(gain);
}

status_t SpeechDriverDSDA::SetDownlinkMute(bool mute_on) {
    ALOGD("%s(), mute_on = %d, old mDownlinkMuteOn = %d", __FUNCTION__, mute_on, mDownlinkMuteOn);
    return pSpeechDriverInternal->SetDownlinkMute(mute_on);
}

status_t SpeechDriverDSDA::SetUplinkMute(bool mute_on) {
    ALOGD("%s(), mute_on = %d, old mUplinkMuteOn = %d", __FUNCTION__, mute_on, mUplinkMuteOn);
    return pSpeechDriverInternal->SetUplinkMute(mute_on);
}

status_t SpeechDriverDSDA::SetUplinkSourceMute(bool mute_on) {
    ALOGD("%s(), mute_on = %d", __FUNCTION__, mute_on);
    return pSpeechDriverInternal->SetUplinkSourceMute(mute_on);
}

status_t SpeechDriverDSDA::SetSidetoneGain(int16_t gain) {
    ALOGD("%s(), gain = 0x%x, old mSideToneGain = 0x%x", __FUNCTION__, gain, mSideToneGain);
    return pSpeechDriverInternal->SetSidetoneGain(gain);
}

/*==============================================================================
 *                     Device related Config
 *============================================================================*/

status_t SpeechDriverDSDA::SetModemSideSamplingRate(uint16_t sample_rate) {
    ALOGD("%s(), sample_rate = %d", __FUNCTION__, sample_rate);

    pSpeechDriverInternal->SetModemSideSamplingRate(sample_rate);
    pSpeechDriverExternal->SetModemSideSamplingRate(sample_rate);

    return NO_ERROR;
}

/*==============================================================================
 *                     Speech Enhancement Control
 *============================================================================*/
status_t SpeechDriverDSDA::SetSpeechEnhancement(bool enhance_on) {
    ALOGD("%s(), enhance_on = %d", __FUNCTION__, enhance_on);
    return pSpeechDriverInternal->SetSpeechEnhancement(enhance_on);
}

status_t SpeechDriverDSDA::SetSpeechEnhancementMask(const sph_enh_mask_struct_t &mask) {
    ALOGD("%s(), main_func = 0x%x, dynamic_func = 0x%x", __FUNCTION__, mask.main_func, mask.dynamic_func);
    return pSpeechDriverInternal->SetSpeechEnhancementMask(mask);
}

status_t SpeechDriverDSDA::SetBtHeadsetNrecOn(const bool bt_headset_nrec_on) {
    ALOGD("%s(), bt_headset_nrec_on = %d", __FUNCTION__, bt_headset_nrec_on);
    return pSpeechDriverInternal->SetBtHeadsetNrecOn(bt_headset_nrec_on);
}
/*==============================================================================
 *                     Speech Enhancement Parameters
 *============================================================================*/

status_t SpeechDriverDSDA::SetNBSpeechParameters(const void *pSphParamNB) {
    return pSpeechDriverInternal->SetNBSpeechParameters(pSphParamNB);
}

status_t SpeechDriverDSDA::SetDualMicSpeechParameters(const void *pSphParamDualMic) {
    return pSpeechDriverInternal->SetDualMicSpeechParameters(pSphParamDualMic);
}

status_t SpeechDriverDSDA::SetMagiConSpeechParameters(const void *pSphParamMagiCon) {
    return pSpeechDriverInternal->SetMagiConSpeechParameters(pSphParamMagiCon);
}

status_t SpeechDriverDSDA::SetHACSpeechParameters(const void *pSphParamHAC) {
    return pSpeechDriverInternal->SetHACSpeechParameters(pSphParamHAC);
}

status_t SpeechDriverDSDA::SetWBSpeechParameters(const void *pSphParamWB) {
    return pSpeechDriverInternal->SetWBSpeechParameters(pSphParamWB);
}

status_t SpeechDriverDSDA::GetVibSpkParam(void *eVibSpkParam) {
    return pSpeechDriverInternal->GetVibSpkParam(eVibSpkParam);
}

status_t SpeechDriverDSDA::SetVibSpkParam(void *eVibSpkParam) {
    return pSpeechDriverInternal->SetVibSpkParam(eVibSpkParam);
}


/*==============================================================================
 *                     Recover State
 *============================================================================*/

void SpeechDriverDSDA::RecoverModemSideStatusToInitState() {
    ALOGW("%s()", __FUNCTION__);
}

/*==============================================================================
 *                     Check Modem Status
 *============================================================================*/
bool SpeechDriverDSDA::CheckModemIsReady() {
    ALOGD("%s()", __FUNCTION__);
    return pSpeechDriverInternal->CheckModemIsReady() & pSpeechDriverExternal->CheckModemIsReady();
};

/*==============================================================================
 *                     Modem Audio DVT and Debug
 *============================================================================*/

status_t SpeechDriverDSDA::SetModemLoopbackPoint(uint16_t loopback_point) {
    ALOGW("%s(), loopback_point = %d", __FUNCTION__, loopback_point);
    return INVALID_OPERATION;
}

status_t SpeechDriverDSDA::SetDownlinkMuteCodec(bool mute_on) {
    ALOGW("%s(), mute_on = %d, old mDownlinkMuteOn = %d", __FUNCTION__, mute_on, mDownlinkMuteOn);
    return INVALID_OPERATION;
}


} // end of namespace android

