
#ifndef __PERFSERV_H__
#define __PERFSERV_H__

#ifdef __cplusplus
extern "C" {
#endif


/*** STANDARD INCLUDES *******************************************************/


/*** PROJECT INCLUDES ********************************************************/


/*** MACROS ******************************************************************/
#define PS_CLUSTER_MAX  8
#define MAX_NAME_LEN    256

#define CMD_SET_DEBUG_MSG 1000 /* sync with MtkQueryCmd and perfservice.h */

/*** GLOBAL TYPES DEFINITIONS ************************************************/
enum tPowerMsg {
    POWER_MSG_MTK_HINT,
    POWER_MSG_MTK_CUS_HINT,
    POWER_MSG_NOTIFY_STATE,
    POWER_MSG_QUERY_INFO,
    POWER_MSG_SCN_REG,
    POWER_MSG_SCN_CONFIG,
    POWER_MSG_SCN_UNREG,
    POWER_MSG_SCN_ENABLE,
    POWER_MSG_SCN_DISABLE,
    POWER_MSG_SCN_DISABLE_ALL,
    POWER_MSG_SCN_RESTORE_ALL,
};

struct tPowerData {
    enum tPowerMsg msg;
    void          *pBuf;
};

struct tHintData {
    int hint;
    int data;
};

struct tAppStateData {
    char pack[MAX_NAME_LEN];
    char activity[MAX_NAME_LEN];
    int  pid;
    int  state;
};

struct tQueryInfoData {
    int cmd;
    int param;
    int value;
};

struct tScnData {
    int handle;
    int command;
    int timeout;
    int param1;
    int param2;
    int param3;
    int param4;
};

struct tCusConfig {
    int hint;
    int command;
    int param1;
    int param2;
    int param3;
    int param4;
};

/*** PRIVATE TYPES DEFINITIONS ***********************************************/


/*** GLOBAL VARIABLE DECLARATIONS (EXTERN) ***********************************/


/*** PUBLIC FUNCTION PROTOTYPES **********************************************/
long power_msg(void * pMsg, void **ppRspMsg);
int powerd_cus_init(int *pCusHintTbl, void *fnptr);

#ifdef __cplusplus
}
#endif

#endif /* End of #ifndef __PERFSERV_H__ */

