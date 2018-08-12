#ifndef __KMSETKEY_H__
#define __KMSETKEY_H__

#define MAX_MSG_SIZE           4096

__BEGIN_DECLS
int32_t ree_import_attest_keybox(const uint8_t *peakb, const uint32_t peakb_len, const uint32_t finish);
int32_t ree_check_attest_keybox(const uint8_t *peakb, const uint32_t peakb_len, const uint32_t finish);
__END_DECLS

#endif
