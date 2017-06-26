Day 1
	Product Hierarchy
		C-DEngine.dll is the basis
		Factory-Relay and Machine-Monitor are higher up
	Port numbers
		Portws is for web service
	ClientBin folder
		All data becuause the registry is not used
	Service
		Something that runs in the background that runs with high permissions
		You can restart services from the task manager
		You must restart the MachineMonitor service if you delete the ClientBin
	Logs
	Found by going to http://localhost:PortNumber/cdeStatus.aspx?<Flag>
	SYSLOG 
		System log
	SESLOG
		Session Log
	SUBDET 
		SUbscription details
	DIAG
		Threads and memory usage
	HSI
		Host service info
		Not expensive
	ALL
		Everything
	FilterStatus = <EngineName>
	/cdeStatus.json
 
 
	S2TF1QW8  --> Security Key
 
 

Day 2
	Connections to client devices
		All connections are started from the device reaching to the cloud, never the other way around. 
		The arrows are incorrect in some of the slides
	C# 
		Properties
			Most important for C-DEngine
		Methods
			None in the C-DEngine perspective
		Events
	Windows Programming
		‘I’ at the beginning of a class means interface
	Services and Engines are both plugins
		Send messages between services and plugins
		Not functions calls between	
 
 

Day 3
	If a node is allowingUnscopedMesh is true, then it can only talk to other unsocped nodes 
	To write to log in plugin, go to project properties and add CDE_SYSLOG to the conditional compilation symbols
	Logging to a text document
		Add two more additions to the add arglist
			ArgList.Add(“LogWriteBuffer”, “5”);  //Batch 5 RAM Before writing
			ArgList.Add(“LogWriteFilterLevel”, “1”); //Essential = 1…
			ArgList.Add(“LogFilePath”, “c://temp//debugwhatever.log”);
	Startup init times available in the thing registry 
		Keep everything under 1 second
		Future releases will kill everything else
 

Day 4
	How to add a sub property
		cdeP MyProperty = this.GetProperty(“SamplyProperty”, true);
		MyProperty.SetProperty(“SubProperty”, “SubProperty datat to store”);
	You can only encrypt string properties when passed to browser, all other data will be reset to zero
