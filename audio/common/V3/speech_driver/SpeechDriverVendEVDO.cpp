#define MTK_LOG_ENABLE 1
#include "SpeechDriverVendEVDO.h"
#include "SpeechEnhancementController.h"
#include "AudioALSAHardwareResourceManager.h"

#include <hardware_legacy/power.h>

#undef LOG_TAG
#define LOG_TAG "SpeechDriverVendEVDO"

static const char CDMA_WAKELOCK_NAME[] = "CDMA_WAKELOCK";

#define ENABLE_XLOG_SPEECH_VendEVDO
#ifdef ENABLE_XLOG_SPEECH_VendEVDO
#include <log/log.h>
#define SPEECH_DRIVER_VEB(fmt, arg...)  SLOGV(fmt, ##arg)
#define SPEECH_DRIVER_DBG(fmt, arg...)  SLOGD(fmt, ##arg)
#define SPEECH_DRIVER_INFO(fmt, arg...) SLOGI(fmt, ##arg)
#define SPEECH_DRIVER_WARN(fmt, arg...) SLOGW(fmt, ##arg)
#define SPEECH_DRIVER_ERR(fmt, arg...)  SLOGE("Err: %5d:, "fmt, __LINE__, ##arg)
#else
#include <utils/Log.h>
#define SPEECH_DRIVER_VEB  ALOGV
#define SPEECH_DRIVER_DBG  ALOGD
#define SPEECH_DRIVER_INFO ALOGI
#define SPEECH_DRIVER_WARN ALOGW
#define SPEECH_DRIVER_ERR  ALOGE
#endif

namespace android {

//Singleton Pattern
SpeechDriverVendEVDO *SpeechDriverVendEVDO::mLad = NULL;

SpeechDriverVendEVDO *SpeechDriverVendEVDO::GetInstance(modem_index_t modem_index = MODEM_1) {
    SpeechDriverVendEVDO *pLad = NULL;  //for return
    SPEECH_DRIVER_DBG("%s(), modem_index = %d", __FUNCTION__, modem_index);

    if (!mLad) {
        mLad = new SpeechDriverVendEVDO(MODEM_1); //VendEVDO speech driver only one modem
    }

    pLad = mLad;
    ASSERT(pLad != NULL);
    return pLad;
}

//Constructor
SpeechDriverVendEVDO::SpeechDriverVendEVDO() {
}

SpeechDriverVendEVDO::SpeechDriverVendEVDO(modem_index_t modem_index) {
    SPEECH_DRIVER_DBG("%s(modem_index = %d)", __FUNCTION__, modem_index);
    mModemIndex = modem_index;
    mInitCheck = false;
    pMSN = new SpeechMessengerVendEVDO();
    status_t ret = pMSN->Initial();
    if (ret == NO_ERROR) {
        mInitCheck = true;
    } else {
        SPEECH_DRIVER_ERR("Initalize SpeechMessengerVendEVDO Failure!");
    }
    mSpeechMode = EVDO_SPEECH_MODE_NO_CONNECT;
    mPhoneCallMode = RAT_2G_MODE;
    mRecordSampleRate = 8000; //8K
    mUplinkMuteOn = false; //for confirm
    mUseBtCodec = false;
    mVolumeIndex = 0x3;
}

//destructor
SpeechDriverVendEVDO::~SpeechDriverVendEVDO() {
    SPEECH_DRIVER_DBG("Destruct %s", __FUNCTION__);
    pMSN->DeInitial();
    if (pMSN) {
        delete pMSN;
        pMSN = NULL;
    }
}

bool SpeechDriverVendEVDO::initCheck() const {
    return mInitCheck;
}



/*==============================================================================
 *                     Speech Control
 *============================================================================*/

status_t SpeechDriverVendEVDO::SetSpeechMode(const audio_devices_t input_device_ori, const audio_devices_t output_device) {
    SPEECH_DRIVER_DBG("%s(input_device=0x%x,output_device=0x%x)", __FUNCTION__, input_device_ori, output_device);

    speech_mode_evdo_t speech_mode = EVDO_SPEECH_MODE_NORMAL;//cdma cp8.2

    audio_devices_t input_device = input_device_ori;
    //clear bit AUDIO_DEVICE_BIT_IN
    input_device &= ~AUDIO_DEVICE_BIT_IN;
    //set speech mode by input/output device
    if (output_device & AUDIO_DEVICE_OUT_ALL_SCO) {
        speech_mode = EVDO_SPEECH_MODE_BTSCO;
    } else if (input_device & AUDIO_DEVICE_IN_BUILTIN_MIC) {
        if (output_device & AUDIO_DEVICE_OUT_EARPIECE) {
            speech_mode = EVDO_SPEECH_MODE_NORMAL;
        } else if (output_device & (AUDIO_DEVICE_OUT_WIRED_HEADSET | AUDIO_DEVICE_OUT_WIRED_HEADPHONE)) {
            speech_mode = EVDO_SPEECH_MODE_HEADPHONE;

        } else if (output_device & AUDIO_DEVICE_OUT_SPEAKER) {
            speech_mode = EVDO_SPEECH_MODE_LOUDSPEAKER;
        }
    } else if (input_device & AUDIO_DEVICE_IN_WIRED_HEADSET) {
        if (output_device & (AUDIO_DEVICE_OUT_WIRED_HEADSET | AUDIO_DEVICE_OUT_WIRED_HEADPHONE)) {
            speech_mode = EVDO_SPEECH_MODE_HEADSET;
        }
    }

    SPEECH_DRIVER_DBG("%s,cdma speech_mode=%d", __FUNCTION__, speech_mode);
    pMSN->Spc_SetAudioMode(speech_mode);
    pMSN->Spc_MuteMicrophone(mUplinkMuteOn);
    return NO_ERROR;
}

status_t SpeechDriverVendEVDO::setMDVolumeIndex(int stream, int device, int index) {
    ALOGD("+%s() stream= %d, device = 0x%x, index =%d", __FUNCTION__, stream, device, index);
    return INVALID_OPERATION;
}

status_t SpeechDriverVendEVDO::SpeechOn() {
    SPEECH_DRIVER_DBG("%s()", __FUNCTION__);
    CheckApSideModemStatusAllOffOrDie();
    pMSN->Spc_MuteMicrophone(mUplinkMuteOn);//double confirm
    pMSN->Spc_Speech_On(mPhoneCallMode);
    pMSN->Spc_SetOutputVolume(mDownlinkGain);
    acquire_wake_lock(PARTIAL_WAKE_LOCK, CDMA_WAKELOCK_NAME);//?
    SetApSideModemStatus(SPEECH_STATUS_MASK);//speech status

    return NO_ERROR;
}

status_t SpeechDriverVendEVDO::SpeechOff() {
    SPEECH_DRIVER_DBG("%s()", __FUNCTION__);
    ResetApSideModemStatus(SPEECH_STATUS_MASK);
    CheckApSideModemStatusAllOffOrDie();

    release_wake_lock(CDMA_WAKELOCK_NAME);//?

    pMSN->Spc_MuteMicrophone(1);
    pMSN->Spc_Speech_Off();
    //Clean gain value and mute status
    mDownlinkGain = 0;
    mUplinkGain = 0;
    mSideToneGain = 0;

    return NO_ERROR;
}
/*
*for feature VT ready
*/
status_t SpeechDriverVendEVDO::VideoTelephonyOn() {
    SPEECH_DRIVER_DBG("%s()", __FUNCTION__);
    CheckApSideModemStatusAllOffOrDie();
    //  pMSN->Spc_Speech_On(RAT_3G324M_MODE);
    SetApSideModemStatus(VT_STATUS_MASK);

    return NO_ERROR;
}

status_t SpeechDriverVendEVDO::VideoTelephonyOff() {
    SPEECH_DRIVER_DBG("%s()", __FUNCTION__);
    ResetApSideModemStatus(VT_STATUS_MASK);
    CheckApSideModemStatusAllOffOrDie();

    //  pMSN->Spc_Speech_Off();
    //Clean gain value and mute status
    mDownlinkGain = 0;
    mUplinkGain = 0;
    mSideToneGain = 0;

    return NO_ERROR;
}

/*==============================================================================
 *                     Recording Control
 *============================================================================*/

status_t SpeechDriverVendEVDO::RecordOn() {
    SPEECH_DRIVER_DBG("%s()", __FUNCTION__);
    SetApSideModemStatus(RECORD_STATUS_MASK);
    pMSN->Spc_OpenNormalRecPath(mRecordSampleRate);

    return NO_ERROR;
}

status_t SpeechDriverVendEVDO::RecordOn(record_type_t type_record) {
    return RecordOn();
}

status_t SpeechDriverVendEVDO::RecordOff() {
    SPEECH_DRIVER_DBG("%s()", __FUNCTION__);
    //reset modem status and clear gain
    ResetApSideModemStatus(RECORD_STATUS_MASK);
    mUplinkGain = 0;

    pMSN->Spc_CloseNormalRecPath();

    return NO_ERROR;
}

status_t SpeechDriverVendEVDO::RecordOff(record_type_t type_record) {
    return RecordOff();
}

status_t SpeechDriverVendEVDO::SetPcmRecordType(record_type_t type_record) {
    ALOGE("%s()", __FUNCTION__);
    return INVALID_OPERATION;
}

//VM Record for VendEVDO disable
status_t SpeechDriverVendEVDO::VoiceMemoRecordOn() {
    return NO_ERROR;
}

status_t SpeechDriverVendEVDO::VoiceMemoRecordOff() {
    return NO_ERROR;
}

uint16_t SpeechDriverVendEVDO::GetRecordSampleRate() const {
    return mRecordSampleRate;
}

uint16_t SpeechDriverVendEVDO::GetRecordChannelNumber() const {
    return 1;//mono
}


/*==============================================================================
 *                     Volume Control
 *============================================================================*/

status_t SpeechDriverVendEVDO::SetDownlinkGain(int16_t gain) {
    SPEECH_DRIVER_DBG("%s(), gain = 0x%x, old mDownlinkGain = 0x%x", __FUNCTION__, gain, mDownlinkGain);
    gain += 1;  //map 0~6 to 1~7
    if (gain == mDownlinkGain) {
        return NO_ERROR;
    }
    pMSN->Spc_SetOutputVolume(gain);
    mDownlinkGain = gain;
    return NO_ERROR;
}

status_t SpeechDriverVendEVDO::SetEnh1DownlinkGain(int16_t gain) {
    SPEECH_DRIVER_DBG("%s(), gain = 0x%x, old SetEnh1DownlinkGain = 0x%x", __FUNCTION__, gain, mDownlinkenh1Gain);
    if (gain == mDownlinkenh1Gain) {
        return NO_ERROR;
    }
    mDownlinkenh1Gain = gain;
    return NO_ERROR;
}

status_t SpeechDriverVendEVDO::SetUplinkGain(int16_t gain) {
    SPEECH_DRIVER_DBG("%s(), gain = 0x%x, old mUplinkGain = 0x%x", __FUNCTION__, gain, mUplinkGain);

    if (gain == mUplinkGain) {
        return NO_ERROR;
    }
    pMSN->Spc_SetMicrophoneVolume(gain);

    mUplinkGain = gain;
    return NO_ERROR;
}

status_t SpeechDriverVendEVDO::SetDownlinkMute(bool mute_on) {
    SPEECH_DRIVER_DBG("%s(), mute_on = %d, old mDownlinkMuteOn = %d", __FUNCTION__, mute_on, mDownlinkMuteOn);
    mDownlinkMuteOn = mute_on;
    return NO_ERROR;
}

status_t SpeechDriverVendEVDO::SetUplinkMute(bool mute_on) {
    SPEECH_DRIVER_DBG("%s(), mute_on = %d, old mUplinkMuteOn = %d", __FUNCTION__, mute_on, mUplinkMuteOn);
    mUplinkMuteOn = mute_on;
    pMSN->Spc_MuteMicrophone(mUplinkMuteOn);
    return NO_ERROR;
}

status_t SpeechDriverVendEVDO::SetUplinkSourceMute(bool mute_on) {
    SPEECH_DRIVER_DBG("%s(), mute_on = %d, old mUplinkMuteOn = %d", __FUNCTION__, mute_on, mUplinkMuteOn);
    mUplinkMuteOn = mute_on;
    pMSN->Spc_MuteMicrophone(mUplinkMuteOn);
    return NO_ERROR;
}

status_t SpeechDriverVendEVDO::SetSidetoneGain(int16_t gain) {
    SPEECH_DRIVER_DBG("%s(), gain = 0x%x, old mSideToneGain = 0x%x", __FUNCTION__, gain, mSideToneGain);

    pMSN->Spc_SetSidetoneVolume(gain);
    if (gain == mSideToneGain) {
        return NO_ERROR;
    }

    mSideToneGain = gain;
    return NO_ERROR;
}


/*==============================================================================
 *                     Warning Tone
 *============================================================================*/

void SpeechDriverVendEVDO::SetWarningTone(int toneid) {
    SPEECH_DRIVER_DBG("%s(toneid=%d)", __FUNCTION__, toneid);
    pMSN->Spc_Default_Tone_Play(toneid);
}

void SpeechDriverVendEVDO::StopWarningTone() {
    SPEECH_DRIVER_DBG("%s()", __FUNCTION__);
    pMSN->Spc_Default_Tone_Stop();
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


/*==============================================================================
 *                     Background Sound
 *============================================================================*/

status_t SpeechDriverVendEVDO::BGSoundOn() {
    SPEECH_DRIVER_DBG("%s()", __FUNCTION__);
    SetApSideModemStatus(BGS_STATUS_MASK);
    return NO_ERROR;
}

status_t SpeechDriverVendEVDO::BGSoundConfig(uint8_t ul_gain, uint8_t dl_gain) {
    SPEECH_DRIVER_DBG("%s(), ul_gain = 0x%x, dl_gain = 0x%x", __FUNCTION__, ul_gain, dl_gain);
    uint16_t param_16bit = (ul_gain << 8) | dl_gain;
    return NO_ERROR;
}

status_t SpeechDriverVendEVDO::BGSoundOff() {
    SPEECH_DRIVER_DBG("%s()", __FUNCTION__);
    ResetApSideModemStatus(BGS_STATUS_MASK);
    return NO_ERROR;
}
/*==============================================================================
 *                     PCM 2 Way
 *============================================================================*/

status_t SpeechDriverVendEVDO::PCM2WayOn(const bool wideband_on) {
    mPCM2WayState = (SPC_PNW_MSG_BUFFER_SPK | SPC_PNW_MSG_BUFFER_MIC | (wideband_on << 4));
    SPEECH_DRIVER_DBG("%s(), mPCM2WayState = 0x%x", __FUNCTION__, mPCM2WayState);
    SetApSideModemStatus(P2W_STATUS_MASK);
    return NO_ERROR;
}

status_t SpeechDriverVendEVDO::PCM2WayOff() {
    mPCM2WayState = 0;
    SPEECH_DRIVER_DBG("%s(), mPCM2WayState = 0x%x", __FUNCTION__, mPCM2WayState);
    ResetApSideModemStatus(P2W_STATUS_MASK);
    return NO_ERROR;
}


/*==============================================================================
 *                     TTY-CTM Control
 *============================================================================*/
status_t SpeechDriverVendEVDO::TtyCtmOn(tty_mode_t ttyMode) {
    SPEECH_DRIVER_DBG("%s(), ttyMode = %d", __FUNCTION__, ttyMode);
    SetApSideModemStatus(TTY_STATUS_MASK);
    return NO_ERROR;
}

status_t SpeechDriverVendEVDO::TtyCtmOff() {
    SPEECH_DRIVER_DBG("%s()", __FUNCTION__);
    ResetApSideModemStatus(TTY_STATUS_MASK);
    return NO_ERROR;
}

status_t SpeechDriverVendEVDO::TtyCtmDebugOn(bool tty_debug_flag) {
    SPEECH_DRIVER_DBG("%s(tty_debug_flag=%d)", __FUNCTION__, tty_debug_flag ? 1 : 0);
    return NO_ERROR;
}
/*==============================================================================
 *                     Acoustic Loopback
 *============================================================================*/
status_t SpeechDriverVendEVDO::SetAcousticLoopback(bool loopback_on) {
    SPEECH_DRIVER_DBG("%s(), loopback_on = %d", __FUNCTION__, loopback_on);
    return NO_ERROR;
}

status_t SpeechDriverVendEVDO::SetAcousticLoopbackBtCodec(bool enable_codec) {
    mUseBtCodec = enable_codec;
    return NO_ERROR;
}

status_t SpeechDriverVendEVDO::SetAcousticLoopbackDelayFrames(int32_t delay_frames) {
    SPEECH_DRIVER_DBG("%s(), delay_frames = %d", __FUNCTION__, delay_frames);
    return NO_ERROR;
}

/*==============================================================================
 *                     Device related Config
 *============================================================================*/
status_t SpeechDriverVendEVDO::SetModemSideSamplingRate(uint16_t sample_rate) {
    SPEECH_DRIVER_DBG("%s(), sample_rate = %d", __FUNCTION__, sample_rate);
    return NO_ERROR;
}

/*==============================================================================
 *                     Speech Enhancement Control
 *============================================================================*/
status_t SpeechDriverVendEVDO::SetSpeechEnhancement(bool enhance_on) {
    SPEECH_DRIVER_DBG("%s(), enhance_on = %d", __FUNCTION__, enhance_on);
    return NO_ERROR;
}

status_t SpeechDriverVendEVDO::SetSpeechEnhancementMask(const sph_enh_mask_struct_t &mask) {
    SPEECH_DRIVER_DBG("%s(), main_func = 0x%x, dynamic_func = 0x%x", __FUNCTION__, mask.main_func, mask.dynamic_func);

    if (AudioALSAHardwareResourceManager::getInstance()->getNumPhoneMicSupport() >= 2) {
        int on = ((mask.dynamic_func & SPH_ENH_DYNAMIC_MASK_DMNR) && (mask.main_func & SPH_ENH_MAIN_MASK_DMNR)) ? 1 : 0;
        SPEECH_DRIVER_DBG("%s(), pMSN->EnableDualMic(%d );", __FUNCTION__, on);
        pMSN->EnableDualMic(on ? true : false);
    }

    return NO_ERROR;
}

status_t SpeechDriverVendEVDO::SetBtHeadsetNrecOn(const bool bt_headset_nrec_on) {
    SPEECH_DRIVER_DBG("%s(), bt_headset_nrec_on = %d", __FUNCTION__, bt_headset_nrec_on);
    return NO_ERROR;
}


/*==============================================================================
 *                     Recover State
 *============================================================================*/

void SpeechDriverVendEVDO::RecoverModemSideStatusToInitState() {

}
/**
 * check whether modem is ready.
 */
bool SpeechDriverVendEVDO::CheckModemIsReady() {
    SPEECH_DRIVER_DBG("%s()", __FUNCTION__);
    return true;
}

status_t SpeechDriverVendEVDO::SpeechRouterOn() { // should not call this
    ALOGE("%s()", __FUNCTION__);
    return INVALID_OPERATION;
}

status_t SpeechDriverVendEVDO::SpeechRouterOff() { // should not call this
    ALOGE("%s()", __FUNCTION__);
    return INVALID_OPERATION;
}


};// namespace android

