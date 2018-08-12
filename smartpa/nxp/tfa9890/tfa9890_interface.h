#ifndef _TFA9887_HEADFILE_
#define _TFA9887_HEADFILE_

#ifdef __cplusplus
extern "C" {
#endif

int tfa9890_check_tfaopen(void);
int  tfa9890_init(void);
int  tfa9890_deinit(void);
void tfa9890_SpeakerOn(void);
void tfa9890_SpeakerOff(void);
void tfa9890_reset(void);
void tfa9890_setSamplerate(int samplerate);
void tfa9890_set_bypass_dsp_incall(int bypass);
void tfa9890_EchoReferenceConfigure(int config);


#ifdef __cplusplus
}
#endif

#endif //_TFA9887_HEADFILE_


