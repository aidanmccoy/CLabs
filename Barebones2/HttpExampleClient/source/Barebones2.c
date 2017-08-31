//Author: Aidan McCoy

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
#include "XdkSensorHandle.h"
#include "WDG_watchdog_ih.h"
#include "button.h"
#include "XdkSensorHandle.h"

/* own header files */
#include "Barebones2.h"

static xTimerHandle connectTimerHandle; /**< variable to store timer handle*/
Ip_Address_T destAddr = UINT32_C(0);/*< variable to store the Ip address of the server */
static char ipAddress[PAL_IP_ADDRESS_SIZE] = { 0 };
static char encodedPath[50] = { 0 };
static int isRegistered = false;
Accelerometer_XyzData_T accelData = { INT32_C(0), INT32_C(0), INT32_C(0) };

//Gyro support
//Gyroscope_XyzData_T gyroData = { INT32_C(0), INT32_C(0), INT32_C(0) };

//Magnometer support
//Magnetometer_XyzData_T magData = {INT32_C(0), INT32_C(0), INT32_C(0), INT32_C(0)};

int successCount = 0;
int failCount = 0;

int Btn1Pressed = UNPRESSED;
int Btn2Pressed = UNPRESSED;

static LED_handle_tp redLedHandle = (LED_handle_tp) NULL; /**< variable to store red led handle */
static LED_handle_tp yellowLedHandle = (LED_handle_tp) NULL; /**< variable to store yellow led handle */
static LED_handle_tp orangeLedHandle = (LED_handle_tp) NULL; /**< variable to store orange led handle */
static BUTTON_handle_tp Button1Handle = (BUTTON_handle_tp) NULL; /**< variable to store button 1 handle */
static BUTTON_handle_tp Button2Handle = (BUTTON_handle_tp) NULL; /**< variable to store button 2 handle */

static void startNetwork(void) {

	LED_setState(orangeLedHandle, LED_SET_ON);
	WDG_feedingWatchdog();

	NetworkConfig_IpSettings_T myIpSettings;
	Ip_Address_T * IpaddressHex = Ip_getMyIpAddr();
	WlanConnect_SSID_T connectSSID;
	WlanConnect_PassPhrase_T connectPassPhrase;
	Retcode_T ReturnValue = (Retcode_T) RETCODE_FAILURE;
	int32_t Result = INT32_C(-1);

	printf("Starting fixed network\n\r");

	PAL_initialize();
	PAL_socketMonitorInit();
	HttpClient_initialize();

	printf("Trying to connect to: %s with password: %s\r\n", FRSSID, FRKEY);
	WDG_feedingWatchdog();

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

	PAL_getIpaddress((uint8_t *) FRNODE, &destAddr);

	printf("Wlan connected successfully, ip address is %s\r\n", ipAddress);
	WDG_feedingWatchdog();

	LED_setState(orangeLedHandle, LED_SET_OFF);
}

static retcode_t callbackOnSent(Callable_T *callfunc, retcode_t status) {
	BCDS_UNUSED(callfunc);

	if (status != RC_OK) {
		printf("Error occurred in sending string\r\n");
	}
	return (RC_OK);
}

static retcode_t httpClientResponseCallback(HttpSession_T *httpSession,
		Msg_T *msg_ptr, retcode_t status) {
	BCDS_UNUSED(httpSession);
	retcode_t rc = status;

	if (msg_ptr == NULL) {
		rc = RC_HTTP_PARSER_INVALID_CONTENT_TYPE;
	} else {
		if (HttpMsg_getStatusCode(msg_ptr) != Http_StatusCode_OK) {
			rc = RC_HTTP_INVALID_RESPONSE;
		} else {

			char const * content_ptr;
			unsigned int len = UINT32_C(0);

			HttpMsg_getContent(msg_ptr, &content_ptr, &len);
			successCount++;

			printf("%d / %d...(Successful/Failed Transmissions)\r\n",
					successCount, failCount);
			if (len == 31) {
				isRegistered = true;
				strcpy(encodedPath, &content_ptr[9]);
				printf("Device Registered Successfully, encPath is ->%s<-\r\n",
						encodedPath);
			}
		}
	}

	if (rc != RC_OK) {
		printf("error occurred in downloading HTML \r\n");
	}
	return (rc);
}

static retcode_t HttpSendString(char *urlPtr) {

	LED_setState(yellowLedHandle, LED_SET_ON);

	Msg_T * msgPtr;
	retcode_t rc = RC_OK;

	static Callable_T SentCallable;
	Callable_T * Callable_pointer;
	Callable_pointer = Callable_assign(&SentCallable, callbackOnSent);

	rc = HttpClient_initRequest(&destAddr, Ip_convertIntToPort(FRPORT),
			&msgPtr);
	if (RC_OK != rc) {
		printf("HttpClient_initRequest call failed to port %i\r\n", FRPORT);
		failCount++;
		printf("%d / %d...(Successful/Failed Transmissions)\r\n", successCount,
				failCount);
		LED_setState(yellowLedHandle, LED_SET_OFF);
		return rc;
	};

	HttpMsg_setReqMethod(msgPtr, Http_Method_Get);

	if (RC_OK != HttpMsg_setReqUrl(msgPtr, urlPtr)) {
		printf("HttpMsg_setReqUrl call failed with url %s\r\n", urlPtr);
		LED_setState(yellowLedHandle, LED_SET_OFF);
		return RC_HTTP_PARSER_URI_TOO_LONG;
	};

	if (RC_OK
			!= HttpClient_pushRequest(msgPtr, &SentCallable,
					httpClientResponseCallback)) {
		printf("HttpClient_pushRequest call failed, message not sent\r\n");
		LED_setState(yellowLedHandle, LED_SET_OFF);
		return RC_HTTP_SEND_ERROR;
	}

	LED_setState(yellowLedHandle, LED_SET_OFF);

	return RC_OK;
}

static void registerDevice(void) {

	_u8 macAddressVal[SL_MAC_ADDR_LEN];
	_u8 macAddressLen = SL_MAC_ADDR_LEN;
	sl_NetCfgGet(SL_MAC_ADDRESS_GET, NULL, &macAddressLen,
			(_u8 *) macAddressVal);

	char urlPtr[255] = { 0 };
	sprintf(urlPtr,
			"/register%s.htm?726da4d2-2749-4e3f-927c-6928ec908fbc&%x-%x-%x-%x",
			FRHOST, macAddressVal[0], macAddressVal[1], macAddressVal[2],
			macAddressVal[3]);
	HttpSendString(urlPtr);
}

static void sendData(void) {

	LED_setState(orangeLedHandle, LED_SET_ON);
	uint16_t milliLuxData = UINT16_C(0);
	char uriPtr[255] = { 0 };
	static int msgCount = 0;
	uint8_t accelDataRec[ACCEL_RECEIVELENGTH] = { 0 };
	//uint8_t gyroDataRec[GYRO_RECEIVELENGTH] = { 0 };
	//uint8_t magDataRec[MAG_RECIEVELENGTH] = { 0 };

	/*Gyroscope_readXyzDegreeValue(xdkGyroscope_BMI160_Handle, &gyroData);

	 printf("Gyro Data :\n\rx =%ld mDeg\n\ry =%ld mDeg\n\rz =%ld"
	 "mDeg\n\r", (long int) gyroData.xAxisData,
	 (long int) gyroData.yAxisData, (long int) gyroData.zAxisData);*/

	/*Magnetometer_readXyzTeslaData(xdkMagnetometer_BMM150_Handle, &magData);

	printf("BMM150 Magnetic Data :\n\rx =%ld mT\n\ry =%ld mT\n\rz =%ld mT\n\r",
	 (long int) magData.xAxisData, (long int) magData.yAxisData, (long int)
	magData.zAxisData);*/

	Accelerometer_readXyzLsbValue(xdkAccelerometers_BMA280_Handle, &accelData);
	LightSensor_readRawData(xdkLightSensor_MAX44009_Handle, &milliLuxData);

	sprintf((char*) accelDataRec, "%ld&%ld&%ld", (long int) accelData.xAxisData,
			(long int) accelData.yAxisData, (long int) accelData.zAxisData);

	/*sprintf((char*) gyroDataRec, "%ld&%ld&%ld", (long int) accelData.xAxisData,
	 (long int) accelData.yAxisData, (long int) accelData.zAxisData);*/

	sprintf(uriPtr, "/esp%s.htm?%s&%i&%i&%i&%d", encodedPath, accelDataRec,
			Btn1Pressed, Btn2Pressed, (unsigned int) milliLuxData, msgCount);

	/*printf(uriPtr, "/esp%s.htm?%s&%i&%i&%i&%i&%i&%d", encodedPath, accelDataRec,
	 Btn1Pressed, Btn2Pressed, (unsigned int) milliLuxData, gyroDataRec, magDataRec, msgCount);
	 */

	Btn1Pressed = UNPRESSED;
	Btn2Pressed = UNPRESSED;

	msgCount++;

	HttpSendString(uriPtr);
}

static void startCommunication(void) {
	printf("\r\n\n");

	if (isRegistered == false) {
		registerDevice();
	} else {
		sendData();
	}
	WDG_feedingWatchdog();
}

void init(void) {

	uint32_t Ticks = CONNECT_TIME_INTERVAL;

	if (Ticks != UINT32_MAX) /* Validated for portMAX_DELAY to assist the task to wait Infinitely (without timing out) */
	{
		Ticks /= portTICK_RATE_MS;
	}
	if (UINT32_C(0) == Ticks) /* ticks cannot be 0 in FreeRTOS timer. So ticks is assigned to 1 */
	{
		Ticks = UINT32_C(1);
	}
	connectTimerHandle = xTimerCreate(
			(const char * const ) "startCommunication", Ticks,
			TIMER_AUTORELOAD_ON, NULL, startCommunication);

	if (connectTimerHandle != NULL) {
		if (xTimerStart(connectTimerHandle, TIMERBLOCKTIME) != pdTRUE) {
			assert(false);
		}
	}
}

static void BtnCallback(void *handle, uint32_t userParameter) {
	switch (userParameter) {
	case CALLBACK_PARAMETER_PB1:
		if (BUTTON_isPressed(handle)) {
			Btn1Pressed = PRESSED;
			printf("Button 1 pressed\r\n");
		}
		break;
	case CALLBACK_PARAMETER_PB2:
		if (BUTTON_isPressed(handle)) {
			Btn2Pressed = PRESSED;
			printf("Button 2 pressed\r\n");
		}
		break;
	default:
		break;
	}
}

static retcode_t initHardware(void) {
	retcode_t retVal = RETCODE_FAILURE;

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

	Button1Handle = BUTTON_create(gpioButton1_Handle, GPIO_STATE_OFF);
	if (Button1Handle != NULL) {
		Button2Handle = BUTTON_create(gpioButton2_Handle, GPIO_STATE_OFF);
	}
	if (Button2Handle == NULL) {
		return RETCODE_FAILURE;
	}

	BUTTON_errorTypes_t buttonReturn = BUTTON_ERROR_INVALID_PARAMETER;
	buttonReturn = BUTTON_enable(Button1Handle);
	if (buttonReturn == BUTTON_ERROR_OK) {
		buttonReturn = BUTTON_enable(Button2Handle);
	}
	if (buttonReturn != BUTTON_ERROR_OK) {
		return RETCODE_FAILURE;
	}

	buttonReturn = BUTTON_setCallback(Button1Handle, BtnCallback,
	CALLBACK_PARAMETER_PB1);
	if (buttonReturn == BUTTON_ERROR_OK) {
		buttonReturn = BUTTON_setCallback(Button2Handle, BtnCallback,
		CALLBACK_PARAMETER_PB2);
	}
	if (buttonReturn != BUTTON_ERROR_OK) {
		return RETCODE_FAILURE;
	}

	/*if (RC_OK != Gyroscope_init(xdkGyroscope_BMG160_Handle)) {
	 return RETCODE_FAILURE;
	 }

	 if (RC_OK !=  Magnetometer_init(xdkMagnetometer_BMM150_Handle)) {
	 return RETCODE_FAILURE; */

	static_assert((portTICK_RATE_MS != 0), "Tick rate MS is zero");
	vTaskDelay((portTickType) 1 / portTICK_RATE_MS);

	if (RC_OK != Accelerometer_init(xdkAccelerometers_BMA280_Handle))
		return RETCODE_FAILURE;

	if (RC_OK != LightSensor_init(xdkLightSensor_MAX44009_Handle))
		return RETCODE_FAILURE;

	return (retVal);
}

void appInitSystem(xTimerHandle xTimer) {

	initHardware();

	WDG_init(1000, WATCHDOG_TIME_INTERVAL);

	startNetwork();
	init();
}
