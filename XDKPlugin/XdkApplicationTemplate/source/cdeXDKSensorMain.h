/**
* * Copyright © 2016-2017 C-Labs Corporation and its licensors. All rights reserved.
*/

/* header definition ******************************************************** */
#ifndef XDK110_XDKAPPLICATIONTEMPLATE_H_
#define XDK110_XDKAPPLICATIONTEMPLATE_H_

/* local interface declaration ********************************************** */

/* local type and macro definitions */

/* local function prototype declarations */

/* local module global variable declarations */

/* local inline function definitions */


/* local interface declaration ********************************************** */

/* local type and macro definitions */

#define CONNECT_TIME_INTERVAL           UINT32_C(300)          /**< Macro to represent connect time interval */
#define TIMERBLOCKTIME                  UINT32_C(0xffff)        /**< Macro used to define blocktime of a timer*/
#define TIMER_AUTORELOAD_ON             UINT32_C(1)             /**< Auto reload of timer is enabled*/
#define ACCEL_RECEIVELENGTH 	      UINT8_C(30)                      /**< Receive length for BLE */

#define CDE_WATCHDOG 1
#define USE_WDG_WATCHDOG 1

#if CDE_WATCHDOG || USE_WDG_WATCHDOG
#define WATCHDOG_TIME_INTERVAL           UINT32_C(30000)          /**< Macro to represent connect time interval */
#endif

static char FRSSID[50];
static char FRKEY[50];
static int FRPort;
static char FRHost[10];
static char FRNode[30] = {0};

/* Will be used after encription #define DEST_SERVER_ADDRESS 			"www.xdk.bosch-connectivity.com" */

/* local module global variable declarations */

/* local inline function definitions */

/**
 * @brief This API connects to the HTTP server periodically and calls the httpClientResponseCallback after downloading the page
 *
 * @param[in]: xTimer
 * 				 The timer handle of the function
 *
*/
void connectServer(xTimerHandle xTimer);

/**
 * @brief This API is called after the HTTP connects with the server
 *
 * @param[in]: callfunc
 * 				 The structure storing the pointer to the message handler
 * @param[in]: retcode_t
 * 				 The return code of the HTTP connect
 * @retval: retcode_t
 * 				 The return code of the callback Function
 */
static retcode_t callbackOnSent(Callable_T *callfunc, retcode_t status);

/**
 * @brief This API is called after downloading the HTTP page from the server
 *
 * @param[in]: HttpSession_T
 * 				 The pointer holding the details of the http session
 * @param[in]: Msg_T
 * 				 The structure storing the pointer to the message handler
 * @param[in]: retcode_t
 * 				 The return code of the HTTP page download
 *
 * @retval: retcode_t
 * 				 The return code of the HTTP connect
 *
*/
static retcode_t httpClientResponseCallback(HttpSession_T *httpSession ,
        Msg_T *msg_ptr, retcode_t status);

/**
 * @brief This API is called when the HTTP page
 *      Connecting to a WLAN Access point.
 *       This function connects to the required AP (SSID_NAME).
 *       The function will return once we are connected and have acquired IP address
 *   @warning
 *      If the WLAN connection fails or we don't acquire an IP address, We will be stuck in this function forever.
 *      Check whether the callback "SimpleLinkWlanEventHandler" or "SimpleLinkNetAppEventHandler" hits once the
 *      sl_WlanConnect() API called, if not check for proper GPIO pin interrupt configuration or for any other issue
 *
 * @retval     RC_OK       IP address returned succesffuly
 *
 * @retval     RC_PLATFORM_ERROR         Error occurred in fetching the ip address
 *
*/
static retcode_t wlanConnect(void);


int mCounter;
static int But1;
static int But2;

#define ZERO_VALUE             UINT32_C(0)        /**< Macro to represent zero value */
#define CALLBACK_PARAMETER_PB1 UINT32_C(0x11)     /**< Macro represents callback parameter for Button1 */
#define CALLBACK_PARAMETER_PB2 UINT32_C(0x12)     /**< Macro represents callback parameter for Button2 */

/** enum to represent return status */
typedef enum return_e
{
    FAILURE,
    SUCCESS,
} return_t;

/* local function prototype declarations */
/**
 * @brief This will create handles for the LEDs
 *
 * @retval FAILURE Led Handle is not created
 * @retval SUCCESS Led Handle is created
 *
 */
static return_t createLed(void);

/**
 * @brief This will create the handles for button
 *
 * @retval FAILURE Button Handle is not created
 * @retval SUCCESS Button Handle is created
 */
static return_t createButton(void);

/**
 * @brief This will enable the  button
 *
 * @retval FAILURE Button Handle is not enabled
 * @retval SUCCESS Button Handle is enabled
 */
static return_t enableButton(void);

/**
 * @brief This will set the callback for the button
 *
 * @retval FAILURE Callback set failed
 * @retval SUCCESS Callback set successfully
 */
static return_t setButtonCallback(void);
/**
 * @brief     this callback will get triggered whenever any one of button is pressed
 *
 * @param[in] handle handle of the push button
 *
 * @param[in] userParameter parameter of the corresponding handle
 */
static void callback(void *handle, uint32_t userParameter);

static void StartFixedNetwork(void);
static void StartAutoNetwork(void);
#endif /* XDK110_XDKAPPLICATIONTEMPLATE_H_ */

/** ************************************************************************* */
