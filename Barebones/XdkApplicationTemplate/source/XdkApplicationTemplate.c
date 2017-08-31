#include "BCDS_Basics.h"
#include <stdio.h>

/* additional interface header files */
#include "FreeRTOS.h"
#include "timers.h"
#include "PAL_initialize_ih.h"
#include "PAL_socketMonitor_ih.h"
#include "BCDS_WlanConnect.h"
#include "BCDS_NetworkConfig.h"
#include "led.h"
#include "XdkBoardHandle.h"
#include <Serval_HttpClient.h>
#include <Serval_Network.h>
#include "simplelink.h"

#include "XdkApplicationTemplate.h"

static char ipAddress[PAL_IP_ADDRESS_SIZE] = { 0 };

static char encodedPath[50] = { 0 };


Ip_Address_T destAddr = UINT32_C(0);
static Callable_T sentCallable;
static int isRegistered = false;
static xTimerHandle connectTimerHandle;

static LED_handle_tp redLedHandle = (LED_handle_tp) NULL; /**< variable to store red led handle */
static LED_handle_tp yellowLedHandle = (LED_handle_tp) NULL; /**< variable to store yellow led handle */
static LED_handle_tp orangeLedHandle = (LED_handle_tp) NULL; /**< variable to store orange led handle */

static void initNetworkingVars(void) {
	FRPort = 80;
	sprintf(FRHost, "HN");					// Suffix for URLs - no need to edit (usually)
	sprintf(FRNode, "192.168.137.1");		// Enter Host Name or IP of Relay to connect to
	sprintf(FRSSID, "FactoryRelayAidan");
	//sprintf(FRSSID, "RelayRouter");			// Enter WIFI SSID
	sprintf(FRKEY, "password");
	//sprintf(FRKEY, "Factory-Relay@2016");// Enter password

}

static void startNetwork(void) {

	LED_setState(orangeLedHandle, LED_SET_ON);

	NetworkConfig_IpSettings_T myIpSettings;
	Ip_Address_T * IpaddressHex = Ip_getMyIpAddr();
	WlanConnect_SSID_T connectSSID;
	WlanConnect_PassPhrase_T connectPassPhrase;
	Retcode_T ReturnValue = (Retcode_T) RETCODE_FAILURE;
	int32_t Result = INT32_C(-1);

	printf("Starting fixed network\n\r");

	initNetworkingVars();
	PAL_initialize();
	PAL_socketMonitorInit();
	HttpClient_initialize();

	printf("Trying to connect to: %s with password: %s\r\n", FRSSID, FRKEY);

	if (RETCODE_OK != WlanConnect_Init()) {
		printf("Error in Wlan init() call...Exiting");
		exit(-1);
	}

	connectSSID = (WlanConnect_SSID_T) FRSSID;
	connectPassPhrase = (WlanConnect_PassPhrase_T) FRKEY;

	ReturnValue = NetworkConfig_SetIpDhcp(NULL);

	if (RETCODE_OK != ReturnValue) {
		printf("Error in setting IP to DHCP\n\r");
		exit(-1);
	}

	if (RETCODE_OK != WlanConnect_WPA(connectSSID, connectPassPhrase, NULL)) {
		printf("Error in WlanConnect_WPA\n\r");
		exit(-1);
	}

	if (RETCODE_OK != NetworkConfig_GetIpSettings(&myIpSettings)) {
		printf("Error in NetworkConfig\n\r");
		exit(-1);
	}

	*IpaddressHex = Basics_htonl(myIpSettings.ipV4);
	Result = Ip_convertAddrToString(IpaddressHex, ipAddress);

	if (Result < 0) {
		printf("Couldn't convert the IP address to string format\r\n");
		exit(-1);
	}

	PAL_getIpaddress((uint8_t *) FRNode, &destAddr);


	printf("Wlan connected successfully, ip address is %s\r\n", ipAddress);

	LED_setState(orangeLedHandle, LED_SET_OFF);
}

static retcode_t initLEDs(void) {
	retcode_t retVal = RC_APP_ERROR;

	redLedHandle = LED_create(gpioRedLed_Handle, GPIO_STATE_OFF);
	if (redLedHandle != NULL) {
		yellowLedHandle = LED_create(gpioYellowLed_Handle, GPIO_STATE_OFF);
	}
	if (yellowLedHandle != NULL) {
		orangeLedHandle = LED_create(gpioOrangeLed_Handle, GPIO_STATE_OFF);
	}
	if (orangeLedHandle != NULL) {
		retVal = RC_OK;
	}
	return (retVal);
}
static retcode_t callbackOnSent(Callable_T *callfunc, retcode_t status)
{
    BCDS_UNUSED(callfunc);

    printf("callBackOnSent status code is %i\r\n", status);

    if (status != RC_OK)
    {
        printf("error occurred in connecting server \r\n");
    }
    return (RC_OK);
}

static retcode_t HttpSendString(char *urlPtr) {
	printf("Creating Init Request...\r\n");

	LED_setState(yellowLedHandle, LED_SET_ON);

	Msg_T * msgPtr;
	retcode_t rc = RC_OK;

	static Callable_T SentCallable;
	Callable_T * Callable_pointer;
	Callable_pointer = Callable_assign(&SentCallable, callbackOnSent);

	//printf("destAddrs ->%s<-\r\n", destAddr);
	//printf("port is ->%i<- and after conversion to net ord ->%08x<-\r\n", FRPort, Ip_convertIntToPort(FRPort));
	//printf("msgPtr is ->%s<-\r\n", msgPtr);
	rc = HttpClient_initRequest(&destAddr, Ip_convertIntToPort(FRPort), &msgPtr);
	if (RC_OK != rc) {
		printf("HttpClient_initRequest call failed to port %i\r\n", FRPort);
		LED_setState(yellowLedHandle, LED_SET_OFF);
		return rc;
	};

	//printf("Setting Req Method...\r\n");
	HttpMsg_setReqMethod(msgPtr, Http_Method_Get);

	//printf("Setting Url...\r\n");
	if (RC_OK != HttpMsg_setReqUrl(msgPtr, urlPtr)) {
		printf("HttpMsg_setReqUrl call failed with url %s\r\n", urlPtr);
		LED_setState(yellowLedHandle, LED_SET_OFF);
		return RC_HTTP_PARSER_URI_TOO_LONG;
	};

	//printf("Sending Req...\r\n");
	if (RC_OK != HttpClient_pushRequest(msgPtr, &SentCallable,
					httpClientResponseCallback)) {
		printf("HttpClient_pushRequest call failed, message not sent\r\n");
		LED_setState(yellowLedHandle, LED_SET_OFF);
		return RC_HTTP_SEND_ERROR;
	}

	printf("HTTP Request Sent to %s with uri %s\r\n", (char *) destAddr,
			urlPtr);

	LED_setState(yellowLedHandle, LED_SET_OFF);

	return RC_OK;
}

static retcode_t httpClientResponseCallback(HttpSession_T *httpSession,
        Msg_T *msg_ptr, retcode_t status)
{
    BCDS_UNUSED(httpSession);
    retcode_t rc = status;

    if (status != RC_OK)
    {
        /* Error occurred in downloading the page */
    }
    else if (msg_ptr == NULL)
    {
        rc = RC_HTTP_PARSER_INVALID_CONTENT_TYPE;
    }
    else
    {
        if (HttpMsg_getStatusCode(msg_ptr) != Http_StatusCode_OK)
        {
            rc = RC_HTTP_INVALID_RESPONSE;
        }
        else
        {
            if (HttpMsg_getContentType(msg_ptr) != Http_ContentType_Text_Html)
            {
                rc = RC_HTTP_INVALID_RESPONSE;
            }
            else
            {
                char const *content_ptr;
                unsigned int len = UINT32_C(0);

                HttpMsg_getContent(msg_ptr, &content_ptr, &len);
                printf("GET Response Content %s length %d\r\n", content_ptr, len);
            }
        }
    }

    if (rc != RC_OK)
    {
        printf("error occurred in downloading HTML\r\n");
    }
    return (rc);
}

/*static retcode_t httpClientResponseCallback(Msg_T *msgPtr, retcode_t status) {

	LED_setState(orangeLedHandle, LED_SET_ON);

	printf("HTTP Response is: %i\r\n", status);

	if (status != RC_OK) {
		printf("Error in downloading HTML in httpClientResponseCallback\r\n");
		LED_setState(orangeLedHandle, LED_SET_OFF);

		return RC_HTTP_INVALID_RESPONSE;
	} else {
		if (HttpMsg_getContentType(msgPtr) != Http_ContentType_Text_Html) {
			return RC_HTTP_INVALID_RESPONSE;
			LED_setState(orangeLedHandle, LED_SET_OFF);

		} else {
			char const *content_ptr;
			unsigned int contentLength = 0;
			HttpMsg_getContent(msgPtr, &content_ptr, &contentLength);

			printf("GET Response Content %s length %d \r\n", content_ptr,
					contentLength);

		}

		LED_setState(orangeLedHandle, LED_SET_OFF);
	}
	return status;
}*/

static void registerDevice(void) {

	//printf("Building register string...\r\n");
			_u8 macAddressVal[SL_MAC_ADDR_LEN];
			_u8 macAddressLen = SL_MAC_ADDR_LEN;
			sl_NetCfgGet(SL_MAC_ADDRESS_GET, NULL, &macAddressLen,
					(_u8 *) macAddressVal);

			char urlPtr[255] = { 0 };
			sprintf(urlPtr,
					"/register%s.htm?726da4d2-2749-4e3f-927c-6928ec908fbc&%x-%x-%x-%x",
					FRHost, macAddressVal[0], macAddressVal[1], macAddressVal[2],
					macAddressVal[3]);


		//printf("Attemping to send...\r\n");
		printf("Http sendString returned %i\r\n", HttpSendString(urlPtr));

	//printf("Device successfully registered\r\n");
}

static void sendData(void) {
	printf(" \r\n\n");
	if(isRegistered == false){
		registerDevice();
	}
}

static void connectToServer(void) {
	uint32_t Ticks = SEND_RATE;

	if (Ticks != UINT32_MAX) {
		Ticks /= portTICK_RATE_MS;
	}
	if (UINT32_C(0) == Ticks) {
		Ticks = UINT32_C(1);
	}
	connectTimerHandle = xTimerCreate(
			(const char * const) "sendData", Ticks, 1, NULL, sendData);

	if (connectTimerHandle != NULL) {
		if (xTimerStart(connectTimerHandle, TIMERBLOCKTIME) != pdTRUE) {
			assert(false);
		}
	}
}

void appInitSystem(xTimerHandle xTimer) {
	(void) (xTimer);


	vTaskDelay(1000);
	printf("appInitSystem...\r\n");

	initLEDs();
	startNetwork();
	connectToServer();
	//registerDevice();
}
