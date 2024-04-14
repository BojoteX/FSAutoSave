# FSAutoSave
Automatically save your Flights in Flight Simulator so you can resume where you left. FSAutoSave is easy to install and use and does not require any external configuration. It will automatically save your flight when you end a session or by pressing CTRL+ALT+S. 

## Installation
Just run in the same computer where you run MSFS. It will automatically detect when the simulator is running. No need to configure anything. Program will show you exactly what it is doing and when it is saving your flight.

## Limitations
The program uses the SimConnect API to save your flight, so if the aircraft you are using does not SAVE internally its state FSAutoSave will simply save the position where you left the aircraft and some other basic variables (Simvars).

## Features
	- Automatically sets local ZULU TIME in the simulator when resuming a flight so you can continue your flight using real time WEATHER and the correct local TIME.
	- Automatically saves your flight when you end a session or by pressing CTRL+ALT+S.
	- Removes the tug from the aircraft when resuming a flight and not using a MSFS loaded flight plan. (tug will only show if you started or resumed a flight that used a MSFS loaded .PLN file)
	- You can use the program in DEBUG mode to see what is happening in the background. This will effectively disable the automatic saving feature and local ZULU TIME setting and makes the program act as a troubleshooting tool.
	- You can use the program in SILENT mode to hide the console window and still have the automatic saving feature and local ZULU TIME setting enabled.

	### Command line usage
		Run the program in DEBUG mode by using the -DEBUG command line argument. (e.g FSAutoSave.exe -DEBUG)
		Run the program in SILENT mode (minimized) by using the -SILENT command line argument. (e.g FSAutoSave.exe -SILENT) or both at the same time (e.g FSAutoSave.exe -DEBUG -SILENT)

## Compiling
If you want to compile the program yourself, you will need to install the MSFS SDK. Thats it, no other dependencies are required and the program should compile without any issues.

## License
This program is free to use and modify. You can distribute it as you wish but you need to include the copyright notice. If you want to contribute to the project, please feel free to do so.

