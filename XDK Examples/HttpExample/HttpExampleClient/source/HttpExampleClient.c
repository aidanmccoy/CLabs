/**
* Licensee agrees that the example code provided to Licensee has been developed and released by Bosch solely as an example to be used as a potential reference for Licensee’s application development. 
* Fitness and suitability of the example code for any use within Licensee’s applications need to be verified by Licensee on its own authority by taking appropriate state of the art actions and measures (e.g. by means of quality assurance measures).
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

/* module includes ********************************************************** */
//lint -esym(956,*) /* Suppressing "Non const, non volatile static or external variable" lint warning*/
/* system header files */
#include <stdio.h>
#include "BCDS_Basics.h"

/* additional interface header files */
#include <FreeRTOS.h>
#include <timers.h>

#include "PAL_initialize_ih.h"
#include "PAL_socketMonitor_ih.h"
#include "BCDS_WlanConnect.h"
#include "BCDS_NetworkConfig.h"
#include <Serval_HttpClient.h>
#include <Serval_Network.h>

/* own header files */
#include "HttpExampleClient.h"

/* constant definitions ***************************************************** */

/* local variables ********************************************************** */

/* global variables ********************************************************* */
static xTimerHandle connectTimerHandle; /**< variable to store timer handle*/
Ip_Address_T destAddr = UINT32_C(0);/*< variable to store the Ip address of the server */

/* inline functions ********************************************************* */

/* local functions ********************************************************** */

/* The description is in the header file. */
static retcode_t wlanConnect(void)
{
    NetworkConfig_IpSettings_T myIpSettings;
    char ipAddress[PAL_IP_ADDRESS_SIZE] = { 0 };
    Ip_Address_T* IpaddressHex = Ip_getMyIpAddr();
    WlanConnect_SSID_T connectSSID;
    WlanConnect_PassPhrase_T connectPassPhrase;
    Retcode_T ReturnValue = (Retcode_T)RETCODE_FAILURE;
    int32_t Result = INT32_C(-1);

    if (RETCODE_OK != WlanConnect_Init())
    {
        return (RC_PLATFORM_ERROR);
    }

    printf("Connecting to %s \r\n ", WLAN_CONNECT_WPA_SSID);

    connectSSID = (WlanConnect_SSID_T) WLAN_CONNECT_WPA_SSID;
    connectPassPhrase = (WlanConnect_PassPhrase_T) WLAN_CONNECT_WPA_PASS;
    ReturnValue = NetworkConfig_SetIpDhcp(NULL);
    if (RETCODE_OK != ReturnValue)
    {
        printf("Error in setting IP to DHCP\n\r");
        return (RC_PLATFORM_ERROR);
    }
    if (RETCODE_OK == WlanConnect_WPA(connectSSID, connectPassPhrase, NULL))
    {
        ReturnValue = NetworkConfig_GetIpSettings(&myIpSettings);
        if (RETCODE_OK == ReturnValue)
        {
            *IpaddressHex = Basics_htonl(myIpSettings.ipV4);
            Result = Ip_convertAddrToString(IpaddressHex, ipAddress);
            if (Result < 0)
            {
                printf("Couldn't convert the IP address to string format \r\n ");
                return (RC_PLATFORM_ERROR);
            }
            printf("Connected to WPA network successfully \r\n ");
            printf(" Ip address of the device %s \r\n ", ipAddress);
            return (RC_OK);
        }
        else
        {
            printf("Error in getting IP settings\n\r");
            return (RC_PLATFORM_ERROR);
        }
    }
    else
    {
        return (RC_PLATFORM_ERROR);
    }

}

/* The description is in the configuration header file. */
static retcode_t callbackOnSent(Callable_T *callfunc, retcode_t status)
{
    BCDS_UNUSED(callfunc);

    if (status != RC_OK)
    {
        printf("error occurred in connecting server \r\n");
    }
    return (RC_OK);
}

/* The description is in the configuration header file. */
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
                printf("GET Response Content %s length %d \r\n", content_ptr, len);
            }
        }
    }

    if (rc != RC_OK)
    {
        printf("error occurred in downloading HTML \r\n");
    }
    return (rc);
}
/* global functions ********************************************************** */

/* The description is in the configuration header file. */
void connectServer(xTimerHandle xTimer)
{
    BCDS_UNUSED(xTimer);

    retcode_t rc = RC_OK;
    Msg_T* msg_ptr;
    Ip_Port_T destPort = (Ip_Port_T) DEST_PORT_NUMBER;
    static Callable_T SentCallable;
    char const *url_ptr = "/"; /* URL string for the http website */
    Callable_T * Callable_pointer;
    Callable_pointer = Callable_assign(&SentCallable, callbackOnSent);
    if (Callable_pointer == NULL)
    {
        printf("Failed Callable_assign\r\n ");
        return;
    }
    rc = HttpClient_initRequest(&destAddr, Ip_convertIntToPort(destPort), &msg_ptr);

    if (rc != RC_OK || msg_ptr == NULL)
    {
        printf("Failed HttpClient_initRequest\r\n ");
        return;
    }

    HttpMsg_setReqMethod(msg_ptr, Http_Method_Get);
    rc = HttpMsg_setReqUrl(msg_ptr, url_ptr);
    if (rc != RC_OK)
    {
        printf("Failed to fill message \r\n ");
        return;
    }

    rc = HttpClient_pushRequest(msg_ptr, &SentCallable,
            httpClientResponseCallback);
    if (rc != RC_OK)
    {
        printf("Failed HttpClient_pushRequest \r\n  ");
        return;
    }
}

/* The description is in the interface header file. */
void init(void)
{
    retcode_t rc = RC_OK;

    rc = wlanConnect();
    if (RC_OK != rc)
    {
        printf("Network init/connection failed %i \r\n", rc);
        return;
    }

    rc = PAL_initialize();
    if (RC_OK != rc)
    {
        printf("PAL and network initialize %i \r\n", rc);
        return;
    }

    PAL_socketMonitorInit();

    /* start client */
    rc = HttpClient_initialize();
    if (rc != RC_OK)
    {
        printf("Failed to initialize http client\r\n ");
        return;
    }

    if (RC_OK != PAL_getIpaddress((uint8_t*) "www.posttestserver.com", &destAddr))
    {
        return;
    }
    else
    {
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
                (const char * const ) "connectServer", Ticks,
                TIMER_AUTORELOAD_ON, NULL, connectServer);

        if (connectTimerHandle != NULL)
        {
            if (xTimerStart(connectTimerHandle, TIMERBLOCKTIME) != pdTRUE)
            {
                assert(false);
            }

        }
    }
}

/**
 * @brief This is a template function where the user can write his custom application.
 *
 */
void appInitSystem(xTimerHandle xTimer)
{
    BCDS_UNUSED(xTimer);
    /*Call the RHC init API */
    init();
}
/** ************************************************************************* */
