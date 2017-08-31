/**
 * * Copyright © 2016-2017 C-Labs Corporation and its licensors. All rights reserved.
 */

/* module includes ********************************************************** */

/* system header files */
#include "BCDS_Basics.h"

/* additional interface header files */
#include "FreeRTOS.h"
#include "timers.h"

/* additional interface header files */
#include "led.h"
#include "button.h"
#include "XdkBoardHandle.h"
#include "BCDS_Accelerometer.h"
#include "simplelink.h"

#include "PAL_initialize_ih.h"
#include "PAL_socketMonitor_ih.h"
#include "BCDS_WlanConnect.h"
#include "BCDS_NetworkConfig.h"
#include <Serval_HttpClient.h>
//#include <Serval_RestClient.h>
#include <Serval_Network.h>

#if USE_WDG_WATCHDOG
#include "WDG_watchdog_ih.h"
#endif

#include "XdkSensorHandle.h"

/* own header files */
#include "cdeXDKSensorMain.h"


static bool IsConnecting = false;
static int watchDogCount = 0;

static LED_handle_tp redLedHandle = (LED_handle_tp) NULL; /**< variable to store red led handle */
static LED_handle_tp yellowLedHandle = (LED_handle_tp) NULL; /**< variable to store yellow led handle */
static LED_handle_tp orangeLedHandle = (LED_handle_tp) NULL; /**< variable to store orange led handle */

static BUTTON_handle_tp Button1Handle = (BUTTON_handle_tp) NULL; /**< variable to store button 1 handle */
static BUTTON_handle_tp Button2Handle = (BUTTON_handle_tp) NULL; /**< variable to store button 2 handle */

static xTimerHandle connectTimerHandle; /**< variable to store timer handle*/
static xTimerHandle watchDogTimerHandle; /**< variable to store watch dog timer handle*/
Ip_Address_T destAddr = UINT32_C(0);/*< variable to store the Ip address of the server */
static char ipAddress[PAL_IP_ADDRESS_SIZE] = { 0 };
static bool isRunning = false;
static bool HasScanned = false;
static bool NoZeroConfig = false;
static int currentErrors = 0;
static Callable_T SentCallable;
static char encodedPath[50] = { 0 };
uint8_t accelDataRec[ACCEL_RECEIVELENGTH] = { 0 };
Accelerometer_XyzData_T getaccelData = { INT32_C(0), INT32_C(0), INT32_C(0) };

int watchDogLastCount = 0;
bool bFirstConnectAttempted = false;

static int autoStartUp = 1; // 0 = button only, 1 = Fixed WIFI network, 2 = Wifi Scan "FactoryRelayXYZ"

/* inline functions ********************************************************* */

static char cdeCodeArray[32] = { '0', '1', '2', '3', '4', '5', '6', '7', '8',
		'9', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'J', 'K', 'M', 'N', 'P',
		'Q', 'R', 'S', 'T', 'V', 'W', 'X', 'Y', 'Z' };

//Returns the index of the desired character from the cdeCodeArray
// by iterating through a for loop
char getCdeArrayIndex(char c) {
	char index = 0;

	for (int i = 0; i < 32; i++) {
		if (cdeCodeArray[i] == c) {
			index = (char) i;
			break;
		}
	}

	return index;
}

//Take a given string and turn it into an int with some weird decryption going on
int cdeUnscrambleToInt(char tAr[]) {
	uint calc = 0;
	for (unsigned int i = 0; i < strlen(tAr); i++) { //For each letter in the char array
		char tShift = (char) getCdeArrayIndex(tAr[i]); //Get the index of symbol in cdeCodeArray
		switch (((strlen(tAr) - 1) - i) % 4) { //Switch on length of the string, -1, - its index, mod 4... WTF?!
		case 0:				//Case on crazy switch
			tShift ^= 0x05;	//New index in cde array is old index XOR with Ox05 (0000 0101)
			break;
		case 1:
			tShift ^= 0x13;	//New index in cde array is old index XOR with 0x13 (0002 0011)
			break;
		case 2:
			tShift ^= 0x0C;		//old XOR 0x0C (0000 1100)
			break;
		case 3:
			tShift ^= 0x16;		//old XOR 0x16 (0001 0110)
			break;
		}

		calc <<= 5;				//Calc left shifted 5
		calc += tShift;		//Add the new index number to it and restart loop
	}
	return calc;
}

//Some crazy decryption stuff, don't really care what it does
void cdeUnscramblePassword(char enb[], char *Pswd) {
	int enLen = strlen(enb);
	for (int i = 0; i < 15; i++) {
		//char t[10] = { 0 };
		switch (i % 4) {
		case 0:
			Pswd[i] = ((((enb[i % enLen]) ^ 0xCF) ^ i) % 89) + 33;
			break;
		case 1:
			Pswd[i] = ((((enb[i % enLen]) ^ 0x9B) ^ i) % 89) + 33;
			break;
		case 2:
			Pswd[i] = ((((enb[i % enLen]) ^ 0x55) ^ i) % 89) + 33;
			break;
		case 3:
			Pswd[i] = ((((enb[i % enLen]) ^ 0x0C) ^ i) % 89) + 33;
			break;
		}
		//if (Pswd[i]=='<')
		//	Pswd[i]='$';
		//else if (Pswd[i]=='>')
		//		Pswd[i]='#';
	}
	Pswd[15] = 0;
}

//Calls set led state fon
static void LED_setStateCM(LED_handle_tp handle, const LED_operations_t state) {
	LED_setState(handle, state);
}

static retcode_t httpClientResponseCallback(HttpSession_T *httpSession,
		Msg_T *msg_ptr, retcode_t status) {
	BCDS_UNUSED(httpSession);
	retcode_t rc = status;

	printf("HTTP Response:...%x\r\n ", status);

	if (status != RC_OK) {
		/* Error occurred in downloading the page */
	} else if (msg_ptr == NULL) {
		rc = RC_HTTP_PARSER_INVALID_CONTENT_TYPE;
		printf("No Msg Pointer %d \r\n", status);
	} else {
		int tstat = HttpMsg_getStatusCode(msg_ptr);
		//printf("GET Response Status %d \r\n", tstat);
		if (tstat != Http_StatusCode_OK) {
			rc = tstat; // RC_HTTP_INVALID_RESPONSE;
		} else {
			if (HttpMsg_getContentType(msg_ptr) != Http_ContentType_Text_Html) {
				rc = RC_HTTP_INVALID_RESPONSE;
			} else {
				char const *content_ptr;
				unsigned int contentLength = 0;
				HttpMsg_getContent(msg_ptr, &content_ptr, &contentLength);
				printf("Content Length: %i\r\n", contentLength);
				//char content[contentLength + 1];
				//strncpy(content, content_ptr, contentLength);
				//content[contentLength] = 0;

				//printf("GET Response Content %s length %d \r\n", content_ptr, len);

				if (contentLength > 2) {
					bool tStartNow = false;
					int cnt = 0;
					for (unsigned int i = 0;
							i < contentLength && i < sizeof(encodedPath) - 1;
							i++) {
						if (content_ptr[i] == ':')
							tStartNow = true;
						else {
							if (tStartNow == true) {
								encodedPath[cnt++] = content_ptr[i];
							}
						}
					}
					if (tStartNow == true) {
						encodedPath[cnt] = 0;
						printf("Publish URL Found %s\r\n", encodedPath);
					}
				}
				currentErrors = 0;
				//printf("HTTP Success - reset error count. Code: %x\r\n",rc);
			}
		}
	}

	if (rc != RC_OK) {
		currentErrors++;
		printf("error (%i) occurred in downloading HTML %i\r\n", currentErrors,
				(int) rc);
	}
	LED_setStateCM(yellowLedHandle, LED_SET_OFF);
	IsConnecting = false;
	printf("Leaving http response callback\r\n");
	return (rc);
}

static retcode_t callbackOnSent(Callable_T *callfunc, retcode_t status) {
	BCDS_UNUSED(callfunc);
	printf("HTTP Sent...%x\r\n ", status);

	if (status != RC_OK) {
		currentErrors++;
		printf("error %i occurred in connecting server \r\n", currentErrors);
		IsConnecting = false;
	}
	LED_setStateCM(orangeLedHandle, LED_SET_OFF);
	LED_setStateCM(yellowLedHandle, LED_SET_ON);
	return (RC_OK);
}
/* local functions ********************************************************** */
static void HttpSendString(char *pUrl_ptr) {
	Msg_T* msg_ptr;
	retcode_t rc = RC_OK;

	LED_setStateCM(orangeLedHandle, LED_SET_ON);

	rc = HttpClient_initRequest(&destAddr, Ip_convertIntToPort(FRPort),
			&msg_ptr);
	if (rc == RC_HTTP_CLIENT_PENDING_CONNECTION) {
		printf("Failed HttpClient_initRequest - still pending to port %i \r\n ",
				FRPort);
		IsConnecting = false;
		return;
	}

	if (rc != RC_OK || msg_ptr == NULL) {
		printf("Failed HttpClient_initRequest %x to port %i \r\n ", rc, FRPort);
		IsConnecting = false;
		return;
	}

	HttpMsg_setReqMethod(msg_ptr, Http_Method_Get);
	rc = HttpMsg_setReqUrl(msg_ptr, pUrl_ptr);
	if (rc != RC_OK) {
		printf("Failed to fill message \r\n ");
		IsConnecting = false;
		return;
	}

	rc = HttpClient_pushRequest(msg_ptr, &SentCallable,
			httpClientResponseCallback);
	printf("HTTP Sending...%d\r\n ", rc);
	if (rc != RC_OK) {
		printf("Failed HttpClient_pushRequest \r\n  ");
		IsConnecting = false;
		return;
	}
	printf("Sending string: %s\r\n", pUrl_ptr);
}

static char url_ptr2[255] = { 0 };
static int synccounter = 0;

void connectServer(xTimerHandle xTimer) {
	BCDS_UNUSED(xTimer);

	//printf("ConnectServer enter...");
	synccounter++;

	if (IsConnecting) {
		//printf(" connectServer left 1\n\r");
		return;
	}
	IsConnecting = true;

	if (currentErrors > 5)
		encodedPath[0] = 0;

	if (encodedPath[0] == 0) {
		if (synccounter < 10) {
			IsConnecting = false;
			//printf(" connectServer left 2\n\r");
			return;
		}
		_u8 macAddressVal[SL_MAC_ADDR_LEN];
		_u8 macAddressLen = SL_MAC_ADDR_LEN;
		sl_NetCfgGet(SL_MAC_ADDRESS_GET, NULL, &macAddressLen,
				(_u8 *) macAddressVal);

		char url_ptr4[255] = { 0 };
		sprintf(url_ptr4,
				"/register%s.htm?726da4d2-2749-4e3f-927c-6928ec908fbc&%x-%x-%x-%x",
				FRHost, macAddressVal[0], macAddressVal[1], macAddressVal[2],
				macAddressVal[3]);
		//sprintf(url_ptr4, "/register%s.htm?726da4d2-2749-4e3f-927c-6928ec908fbc&%x-%x-%x-%x",FRHost, macAddressVal[0],macAddressVal[1],macAddressVal[2],macAddressVal[3]);
		printf("Send Register to %s\n\r", url_ptr4);
//#ifdef USE_WDG_WATCHDOG
//		WDG_feedingWatchdog();
//#endif
//		watchDogCount++;
		HttpSendString(url_ptr4);
		synccounter = 0;
	} else {
		if (synccounter < 3) {
			IsConnecting = false;
			//printf(" connectServer left 3\n\r");
			return;
		}
		//SensorDeviceEnvironment_Update(CURRENT,false);
		//printf("Environment :%f\n\r",TemperatureSensorDeviceData.data);
		//printf("ConnectServer...\r\n ");

		Retcode_T advancedApiRetValue = (Retcode_T) RETCODE_FAILURE;
		advancedApiRetValue = Accelerometer_readXyzLsbValue(
				xdkAccelerometers_BMA280_Handle, &getaccelData);
		if ( RETCODE_OK == advancedApiRetValue) {
			sprintf((char*) accelDataRec, "%ld&%ld&%ld",
					(long int) getaccelData.xAxisData,
					(long int) getaccelData.yAxisData,
					(long int) getaccelData.zAxisData);
			//sprintf((char*) accelDataRec, "%ld&%ld&%ld", (long int)mCounter,1,2);
			//printf("BMA280 Accel Raw Data :%ld,%ld,%ld\n\r",(long int) getaccelData.xAxisData, (long int) getaccelData.yAxisData, (long int) getaccelData.zAxisData);

			/* Read and print light sensor data */
			uint16_t milliLuxData = UINT16_C(0);
			advancedApiRetValue = LightSensor_readRawData(
					xdkLightSensor_MAX44009_Handle, &milliLuxData);
			printf("Light data read is %i\r\n", milliLuxData);
			if (RETCODE_OK == advancedApiRetValue) {
				//printf("Light sensor data obtained in milli lux :%d \n\r",(unsigned int) milliLuxData);

				mCounter++;
				sprintf(url_ptr2, "/esp%s.htm?%s&%i&%i&%i%d", encodedPath,
						accelDataRec, But1, But2, mCounter,
						(unsigned int) milliLuxData);
				//sprintf(url_ptr2, "/esp%s.htm?%s&%i&%i&%i%d",encodedPath,accelDataRec,But1,But2,mCounter,(unsigned int)milliLuxData);
				//printf("\r\nUrl: ");
				//printf(url_ptr2);
				//printf("\r\n");
				HttpSendString(url_ptr2);
				synccounter = 0;
			}
			//sprintf(url_ptr2, "/esp%s",encodedPath);
			//char quer[255] = { 0 };
			//sprintf(quer, "%s&%i&%i&%i",accelDataRec,But1,But2,mCounter);
			//printf("GETtin to %s ? %s\n\r",url_ptr2, quer);
			//RESTSendString(url_ptr2,quer);
			//printf("REST Sent...\r\n ");
		} else {
			printf("BMA280 XYZ Data read FAILED\n\r");
			IsConnecting = false;
		}
		IsConnecting = false;
#ifdef USE_WDG_WATCHDOG
		WDG_feedingWatchdog();
#endif
		watchDogCount++;
	}
	//printf(" connectServer left 4. IsConnecting %i\n\r", IsConnecting);
}

void StartNetwork(void) {
	if (isRunning)
		return;
	retcode_t rc = RC_OK;
	LED_errorTypes_t returnValue = LED_ERROR_INVALID_PARAMETER;

	LED_setStateCM(yellowLedHandle, LED_SET_ON);
#ifdef USE_WDG_WATCHDOG
	WDG_feedingWatchdog();
#endif
	watchDogCount++;

	printf("Trying to connect... %i \r\n", returnValue);
	rc = wlanConnect();
	if (RC_OK != rc) {
		printf("Networky init/connection failed %i \r\n", rc);
		LED_setStateCM(redLedHandle, LED_SET_ON);
		return;
	}

#ifdef USE_WDG_WATCHDOG
	WDG_feedingWatchdog();
#endif
	watchDogCount++;

	rc = PAL_initialize();
	if (RC_OK != rc) {
		printf("PAL and network initialize %i \r\n", rc);
		LED_setStateCM(redLedHandle, LED_SET_ON);
		return;
	}

	PAL_socketMonitorInit();
#ifdef USE_WDG_WATCHDOG
	WDG_feedingWatchdog();
#endif
	watchDogCount++;

	/* start client */
	rc = HttpClient_initialize();
	if (rc != RC_OK) {
		printf("Failed to initialize http client\r\n ");
		LED_setStateCM(redLedHandle, LED_SET_ON);
		return;
	}
#ifdef USE_WDG_WATCHDOG
	WDG_feedingWatchdog();
#endif
	watchDogCount++;

	//rc=RestClient_initialize();
	if (rc != RC_OK) {
		printf("Failed to initialize REST client\r\n ");
		LED_setStateCM(redLedHandle, LED_SET_ON);
		return;
	}
	// if (RC_OK != PAL_getIpaddress((uint8_t*) "192.168.115.1", &destAddr))
	// {
	//     LED_setStateCM(redLedHandle, LED_SET_ON);
	//     return;
	// }
	//  else
	// {
	Callable_T * Callable_pointer;
	Callable_pointer = Callable_assign(&SentCallable, callbackOnSent);
	if (Callable_pointer == NULL) {
		printf("Failed Callable_assign\r\n ");
		LED_setStateCM(redLedHandle, LED_SET_ON);
		return;
	}

	printf(" Connecting to IP %s \r\n ", ipAddress);
	LED_setStateCM(yellowLedHandle, LED_SET_OFF);
	uint32_t Ticks = CONNECT_TIME_INTERVAL;

	if (Ticks != UINT32_MAX) /* Validated for portMAX_DELAY to assist the task to wait Infinitely (without timing out) */
	{
		Ticks /= portTICK_RATE_MS;
	}
	if (UINT32_C(0) == Ticks) /* ticks cannot be 0 in FreeRTOS timer. So ticks is assigned to 1 */
	{
		Ticks = UINT32_C(1);
	}
	connectTimerHandle = xTimerCreate((const char * const ) "connectServer",
			Ticks,
			TIMER_AUTORELOAD_ON, NULL, connectServer);

	if (connectTimerHandle != NULL) {
		if (xTimerStart(connectTimerHandle, TIMERBLOCKTIME) != pdTRUE) {
			assert(false);
		}

	}
#ifdef USE_WDG_WATCHDOG
	WDG_feedingWatchdog();
#endif
	watchDogCount++;

	isRunning = true;
	//}
}

// Callback function
void myDhcpIpCallbackFunc(NetworkConfig_IpStatus_T returnStatus) {
	printf("DHCP Set status %d \r\n ", returnStatus); // WLAN_CONNECT_WPA_SSID);
}

static retcode_t wlanConnect(void) {
	NetworkConfig_IpSettings_T myIpSettings;

	Ip_Address_T* IpaddressHex = Ip_getMyIpAddr();
	WlanConnect_SSID_T connectSSID;
	WlanConnect_PassPhrase_T connectPassPhrase;
	Retcode_T ReturnValue = (Retcode_T) RETCODE_FAILURE;
	int32_t Result = INT32_C(-1);
	NetworkConfig_IpCallback_T myIpCallback;


	//sprintf(FRSSID, "FactoryRelayAidan");
	sprintf(FRSSID, "RelayRouter435");
	sprintf(FRKEY,"jq*\gs/Yan4Sfm6");
	//sprintf(FRKEY, "password");
	//sprintf(FRKEY, "Factory-Relay@2016");

	if (RETCODE_OK != WlanConnect_Init()) {
		return (RC_PLATFORM_ERROR);
	}
	printf("Connecting to %s \r\n ", FRSSID); // WLAN_CONNECT_WPA_SSID);

	connectSSID = (WlanConnect_SSID_T) FRSSID; // WLAN_CONNECT_WPA_SSID;
	connectPassPhrase = (WlanConnect_PassPhrase_T) FRKEY; // WLAN_CONNECT_WPA_PASS;
	if (NoZeroConfig) {
		NetworkConfig_IpSettings_T myIpSet;
		// IP settings that will be used.
		myIpSet.isDHCP = (uint8_t) NETWORKCONFIG_DHCP_DISABLED;
		myIpSet.ipV4 = NetworkConfig_Ipv4Value(192, 168, 248, 100);
		myIpSet.ipV4DnsServer = NetworkConfig_Ipv4Value(192, 168, 248, 254);
		myIpSet.ipV4Gateway = NetworkConfig_Ipv4Value(192, 168, 248, 254);
		myIpSet.ipV4Mask = NetworkConfig_Ipv4Value(255, 255, 255, 0);

		// Set the static IP
		ReturnValue = NetworkConfig_SetIpStatic(myIpSet);
		if (RETCODE_OK != ReturnValue) {
			printf("Error in setting STATIC IP %x\n\r",
					(unsigned int) ReturnValue);
			return (RC_PLATFORM_ERROR);
		} else {
			printf("Static IP Set\n\r");
		}
	} else {
		myIpCallback = myDhcpIpCallbackFunc;
		ReturnValue = NetworkConfig_SetIpDhcp(myIpCallback);
		if (RETCODE_OK != ReturnValue) {
			printf("Error in setting IP to DHCP\n\r");
			return (RC_PLATFORM_ERROR);
		}
	}
	if (RETCODE_OK == WlanConnect_WPA(connectSSID, connectPassPhrase, NULL)) {
		ReturnValue = NetworkConfig_GetIpSettings(&myIpSettings);
		if (RETCODE_OK == ReturnValue) {
			*IpaddressHex = Basics_htonl(myIpSettings.ipV4);
			Result = Ip_convertAddrToString(IpaddressHex, ipAddress);
			if (Result < 0) {
				printf(
						"Couldn't convert the IP address to string format \r\n ");
				return (RC_PLATFORM_ERROR);
			}

			printf("Connected to WPA network successfully \r\n ");
			printf(" Ip address of the device %s \r\n ", ipAddress);

			*IpaddressHex = Basics_htonl(myIpSettings.ipV4Gateway);
			Result = Ip_convertAddrToString(IpaddressHex, ipAddress);
			if (Result < 0) {
				printf(
						"Couldn't convert the GateWay-IP address to string format \r\n ");
				return (RC_PLATFORM_ERROR);
			}
			printf(" Gateway Ip address of the device %s \r\n ", ipAddress);
			if (FRNode[0] != 0) {
				sprintf(ipAddress, FRNode);
				printf(" FR Node used %s \r\n ", ipAddress);
			}
			if (RC_OK != PAL_getIpaddress((uint8_t*) ipAddress, &destAddr)) {
			}

			return (RC_OK);
		} else {
			printf("Error in getting IP settings\n\r");
			return (RC_PLATFORM_ERROR);
		}
	} else {
		return (RC_PLATFORM_ERROR);
	}

}

void ScanWIFI(void) {
	if (RETCODE_OK != WlanConnect_Init()) {
		return;
	}
	WlanConnect_ScanInterval_T scanInterval;
	WlanConnect_ScanList_T scanList;

	LED_setStateCM(yellowLedHandle, LED_SET_ON);
	// Set scan interval
	scanInterval = 5;
	printf("Scanning for Wifi\n\r");

	// Fill out the scan list by calling the scan network function
	WlanConnect_ScanNetworks(scanInterval, &scanList);

	// Set the number of entries
	int nbEntries = scanList.NumOfScanEntries;

	// Print all the non-empty SSIDs
	for (int i = 0; i < nbEntries; i++) {
		if (0 != scanList.ScanData[i].SsidLength) {
			printf(" - found SSID number %d is : %s\n\r", i,
					scanList.ScanData[i].Ssid);

			sprintf(FRSSID, scanList.ScanData[i].Ssid);

			if (strlen(FRSSID) > 12) {
				char fr[13];
				sprintf(fr, "%s", FRSSID);
				fr[12] = 0;
				if (strcmp(fr, "FactoryRelay") == 0) {
					sprintf(FRHost, "%s", &FRSSID[12]);
					FRPort = cdeUnscrambleToInt(FRHost);
					cdeUnscramblePassword(FRHost, FRKEY);
					printf(
							"Found AccessPoint at %s with Keys:%s and FR-Port is now %i\n\r",
							FRSSID, FRKEY, FRPort);
					HasScanned = true;
					break;
				}
			}
			/*if (strcmp(FRSSID, "McCoy") == 0) {
			 printf("Found network McCoy, ready to connect");
			 }*/
			// Delay each printf with 0,5 seconds
			vTaskDelay(500);
		}
	}
	LED_setStateCM(yellowLedHandle, LED_SET_OFF);
}

return_t ledAndButtonInit(void) {
	return_t returnValue = FAILURE;

	returnValue = createLed();
	if (returnValue == SUCCESS) {
		returnValue = createButton();
	} else {
		printf("Error in creating LED\n\r");
	}
	if (returnValue == SUCCESS) {
		returnValue = enableButton();
	} else {
		printf("Error in creating button\n\r");

	}
	if (returnValue == SUCCESS) {
		returnValue = setButtonCallback();
	} else {
		printf("Error in enabling button\n\r");
	}
	return (returnValue);
}

static void StartFixedNetwork() {
	printf("Starting fixed network\n\r");
	FRPort = 80; //FRPort=8701;
	sprintf(FRHost, "HN"); //sprintf(FRHost,"4WR");        // Suffix for URLs - no need to edit (usually)
	sprintf(FRNode, "192.168.137.1"); // Enter Host Name or IP of Relay to connect to
	sprintf(FRSSID, "FactoryRelay435"); // Enter WIFI SSID
	sprintf(FRKEY, "jq*\gs/Yan4Sfm6");  // Enter password

	HasScanned = true;
	NoZeroConfig = false;
	StartNetwork();
}

static void StartAutoNetwork() {
	printf("Starting auto provisioned network\n\r");
	if (!HasScanned)
		ScanWIFI();
	StartNetwork();
}

static void StartNetworkOnTimer() {
	switch (autoStartUp) {
	case 1:
		StartFixedNetwork();
		break;
	case 2:
		StartAutoNetwork();
		break;
	default:
		break;
	}
}

static void callback(void *handle, uint32_t userParameter) {
	switch (userParameter) {
	/*  Button 1 press/release */
	case CALLBACK_PARAMETER_PB1:
		if (BUTTON_isPressed(handle)) {
			But1 = 1;
			if (!isRunning) {
				printf("PB1 pressed - Provisioning for AXOOM Booth \n\r");
				StartFixedNetwork();
			}
		}
		if (BUTTON_isReleased(handle)) {
			But1 = 0;
			printf("PB1 released\n\r");
		}
		break;

		/* Button 2 press/release */
	case CALLBACK_PARAMETER_PB2:
		if (BUTTON_isPressed(handle)) {
			But2 = 1;
			if (!isRunning) {
				StartAutoNetwork();
			}
		}
		if (BUTTON_isReleased(handle)) {
			But2 = 0;
			printf("PB2 released\n\r");
		}
		break;
	default:
		printf("Button not available \n\r");
		break;
	}
}

/**
 * @brief This will create handles for the LEDs
 *
 * @retval FAILURE Led Handle is not created
 * @retval SUCCESS Led Handle is created
 *
 */
static return_t createLed(void) {
	return_t returnValue = FAILURE;

	redLedHandle = LED_create(gpioRedLed_Handle, GPIO_STATE_OFF);
	if (redLedHandle != NULL) {
		yellowLedHandle = LED_create(gpioYellowLed_Handle, GPIO_STATE_OFF);
	}
	if (yellowLedHandle != NULL) {
		orangeLedHandle = LED_create(gpioOrangeLed_Handle, GPIO_STATE_OFF);
	}
	if (orangeLedHandle != NULL) {
		returnValue = SUCCESS;
	}
	return (returnValue);
}

/**
 * @brief This will create the handles for button
 *
 * @retval FAILURE Button Handle is not created
 * @retval SUCCESS Button Handle is created
 */
static return_t createButton(void) {
	return_t returnValue = FAILURE;

	Button1Handle = BUTTON_create(gpioButton1_Handle, GPIO_STATE_OFF);
	if (Button1Handle != NULL) {
		Button2Handle = BUTTON_create(gpioButton2_Handle, GPIO_STATE_OFF);
	}
	if (Button2Handle != NULL) {
		returnValue = SUCCESS;
	}
	return (returnValue);
}

/**
 * @brief This will enable the  button
 *
 * @retval FAILURE Button Handle is not enabled
 * @retval SUCCESS Button Handle is enabled
 */
static return_t enableButton(void) {
	return_t returnValue = FAILURE;
	BUTTON_errorTypes_t buttonReturn = BUTTON_ERROR_INVALID_PARAMETER;
	buttonReturn = BUTTON_enable(Button1Handle);
	if (buttonReturn == BUTTON_ERROR_OK) {
		buttonReturn = BUTTON_enable(Button2Handle);
	}
	if (buttonReturn == BUTTON_ERROR_OK) {
		returnValue = SUCCESS;
	}
	return (returnValue);
}

/**
 * @brief This will set the callback for the button
 *
 * @retval FAILURE Callback set failed
 * @retval SUCCESS Callback set successfully
 */
static return_t setButtonCallback(void) {
	return_t returnValue = FAILURE;
	BUTTON_errorTypes_t buttonReturn = BUTTON_ERROR_INVALID_PARAMETER;

	buttonReturn = BUTTON_setCallback(Button1Handle, callback,
	CALLBACK_PARAMETER_PB1);
	if (buttonReturn == BUTTON_ERROR_OK) {
		buttonReturn = BUTTON_setCallback(Button2Handle, callback,
		CALLBACK_PARAMETER_PB2);
	}
	if (buttonReturn == BUTTON_ERROR_OK) {
		returnValue = SUCCESS;
	}
	return (returnValue);
}


void watchDogTimer(xTimerHandle xTimer) {
	BCDS_UNUSED(xTimer);
	if (watchDogCount > watchDogLastCount) {
		watchDogLastCount = watchDogCount;
	} else {
		if (!bFirstConnectAttempted) {
			printf("WatchDog: attempt fist connect\r\n");
			watchDogLastCount = watchDogCount;
			bFirstConnectAttempted = true;
			StartNetworkOnTimer();
		} else {
			printf("WatchDog failure: restarting\r\n");
			NVIC_SystemReset();
		}
	}
}

/* The description is in the interface header file. */
void init(void) {
	return_t returnValueinit = ledAndButtonInit();
	if (returnValueinit != SUCCESS) {
		printf("Error in led and button initialization\n\r");
	}

	printf("Getting Accel Ready\n\r");

	/* avoid synchronize problem to get proper BMA280 chipID this delay is mandatory */
	static_assert((portTICK_RATE_MS != 0), "Tick rate MS is zero");
	vTaskDelay((portTickType) 1 / portTICK_RATE_MS);

	/*initialize accel*/
	Retcode_T advancedApiRetValue = (Retcode_T) RETCODE_FAILURE;
	advancedApiRetValue = Accelerometer_init(xdkAccelerometers_BMA280_Handle);

	// SensorDeviceEnvironment_Activate(true,true,true);

	if ( RETCODE_OK == advancedApiRetValue) {
		printf("Accelerometer initialization SUCCESS\n\r");
	} else {
		printf("Accelerometer initialization FAILED\n\r");
	}

	returnValueinit = LightSensor_init(xdkLightSensor_MAX44009_Handle);
	if ( RETCODE_OK != returnValueinit) {
		printf("Light Sensor initialization Failed\n\r");
	}

	printf("System Ready\n\r");
}
void appInitSystem(xTimerHandle xTimer) {
	(void) (xTimer);

	uint32_t Ticks = WATCHDOG_TIME_INTERVAL;

	if (Ticks != UINT32_MAX) /* Validated for portMAX_DELAY to assist the task to wait Infinitely (without timing out) */
	{
		Ticks /= portTICK_RATE_MS;
	}
	if (UINT32_C(0) == Ticks) /* ticks cannot be 0 in FreeRTOS timer. So ticks is assigned to 1 */
	{
		Ticks = UINT32_C(1);
	}

#ifdef CDE_WATCHDOG
	watchDogTimerHandle = xTimerCreate(
			(const char * const ) "watchDogServer", Ticks,
			TIMER_AUTORELOAD_ON, NULL, watchDogTimer);

	if (watchDogTimerHandle != NULL)
	{
		if (xTimerStart(watchDogTimerHandle, TIMERBLOCKTIME) != pdTRUE)
		{
			assert(false);
		}

	}
#endif
	init();

#ifdef USE_WDG_WATCHDOG
	WDG_init(1000, WATCHDOG_TIME_INTERVAL);
#endif
	StartNetworkOnTimer();
}

/** ************************************************************************* */
