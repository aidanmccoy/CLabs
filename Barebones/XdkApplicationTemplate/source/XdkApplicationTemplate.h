#ifndef XDK110_XDKAPPLICATIONTEMPLATE_H_
#define XDK110_XDKAPPLICATIONTEMPLATE_H_

#define SEND_RATE UINT32_C(10000)
#define TIMERBLOCKTIME UINT32_C(0xffff)

static char FRSSID[50];
static char FRKEY[50];
static int FRPort;
static char FRHost[10];
static char FRNode[30] = {0};



static void initNetworkingVars(void);

static void startNetwork(void);

static retcode_t initLEDs(void);

static retcode_t HttpSendString(char *urlPtr);

static retcode_t httpClientResponseCallback(HttpSession_T *httpSession, Msg_T *msgPtr, retcode_t status);

static void registerDevice(void);

static void connectToServer(void);

static void sendData(void);

#endif /* XDK110_XDKAPPLICATIONTEMPLATE_H_ */
