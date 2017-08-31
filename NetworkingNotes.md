1) Issues are located somewhere between XDK and local server
2) Http GET Requests are able to make it out to remote servers with close to 100% success
	ex: posttestserver.com and xdk.bosch-connectivity.com, having issues with httpbin.org/ip although bosch said it works
3) Failing in the init http request section in the following way
	It successfull allocates the Msg struct
	Checks to see if a session is already happening with the server and does not find one
	Next it creates a TCP session with the server (THIS IS WHERE IS FAILS)
	Returns RC_TCP_NOT_CONNECTED as error and exits
	Sometimes after a large group of fails happen, one successful connection can recieve a group of responses suggesting a que builing up
4) XDK can support multiple tcp sessions at once as long as they are with different hosts 
5) When a message is sent, it takes 7 - 10 seconds waiting for a response blocking on that session. During this time other init requests fail.
	This translates to a RC_HTTP_SEND_ERROR (1375) returned from the callback function that recieves the response from the server. This is
	triggered when the pushRequest() function is called.
6) Diagnosis: Somewhere in the server side the TCP connection is breaking and is causing this loss in connection for the 7-10 seconds as the last sent
	request times out. 
