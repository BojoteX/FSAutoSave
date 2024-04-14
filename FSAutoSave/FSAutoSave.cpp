// Copyright (c) Jesus "Bojote" Altuve, All rights reserved.
// FSAutoSave - A simple program to automatically save your last flight so you can resume exactly where you left.
// 
// CTRL+ALT+S  =  to save the current flight 
//        ESC  =  to save the current flight.
// ---------------------------------------------------------

#include <Windows.h>
#include <tchar.h>
#include <chrono>
#include <string>
#include <iostream>
#include <algorithm>
#include "SimConnect.h"

HRESULT hr;
HANDLE  hSimConnect = NULL;
int     quit = 0;

// Define a unique filename for the saved flight situation
const char* szFileName = "Missions\\Custom\\CustomFlight\\CustomFlight";
const char* szTitle = "FSAutoSave generated file";
const char* szDescription = "This is a save of your last flight so you can resume exactly where you left.";

// Global variables to hold the current states
std::string currentAircraft;
std::string currentSaveFlight;
std::string currentSaveFlightPath;
std::string currentFlightPlan;
std::string currentFlightPlanPath;
std::string currentFlight;
std::string currentFlightPath;

// Debug flags
std::string flagDEBUG;

// Flags to control the application
bool isOnMenuScreen = FALSE;        // Set TRUE when FLT FILE == MAINMENU.FLT
bool isFlightPlanActive = FALSE;        // FALSE when Flight plan DEACTIVATED, TRUE when ACTIVATED
bool wasReset = FALSE;        // TRUE when the sim was reset using SITUATION_RESET
bool wasSoftPaused = FALSE;        // TRUE when the sim is entered in soft pause state for the player
bool wasFullyPaused = FALSE;        // TRUE when the sim is entered a fully paused state for the player
bool isFinalSave = FALSE;        // TRUE when the save is the last
bool isFirstSave = FALSE;        // TRUE when the save is the first
bool isPauseBeforeStart = FALSE;        // TRUE when the sim is paused before the start
bool userLoadedPLN = FALSE;        // TRUE when the user loads a .PLN file to start a flight

// Realtime flags
DWORD isSimRunning = 0;            // TRUE when the sim is running

#define PAUSE_STATE_FLAG_OFF 0              // No Pause
#define PAUSE_STATE_FLAG_PAUSE 1            // Full Pause
#define PAUSE_STATE_FLAG_PAUSE_WITH_SOUND 2 // FSX Legacy Pause (not used anymore)
#define PAUSE_STATE_FLAG_ACTIVE_PAUSE 4     // Active Pause
#define PAUSE_STATE_FLAG_SIM_PAUSE 8        // Sim Pause (traffic, multi, etc., will still run)

// Input definitions. Used to map a key to a client event
enum INPUT_ID {
    INPUT0,
};

// Group definitions. Used to assign priority to events
enum GROUP_ID {
    GROUP0,
    GROUP1,
};

// Data definitions. Used with SimConnect_AddToDataDefinition and SimConnect_RequestDataOnSimObjectType to request data from the simulator
enum DATA_DEFINE_ID {
    DEFINITION_ZULU_TIME,
};

// Same as above, but used with SimConnect_RequestSystemState to request system state data from the simulator
enum DATA_REQUEST_ID {
    REQUEST_SIM_STATE,
    REQUEST_AIRCRAFT_STATE,
    REQUEST_FLIGHTLOADED_STATE,
    REQUEST_FLIGHTPLAN_STATE,
    REQUEST_ZULU_TIME,
};

// Define the data structure
enum EVENT_ID {
    EVENT_FLIGHT_LOAD,
    EVENT_FLIGHT_SAVED,
    EVENT_SIM_START,
    EVENT_SIM_STOP,
    EVENT_SIM_PAUSED,
    EVENT_SIM_UNPAUSED,
    EVENT_SIM_PAUSE_EX1,
    EVENT_FLIGHTPLAN_DEACTIVATED,
    EVENT_FLIGHTPLAN_ACTIVATED,
    EVENT_FLIGHTPLAN_LOAD,
    EVENT_FLIGHTPLAN_RESET,
    EVENT_AIRCRAFT_LOADED,
    EVENT_ZULU_DAY_SET,
    EVENT_ZULU_HOURS_SET,
    EVENT_ZULU_MINUTES_SET,
    EVENT_ZULU_YEAR_SET,
    EVENT_SITUATION_SAVE,
    EVENT_SITUATION_RESET,
    EVENT_SITUATION_RELOAD,
    EVENT_RECUR_FRAME,
};

struct ZuluTimeData {
    double zuluTime;
};

struct ZuluTime {
    DWORD minute;
    DWORD hour;
    DWORD dayOfYear;
    DWORD year;
};

bool enableANSI() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) {
        return false;
    }

    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) {
        return false;
    }

    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (!SetConsoleMode(hOut, dwMode)) {
        return false;
    }
    return true;
}

ZuluTime getZuluTime() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);

    std::tm now_tm = {};
    gmtime_s(&now_tm, &now_c);

    ZuluTime zuluTime;
    zuluTime.hour = static_cast<DWORD>(now_tm.tm_hour);
    zuluTime.dayOfYear = static_cast<DWORD>(now_tm.tm_yday + 1); // tm_yday is 0 based.
    zuluTime.minute = static_cast<DWORD>(now_tm.tm_min);
    zuluTime.year = static_cast<DWORD>(now_tm.tm_year + 1900);

    return zuluTime;
}

std::string NormalizePath(const std::string& fullPath) {
    std::string result;
    // Identify if the path ends with "aircraft.cfg" in a case-insensitive manner.
    std::string lowerPath = fullPath;
    std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::tolower);
    size_t cfgPos = lowerPath.rfind("\\aircraft.cfg");

    if (cfgPos != std::string::npos) {
        // If the path is for an aircraft, extract the directory name.
        size_t lastSlashPosBeforeCfg = fullPath.rfind("\\", cfgPos - 1);
        if (lastSlashPosBeforeCfg != std::string::npos) {
            result = fullPath.substr(lastSlashPosBeforeCfg + 1, cfgPos - lastSlashPosBeforeCfg - 1);
        }
    }
    else {
        // Otherwise, extract the filename for flight loaded and flight plan paths.
        size_t lastSlashPos = fullPath.rfind("\\");
        if (lastSlashPos != std::string::npos) {
            result = fullPath.substr(lastSlashPos + 1);
        }
    }

    // Convert the extracted segment to uppercase.
    std::transform(result.begin(), result.end(), result.begin(), ::toupper);
    return result;
}

void currentStatus() {
    // Function to return formatted string based on content
    auto formatOutput = [](const std::string& value) -> std::string {
        if (value.empty()) {
            return std::string("\033[90mN/A\033[97m");  // Very light grey for empty, then reset to bright white
        }
        else {
            return std::string("\033[32m") + value + "\033[97m";  // Green for non-empty, then reset to bright white
        }
        };

    // Format strings based on their contents
    std::string aircraftOutput = formatOutput(currentAircraft);
    std::string flightOutput = formatOutput(currentFlight);
    std::string planOutput = formatOutput(currentFlightPlan);

    // Print the output
    printf("[CURRENT STATUS] Aircraft: %s - Flight: %s - Plan: %s\n",
        aircraftOutput.c_str(), flightOutput.c_str(), planOutput.c_str());

    if (userLoadedPLN) {
        std::string cleanPlanOutput = currentFlightPlan.empty() ? "N/A" : currentFlightPlan; // Clean plan output without previous formatting.
        // printf("\033[31m *** [WARNING] userLoadedPLN is set to TRUE as Plan %s is ACTIVE in menus!\n\033[97m", cleanPlanOutput.c_str());
    }

    printf("\n");
}

void sendText(HANDLE hSimConnect, const std::string& text) {
    const char* prefix = "\r\n"; // Line break to prepend
    const char* suffix = "\r\n\n"; // Additional line break to append

    // Calculate total required size, including the null terminator
    size_t totalSize = text.length() + strlen(prefix) + strlen(suffix) + 1;

    // Maximum text length that can be safely included without exceeding buffer size
    size_t maxTextLength = 256 - strlen(prefix) - strlen(suffix) - 1; // -1 for null terminator

    if (totalSize > 256) { // Check if totalSize exceeds buffer capacity
        printf("[WARNING] Text is too long, will be truncated to fit the buffer.\n");
        // Adjust totalSize to exactly fit the buffer size
        totalSize = 256;
    }

    char result[256] = ""; // Initialize buffer and ensure it's clean

    // Safely construct the message within result, ensuring text fits within the calculated maxTextLength
    snprintf(result, sizeof(result), "%s%s%s", prefix, text.substr(0, maxTextLength).c_str(), suffix);

    // Ensure no data loss in conversion
    DWORD messageLength = static_cast<DWORD>(totalSize > UINT_MAX ? UINT_MAX : totalSize);

    // Send the message, now safely within bounds and types
    SimConnect_Text(hSimConnect, SIMCONNECT_TEXT_TYPE_PRINT_WHITE, 5.0, 0, messageLength, (void*)result);
}

void saveNotAllowed() {
    if (currentFlight == "MAINMENU.FLT")
        printf("Not saving situation or setting local ZULU as sim is on menu screen.\n\n");
    else if (isFinalSave)
        printf("Not saving situation or setting local ZULU as we saved already.\n\n");
    else
        printf("Not saving situation or setting local ZULU as user loaded a mission or a non persistent .FLT file.\n\n");
}

void saveAndSetZULU() {
    if ((currentFlight == "LAST.FLT" || currentFlight == "CUSTOMFLIGHT.FLT") && (!isFinalSave)) {
        if (!wasReset) {
            // We set time after aircraft is loaded as we know we are almost ready
            ZuluTime zuluTime = getZuluTime();
            SimConnect_TransmitClientEvent(hSimConnect, SIMCONNECT_OBJECT_ID_USER, EVENT_ZULU_MINUTES_SET, zuluTime.minute, SIMCONNECT_GROUP_PRIORITY_HIGHEST, SIMCONNECT_EVENT_FLAG_GROUPID_IS_PRIORITY);
            SimConnect_TransmitClientEvent(hSimConnect, SIMCONNECT_OBJECT_ID_USER, EVENT_ZULU_HOURS_SET, zuluTime.hour, SIMCONNECT_GROUP_PRIORITY_HIGHEST, SIMCONNECT_EVENT_FLAG_GROUPID_IS_PRIORITY);
            SimConnect_TransmitClientEvent(hSimConnect, SIMCONNECT_OBJECT_ID_USER, EVENT_ZULU_DAY_SET, zuluTime.dayOfYear, SIMCONNECT_GROUP_PRIORITY_HIGHEST, SIMCONNECT_EVENT_FLAG_GROUPID_IS_PRIORITY);
            SimConnect_TransmitClientEvent(hSimConnect, SIMCONNECT_OBJECT_ID_USER, EVENT_ZULU_YEAR_SET, zuluTime.year, SIMCONNECT_GROUP_PRIORITY_HIGHEST, SIMCONNECT_EVENT_FLAG_GROUPID_IS_PRIORITY);
            printf(" *** Setting Local ZULU Time *** \n\n");

            // Save the situation
            SimConnect_TransmitClientEvent(hSimConnect, 0, EVENT_SITUATION_SAVE, 99, SIMCONNECT_GROUP_PRIORITY_HIGHEST, SIMCONNECT_EVENT_FLAG_GROUPID_IS_PRIORITY);
        }
        else {
            printf("[ALERT] Skipping SAVE and local ZULU set as this was a situation reset/reload.\n\n");
        }
    }
    else {
        // Handle cases where saves are not allowed
        saveNotAllowed();
    }
}

void simStatus(bool running) {
    if (running) {
        if (currentFlight == "MAINMENU.FLT" || currentFlight == "") {

            // Magenta for "ON MENU SCREEN"
            printf("[SIM STATE] ********* \033[32m [ RUNNING ] \033[0m ********* -> (IS ON MENU SCREEN) \n");

            // Set the flag to TRUE when the flight plan is activated and if one is loaded by the user
            if (currentFlight == "MAINMENU.FLT" && currentFlightPlan == "LAST.PLN")
                userLoadedPLN = TRUE;

            wasReset = FALSE; // Reset the flag when the sim is reset
            isFinalSave = FALSE; // Reset the flag when the sim is reset

            currentStatus();
        }
        else {
            // Green for "RUNNING"
            printf("[SIM STATE] ********* \033[32m [ RUNNING ] \033[0m *********\n");
            currentStatus();

            // Try to do a save and set ZULU time
            saveAndSetZULU();
        }
    }
    else {
        // Red for "STOPPED"
        printf("[SIM STATE] ********* \033[31m [ STOPPED ] \033[0m *********\n");
        currentStatus();
    }
}

void initApp() {

    // Read early any data I need
    // hr = SimConnect_AddToDataDefinition(hSimConnect, DEFINITION_ZULU_TIME, "ZULU DAY OF YEAR", NULL);
    // hr = SimConnect_RequestDataOnSimObjectType(hSimConnect, REQUEST_ZULU_TIME, DEFINITION_ZULU_TIME, 0, SIMCONNECT_SIMOBJECT_TYPE_USER);

    // Request States (for when the program starts so we know the current state)
    hr = SimConnect_RequestSystemState(hSimConnect, REQUEST_AIRCRAFT_STATE, "AircraftLoaded");      // szString contains the aircraft loaded path
    hr = SimConnect_RequestSystemState(hSimConnect, REQUEST_FLIGHTPLAN_STATE, "FlightPlan");        // szString contains the flight plan path 
    hr = SimConnect_RequestSystemState(hSimConnect, REQUEST_FLIGHTLOADED_STATE, "FlightLoaded");    // szString contains the flight loaded path  
    hr = SimConnect_RequestSystemState(hSimConnect, REQUEST_SIM_STATE, "Sim");                      // What is the current state of the sim?

    // System events (the ones we need to know when they happen) ORDER MATTERS!
    hr = SimConnect_SubscribeToSystemEvent(hSimConnect, EVENT_RECUR_FRAME, "frame");
    hr = SimConnect_SubscribeToSystemEvent(hSimConnect, EVENT_SIM_START, "SimStart");
    hr = SimConnect_SubscribeToSystemEvent(hSimConnect, EVENT_SIM_STOP, "SimStop");
    hr = SimConnect_SubscribeToSystemEvent(hSimConnect, EVENT_AIRCRAFT_LOADED, "AircraftLoaded");
    hr = SimConnect_SubscribeToSystemEvent(hSimConnect, EVENT_FLIGHTPLAN_DEACTIVATED, "FlightPlanDeactivated");
    hr = SimConnect_SubscribeToSystemEvent(hSimConnect, EVENT_FLIGHTPLAN_ACTIVATED, "FlightPlanActivated");
    hr = SimConnect_SubscribeToSystemEvent(hSimConnect, EVENT_FLIGHT_SAVED, "FlightSaved");
    hr = SimConnect_SubscribeToSystemEvent(hSimConnect, EVENT_SIM_PAUSE_EX1, "Pause_EX1");
    hr = SimConnect_SubscribeToSystemEvent(hSimConnect, EVENT_FLIGHT_LOAD, "FlightLoaded");

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

    // Input Events
    hr = SimConnect_MapInputEventToClientEvent_EX1(hSimConnect, INPUT0, "esc", EVENT_SITUATION_SAVE);
    hr = SimConnect_MapInputEventToClientEvent_EX1(hSimConnect, INPUT0, "VK_LCONTROL+VK_LMENU+s", EVENT_SITUATION_SAVE);

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

    // Set priority for the notification group
    hr = SimConnect_SetNotificationGroupPriority(hSimConnect, GROUP0, SIMCONNECT_GROUP_PRIORITY_HIGHEST);
    // hr = SimConnect_SetNotificationGroupPriority(hSimConnect, GROUP1, SIMCONNECT_GROUP_PRIORITY_DEFAULT);

    // Set the input group state
    hr = SimConnect_SetInputGroupState(hSimConnect, INPUT0, SIMCONNECT_STATE_ON);

    // Set the system event state (toggle ON or OFF)
    hr = SimConnect_SetSystemEventState(hSimConnect, EVENT_RECUR_FRAME, SIMCONNECT_STATE_OFF); // Enable it when we need to analyze every frame

    if (FAILED(hr)) {
        printf(" [ERROR] ****************** See code: %lx\n", hr);
    }
}

void CALLBACK Dispatcher(SIMCONNECT_RECV* pData, DWORD cbData, void* pContext)
{
    switch (pData->dwID)
    {

    case SIMCONNECT_RECV_ID_EVENT_FRAME:
    {
        SIMCONNECT_RECV_EVENT_FRAME* evt = (SIMCONNECT_RECV_EVENT_FRAME*)pData;
        switch (evt->uEventID)
        {
        case EVENT_RECUR_FRAME:
            hr = SimConnect_RequestSystemState(hSimConnect, REQUEST_SIM_STATE, "Sim"); // What is the current state of the sim?

            // Below we can get real-time data from the sim changing every frame, use judiciously

            break;
        default:
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
                printf("[SITUATION EVENT] New Flight Loaded: %s\n", currentFlight.c_str());
            else
                printf("[SITUATION EVENT] No flight loaded\n");

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

            if (currentSaveFlight != "")
                printf("[SITUATION EVENT] Flight Saved: %s\n", currentSaveFlight.c_str());
            else
                printf("[SITUATION EVENT] No flight saved\n");

            currentStatus();

            break;
        case EVENT_FLIGHTPLAN_ACTIVATED:
            currentFlightPlan = NormalizePath(evt->szFileName);
            currentFlightPlanPath = evt->szFileName;
            isFlightPlanActive = TRUE;

            if (currentFlightPlan != "") {
                printf("[SITUATION EVENT] New Flight Plan Activated: %s\n", currentFlightPlan.c_str());

                if (currentFlight == "MAINMENU.FLT" && currentFlightPlan == "LAST.PLN")
                    userLoadedPLN = TRUE;
            }
            else {
                printf("[SITUATION EVENT] No flight plan activated\n");
            }

            currentStatus();
            break;
        case EVENT_AIRCRAFT_LOADED:
            currentAircraft = NormalizePath(evt->szFileName);

            if (currentAircraft != "")
                printf("[SITUATION EVENT] New Aircraft Loaded: %s\n", currentAircraft.c_str());
            else
                printf("[SITUATION EVENT] No aircraft loaded\n");

            currentStatus();

            break;
        default:
            break;
        }
        break;
    }

    case SIMCONNECT_RECV_ID_SIMOBJECT_DATA_BYTYPE: {
        SIMCONNECT_RECV_SIMOBJECT_DATA_BYTYPE* pObjData = (SIMCONNECT_RECV_SIMOBJECT_DATA_BYTYPE*)pData;
        switch (pObjData->dwRequestID) {
        case REQUEST_ZULU_TIME: {
            ZuluTimeData* pZuluTime = (ZuluTimeData*)&pObjData->dwData;
            int zuluTimeInt = static_cast<int>(pZuluTime->zuluTime); // Truncate to integer
            printf("[READING DATA] Day of the year: %d\n", zuluTimeInt);
            break;
        }
        default:
            break;
        }
        break;
    }

                                                 // I can receive here either szString, dwInteger, or fFloat it will depend on the data type I requested
    case SIMCONNECT_RECV_ID_SYSTEM_STATE: {
        SIMCONNECT_RECV_SYSTEM_STATE* pState = (SIMCONNECT_RECV_SYSTEM_STATE*)pData;
        switch (pState->dwRequestID) {
        case REQUEST_FLIGHTLOADED_STATE:
            currentFlight = NormalizePath(pState->szString);
            currentFlightPath = pState->szString;

            if (currentFlight != "")
                printf("[CURRENT STATE] Flight currently loaded: %s\n", currentFlight.c_str());
            else
                printf("[CURRENT STATE] No flight currently loaded\n");

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
                printf("[CURRENT STATE] Flight plan currently active: %s\n", currentFlightPlan.c_str());
            else
                printf("[CURRENT STATE] No flight plan currently active\n");

            currentStatus();

            // Set the flag to TRUE when the flight plan is activated and if one is loaded
            if (currentFlightPlan != "")
                isFlightPlanActive = TRUE;

            break;
        case REQUEST_AIRCRAFT_STATE:
            currentAircraft = NormalizePath(pState->szString);

            if (currentAircraft != "")
                printf("[CURRENT STATE] Aircraft currently loaded: %s\n", currentAircraft.c_str());
            else
                printf("[CURRENT STATE] No aircraft currently loaded\n");

            currentStatus();
            break;
        default:
            break;
        }
        break;
    }

    case SIMCONNECT_RECV_ID_EVENT:
    {
        SIMCONNECT_RECV_EVENT* evt = (SIMCONNECT_RECV_EVENT*)pData;
        if (evt->uEventID == EVENT_SIM_PAUSE_EX1) { // Pause events
            switch (evt->dwData) {
            case PAUSE_STATE_FLAG_OFF: {
                if (isPauseBeforeStart) {
                    isPauseBeforeStart = FALSE;

                    printf("[STATUS] Simulator is now READY\n");
                    currentStatus();

                    sendText(hSimConnect, "Press CTRL+ALT+S to save anytime. A save is also triggered automatically when you exit you flight session (by pressing the ESC key)");
                }
                break;
            }
            case PAUSE_STATE_FLAG_PAUSE:
                // printf("[PAUSE EX1] Fully paused\n");
                // currentStatus();
                wasFullyPaused = TRUE;
                break;
            case PAUSE_STATE_FLAG_PAUSE_WITH_SOUND:
                printf("[PAUSE EX1] Legacy NOT USED\n");
                currentStatus();
                // Legacy, might not be used
                break;
            case PAUSE_STATE_FLAG_ACTIVE_PAUSE:
                // printf("[PAUSE EX1] Active Pause enabled\n");
                // currentStatus();
                break;
            case PAUSE_STATE_FLAG_SIM_PAUSE: {
                // printf("[PAUSE EX1] Simulation paused for player, but traffic and multiplayer active.\n");
                // currentStatus();
                wasSoftPaused = TRUE; // Set it so than we simulation starts we know it came from a soft pause

                if (!isFinalSave && !isOnMenuScreen && isFirstSave) {
                    isPauseBeforeStart = TRUE;
                    printf("[STATUS] Simulator is paused before the start\n");
                    currentStatus();
                }

                break;
            }
            default:
                printf("[PAUSE EX1] Mission paused\n");
                currentStatus();
                break;
            }
            break;
        }
        else { // Other events
            switch (evt->uEventID)
            {
            case EVENT_SITUATION_SAVE: { // RIGHT ALT + s

                // Only the following are allowed to be saved
                if (currentFlight == "LAST.FLT" || currentFlight == "CUSTOMFLIGHT.FLT") {
                    // For a save trigged by setZuluAndSave we will pass a value of 99
                    if (evt->dwData == 99) { // INITIAL SAVE
                        isFinalSave = FALSE;
                        isFirstSave = TRUE;
                        printf("[NOTICE] Initial Save... (check confirmation below)\n\n");
                        if (currentFlight == "CUSTOMFLIGHT.FLT" && currentFlightPlan == "CUSTOMFLIGHT.PLN") {
                            if (userLoadedPLN) {
                                printf("User loaded LAST.PLN file to start a flight with ATC active.\n\n");
                                userLoadedPLN = FALSE; // Reset the flag
                            }
                            else {
                                printf("User selected BOTH a Departure and ARRIVAL airport to start a flight but no ATC Flight Plan.\n\n");
                                SimConnect_FlightPlanLoad(hSimConnect, ""); // Deactivate the flight plan before saving
                                SimConnect_FlightSave(hSimConnect, szFileName, "My previous flight", "FSAutoSave Generated File", 0);
                                SimConnect_FlightSave(hSimConnect, "LAST.FLT", "My previous flight", "FSAutoSave Generated File", 0);
                            }
                        }
                        // User is resuming a flight after originally starting a flight with a LAST.PLN file
                        else if (currentFlight == "LAST.FLT" && currentFlightPlan == "CUSTOMFLIGHT.PLN") {
                            printf("User opened LAST.FLT after starting a flight with a LAST.PLN file\n\n");
                            SimConnect_FlightPlanLoad(hSimConnect, ""); // Activate the most current flight plan
                            SimConnect_FlightPlanLoad(hSimConnect, "LAST.PLN"); // Activate the most current flight plan
                            SimConnect_FlightSave(hSimConnect, szFileName, "My previous flight", "FSAutoSave Generated File", 0);
                            SimConnect_FlightSave(hSimConnect, "LAST.FLT", "My previous flight", "FSAutoSave Generated File", 0);
                        }
                        // User has been repeteadly opening LAST.FLT file after starting a flight with a LAST.PLN file (3 or more times)
                        else if (currentFlight == "LAST.FLT" && currentFlightPlan == "LAST.PLN") {
                            userLoadedPLN = FALSE; // Reset the flag again as there is a special case here
                            printf("User has opened LAST.FLT for 3 or mores time after originally starting a flight with a LAST.PLN file\n\n");
                            SimConnect_FlightSave(hSimConnect, "LAST.FLT", "My previous flight", "FSAutoSave Generated File", 0);
                            break;
                        }
                        // For Flights Initiated or Resumed by selecting a DEPARTURE AIRPORT ONLY
                        else if ((currentFlight == "LAST.FLT" || currentFlight == "CUSTOMFLIGHT.FLT") && currentFlightPlan == "") {
                            if (currentFlight == "CUSTOMFLIGHT.FLT") {
                                printf("User selected a DEPARTURE airport and started a flight. ");
                                SimConnect_FlightSave(hSimConnect, szFileName, "My previous flight", "FSAutoSave Generated File", 0);
                                SimConnect_FlightSave(hSimConnect, "LAST.FLT", "My previous flight", "FSAutoSave Generated File", 0);
                            }
                            else if (currentFlight == "LAST.FLT") {
                                printf("User RESUMED a flight with no Flight Plan (where only DEPARTURE and/or ARRIVAL was selected). ");
                            }
                            else {
                                printf("An Unknown situation happened - ERROR CODE: 3\n");
                                break;
                            }
                            printf("No flight plan is loaded or needed\n\n");
                        }
                        else { // Can't think of any other edge case
                            printf("An Unknown situation happened - ERROR CODE: 4\n");
                            break;
                        }
                    }
                    else if (evt->dwData == 0) { // FINAL SAVE (or ESC triggered)
                        printf("[NOTICE] Saving... (check confirmation below)\n\n");
                        isFinalSave = TRUE;
                        isFirstSave = FALSE;

                        // We save BOTH to avoid discrepancies when starting a flight from the menu as CUSTOMFLIGHT.FLT is used 
                        // when selecting DEPARTURE/DESTINATION directly to start a flight. DO NOT CHANGE THIS
                        SimConnect_FlightSave(hSimConnect, szFileName, "My previous flight", "FSAutoSave Generated File", 0);
                        SimConnect_FlightSave(hSimConnect, "LAST.FLT", "My previous flight", "FSAutoSave Generated File", 0);
                    }
                    else { // other values for dwData not implemented yet
                        // Notice that dwData is not 99 or 0, so we will not save the situation, this is to identify how the event was triggered
                        printf("[ALERT] FLIGHT SITUATION WAS NOT SAVED. RECEIVED %d AS dwData\n\n", evt->dwData);
                    }
                }
                else {
                    saveNotAllowed();
                }
                break;
            }
            case EVENT_SITUATION_RELOAD: { // RIGHT CONTROL + RIGHT ALT + r
                printf("[EVENT_SITUATION_RELOAD] Current flight has been reloaded\n");
                currentStatus();
                wasReset = TRUE; // Set the flag so we know the sim was reset (RELOADED)
                SimConnect_FlightLoad(hSimConnect, currentFlightPath.c_str());
                break;
            }
            case EVENT_SITUATION_RESET: { // RIGHT CONTROL + r
                printf("[EVENT_SITUATION_RESET] Current flight has been reset\n");
                wasReset = TRUE; // Set the flag so we know the sim was reset
                currentStatus();
                break;
            }
            case EVENT_FLIGHTPLAN_RESET: { // RIGHT ALT + r
                printf("[EVENT_FLIGHTPLAN_RESET] Current flight plan has been DEACTIVATED\n");
                SimConnect_FlightPlanLoad(hSimConnect, "");
                sendText(hSimConnect, "Current Flight Plan has been DEACTIVATED");
                break;
            }
            case EVENT_FLIGHTPLAN_LOAD: { // RIGHT ALT + f
                printf("[EVENT_FLIGHTPLAN_LOAD] Flight Plan LAST.PLN has been loaded\n");
                SimConnect_FlightPlanLoad(hSimConnect, "LAST.PLN");
                sendText(hSimConnect, "Flight Plan LAST.PLN has been loaded");
                break;
            }
            case EVENT_FLIGHTPLAN_DEACTIVATED:
                printf("[EVENT_FLIGHTPLAN_DEACTIVATED] FLIGHT PLAN DEACTIVATED\n");
                currentFlightPlan = ""; // Reset the flight plan
                currentFlightPlanPath = ""; // Reset the flight plan Path
                isFlightPlanActive = FALSE;

                currentStatus();
                break;
            case EVENT_SIM_START: {
                simStatus(1);
                break;
            }
            case EVENT_SIM_STOP:
                simStatus(0);
                break;
            default:
                break;
            }
            break;
        }
    }

    case SIMCONNECT_RECV_ID_EXCEPTION:
    {
        SIMCONNECT_RECV_EXCEPTION* except = (SIMCONNECT_RECV_EXCEPTION*)pData;
        printf("Exception received: %d, SendID: %d, Index: %d\n", except->dwException, except->dwSendID, except->dwIndex);
        currentStatus();
        break;
    }

    case SIMCONNECT_RECV_ID_QUIT:
    {
        quit = 1;
        break;
    }

    default:
        break;
    }
}

void sc()
{

    printf("\n[SIMCONNECT] Trying to establish connection with Flight Simulator, please wait...\n");

    while (!quit) {
        if (SUCCEEDED(SimConnect_Open(&hSimConnect, "FSAutoSave", NULL, 0, 0, 0))) {
            printf("[SIMCONNECT] Connected to Flight Simulator!\n\n");
            break; // Exit the loop if connected
        }
        else {
            printf("Failed to connect to Flight Simulator. Retrying...\n");
            Sleep(1000); // Wait for 1 second before retrying
        }
    }

    // Set up the data definition
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

int __cdecl _tmain(int argc, _TCHAR* argv[])
{
    bool DEBUG = FALSE;
    bool minimizeOnStart = FALSE;

    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        if (_tcscmp(argv[i], _T("-DEBUG")) == 0) {
            DEBUG = TRUE;
        }
        else if (_tcscmp(argv[i], _T("-minimize")) == 0) {
            minimizeOnStart = TRUE;
        }
    }

    // Minimize the console window if the flag is set
    if (minimizeOnStart) {
        HWND hWnd = GetConsoleWindow();
        if (hWnd != NULL) {
            ShowWindow(hWnd, SW_MINIMIZE);
        }
    }

    if (!enableANSI()) {
        std::cout << "ANSI color is not supported.\n";
    }
    else {

        // Set console output to UTF-8
        SetConsoleOutputCP(CP_UTF8);

        // Ensure the console can handle Unicode characters
        SetConsoleCP(CP_UTF8);

        // Set the default console text color to bright white
        std::cout << "\033[97m";

    }

    // Try to create a named mutex.
    HANDLE hMutex = CreateMutex(NULL, FALSE, _T("Global\\FSAutoSave"));

    // Check if the mutex was created successfully.
    if (hMutex == NULL) {
        printf("[ERROR] Could NOT create the mutex!\n");
        return 1; // Exit program if we cannot create the mutex.
    }

    // Check if the mutex already exists.
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        // Another instance of the program is already running.
        printf("[ERROR] Program is already running!\n");
        CloseHandle(hMutex); // close handles we don't need.
        return 1; // Exit the program.
    }

    // main program.
    sc();

    // Release the mutex when done.
    CloseHandle(hMutex);

    return 0;
}