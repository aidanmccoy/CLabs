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

//Author: Aidan McCoy
/*Indiciator light key
 Orange light at startup -> Connecting to wifi network
 Blinking Yellow only -> Sending register requests, waiting for response
 Blinking Yellow with solid Orange -> Successful register, sending data*/

#ifndef XDK110_HTTPEXAMPLECLIENT_H_
#define XDK110_HTTPEXAMPLECLIENT_H_

#define FRSSID           				"RelayRouter"         /**< Macros to define WPA/WPA2 network settings */
#define FRKEY           				"Factory-Relay@2016"      /**< Macros to define WPA/WPA2 network settings */
#define FRHOST							"HN"
#define FRNODE							"10.1.10.112"
#define FRPORT							UINT16_C(80/*56807*/)

#define CALLBACK_PARAMETER_PB1 UINT32_C(0x11)     /**< Macro represents callback parameter for Button1 */
#define CALLBACK_PARAMETER_PB2 UINT32_C(0x12)     /**< Macro represents callback parameter for Button2 */
#define ACCEL_RECEIVELENGTH 	      UINT8_C(30)                      /**< Receive length for BLE */
//#define GYRO_RECEIVELENGTH 	      	  UINT8_C(30)
//#define MAG_RECIEVELENGTH				UINT8_C(30)

#define PRESSED UINT32_C(1)
#define UNPRESSED UINT32_C(0)
#define CONNECT_TIME_INTERVAL           UINT32_C(450)          /**< Macro to represent connect time interval */
#define TIMERBLOCKTIME                  UINT32_C(0xffff)        /**< Macro used to define blocktime of a timer*/
#define TIMER_AUTORELOAD_ON             UINT32_C(1)             /**< Auto reload of timer is enabled*/
#define WATCHDOG_TIME_INTERVAL           UINT32_C(5000)			/**< Macro for how long to wait before resetting on stall */

//Starts the wifi connection with the variables listed above
static void startNetwork(void);

//When a message is sent, this method checks for failure and reports it
static retcode_t callbackOnSent(Callable_T *callfunc, retcode_t status);

//When a response from the server arrives, this handles it.
//	It checks for a successful code
//	Checks the length to be 31 (The length of an encoded path message)
//	Stores the encoded path and confirms resitration
static retcode_t httpClientResponseCallback(HttpSession_T *httpSession,
		Msg_T *msg_ptr, retcode_t status);

//Generic method that sends a GET request for a given url error checking along the way
static retcode_t HttpSendString(char *urlPtr);

//Builds a string to register the XDK and calls httpSendString to send it
static void registerDevice(void);

//After a successful registration, data is sent to the server
//	Data is read from the hardware
//	Data is compiled into a url with encoded path ready to be sent
//	httpSendString is called to send the message to the server
static void sendData(void);

//Checks to see if the XDK is successfully registered or not and either sends
// data or registers the XDK
static void startCommunication(void);

//Creates the timer to start each sequence of message passing
void init(void);

//Called when a button is pressed and sets a flag accordingly
static void BtnCallback(void *handle, uint32_t userParameter);

//Initializes the buttons, accelerometer, and light sensor returning error messages
static retcode_t initHardware(void);

//Overall initialization to start the program
void appInitSystem(xTimerHandle xTimer);

#endif /* XDK110_HTTPEXAMPLECLIENT_H_ */
