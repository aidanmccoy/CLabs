/**
* Licensee agrees that the example code provided to Licensee has been developed and released by Bosch solely as an example to be used as a potential reference for Licensee�s application development. 
* Fitness and suitability of the example code for any use within Licensee�s applications need to be verified by Licensee on its own authority by taking appropriate state of the art actions and measures (e.g. by means of quality assurance measures).
* Licensee shall be responsible for conducting the development of its applications as well as integration of parts of the example code into such applications, taking into account the state of the art of technology and any statutory regulations and provisions applicable for such applications. Compliance with the functional system requirements and testing there of (including validation of information/data security aspects and functional safety) and release shall be solely incumbent upon Licensee. 
* For the avoidance of doubt, Licensee shall be responsible and fully liable for the applications and any distribution of such applications into the market.
* 
* 
* Redistribution and use in source and binary forms, with or without 
* modification, are permitted provided that the following conditions are 
* met:
* 
*     (1) Redistributions of source code must retain the above copyright
*     notice, this list of conditions and the following disclaimer. 
* 
*     (2) Redistributions in binary form must reproduce the above copyright
*     notice, this list of conditions and the following disclaimer in
*     the documentation and/or other materials provided with the
*     distribution.  
*     
*     (3)The name of the author may not be used to
*     endorse or promote products derived from this software without
*     specific prior written permission.
* 
*  THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR 
*  IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
*  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
*  DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
*  INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
*  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
*  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
*  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
*  STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
*  IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
*  POSSIBILITY OF SUCH DAMAGE.
*/

/* header definition ******************************************************** */
#ifndef XDK110_HTTPEXAMPLECLIENT_H_
#define XDK110_HTTPEXAMPLECLIENT_H_

/* local interface declaration ********************************************** */

/* local type and macro definitions */

#define CONNECT_TIME_INTERVAL           UINT32_C(10000)          /**< Macro to represent connect time interval */
#define TIMERBLOCKTIME                  UINT32_C(0xffff)        /**< Macro used to define blocktime of a timer*/
#define TIMER_AUTORELOAD_ON             UINT32_C(1)             /**< Auto reload of timer is enabled*/
#warning Please enter WLAN configurations with valid SSID & WPA key in below Macros and remove this line to avoid warnings.
#define WLAN_CONNECT_WPA_SSID                "McCoy"         /**< Macros to define WPA/WPA2 network settings */
#define WLAN_CONNECT_WPA_PASS                "2fly4wifi"      /**< Macros to define WPA/WPA2 network settings */
#define DEST_PORT_NUMBER        		UINT16_C(80)            /**< Macro to define the unsecure Server Port number */

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

#endif /* XDK110_HTTPEXAMPLECLIENT_H_ */

/** ************************************************************************* */
