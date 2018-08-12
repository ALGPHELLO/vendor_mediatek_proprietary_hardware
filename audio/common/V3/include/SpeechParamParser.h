#ifndef _SPEECH_PARAM_PARSER_H_
#define _SPEECH_PARAM_PARSER_H_

/*
 * =============================================================================
 *                     external references
 * =============================================================================
 */
#include "AudioType.h"
#include "SpeechType.h"
#include <vector>
#include "AudioParamParser.h"

namespace android {

/*
 * =============================================================================
 *                     typedef
 * =============================================================================
 */

typedef struct _SPEECH_DYNAMIC_PARAM_UNIT_HDR {
    uint16_t sphParserVer;
    uint16_t numLayer ;
    uint16_t numEachLayer ;
    uint16_t paramHeader[4] ;//Network, VoiceBand, Reserved, Reserved
    uint16_t sphUnitMagiNum;

} SPEECH_DYNAMIC_PARAM_UNIT_HDR_STRUCT;

typedef struct _AUDIO_TYPE_SPEECH_LAYERINFO_STRUCT {
    char *audioTypeName;
    char numCategoryType;//4
    std::vector<String8> categoryType;
    std::vector<String8> categoryName;
    char numParam;//4
    std::vector<String8> paramName;
    char *logPrintParamUnit;
} AUDIO_TYPE_SPEECH_LAYERINFO_STRUCT;

enum speech_profile_t
{
    SPEECH_PROFILE_HANDSET = 0,
    SPEECH_PROFILE_4_POLE_HEADSET = 1,
    SPEECH_PROFILE_HANDSFREE = 2,
    SPEECH_PROFILE_BT_EARPHONE = 3,
    SPEECH_PROFILE_BT_NREC_OFF = 4,
    SPEECH_PROFILE_MAGICONFERENCE = 5,
    SPEECH_PROFILE_HAC = 6,
    SPEECH_PROFILE_LPBK_HANDSET = 7,
    SPEECH_PROFILE_LPBK_HEADSET = 8,
    SPEECH_PROFILE_LPBK_HANDSFREE = 9,
    SPEECH_PROFILE_3_POLE_HEADSET = 10,
    SPEECH_PROFILE_5_POLE_HEADSET = 11,
    SPEECH_PROFILE_5_POLE_HEADSET_ANC = 12,
    SPEECH_PROFILE_USB_HEADSET = 13,
    SPEECH_PROFILE_HANDSET_SV = 14,
    SPEECH_PROFILE_HANDSFREE_SV = 15,
    SPEECH_PROFILE_TTY_HCO_HANDSET = 16,
    SPEECH_PROFILE_TTY_HCO_HANDSFREE = 17,
    SPEECH_PROFILE_TTY_VCO_HANDSET = 18,
    SPEECH_PROFILE_TTY_VCO_HANDSFREE = 19,

    SPEECH_PROFILE_MAX_NUM = 20
};

typedef struct _SPEECH_PARAM_INFO_STRUCT {
    speech_mode_t speechMode;
    unsigned int idxVolume;
    bool isBtNrecOn;
    bool isLPBK;
    unsigned char numHeadsetPole;
    bool isSingleBandTransfer;
    unsigned char idxVoiceBandStart;
    bool isSV;
    unsigned char idxTTY;

} SPEECH_PARAM_INFO_STRUCT;

typedef struct _SPEECH_PARAM_SUPPORT_STRUCT {
    bool isNetworkSupport;
    bool isTTYSupport;
    bool isSuperVolumeSupport;

} SPEECH_PARAM_SUPPORT_STRUCT;

typedef struct _SPEECH_NETWORK_STRUCT {
    char name[128];
    uint16_t supportBit;//4

} SPEECH_NETWORK_STRUCT;

enum tty_param_t
{
    TTY_PARAM_OFF        = 0,
    TTY_PARAM_HCO        = 1,
    TTY_PARAM_VCO        = 2
};

typedef struct _SPEECH_ECHOREF_PARAM_STRUCT {
    /* speech common parameters */
    unsigned short speech_common_para[3] ;
} SPEECH_ECHOREF_PARAM_STRUCT;

/*
 * =============================================================================
 *                     class
 * =============================================================================
 */

class SpeechParamParser {
public:
    virtual ~SpeechParamParser();
    static SpeechParamParser *getInstance();
    void Init();
    bool GetSpeechParamSupport(const char *paramName);
    int GetSpeechParamUnit(char *bufParamUnit, int *paramArg);
    int GetGeneralParamUnit(char *bufParamUnit);
    int GetDmnrParamUnit(char *bufParamUnit);
    int GetMagiClarityParamUnit(char *bufParamUnit);
    int GetEchoRefParamUnit(char *bufParamUnit);
    status_t SetParamInfo(const String8 &keyParamPairs);
    int GetBtDelayTime(const char *btDeviceName);
    char *GetNameForEachSpeechNetwork(unsigned char bitIndex);
    bool GetParamStatus(const char *paramName);

protected:


private:
    SpeechParamParser();
    static SpeechParamParser *UniqueSpeechParamParser;
    AppHandle *mAppHandle;
    speech_profile_t GetSpeechProfile(const speech_mode_t sphMode, bool btHeadsetNrecOn);

    void InitAppParser();
    void Deinit();
    status_t GetSpeechParamFromAppParser(uint16_t idxSphType,
                                         AUDIO_TYPE_SPEECH_LAYERINFO_STRUCT *paramLayerInfo,
                                         char *bufParamUnit,
                                         uint16_t *sizeByteTotal);
    uint16_t sizeByteParaData(DATA_TYPE dataType, uint16_t arraySize);
    status_t SpeechDataDump(char *bufDump,
                            uint16_t idxSphType,
                            const char *nameParam,
                            const char *speechParamData);
    status_t SetMDParamUnitHdr(speech_type_dynamic_param_t idxAudioType,
                               SPEECH_DYNAMIC_PARAM_UNIT_HDR_STRUCT *paramUnitHdr,
                               uint16_t configValue);
    uint16_t SetMDParamDataHdr(SPEECH_DYNAMIC_PARAM_UNIT_HDR_STRUCT paramUnitHdr,
                               Category *cateBand,
                               Category *cateNetwork);
    int InitSpeechNetwork(void);

    SPEECH_PARAM_INFO_STRUCT mSphParamInfo;
    SPEECH_NETWORK_STRUCT mListSpeechNetwork[12];
    uint8_t numSpeechNetwork, mSpeechParamVerFirst, mSpeechParamVerLast, numSpeechParam;
    SPEECH_NETWORK_STRUCT mNameForEachSpeechNetwork[12];
    SPEECH_PARAM_SUPPORT_STRUCT mSphParamSupport;


};   //SpeechParamParser

}   //namespace android

#endif   //_SPEECH_ANC_CONTROL_H_
