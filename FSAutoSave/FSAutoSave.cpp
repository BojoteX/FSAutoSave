#define NOMINMAX
#include <windows.h>
#include <thread>
#include "FSAutoSave.h"
#include "Globals.h"
#include "Utility.h"

int positionRequester = 0;

void initApp() {

    // Run monitoring for CustomFlight.FLT file writes in a separate thread.
    std::thread fileMonitorThread(monitorCustomFlightChanges);
    fileMonitorThread.detach();  // Detach the thread to run independently

    // Initilize Facility Definitions (for data I might need)
    hr = SimConnect_AddToFacilityDefinition(hSimConnect, DEFINITION_FACILITY_AIRPORT, "OPEN AIRPORT");
    hr = SimConnect_AddToFacilityDefinition(hSimConnect, DEFINITION_FACILITY_AIRPORT, "NAME64");
    hr = SimConnect_AddToFacilityDefinition(hSimConnect, DEFINITION_FACILITY_AIRPORT, "ICAO");

    hr = SimConnect_AddToFacilityDefinition(hSimConnect, DEFINITION_FACILITY_AIRPORT, "OPEN TAXI_PARKING");
    hr = SimConnect_AddToFacilityDefinition(hSimConnect, DEFINITION_FACILITY_AIRPORT, "NAME");
    hr = SimConnect_AddToFacilityDefinition(hSimConnect, DEFINITION_FACILITY_AIRPORT, "SUFFIX");
    hr = SimConnect_AddToFacilityDefinition(hSimConnect, DEFINITION_FACILITY_AIRPORT, "NUMBER");
    hr = SimConnect_AddToFacilityDefinition(hSimConnect, DEFINITION_FACILITY_AIRPORT, "CLOSE TAXI_PARKING");

    hr = SimConnect_AddToFacilityDefinition(hSimConnect, DEFINITION_FACILITY_AIRPORT, "CLOSE AIRPORT");

    if (hr != S_OK) {
        printf("\nFailed to Add to Data Definition\n");
    }

    // To determine aircraft position and state
    hr = SimConnect_AddToDataDefinition(hSimConnect, DEFINITION_POSITION_DATA, "PLANE LATITUDE", "degrees");
    hr = SimConnect_AddToDataDefinition(hSimConnect, DEFINITION_POSITION_DATA, "PLANE LONGITUDE", "degrees");
    hr = SimConnect_AddToDataDefinition(hSimConnect, DEFINITION_POSITION_DATA, "PLANE ALTITUDE", "feet");
    hr = SimConnect_AddToDataDefinition(hSimConnect, DEFINITION_POSITION_DATA, "AIRSPEED INDICATED", "feet per second"); // Used for ZVelBodyAxis in [SimVars.0] to adjust speed to IAS
    hr = SimConnect_AddToDataDefinition(hSimConnect, DEFINITION_POSITION_DATA, "GROUND VELOCITY", "knots");
    hr = SimConnect_AddToDataDefinition(hSimConnect, DEFINITION_POSITION_DATA, "PLANE HEADING DEGREES MAGNETIC", "degrees");
    hr = SimConnect_AddToDataDefinition(hSimConnect, DEFINITION_POSITION_DATA, "SIM ON GROUND", "Bool");

    // To determine where we are in the menus
    hr = SimConnect_AddToDataDefinition(hSimConnect, DEFINITION_CAMERA_STATE, "CAMERA STATE", "number");

    // ZULU Time Data Definition to obtain day of year (not really used as we can get it from the actual system clock)
    // hr = SimConnect_AddToDataDefinition(hSimConnect, DEFINITION_ZULU_TIME, "ZULU DAY OF YEAR", "number");

    // One request for the user aircraft position polls every second, the other request for the user aircraft position polls only once
    // hr = SimConnect_RequestDataOnSimObject(hSimConnect, REQUEST_POSITION, DEFINITION_POSITION_DATA, SIMCONNECT_OBJECT_ID_USER, SIMCONNECT_PERIOD_SECOND, SIMCONNECT_DATA_REQUEST_FLAG_CHANGED);

    // Request data on specific Simvars (e.g. ZULU time or CAMERA STATE)
    hr = SimConnect_RequestDataOnSimObject(hSimConnect, REQUEST_CAMERA_STATE, DEFINITION_CAMERA_STATE, SIMCONNECT_OBJECT_ID_USER, SIMCONNECT_PERIOD_SECOND, SIMCONNECT_DATA_REQUEST_FLAG_CHANGED);

    // Read early any data I need (this is just a sample for ZULU time) 
    // hr = SimConnect_RequestDataOnSimObject(hSimConnect, REQUEST_ZULU_TIME, DEFINITION_ZULU_TIME, SIMCONNECT_OBJECT_ID_USER, SIMCONNECT_PERIOD_ONCE, SIMCONNECT_DATA_REQUEST_FLAG_DEFAULT);

    // hr = SimConnect_RequestDataOnSimObjectType(hSimConnect, REQUEST_ZULU_TIME, DEFINITION_ZULU_TIME, 0, SIMCONNECT_SIMOBJECT_TYPE_USER);

    // Request States (for when the program starts so we know the current state)
    hr = SimConnect_RequestSystemState(hSimConnect, REQUEST_AIRCRAFT_STATE, "AircraftLoaded");      // szString contains the aircraft loaded path
    hr = SimConnect_RequestSystemState(hSimConnect, REQUEST_FLIGHTPLAN_STATE, "FlightPlan");        // szString contains the flight plan path 
    hr = SimConnect_RequestSystemState(hSimConnect, REQUEST_FLIGHTLOADED_STATE, "FlightLoaded");    // szString contains the flight loaded path
    hr = SimConnect_RequestSystemState(hSimConnect, REQUEST_SIM_STATE, "Sim");                      // What is the current state of the sim?
    hr = SimConnect_RequestSystemState(hSimConnect, REQUEST_DIALOG_STATE, "DialogMode");            // Are we in dialog mode?

    // System events (the ones we need to know when they happen) 
    hr = SimConnect_SubscribeToSystemEvent(hSimConnect, EVENT_RECUR_FRAME, "frame");
    hr = SimConnect_SubscribeToSystemEvent(hSimConnect, EVENT_SIM_START, "SimStart");
    hr = SimConnect_SubscribeToSystemEvent(hSimConnect, EVENT_SIM_STOP, "SimStop");
    hr = SimConnect_SubscribeToSystemEvent(hSimConnect, EVENT_SIM_PAUSE_EX1, "Pause_EX1");
    hr = SimConnect_SubscribeToSystemEvent(hSimConnect, EVENT_AIRCRAFT_LOADED, "AircraftLoaded");
    hr = SimConnect_SubscribeToSystemEvent(hSimConnect, EVENT_FLIGHTPLAN_DEACTIVATED, "FlightPlanDeactivated");
    hr = SimConnect_SubscribeToSystemEvent(hSimConnect, EVENT_FLIGHTPLAN_ACTIVATED, "FlightPlanActivated");
    hr = SimConnect_SubscribeToSystemEvent(hSimConnect, EVENT_FLIGHT_SAVED, "FlightSaved");
    hr = SimConnect_SubscribeToSystemEvent(hSimConnect, EVENT_FLIGHT_LOAD, "FlightLoaded");
    hr = SimConnect_SubscribeToSystemEvent(hSimConnect, EVENT_SIM_VIEW, "View");
    hr = SimConnect_SubscribeToSystemEvent(hSimConnect, EVENT_SIM_CRASHED, "Crashed");
    hr = SimConnect_SubscribeToSystemEvent(hSimConnect, EVENT_SIM_CRASHRESET, "CrashReset");

    // ZULU Client Events. No need to set notification group for them as we don't need info back
    hr = SimConnect_MapClientEventToSimEvent(hSimConnect, EVENT_ZULU_MINUTES_SET, "ZULU_MINUTES_SET");
    hr = SimConnect_MapClientEventToSimEvent(hSimConnect, EVENT_ZULU_HOURS_SET, "ZULU_HOURS_SET");
    hr = SimConnect_MapClientEventToSimEvent(hSimConnect, EVENT_ZULU_DAY_SET, "ZULU_DAY_SET");
    hr = SimConnect_MapClientEventToSimEvent(hSimConnect, EVENT_ZULU_YEAR_SET, "ZULU_YEAR_SET");

    // Other Misc Client Events
    hr = SimConnect_MapClientEventToSimEvent(hSimConnect, EVENT_SITUATION_RESET, "SITUATION_RESET");

    // Custom Client Events
    hr = SimConnect_MapClientEventToSimEvent(hSimConnect, EVENT_FLIGHTPLAN_RESET, "custom.fp.reset");
    hr = SimConnect_MapClientEventToSimEvent(hSimConnect, EVENT_FLIGHTPLAN_LOAD, "custom.fp.load");
    hr = SimConnect_MapClientEventToSimEvent(hSimConnect, EVENT_SITUATION_SAVE, "custom.save");
    hr = SimConnect_MapClientEventToSimEvent(hSimConnect, EVENT_SITUATION_RELOAD, "custom.reload");
    hr = SimConnect_MapClientEventToSimEvent(hSimConnect, EVENT_CLOSEST_AIRPORT, "custom.position");

    // Input Events
    // hr = SimConnect_MapInputEventToClientEvent_EX1(hSimConnect, INPUT0, "esc", EVENT_SITUATION_SAVE);
    hr = SimConnect_MapInputEventToClientEvent_EX1(hSimConnect, INPUT0, "VK_LCONTROL+VK_LMENU+s", EVENT_SITUATION_SAVE, 55);
    hr = SimConnect_MapInputEventToClientEvent_EX1(hSimConnect, INPUT0, "VK_LCONTROL+VK_LMENU+p", EVENT_CLOSEST_AIRPORT);

    // Disable the following as they are not needed for now. Maybe future use
    // hr = SimConnect_MapInputEventToClientEvent_EX1(hSimConnect, INPUT0, "VK_RMENU+f", EVENT_FLIGHTPLAN_LOAD);
    // hr = SimConnect_MapInputEventToClientEvent_EX1(hSimConnect, INPUT0, "VK_RMENU+r", EVENT_FLIGHTPLAN_RESET);
    // hr = SimConnect_MapInputEventToClientEvent_EX1(hSimConnect, INPUT0, "VK_RCONTROL+r", EVENT_SITUATION_RESET);
    // hr = SimConnect_MapInputEventToClientEvent_EX1(hSimConnect, INPUT0, "VK_RCONTROL+VK_RMENU+r", EVENT_SITUATION_RELOAD);

    // Add Notifications for client events and input events I need. No need to do this for system events as they are already subscribed
    hr = SimConnect_AddClientEventToNotificationGroup(hSimConnect, GROUP0, EVENT_FLIGHTPLAN_RESET);
    hr = SimConnect_AddClientEventToNotificationGroup(hSimConnect, GROUP0, EVENT_SITUATION_RESET);
    hr = SimConnect_AddClientEventToNotificationGroup(hSimConnect, GROUP0, EVENT_FLIGHTPLAN_LOAD);
    hr = SimConnect_AddClientEventToNotificationGroup(hSimConnect, GROUP0, EVENT_SITUATION_RELOAD);
    hr = SimConnect_AddClientEventToNotificationGroup(hSimConnect, GROUP0, EVENT_SITUATION_SAVE);
    hr = SimConnect_AddClientEventToNotificationGroup(hSimConnect, GROUP0, EVENT_CLOSEST_AIRPORT);

    // Set priority for the notification group
    hr = SimConnect_SetNotificationGroupPriority(hSimConnect, GROUP0, SIMCONNECT_GROUP_PRIORITY_HIGHEST);
    // hr = SimConnect_SetNotificationGroupPriority(hSimConnect, GROUP1, SIMCONNECT_GROUP_PRIORITY_DEFAULT);

    // Set the input group state
    hr = SimConnect_SetInputGroupState(hSimConnect, INPUT0, SIMCONNECT_STATE_ON);

    // Set the system event state (toggle ON or OFF)
    hr = SimConnect_SetSystemEventState(hSimConnect, EVENT_RECUR_FRAME, SIMCONNECT_STATE_OFF); // Enable it when we DON'T need to analyze every frame
    // hr = SimConnect_SetSystemEventState(hSimConnect, EVENT_RECUR_FRAME, SIMCONNECT_STATE_ON); // Enable it when we need to analyze every frame
}

void CALLBACK Dispatcher(SIMCONNECT_RECV* pData, DWORD cbData, void* pContext)
{
    if(DEBUG)
        printf("Received callback with data size: %lu bytes\n", cbData); // General data size

    switch (pData->dwID)
    {

    case SIMCONNECT_RECV_ID_NULL: {
        printf("NULL received\n");
        break;
    }

    case SIMCONNECT_RECV_ID_FACILITY_DATA: {
        SIMCONNECT_RECV_FACILITY_DATA* pFacilityData = (SIMCONNECT_RECV_FACILITY_DATA*)pData;

        switch (pFacilityData->Type)
        {

        case SIMCONNECT_FACILITY_DATA_AIRPORT: {
            sAirport* airport = (sAirport*)&pFacilityData->Data;

            if (positionRequester == 0) {
				printf("Closest airport is %s (%s)\n", airport->name, airport->icao);
			}

            airportName = std::string(airport->name);
            airportICAO = std::string(airport->icao);
            break;
        }

        case SIMCONNECT_FACILITY_DATA_RUNWAY:
        {
            printf("Runway data received. NOT IMPLEMENTED YET\n");
            break;
        }

        case SIMCONNECT_FACILITY_DATA_JETWAY:
        {
            sJetways* jetway = (sJetways*)&pFacilityData->Data;

            countJetways++;
            break;
        }

        case SIMCONNECT_FACILITY_DATA_TAXI_PARKING:
        {
            sTaxiParkings* taxiparking = (sTaxiParkings*)&pFacilityData->Data;

            if ((countTaxiParking == parkingIndex) && parkingIndex != NULL) {
                GateInfo gateInfo = formatGateName(taxiparking->NAME);
                GateInfo gateSuffixInfo = formatGateName(taxiparking->SUFFIX);
                int clockPos = calculateClockPosition(JetwayBearing, myHeading);

                parkingGate = gateInfo.gateString;
                parkingGateSuffix = gateSuffixInfo.gateString;
                parkingNumber = taxiparking->NUMBER;

                if (positionRequester == 0) {
                    std::string gateString = "Closest Jetway is " + gateInfo.friendlyName + " " + std::to_string(taxiparking->NUMBER) + " at " + airportName + ". Distance from your aircraft is " + std::to_string(int(metersToFeet(JetwayDistance))) + " meters (" + std::to_string(int(JetwayDistance)) + " feet) at your " + std::to_string(clockPos) + " o'clock";
                    sendText(hSimConnect, gateString);
                    printf("Closest Jetway is %s %d\n", gateInfo.friendlyName.c_str(), taxiparking->NUMBER);
                }

                taxiparking = nullptr;
            }

            countTaxiParking++;
            break;
        }

        case SIMCONNECT_FACILITY_DATA_TAXI_PATH:
        {
            printf("Taxi path data received. NOT IMPLEMENTED YET\n");
            break;
        }

        case SIMCONNECT_FACILITY_DATA_FREQUENCY:
        {
            printf("Frequency data received. NOT IMPLEMENTED YET\n");
            break;
        }

        case SIMCONNECT_FACILITY_DATA_VOR:
        {
            printf("VOR data received. NOT IMPLEMENTED YET\n");
            break;
        }

        case SIMCONNECT_FACILITY_DATA_WAYPOINT:
        {
            printf("Waypoint data received. NOT IMPLEMENTED YET\n");
            break;
        }

        default:
            printf("Unhandled request ID: %lu\n", pFacilityData->UserRequestId); // Log unhandled request IDs
            break;
        }
        break;
    }

    case SIMCONNECT_RECV_ID_FACILITY_DATA_END: {
        SIMCONNECT_RECV_FACILITY_DATA_END* pFacilityData = (SIMCONNECT_RECV_FACILITY_DATA_END*)pData;

        // printf("Request ID %u have been processed succesfully, reset values\n", pFacilityData->RequestId);

        finalFLTchange(); // MODIFY the .FLT file to set the FirstFlightState to firstFlightState* but only do it for the final save and when flight is LAST.FLT

        // Reset the counters
        countJetways = 0;
        countTaxiParking = 0;

        // Reset the requester after use
        positionRequester = 0; // Reset the position requester

        // Reset names after use
        airportName = "";
        airportICAO = "";

        JetwayDistance = NULL;
        JetwayBearing = NULL;

        // After using the data, reset the values
        parkingIndex = NULL;

        break;
    }

    case SIMCONNECT_RECV_ID_JETWAY_DATA:
    {
        SIMCONNECT_RECV_JETWAY_DATA* pJetwayData = (SIMCONNECT_RECV_JETWAY_DATA*)pData;
        unsigned int count = static_cast<unsigned int>(pJetwayData->dwArraySize);

        if (count > 0) {


            double closestJetwayDistance = std::numeric_limits<double>::max();
            double closestJetwayBearing  = std::numeric_limits<double>::max();

            SIMCONNECT_JETWAY_DATA* closestJetway = nullptr;
            parkingIndex = NULL;  // Initialize the parking index to NULL as 0 is a valid index

            for (unsigned int i = 0; i < count; ++i) {
                SIMCONNECT_JETWAY_DATA& jetway = pJetwayData->rgData[i];
                DistanceAndBearing result = calculateDistanceAndBearing(myLatitude, myLongitude, jetway.Lla.Latitude, jetway.Lla.Longitude);
                if (result.distance < closestJetwayDistance) {
                    closestJetwayDistance   = result.distance;
                    closestJetwayBearing    = result.bearing;
                    closestJetway = &jetway;
                }
            }
  
            if (closestJetway) {
                parkingIndex = closestJetway->ParkingIndex;
                int clockPos = calculateClockPosition(closestJetwayBearing, myHeading);
                // printf("Closest Jetway is %.0f feet away (%.0f meters) bearing %.0f degrees (your %d o'clock)\n", metersToFeet(closestJetwayDistance), closestJetwayDistance, closestJetwayBearing, clockPos);
                JetwayDistance = closestJetwayDistance;
                JetwayBearing = closestJetwayBearing;
            }

        }
        else {
            printf("No Jetways found\n");
        }

        break;
    }

    case SIMCONNECT_RECV_ID_SIMOBJECT_DATA_BYTYPE: {
        SIMCONNECT_RECV_SIMOBJECT_DATA_BYTYPE* pObjData = (SIMCONNECT_RECV_SIMOBJECT_DATA_BYTYPE*)pData;
        switch (pObjData->dwRequestID) {
        case REQUEST_ZULU_TIME:
        {
            SimDayOfYear* pDOY = (SimDayOfYear*)&pObjData->dwData;
            printf("In-Sim ZULU Day of Year: %.0lf\n", pDOY->dayOfYear);
            break;
        }
        default:
            printf("Unhandled request ID: %lu\n", pObjData->dwRequestID); // Log unhandled request IDs
            break;
        }
        break;
    }

    case SIMCONNECT_RECV_ID_AIRPORT_LIST: {
        SIMCONNECT_RECV_AIRPORT_LIST* pAirList = (SIMCONNECT_RECV_AIRPORT_LIST*)pData;
        char closestAirportIdent[8] = "";
        double closestDistance = DBL_MAX;

        if (pAirList->dwArraySize > 0) {
            SIMCONNECT_DATA_FACILITY_AIRPORT* airports = (SIMCONNECT_DATA_FACILITY_AIRPORT*)(pAirList + 1);
            for (DWORD i = 0; i < pAirList->dwArraySize; ++i) {
                if (airports[i].Ident[0] != '\0') {
                    double distanceSquared = pow(airports[i].Latitude - myLatitude, 2) + pow(airports[i].Longitude - myLongitude, 2);
                    if (distanceSquared < closestDistance) {
                        closestDistance = distanceSquared;
                        strncpy_s(closestAirportIdent, sizeof(closestAirportIdent), airports[i].Ident, _TRUNCATE);
                    }
                }
            }

            if (closestAirportIdent[0] != '\0') {
                // printf("Closest airport ICAO: %s\n", closestAirportIdent);

                if (isSimOnGround) {
                    // Request data only if on the ground
                    handleGroundOperations(closestAirportIdent); // Request Jetway data
                }
                else {
                    printf("You are currently in the air. Not Jetway/Gate data available.\n");
                }

                // Reset names before calling FACILITY DATA. If the request fails, we will have empty strings
                airportName = "";
                airportICAO = "";

                // This should be somewhere else... need to investigate DEBUG THIS
                hr = SimConnect_RequestFacilityData(hSimConnect, DEFINITION_FACILITY_AIRPORT, FACILITY_DATA_DEF_REQUEST_START + g_RequestCount, closestAirportIdent);
                if (hr == S_OK) {
                    g_RequestCount++;
                }
                else {
                    printf("Failed to obtain airport name\n");
                }

            }
            else {
                printf("No airports found. Check cache\n");
            }
        }
        else {
            printf("No airport data received. You are literally in the middle of nowhere (or in the menu screen)\n");
        }
        break;
    }

    case SIMCONNECT_RECV_ID_SIMOBJECT_DATA: 
    {
        SIMCONNECT_RECV_SIMOBJECT_DATA* pObjData = (SIMCONNECT_RECV_SIMOBJECT_DATA*)pData;

        if(DEBUG)
            printf("SIMOBJECT_DATA received with request ID: %lu\n", pObjData->dwRequestID); // Identify request ID

        switch (pObjData->dwRequestID)
        {
        
        case REQUEST_CAMERA_STATE:
        {
            CameraState* pCS = (CameraState*)&pObjData->dwData;

            // printf("\nCamera state is %0.f\n", pCS->state);

            if (pCS->state == 12) { // 12 is the value for the "World Map" camera state
                flightInitialized = TRUE;

                // Fix the MSFS bug when entering the World Map
                fixMSFSbug(customFlightmod); 
                fixMSFSbug(lastMOD); 

                // Remove [LocalVars.0] section from LAST.FLT
                // fixLASTflight(lastMOD); 

                fpDisableCount = 0; // Reset the counter
                userLoadedPLN = FALSE;

                // printf("\nEntering World Map\n");

                // Use GetFP to get the flight plan when we enter the World Map if using SimBrief
                std::thread(getFP).detach(); // Spawn a new thread to get the flight plan so we don't block the main thread. It will be cleaned up automatically
			}
            else if (pCS->state == 11) { // 11 is used when first loading or exiting a flight

                if (!flightInitialized) {
                    // printf("\nEntering main menu...\n");
				}

                flightInitialized = TRUE;
            }
            else if (pCS->state == 15) { // 15 is used when in the menu screen
                // printf("\nIn menu screen\n");
                flightInitialized = TRUE;
            }
            else if (pCS->state == 2) { // 2 is used when in the sim/cockpit
                // printf("\nIn Cockpit\n");
                flightInitialized = FALSE;
            }
            else {
                flightInitialized = FALSE;
                // Check if pObjData and pCS pointers are valid
                if (pObjData && pCS) {
                    // printf("Camera state is %0.f\n", pCS->state);
                }
                else {
                    printf("Invalid data pointer(s). Unable to retrieve camera state.\n");
                }
            }
            break;
        }
        case REQUEST_POSITION:
        {
            AircraftPosition* pS = (AircraftPosition*)&pObjData->dwData;

            int lat_int = static_cast<int>(pS->latitude);
            int lon_int = static_cast<int>(pS->longitude);

            if (lat_int == 0 && lon_int == 0) {
				printf("Aircraft Position: Not available or in Main Menu\n");
			}
            else {
                printf("Latitude: %f - Longitude: %f - Altitude: %.0f feet - Ground Speed: %.0f knots - Heading: %.0f degrees - isOnGround: %.0f\n", pS->latitude, pS->longitude, pS->altitude, pS->airspeed, pS->mag_heading, pS->sim_on_ground);
            }
            break;
        }
        case REQUEST_POSITION_ONCE:
        {
            AircraftPosition* pS = (AircraftPosition*)&pObjData->dwData;

            // These will store our current position so we can use with airport list to determine the nearest airport
            myLatitude      = pS->latitude;
            myLongitude     = pS->longitude;
            myAltitude      = pS->altitude;
            myIASinFPS	    = pS->IASinFPS;
            myAirspeed      = pS->airspeed;
            myHeading       = pS->mag_heading;
            isSimOnGround   = pS->sim_on_ground;

            int lat_int = static_cast<int>(pS->latitude);
            int lon_int = static_cast<int>(pS->longitude);

            if (lat_int == 0 && lon_int == 0) {
                printf("Aircraft Position: Not available or in Main Menu\n");
            }
            else {
                if (positionRequester == 0) {
                    if (pS->sim_on_ground) {
                        if (pS->airspeed < 1) {
                            printf("Currently parked/stopped at Latitude: %f - Longitude: %f\n", pS->latitude, pS->longitude);
                        }
                        else {
                            printf("On the ground, moving at %.0f knots. Heading: %.0f degrees. Current position is Latitude: %f - Longitude: %f\n", pS->airspeed, pS->mag_heading, pS->latitude, pS->longitude);
                        }
                    }
                    else {
                        printf("Current position is Latitude: %f - Longitude: %f - Altitude: %.0f feet - Ground Speed: %.0f knots - Heading: %.0f degrees\n", pS->latitude, pS->longitude, pS->altitude, pS->airspeed, pS->mag_heading);

                    }
                }
            }
            break;
        }
        case REQUEST_ZULU_TIME:
        {
            SimDayOfYear* pDOY = (SimDayOfYear*)&pObjData->dwData;
            printf("In-Sim ZULU Day of Year: %.0lf\n", pDOY->dayOfYear);
            break;
        }
        default:
            printf("Unhandled request ID: %lu\n", pObjData->dwRequestID); // Log unhandled request IDs
            break;
        }
        break;
    }

    case SIMCONNECT_RECV_ID_EVENT_FRAME:
    {
        SIMCONNECT_RECV_EVENT_FRAME* evt = (SIMCONNECT_RECV_EVENT_FRAME*)pData;
        switch (evt->uEventID)
        {
        case EVENT_RECUR_FRAME:

            // Bugged. Its clearly not working
            // hr = SimConnect_RequestSystemState(hSimConnect, REQUEST_DIALOG_STATE, "DialogMode"); // What is the current state of the sim?

            // Below we can get real-time data from the sim changing every frame, use judiciously

            break;
        default:
            printf("Unhandled event ID for SIMCONNECT_RECV_ID_EVENT_FRAME: %lu\n", evt->uEventID); // Log unhandled request IDs
            break;
        }
        break;
    }
    case SIMCONNECT_RECV_ID_EVENT_FILENAME:
    {
        SIMCONNECT_RECV_EVENT_FILENAME* evt = (SIMCONNECT_RECV_EVENT_FILENAME*)pData;
        switch (evt->uEventID)
        {
        case EVENT_FLIGHT_LOAD:

            currentFlight = NormalizePath(evt->szFileName);
            currentFlightPath = evt->szFileName;

            if (currentFlight != "")
                printf("\n[SITUATION EVENT] New Flight Loaded: %s\n", currentFlight.c_str());
            else
                printf("\n[SITUATION EVENT] No flight loaded\n");
  
            currentStatus();

            // Identify if we are in the menu screen by checking if the flight we just loaded is MAINMENU.FLT
            if (currentFlight == "MAINMENU.FLT") {
                isOnMenuScreen = TRUE;
            }
            else {
                isOnMenuScreen = FALSE;
            }

            break;
        case EVENT_FLIGHT_SAVED:
            currentSaveFlight = NormalizePath(evt->szFileName);
            currentSaveFlightPath = evt->szFileName;

            if (currentSaveFlight.empty()) {
                printf("\n[SITUATION EVENT] No flight saved\n");
            }
            else if (currentSaveFlight == "ACTIVITIES.FLT") {
                printf("\n[SITUATION EVENT] Flight %s Saved to %s \n", currentSaveFlight.c_str(), currentSaveFlightPath.c_str());
            }
            else {
                printf("\n[SITUATION EVENT] Flight Saved: %s\n", currentSaveFlight.c_str());

                if (currentFlight == "LAST.FLT") {
                    auto last_modified = fs::last_write_time(currentFlightPath);
                    printf("\nWaiting SAVE to complete... ");
                    while (true) {
                        Sleep(100); // Check every 100 milliseconds
                        if (hasFileUpdated(currentFlightPath, last_modified)) {
                            printf("Done! SAVE completed\n");
                            break; // File has been updated
                        }
                    }
                }

            }

            currentStatus();

            break;
        case EVENT_FLIGHTPLAN_ACTIVATED:

            currentFlightPlan = NormalizePath(evt->szFileName);
            currentFlightPlanPath = evt->szFileName;
            isFlightPlanActive = TRUE;

            if (currentFlightPlan != "") {
                printf("\n[SITUATION EVENT] New Flight Plan Activated: %s\n", currentFlightPlan.c_str());

                if (currentFlight == "MAINMENU.FLT" && currentFlightPlan == "LAST.PLN") {
                    userLoadedPLN = TRUE;

                    if(fpDisableCount)
                        // printf("\n[INFO] Before this activation (%d) DEACTIVATIONS ocurred. Will reset counter.\n", fpDisableCount);
                    
                    fpDisableCount = 0; // Reset the counter
                }
            }
            else {
                printf("\n[SITUATION EVENT] No flight plan activated\n");
            }
            currentStatus();
            break;
        case EVENT_AIRCRAFT_LOADED: {

            currentAircraft = NormalizePath(evt->szFileName);

            if (currentAircraft != "") {
                printf("\n[SITUATION EVENT] New Aircraft Loaded: %s\n", currentAircraft.c_str());
			}
            else {
                printf("\n[SITUATION EVENT] No aircraft loaded\n");
            }

            currentStatus();

            break;
        }
        default:
            printf("Unhandled event ID for SIMCONNECT_RECV_ID_EVENT_FILENAME: %lu\n", evt->uEventID); // Log unhandled request IDs
            break;
        }
        break;
    }

    // I can receive here either szString, dwInteger, or fFloat it will depend on the data type I requested
    case SIMCONNECT_RECV_ID_SYSTEM_STATE: {
        SIMCONNECT_RECV_SYSTEM_STATE* pState = (SIMCONNECT_RECV_SYSTEM_STATE*)pData;
        switch (pState->dwRequestID) {

        case REQUEST_DIALOG_STATE: {
            // Bugged or not working as expected
            if (pState->dwInteger) {
			    // printf("\n[CURRENT STATE] Dialog Mode is ON -> %f - %s\n", pState->fFloat, pState->szString);
		    }
            else {
			    // printf("\n[CURRENT STATE] Dialog Mode is OFF -> %f - %s\n", pState->fFloat, pState->szString);
		    }
		    break;
		}
        case REQUEST_FLIGHTLOADED_STATE:

            currentFlight = NormalizePath(pState->szString);
            currentFlightPath = pState->szString;

            if (currentFlight != "")
                printf("\n[CURRENT STATE] Flight currently loaded: %s\n", currentFlight.c_str());
            else
                printf("\n[CURRENT STATE] No flight currently loaded\n");

            currentStatus();

            // Identify if we are in the menu screen by checking if the flight loaded is MAINMENU.FLT
            if (currentFlight == "MAINMENU.FLT") {
                isOnMenuScreen = TRUE;
            }
            else {
                isOnMenuScreen = FALSE;
            }

            break;
        case REQUEST_SIM_STATE:
            // This will print only when the sim state changes
            if (isSimRunning != pState->dwInteger) {

                if (pState->dwInteger) {
                    simStatus(pState->dwInteger);
                }
                else {
                    simStatus(pState->dwInteger);
                }

            }
            isSimRunning = pState->dwInteger;
            break;
        case REQUEST_FLIGHTPLAN_STATE:
            currentFlightPlan = NormalizePath(pState->szString);
            currentFlightPlanPath = pState->szString;

            if (currentFlightPlan != "")
                printf("\n[CURRENT STATE] Flight plan currently active: %s\n", currentFlightPlan.c_str());
            else
                printf("\n[CURRENT STATE] No flight plan currently active\n");

            currentStatus();

            // Set the flag to TRUE when the flight plan is activated and if one is loaded
            if (currentFlightPlan != "")
                isFlightPlanActive = TRUE;

            break;
        case REQUEST_AIRCRAFT_STATE:
            currentAircraft = NormalizePath(pState->szString);

            if (currentAircraft != "")
                printf("\n[CURRENT STATE] Aircraft currently loaded: %s\n", currentAircraft.c_str());
            else
                printf("\n[CURRENT STATE] No aircraft currently loaded\n");

            currentStatus();
            break;
        default:
            printf("Unhandled state request ID: %lu\n", pState->dwRequestID); // Log unhandled request IDs
            break;
        }
        break;
    }

    case SIMCONNECT_RECV_ID_EVENT:
    {
        SIMCONNECT_RECV_EVENT* evt = (SIMCONNECT_RECV_EVENT*)pData;

        if (evt->uEventID == 0) { // What event is this?
            switch (evt->dwData) {
            case 65536:
                // printf("\n[EVENT] Sending message via TIP screen\n");
                break;
            case 65540:
                // printf("\n[EVENT] Message sent!\n");
                break;
            default:
                // printf("\n[EVENT] (default) Received dwData: %d\n", evt->dwData);
                break;
            }
            break;
        }
        else if (evt->uEventID == EVENT_SIM_PAUSE_EX1) { // Pause events
            switch (evt->dwData) {
            case PAUSE_STATE_FLAG_OFF: {

                if (isPauseBeforeStart) {
                    isPauseBeforeStart = FALSE;

                    printf("\n[STATUS] Simulator is now READY\n");
                    currentStatus();

                    sendText(hSimConnect, "Press CTRL+ALT+S to save anytime. A save is also triggered automatically when you exit you flight session (by pressing the ESC key)");
                }
                break;
            }
            case PAUSE_STATE_FLAG_PAUSE:
                // printf("\n[PAUSE EX1] Fully paused\n");
                // currentStatus();
                wasFullyPaused = TRUE;
                break;
            case PAUSE_STATE_FLAG_PAUSE_WITH_SOUND:
                printf("\n[PAUSE EX1] Legacy NOT USED\n");
                currentStatus();
                // Legacy, might not be used
                break;
            case PAUSE_STATE_FLAG_ACTIVE_PAUSE:
                printf("\n[PAUSE EX1] Active Pause enabled\n");
                currentStatus();
                break;
            case PAUSE_STATE_FLAG_SIM_PAUSE: {
                wasSoftPaused = TRUE; // Set it so than we simulation starts we know it came from a soft pause

                // Check if we are in the menu screen before starting the flight by checking the following conditions
                if (!isFinalSave && !isOnMenuScreen && isFirstSave && flightInitialized) {
                    isPauseBeforeStart = TRUE;
                    printf("\n[STATUS] Simulator is in Briefing screen before start (Press READY TO FLY)\n");
                    currentStatus();
                } // Same here... check all conditions OR if flight was never initilized we assume the app was started while already in the sim
                else if((!isOnMenuScreen && !isFirstSave && isFinalSave) || !flightInitialized) { // This is the case when we are in the sim and we press ESC

                    // Save the situation (without needing to press CTRL+ALT+S or ESC)
                    SimConnect_TransmitClientEvent(hSimConnect, 0, EVENT_SITUATION_SAVE, 98, SIMCONNECT_GROUP_PRIORITY_HIGHEST, SIMCONNECT_EVENT_FLAG_GROUPID_IS_PRIORITY);

                    // Get the current position and the closest airport (including gate)
                    // SimConnect_TransmitClientEvent(hSimConnect, 0, EVENT_CLOSEST_AIRPORT, 666, SIMCONNECT_GROUP_PRIORITY_HIGHEST, SIMCONNECT_EVENT_FLAG_GROUPID_IS_PRIORITY);
				}
                else {
					printf("\n[PAUSE EX1] Simulator is paused\n");
					currentStatus();
				}
                break;
            }
            default:
                printf("\n[PAUSE EX1] Mission paused\n");
                currentStatus();
                break;
            }
            break;
        }
        else { // Other events
            switch (evt->uEventID)
            {
            case EVENT_CLOSEST_AIRPORT: { // CTRL+ALT+P - Request current position, then request the closest airport and gate

                positionRequester = evt->dwData;

                if (positionRequester == 0) {
                    printf("\n[STATUS] Will try to obtain our current position and GATE...\n");
                }

                // Get our current position so we can determine the nearest airport
                hr = SimConnect_RequestDataOnSimObject(hSimConnect, REQUEST_POSITION_ONCE, DEFINITION_POSITION_DATA, SIMCONNECT_OBJECT_ID_USER, SIMCONNECT_PERIOD_ONCE, SIMCONNECT_DATA_REQUEST_FLAG_DEFAULT);
                if (hr != S_OK) {
                    printf("\nFailed to obtain our position\n");
                }
                else {
                    // Try to obtain ther closest airport to the user aircraft
                    hr = SimConnect_RequestFacilitiesList_EX1(hSimConnect, SIMCONNECT_FACILITY_LIST_TYPE_AIRPORT, REQUEST_CLOSEST_AIRPORT);
                    if (hr != S_OK) {
                        printf("\nFailed to obtain closest airport to our position\n");
                    }
                }
                break;
            }
            case EVENT_SITUATION_SAVE: { // CTRL+ALT+S or ESC triggered - Also for the automatic initial save (to set local ZULU time)

                // Only the following Flights are allowed to be saved
                if (currentFlight == "LAST.FLT" || currentFlight == "CUSTOMFLIGHT.FLT") {

                    if (evt->dwData == 99) { // INITIAL SAVE - Saves triggered by setZuluAndSave (we pass 99 as custom value)
                        firstSave();
                    }
                    else if (evt->dwData == 55) { // USER USER SAVE (CTRL+ALT+S triggered)
                        sendText(hSimConnect, "Flight saved succesfully! you can now quit your session and RESUME the flight by simply loading LAST.FLT in the world map screen.");
                        finalSave();
                    }
                    else if (evt->dwData == 98) { // NORMAL SAVE (ESC triggered)
                        printf("\n[EVENT_SITUATION_SAVE] Final save before exiting.. please wait.\n");
                        finalSave();
                    }
                    else if (evt->dwData == 0) { // NORMAL SAVE (ESC triggered)
                        printf("\n[EVENT_SITUATION_SAVE] Final save triggered by pressing ESC key\n");
                        finalSave();
                    }
                    else { // Values for dwData other than 0 or 99 (not implemented yet)
                        printf("\n[ALERT] FLIGHT SITUATION WAS NOT SAVED. RECEIVED %d AS dwData\n", evt->dwData);
                    }
                }
                else {
                    saveNotAllowed();
                }
                break;
            }
            case EVENT_SITUATION_RELOAD: { // RIGHT CONTROL + RIGHT ALT + r
                printf("\n[EVENT_SITUATION_RELOAD] Current flight has been reloaded\n");
                currentStatus();
                wasReset = TRUE; // Set the flag so we know the sim was reset (RELOADED)
                break;
            }
            case EVENT_SITUATION_RESET: { // RIGHT CONTROL + r
                printf("\n[EVENT_SITUATION_RESET] Current flight has been reset\n");
                wasReset = TRUE; // Set the flag so we know the sim was reset
                currentStatus();
                break;
            }
            case EVENT_FLIGHTPLAN_RESET: { // RIGHT ALT + r
                printf("\n[EVENT_FLIGHTPLAN_RESET] Current flight plan has been DEACTIVATED\n");
                SimConnect_FlightPlanLoad(hSimConnect, "");
                // sendText(hSimConnect, "Current Flight Plan has been DEACTIVATED");
                break;
            }
            case EVENT_FLIGHTPLAN_LOAD: { // RIGHT ALT + f
                printf("\n[EVENT_FLIGHTPLAN_LOAD] Flight Plan LAST.PLN has been loaded\n");
                SimConnect_FlightPlanLoad(hSimConnect, "LAST.PLN");
                // sendText(hSimConnect, "Flight Plan LAST.PLN has been loaded");
                break;
            }
            case EVENT_FLIGHTPLAN_DEACTIVATED:
                printf("\n[EVENT_FLIGHTPLAN_DEACTIVATED] FLIGHT PLAN DEACTIVATED\n");
                currentFlightPlan = ""; // Reset the flight plan
                currentFlightPlanPath = ""; // Reset the flight plan Path
                isFlightPlanActive = FALSE;
                fpDisableCount++;
                currentStatus();
                break;
            case EVENT_SIM_START: {
                simStatus(1);
                // See logic inside the function for when to trigger the initial save
                // initialFLTchange();
                break;
            }
            case EVENT_SIM_STOP: {
                simStatus(0);
                break;
            }
            case EVENT_SIM_VIEW: {
                if (evt->dwData == 0) {
                    // printf("\n[EVENT_SIM_VIEW] Entering Main Menu\n");
                }
                else if (evt->dwData == 2) {
                    // printf("\n[EVENT_SIM_VIEW] Exiting Main Menu\n");
                }
                else {
                    // printf("\n[EVENT_SIM_VIEW] Changed views\n");
                }
                break;
            }
            case EVENT_SIM_CRASHED: {
				printf("\n[EVENT_SIM_CRASHED] Aircraft Crashed, will reload your flight on your last SAVE\n");

                hr = SimConnect_FlightLoad(hSimConnect, currentFlightPath.c_str());
                if (hr != S_OK) {
					printf("\nFailed to reload the flight\n");
				}
                else {
					printf("\nFlight has been reloaded\n");
				}

                aircraftCrashed = TRUE;
				break;
			}
            case EVENT_SIM_CRASHRESET: {
				printf("\n[EVENT_SIM_CRASHRESET] Aircraft Crashed and Reset\n");
                aircraftCrashed = TRUE;
                break;
            }
            default:
                printf("Unhandled event ID for SIMCONNECT_RECV_ID_EVENT: %lu\n", evt->uEventID); // Log unhandled request IDs
                break;
            }
            break;
        }
    }

    case SIMCONNECT_RECV_ID_FACILITY_MINIMAL_LIST:
    {
        SIMCONNECT_RECV_FACILITY_MINIMAL_LIST* msg = (SIMCONNECT_RECV_FACILITY_MINIMAL_LIST*)pData;

        printf("Received Facility Minimal List: %lu\n", msg->dwArraySize);
        for (unsigned i = 0; i < msg->dwArraySize; ++i)
        {
            SIMCONNECT_FACILITY_MINIMAL& fm = msg->rgData[i];
            printf("ICAO => Type: %c, Ident: %s, Region: %s, Airport: %s => Lat: %lf, Lat: %lf, Alt: %lf\n", fm.icao.Type, fm.icao.Ident, fm.icao.Region, fm.icao.Airport, fm.lla.Latitude, fm.lla.Longitude, fm.lla.Altitude);
        }

        int randIndex = rand() % msg->dwArraySize;
        break;
    }

    case SIMCONNECT_RECV_ID_EXCEPTION:
    {
        SIMCONNECT_RECV_EXCEPTION* except = (SIMCONNECT_RECV_EXCEPTION*)pData;

        // Check if this is a jetway-related exception
        if (except->dwException == SIMCONNECT_EXCEPTION_JETWAY_DATA) {
            switch (except->dwIndex) {
            case 1:
                printf("Jetway data request failed: Incorrect ICAO or airport not spawned.\n");
                break;
            case 2:
                printf("Jetway data request failed: Invalid parking index.\n");
                break;
            case 99:
                printf("Jetway data request failed: Internal error. Too far from a Jetway perhaps?\n");
                break;
            default:
                printf("Jetway data request failed: Unknown error.\n");
                break;
            }
        }
        else if (except->dwException == SIMCONNECT_EXCEPTION_DATA_ERROR) {
			printf("Exception received for SIMCONNECT_EXCEPTION_DATA_ERROR. Debug here\n");
		}
        else if (except->dwException == SIMCONNECT_EXCEPTION_LOAD_FLIGHTPLAN_FAILED) {
			printf("Exception received for SIMCONNECT_EXCEPTION_LOAD_FLIGHTPLAN_FAILED. Debug here\n");
		}
        else if (except->dwException == SIMCONNECT_EXCEPTION_WEATHER_UNABLE_TO_GET_OBSERVATION) {
            printf("Exception received for SIMCONNECT_EXCEPTION_WEATHER_UNABLE_TO_GET_OBSERVATION. Debug here\n");
        }
        else if (except->dwException == SIMCONNECT_EXCEPTION_WEATHER_UNABLE_TO_CREATE_STATION) {
			printf("Exception received for SIMCONNECT_EXCEPTION_WEATHER_UNABLE_TO_CREATE_STATION. Debug here\n");
		}
        else if (except->dwException == SIMCONNECT_EXCEPTION_WEATHER_UNABLE_TO_REMOVE_STATION) {
			printf("Exception received for SIMCONNECT_EXCEPTION_WEATHER_UNABLE_TO_REMOVE_STATION. Debug here\n");
		}
        else if (except->dwException == SIMCONNECT_EXCEPTION_INVALID_DATA_TYPE) {
            printf("Exception received for SIMCONNECT_EXCEPTION_INVALID_DATA_TYPE. Debug here\n");
        }
        else if (except->dwException == SIMCONNECT_EXCEPTION_INVALID_DATA_SIZE) {
			printf("Exception received for SIMCONNECT_EXCEPTION_INVALID_DATA_SIZE. Debug here\n");
		}
        else if (except->dwException == SIMCONNECT_EXCEPTION_INVALID_ARRAY) {
			printf("Exception received for SIMCONNECT_EXCEPTION_INVALID_ARRAY. Debug here\n");
		}
        else if (except->dwException == SIMCONNECT_EXCEPTION_CREATE_OBJECT_FAILED) {
			printf("Exception received for SIMCONNECT_EXCEPTION_CREATE_OBJECT_FAILED. Debug here\n");
		}
        else if (except->dwException == SIMCONNECT_EXCEPTION_OPERATION_INVALID_FOR_OBJECT_TYPE) {
			printf("Exception received for SIMCONNECT_EXCEPTION_OPERATION_INVALID_FOR_OBJECT_TYPE. Debug here\n");
		}
        else if (except->dwException == SIMCONNECT_EXCEPTION_ILLEGAL_OPERATION) {
			printf("Exception received for SIMCONNECT_EXCEPTION_ILLEGAL_OPERATION. Debug here\n");
		}
        else if (except->dwException == SIMCONNECT_EXCEPTION_ALREADY_SUBSCRIBED) {
			printf("Exception received for SIMCONNECT_EXCEPTION_ALREADY_SUBSCRIBED. Debug here\n");
		}
        else if (except->dwException == SIMCONNECT_EXCEPTION_INVALID_ENUM) {
			printf("Exception received for SIMCONNECT_EXCEPTION_INVALID_ENUM. Debug here\n");
		}
        else if (except->dwException == SIMCONNECT_EXCEPTION_DEFINITION_ERROR) {
			printf("Exception received for SIMCONNECT_EXCEPTION_DEFINITION_ERROR. Debug here\n");
		}
        else if (except->dwException == SIMCONNECT_EXCEPTION_DUPLICATE_ID) {
			printf("Exception received for SIMCONNECT_EXCEPTION_DUPLICATE_ID. Debug here\n");
		}
        else if (except->dwException == SIMCONNECT_EXCEPTION_DATUM_ID) {
			printf("Exception received for SIMCONNECT_EXCEPTION_DATUM_ID. Debug here\n");
		}
        else if (except->dwException == SIMCONNECT_EXCEPTION_OUT_OF_BOUNDS) {
			printf("Exception received for SIMCONNECT_EXCEPTION_OUT_OF_BOUNDS. Debug here\n");
		}
        else if (except->dwException == SIMCONNECT_EXCEPTION_ALREADY_CREATED) {
			printf("Exception received for SIMCONNECT_EXCEPTION_ALREADY_CREATED. Debug here\n");
		}
        else if (except->dwException == SIMCONNECT_EXCEPTION_OBJECT_OUTSIDE_REALITY_BUBBLE) {
			printf("Exception received for SIMCONNECT_EXCEPTION_OBJECT_OUTSIDE_REALITY_BUBBLE. Debug here\n");
		}
        else if (except->dwException == SIMCONNECT_EXCEPTION_OBJECT_CONTAINER) {
            printf("Exception received for SIMCONNECT_EXCEPTION_OBJECT_CONTAINER. Debug here\n");
        }
        else if (except->dwException == SIMCONNECT_EXCEPTION_OBJECT_AI) {
			printf("Exception received for SIMCONNECT_EXCEPTION_OBJECT_AI. Debug here\n");
		}
        else if (except->dwException == SIMCONNECT_EXCEPTION_OBJECT_ATC) {
			printf("Exception received for SIMCONNECT_EXCEPTION_OBJECT_ATC. Debug here\n");
		}
        else if (except->dwException == SIMCONNECT_EXCEPTION_OBJECT_SCHEDULE) {
			printf("Exception received for SIMCONNECT_EXCEPTION_OBJECT_SCHEDULE. Debug here\n");
		}
        else if (except->dwException == SIMCONNECT_EXCEPTION_ACTION_NOT_FOUND) {
			printf("Exception received for SIMCONNECT_EXCEPTION_ACTION_NOT_FOUND. Debug here\n");
		}
        else if (except->dwException == SIMCONNECT_EXCEPTION_NOT_AN_ACTION) {
			printf("Exception received for SIMCONNECT_EXCEPTION_NOT_AN_ACTION. Debug here\n");
		}
        else if (except->dwException == SIMCONNECT_EXCEPTION_INCORRECT_ACTION_PARAMS) {
			printf("Exception received for SIMCONNECT_EXCEPTION_INCORRECT_ACTION_PARAMS. Debug here\n");
		}
        else if (except->dwException == SIMCONNECT_EXCEPTION_GET_INPUT_EVENT_FAILED) {
			printf("Exception received for SIMCONNECT_EXCEPTION_GET_INPUT_EVENT_FAILED. Debug here\n");
		}
        else if (except->dwException == SIMCONNECT_EXCEPTION_SET_INPUT_EVENT_FAILED) {
			printf("Exception received for SIMCONNECT_EXCEPTION_SET_INPUT_EVENT_FAILED. Debug here\n");
		}
        else if (except->dwException == SIMCONNECT_EXCEPTION_TOO_MANY_GROUPS) {
            printf("Exception received for SIMCONNECT_EXCEPTION_TOO_MANY_GROUPS. Debug here\n");
        }
        else if (except->dwException == SIMCONNECT_EXCEPTION_NAME_UNRECOGNIZED) {
			printf("Exception received for SIMCONNECT_EXCEPTION_NAME_UNRECOGNIZED. Debug here\n");
		}
        else if (except->dwException == SIMCONNECT_EXCEPTION_TOO_MANY_EVENT_NAMES) {
			printf("Exception received for SIMCONNECT_EXCEPTION_TOO_MANY_EVENT_NAMES. Debug here\n");
		}
        else if (except->dwException == SIMCONNECT_EXCEPTION_EVENT_ID_DUPLICATE) {
			printf("Exception received for SIMCONNECT_EXCEPTION_EVENT_ID_DUPLICATE. Debug here\n");
		}
        else if (except->dwException == SIMCONNECT_EXCEPTION_TOO_MANY_MAPS) {
			printf("Exception received for SIMCONNECT_EXCEPTION_TOO_MANY_MAPS. Debug here\n");
		}
        else if (except->dwException == SIMCONNECT_EXCEPTION_TOO_MANY_OBJECTS) {
			printf("Exception received for SIMCONNECT_EXCEPTION_TOO_MANY_OBJECTS. Debug here\n");
		}
        else if (except->dwException == SIMCONNECT_EXCEPTION_TOO_MANY_REQUESTS) {
			printf("Exception received for SIMCONNECT_EXCEPTION_TOO_MANY_REQUESTS. Debug here\n");
		}
        else if (except->dwException == SIMCONNECT_EXCEPTION_WEATHER_INVALID_PORT) {
			printf("Exception received for SIMCONNECT_EXCEPTION_WEATHER_INVALID_PORT. Debug here\n");
		}
        else if (except->dwException == SIMCONNECT_EXCEPTION_WEATHER_INVALID_METAR) {
			printf("Exception received for SIMCONNECT_EXCEPTION_WEATHER_INVALID_METAR. Debug here\n");
		}
        else if (except->dwException == SIMCONNECT_EXCEPTION_NONE) {
            printf("Exception received for SIMCONNECT_EXCEPTION_NONE. Debug here\n");
        }
        else if (except->dwException == SIMCONNECT_EXCEPTION_ERROR) {
            printf("Exception received for SIMCONNECT_EXCEPTION_ERROR. Debug here\n");
        }
        else if (except->dwException == SIMCONNECT_EXCEPTION_SIZE_MISMATCH) {
            printf("Exception received for SIMCONNECT_EXCEPTION_SIZE_MISMATCH. Debug here\n");
        }
        else if (except->dwException == SIMCONNECT_EXCEPTION_UNRECOGNIZED_ID) {
            printf("Exception received for SIMCONNECT_EXCEPTION_UNRECOGNIZED_ID. Debug here\n");
        }
        else if (except->dwException == SIMCONNECT_EXCEPTION_UNOPENED) {
            printf("Exception received for SIMCONNECT_EXCEPTION_UNOPENED. Debug here\n");
        }
        else if (except->dwException == SIMCONNECT_EXCEPTION_VERSION_MISMATCH) {
            printf("Exception received for SIMCONNECT_EXCEPTION_VERSION_MISMATCH. Debug here\n");
        }
        else if (except->dwException == SIMCONNECT_EXCEPTION_TOO_MANY_REQUESTS) {
            printf("Exception received for SIMCONNECT_EXCEPTION_TOO_MANY_REQUESTS. Debug here\n");
        }
        else {
            printf("Unknown exception received: %d, SendID: %d, Index: %d (ID is %d)\n", except->dwException, except->dwSendID, except->dwIndex, except->dwID);
        }
        currentStatus();
        break;
    }

    case SIMCONNECT_RECV_ID_QUIT:
    {
        quit = 1;
        break;
    }

    case SIMCONNECT_RECV_ID_OPEN:
    {
        SIMCONNECT_RECV_OPEN* openData = (SIMCONNECT_RECV_OPEN*)pData;
        printf("\n[SIMCONNECT] Connected to Flight Simulator! (%s Version %d.%d - Build %d)\n", openData->szApplicationName, openData->dwApplicationVersionMajor, openData->dwApplicationVersionMinor, openData->dwApplicationBuildMajor);

        // Fix the MSFS bug when a connection is established
        // fixMSFSbug(customFlightmod); 
        // fixMSFSbug(lastMOD);

        // Remove [LocalVars.0] section from LAST.FLT
        // fixLASTflight(lastMOD);

        break;
    }

    default:
        printf("Unhandled data ID: %lu\n", pData->dwID); // Log unhandled data IDs
        break;
    }
}

void sc()
{

    printf("\n[SIMCONNECT] Trying to establish connection with Flight Simulator, please wait...\n");

    while (!quit) {
        if (SUCCEEDED(SimConnect_Open(&hSimConnect, "FSAutoSave", NULL, 0, 0, 0))) {
            break; // Exit the loop if connected
        }
        else {
            printf("Failed to connect to Flight Simulator. Retrying...\n");
            Sleep(1000); // Wait for 1 second before retrying
        }
    }

    initApp();

    if (hSimConnect != NULL) {
        while (0 == quit) {
            SimConnect_CallDispatch(hSimConnect, Dispatcher, NULL);
            Sleep(1);
        }

        hr = SimConnect_Close(hSimConnect);
        printf("[SIMCONNECT] Disconnected from Flight Simulator!\n");
    }
}