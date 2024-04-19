// Copyright (c) Jesus "Bojote" Altuve, All rights reserved.
// FSAutoSave - A simple program to automatically save your last flight so you can resume exactly where you left.
// 
// CTRL+ALT+S  =  to save the current flight 
//        ESC  =  to save the current flight.
// ---------------------------------------------------------

#include <windows.h>
#include <thread>
#include <map>
#include <tchar.h>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include "SimConnect.h"

// Define the SimConnect object and other global variables
HRESULT hr;
HANDLE  hSimConnect       = NULL;
bool    DEBUG             = FALSE;
bool    minimizeOnStart   = FALSE;
bool	resetSaves        = FALSE;    
bool	isBUGfixed	      = FALSE;
int     startCounter      = 0;
int     quit              = 0;
int     fpDisableCount    = 0;

// Global flag to indicate whether the application is currently modifying an .FLT file.
std::atomic<bool> isModifyingFile(false);

// Monitor file writes
HANDLE g_hEvent; // Global event handle

// Define a unique marker for deletion operations (Used for fixing MSFS bug when Flight State is set to LANDING_GATE / WAITING in .FLT file)
const std::string DELETE_MARKER         = "!DELETE!";           // Unique marker for deletion operations
const std::string DELETE_SECTION_MARKER = "!DELETE_SECTION!";   // Unique marker for section deletions
const std::string enableAirportLife     = "0";                  // String to enable or disable the Taxi Tug 

// Define the namespace for the filesystem
namespace fs = std::filesystem; // Namespace alias for std::filesystem

// Define a unique filename for the saved flight situation
const char* szFileName      = "Missions\\Custom\\CustomFlight\\CustomFlight";
const char* szTitle         = "FSAutoSave generated file";
const char* szDescription   = "This is a save of your last flight so you can resume exactly where you left.";

// Global variables to hold the current states
std::string currentAircraft;
std::string currentSaveFlight;
std::string currentSaveFlightPath;
std::string currentFlightPlan;
std::string currentFlightPlanPath;
std::string currentFlight;
std::string currentFlightPath;

// MSFS directory & Community path
std::string MSFSPath;
std::string CommunityPath;
std::string pathToMonitor;

// Define the path to the GetFP.exe program to download your MSFS Flight Plan automatically from Simbrief
wchar_t GetFPpath[1024];

// Flags to track application states
bool isOnMenuScreen     = FALSE;            // Set TRUE when FLT FILE == MAINMENU.FLT
bool isFlightPlanActive = FALSE;            // FALSE when Flight plan DEACTIVATED, TRUE when ACTIVATED
bool wasReset           = FALSE;            // TRUE when the sim was reset using SITUATION_RESET
bool wasSoftPaused      = FALSE;            // TRUE when the sim is entered in soft pause state for the player
bool wasFullyPaused     = FALSE;            // TRUE when the sim is entered a fully paused state for the player
bool isFinalSave        = FALSE;            // TRUE when the save is the last
bool isFirstSave        = FALSE;            // TRUE when the save is the first
bool isPauseBeforeStart = FALSE;            // TRUE when the sim is paused before the start
bool userLoadedPLN      = FALSE;            // TRUE when the user loads a .PLN file to start a flight

// Realtime flags
DWORD isSimRunning      = 0;                // TRUE when the sim is running

#define PAUSE_STATE_FLAG_OFF 0              // No Pause
#define PAUSE_STATE_FLAG_PAUSE 1            // Full Pause
#define PAUSE_STATE_FLAG_PAUSE_WITH_SOUND 2 // FSX Legacy Pause (not used anymore)
#define PAUSE_STATE_FLAG_ACTIVE_PAUSE 4     // Active Pause
#define PAUSE_STATE_FLAG_SIM_PAUSE 8        // Sim Pause (traffic, multi, etc., will still run)

// Important for all my data requests types (see below)
struct AircraftPosition {
    double latitude;
    double longitude;
    double altitude;
};

struct SimDayOfYear {
    double dayOfYear;
};

struct CameraState {
    double state;
};

struct ZuluTime {
    DWORD minute;
    DWORD hour;
    DWORD dayOfYear;
    DWORD year;
};

struct SIMCONNECT_RECV_FACILITY_AIRPORT_LIST {
    char ident[6];
    char region[3];
    double  Latitude;
    double  Longitude;
    double  Altitude;
};

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
    DEFINITION_POSITION_DATA,
    DEFINITION_CAMERA_STATE,
};

// Same as above, but used with SimConnect_RequestSystemState to request system state data from the simulator or RequestObjectType to request object data
enum DATA_REQUEST_ID {
    REQUEST_SIM_STATE,
    REQUEST_AIRCRAFT_STATE,
    REQUEST_FLIGHTLOADED_STATE,
    REQUEST_FLIGHTPLAN_STATE,
    REQUEST_ZULU_TIME,
    REQUEST_POSITION,
    REQUEST_DIALOG_STATE,
    REQUEST_CAMERA_STATE,
    REQUEST_AIRPORT_LIST,
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
    EVENT_SIM_VIEW,
    EVENT_SIM_CRASHED,
    EVENT_SIM_CRASHRESET,
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

void SafeCopyPath(const wchar_t* source) {
    errno_t err = wcscpy_s(GetFPpath, _countof(GetFPpath), source);
    if (err != 0) {
        wprintf(L"[ERROR] Could not get GetFP.exe path. Error code: %d\n", err);
    }
    else {
        if (fs::exists(GetFPpath)) {
            wprintf(L"[INFO] Simbrief integration enabled using %s\n", GetFPpath);
        }
        else {
            wprintf(L"[INFO] You tried to enable Simbrief integration with GetFP.exe, but path %s is wrong (file does not exist)\n", GetFPpath);
            std::fill(GetFPpath, GetFPpath + _countof(GetFPpath), L'\0');  // Properly clear the array
        }
    }
}

void getFP() {
    std::wstring programPath = GetFPpath;

    if (wcslen(GetFPpath) > 0) {
        wprintf(L"Downloading Flight Plan from Simbrief using %s\n", programPath.c_str());
    }
    else {
        wprintf(L"For GetFP.exe integration use -SIMBRIEF:\"C:\\PATH_TO_PROGRAM\\GetFP.exe\"\n");
        return;
    }

    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    // Start the child process.
    if (!CreateProcess(
        NULL,           // No module name (use command line)
        (LPWSTR)programPath.c_str(), // Command line
        NULL,           // Process handle not inheritable
        NULL,           // Thread handle not inheritable
        FALSE,          // Set handle inheritance to FALSE
        0,              // No creation flags
        NULL,           // Use parent's environment block
        NULL,           // Use parent's starting directory 
        &si,            // Pointer to STARTUPINFO structure
        &pi)           // Pointer to PROCESS_INFORMATION structure
        ) {
        std::cerr << "Process failed (" << GetLastError() << ").\n";
        return;
    }

    // Wait until child process exits.
    WaitForSingleObject(pi.hProcess, INFINITE);

    // Close process and thread handles.
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
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

// Define a map to hold different sets of files for RESETs which can be ONLY user initiated. Perfectly safe to use.
std::map<std::string, std::vector<std::string>> fileSets = {

    // Core set of files to be deleted
    {"MSFS Situation Files", {
        "Missions\\Custom\\CustomFlight\\CustomFlight.FLT",
        "Missions\\Custom\\CustomFlight\\CustomFlight.PLN",
        "Missions\\Custom\\CustomFlight\\CustomFlight.WX",
        "Missions\\Custom\\CustomFlight\\CustomFlight.SPB",
    }},
    // FSAutoSave generated files
    {"FSAutoSave generated Situation Files", {
        "LAST.FLT",
        "LAST.PLN",
        "LAST.WX",
        "LAST.SPB",
    }},
    // Aircraft Addon files
    {"PMDG 737-800", {
        "Packages\\pmdg-aircraft-738\\work\\PanelState\\CustomFlight.sav",
        "Packages\\pmdg-aircraft-738\\work\\PanelState\\CustomFlight.fmc",     
        "Packages\\pmdg-aircraft-738\\work\\PanelState\\LAST.sav",
        "Packages\\pmdg-aircraft-738\\work\\PanelState\\LAST.fmc",
    }},

    // Additional sets can be added here
};

// Function to delete all files from all sets or simulate the deletion process
void deleteAllSavedSituations() {
    printf("\n[RESET] Resetting all saved situations from %s\n", MSFSPath.c_str());
    for (const auto& pair : fileSets) {
        const std::string& setName = pair.first;
        const auto& files = pair.second;

        printf("\n[RESET] Processing set: %s\n", setName.c_str());
        for (const auto& file : files) {
            fs::path fullPath = fs::path(MSFSPath) / file;
            if (DEBUG) {
                // In DEBUG mode, simulate the file deletion
                if (fs::exists(fullPath)) {
                    printf("[DEBUG] Would delete %s\n", fullPath.string().c_str());
                }
                else {
                    printf("[DEBUG] File does not exist: %s\n", fullPath.string().c_str());
                }
            }
            else {
                // In normal mode, actually delete the file
                if (fs::remove(fullPath)) {
                    printf("[RESET] Successfully removed %s\n", fullPath.string().c_str());
                }
                else {
                    if (fs::exists(fullPath)) {
                        printf("[ERROR] Failed to remove %s\n", fullPath.string().c_str());
                    }
                    else {
                        printf("[INFO] %s does not exist.\n", fullPath.string().c_str());
                    }
                }
            }
        }
    }
}

std::string get_env_variable(const char* env_var) {
    char* buffer = nullptr;
    size_t size = 0;
    errno_t err = _dupenv_s(&buffer, &size, env_var);

    std::string result;
    if (buffer != nullptr) {
        result = buffer;  // Convert C-style string to std::string
        free(buffer);     // Free the dynamically allocated memory
    }
    else {
        printf("%s environment variable not found.\n", env_var);
    }
    return result;
}

bool isMSFSDirectoryWritable(const std::string& directoryPath) {
    if (directoryPath.empty()) {
        return false;
    }

    // Create a test file path
    std::string testFilePath = directoryPath + "/test_file.txt";
    // Try to write to the test file
    std::ofstream testFile(testFilePath);
    if (testFile) {
        testFile << "Testing write permissions.";
        testFile.close(); // Close the file to flush changes

        // Attempt to remove the test file as cleanup
        if (fs::remove(testFilePath)) {
            return true; // Successfully wrote and deleted the test file
        }
        else {
            return false; // Write succeeded but delete failed
        }
    }
    else {
        return false; // Failed to write
    }
}

std::string getMSFSdir() {
    std::string localAppData = get_env_variable("LOCALAPPDATA");
    std::string appData = get_env_variable("APPDATA");

    if (localAppData.empty() || appData.empty()) {
        printf("Required environment variable not found.\n");
        return "";
    }

    // Paths for MS Store and DVD version
    std::string ms_store_dir = localAppData + "\\Packages\\Microsoft.FlightSimulator_8wekyb3d8bbwe\\LocalCache";
    // Path for Steam version
    std::string steam_dir = appData + "\\Microsoft Flight Simulator";

    // Check if the directories exist
    if (fs::exists(ms_store_dir)) {
        return ms_store_dir;
    }
    else if (fs::exists(steam_dir)) {
        return steam_dir;
    }
    else {
        printf("MSFS directory not found.\n");
        return "";
    }
}

std::string getCommunityPath(const std::string& user_cfg_path) {
    if (!fs::exists(user_cfg_path)) {
        printf("File does not exist: %s\n", user_cfg_path.c_str());
        return "";
    }

    std::ifstream file(user_cfg_path);
    if (!file) {
        printf("Failed to open file: %s\n", user_cfg_path.c_str());
        return "";
    }

    std::string line;
    std::string key = "InstalledPackagesPath";
    size_t pos;
    while (getline(file, line)) {
        pos = line.find(key);
        if (pos != std::string::npos) {
            size_t start_quote = line.find('"', pos + key.length());
            if (start_quote != std::string::npos) {
                size_t end_quote = line.find('"', start_quote + 1);
                if (end_quote != std::string::npos) {
                    std::string path = line.substr(start_quote + 1, end_quote - start_quote - 1);
                    return path;
                }
            }
        }
    }
    return "";
}

std::string NormalizePath(const std::string& fullPath) {
    
    // printf("fullpath: %s\n", fullPath.c_str());

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

std::string readConfigFile(const std::string& iniFilePath, const std::string& section, const std::string& key) {
    char buffer[1024];  // Buffer to hold the value read from the INI file

    // Read the value
    DWORD charsRead = GetPrivateProfileStringA(
        section.c_str(),  // Section name
        key.c_str(),      // Key name
        "",               // Default value if the key is not found (return empty string)
        buffer,           // Buffer to store the retrieved string
        sizeof(buffer),   // Size of the buffer
        iniFilePath.c_str()  // Path to the INI file
    );

    if (charsRead > 0) {
        return std::string(buffer);  // Return the value read from the file
    }
    else {
        return "";  // Return empty string if the key was not found or an error occurred
    }
}

std::string modifyConfigFile(const std::string& filePath, const std::map<std::string, std::map<std::string, std::string>>& inputChanges) {
    for (const auto& section : inputChanges) {
        const auto& sectionName = section.first;

        // Check if the entire section should be deleted
        if (section.second.size() == 1 && section.second.count(DELETE_SECTION_MARKER) && section.second.at(DELETE_SECTION_MARKER) == DELETE_MARKER) {
            if (!WritePrivateProfileStringA(sectionName.c_str(), NULL, NULL, filePath.c_str())) {
                std::cout << "Failed to delete section: " << sectionName << std::endl;
                return "";  // If deletion fails, return an empty string immediately
            }
            continue;  // Skip further processing for this section as it has been deleted
        }

        // Process keys for deletion or modification if the section is not marked for complete deletion
        for (const auto& key : section.second) {
            if (key.second == DELETE_MARKER) {
                if (!WritePrivateProfileStringA(sectionName.c_str(), key.first.c_str(), NULL, filePath.c_str())) {
                    std::cout << "Failed to delete key: " << key.first << " in section: " << sectionName << std::endl;
                    return "";  // If deletion fails, return an empty string immediately
                }
            }
            else {
                // Write or modify the key
                if (!WritePrivateProfileStringA(sectionName.c_str(), key.first.c_str(), key.second.c_str(), filePath.c_str())) {
                    std::cout << "Failed to write key: " << key.first << " in section: " << sectionName << std::endl;
                    return "";  // If writing fails, return an empty string
                }
            }
        }
    }

    return filePath;  // Return the file path if all operations are successful
}

std::string wideToNarrow(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(sizeNeeded, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], sizeNeeded, NULL, NULL);
    return strTo;
}

void fixMSFSbug(const std::string& filePath) {
    // Check if the last flight state is set to LANDING_GATE or WAITING and FIX it. Also we change PREFLIGHT_TAXI to PREFLIGHT_GATE for consistency 

    std::string MODfile = filePath;
    std::string ffSTATE;

    ffSTATE = readConfigFile(MODfile, "FreeFlight", "FirstFlightState");
    if (!ffSTATE.empty()) {
        if (ffSTATE == "LANDING_GATE" || ffSTATE == "WAITING") {
            std::map<std::string, std::map<std::string, std::string>> fixState = {
                {"FreeFlight", {{"FirstFlightState", "PREFLIGHT_GATE"}} } // Change the FirstFlightState to PREFLIGHT_GATE to avoid the MSFS bug/crash
            };
            std::string applyFIX = modifyConfigFile(MODfile, fixState);
            MODfile = NormalizePath(MODfile);
            if (!applyFIX.empty()) {
                printf("\n[FIX] Replacing %s with %s in %s\n", ffSTATE.c_str(), "PREFLIGHT_GATE", MODfile.c_str());
                isBUGfixed = TRUE;
            }
            else {
				printf("\n[ERROR] ********* [ %s READ OK, BUT FAILED TO FIX BUG ] *********\n", MODfile.c_str());
			}
        }
        return;
    }
    MODfile = NormalizePath(MODfile);
    printf("\n[ERROR] ********* [ FAILED TO READ %s ] *********\n", MODfile.c_str());
}

void finalFLTchange() {
    // We don't need a counter here as finalFLTchange is only called after the final save

    std::string customFlightmod = MSFSPath + "\\" + szFileName + ".FLT";
    std::string lastMOD = MSFSPath + "\\LAST.FLT";
    
    // Define or compute your variable
    std::string missionLocation = "$$: Miami"; // Use LAT/LON format to use a specific location
    std::string dynamicTitle = "Resume your flight";
    std::string dynamicBrief = "Welcome back! ready to resume your flight?";

    std::string ffSTATE1 = readConfigFile(lastMOD, "FreeFlight", "FirstFlightState");
    std::string ffSTATE2 = readConfigFile(customFlightmod, "FreeFlight", "FirstFlightState");

    // This is just for consistency as we want to start from the gate if you are resuming a flight from the actual gate
    if (ffSTATE1 == "PREFLIGHT_PUSHBACK" || ffSTATE1 == "") {
        ffSTATE1 = "PREFLIGHT_GATE";
    }
    if (ffSTATE2 == "PREFLIGHT_PUSHBACK" || ffSTATE2 == "") {
        ffSTATE2 = "PREFLIGHT_GATE";
    }
        
    // Create a map of changes if the bug was fixed (we also delete the entire Departure and Arrival sections as your flight has been completed)
    std::map<std::string, std::map<std::string, std::string>> finalsave = {
        {"Departure", {{"!DELETE_SECTION!", "!DELETE!"}}},  // Used to DELETE entire section. 
        {"Arrival", {{"!DELETE_SECTION!", "!DELETE!"}}},    // Used to DELETE entire section. 
        {"LivingWorld", {{"AirportLife", enableAirportLife}}},
        {"Main", {
            {"Title", dynamicTitle },
            {"MissionLocation", missionLocation},
        }},
        {"Briefing", {
            {"BriefingText", dynamicBrief },
        }},
    };

    // Create a map of changes if the bug was not fixed (or not present) for the LAST.FLT file
    std::map<std::string, std::map<std::string, std::string>> finalsave1 = {
        {"LivingWorld", {{"AirportLife", enableAirportLife}}},
        {"FreeFlight", {{"FirstFlightState", ffSTATE1}}},
        {"Main", {
            {"Title", dynamicTitle },
        }},
        {"Briefing", {
            {"BriefingText", dynamicBrief },
        }},
    };

    // Create a map of changes if the bug was not fixed (or not present) for the CUSTOMFLIGHT.FLT file
    std::map<std::string, std::map<std::string, std::string>> finalsave2 = {
        {"LivingWorld", {{"AirportLife", enableAirportLife}}},
        {"FreeFlight", {{"FirstFlightState", ffSTATE2}}},
        {"Main", {
            {"Title", dynamicTitle },
        }},
        {"Briefing", {
            {"BriefingText", dynamicBrief },
        }},
    };

    // Fix the MSFS bug where the FirstFlightState is set to LANDING_GATE or WAITING in LAST.FLT
    fixMSFSbug(lastMOD);
    if (isBUGfixed && isFinalSave) {
        isBUGfixed = FALSE; // Reset the flag
        lastMOD = modifyConfigFile(lastMOD, finalsave);
    }
    else {
		lastMOD = modifyConfigFile(lastMOD, finalsave1);
	}

    // Fix the MSFS bug where the FirstFlightState is set to LANDING_GATE or WAITING in CUSTOMFLIGHT.FLT
    fixMSFSbug(customFlightmod);
    if (isBUGfixed && isFinalSave) {
        isBUGfixed = FALSE; // Reset the flag
        customFlightmod = modifyConfigFile(customFlightmod, finalsave);
    }
    else {
        customFlightmod = modifyConfigFile(customFlightmod, finalsave2);
    }

    lastMOD = NormalizePath(lastMOD);
    if (!lastMOD.empty())
        printf("\n[FLIGHT SITUATION] ********* \033[35m [ UPDATED %s ] \033[0m *********\n", lastMOD.c_str());
    else
        printf("\n[ERROR] ********* \033[31m [ %s UPDATE FAILED ] \033[0m *********\n", lastMOD.c_str());

    customFlightmod = NormalizePath(customFlightmod);
    if (!customFlightmod.empty())
        printf("\n[FLIGHT SITUATION] ********* \033[35m [ UPDATED %s ] \033[0m *********\n", customFlightmod.c_str());
    else
        printf("\n[ERROR] ********* \033[31m [ %s UPDATE FAILED ] \033[0m *********\n", customFlightmod.c_str());
}

void initialFLTchange() { // We just wrap the finalFLTchange() function here as we only need to call it once
    // We use the counter to track how many times the sim engine is running while on the menu screen
    if (currentFlight == "MAINMENU.FLT" || currentFlight == "") {
        if (startCounter == 0) { // Only modify the file once, the first time the sim engine is running while on the menu screen
            finalFLTchange();
        }
        startCounter++;
    }
}

void saveNotAllowed() {
    if (currentFlight == "MAINMENU.FLT") {
        printf("\n[INFO]Not saving situation or setting local ZULU as sim is on menu screen.\n");
    }
    else if (isFinalSave) {
        printf("\n[INFO] Not saving situation or setting local ZULU as we saved already.\n");

        // Reset the counter as we are done and going back to the menu
        startCounter = 0;

        // MODIFY the .FLT file to set the FirstFlightState to PREFLIGHT_GATE but only do it for the final save and when flight is LAST.FLT
        if (currentFlight == "LAST.FLT" || currentFlight == "CUSTOMFLIGHT.FLT") {
            // Make all neccessary changes to the .FLT files
            finalFLTchange();
        }
    }
    else {
        printf("\n[INFO] Not saving situation or setting local ZULU as user loaded a mission or a non persistent .FLT file.\n");
    }
}

void saveAndSetZULU() {
    if ((currentFlight == "LAST.FLT" || currentFlight == "CUSTOMFLIGHT.FLT") && (!isFinalSave)) {
        if (!wasReset) {

            // We set time after aircraft is loaded as we know we are almost ready
            ZuluTime zuluTime = getZuluTime();

            if (!DEBUG) {
                SimConnect_TransmitClientEvent(hSimConnect, SIMCONNECT_OBJECT_ID_USER, EVENT_ZULU_MINUTES_SET, zuluTime.minute, SIMCONNECT_GROUP_PRIORITY_HIGHEST, SIMCONNECT_EVENT_FLAG_GROUPID_IS_PRIORITY);
                SimConnect_TransmitClientEvent(hSimConnect, SIMCONNECT_OBJECT_ID_USER, EVENT_ZULU_HOURS_SET, zuluTime.hour, SIMCONNECT_GROUP_PRIORITY_HIGHEST, SIMCONNECT_EVENT_FLAG_GROUPID_IS_PRIORITY);
                SimConnect_TransmitClientEvent(hSimConnect, SIMCONNECT_OBJECT_ID_USER, EVENT_ZULU_DAY_SET, zuluTime.dayOfYear, SIMCONNECT_GROUP_PRIORITY_HIGHEST, SIMCONNECT_EVENT_FLAG_GROUPID_IS_PRIORITY);
                SimConnect_TransmitClientEvent(hSimConnect, SIMCONNECT_OBJECT_ID_USER, EVENT_ZULU_YEAR_SET, zuluTime.year, SIMCONNECT_GROUP_PRIORITY_HIGHEST, SIMCONNECT_EVENT_FLAG_GROUPID_IS_PRIORITY);

                printf("\n[INFO] *** Setting Local ZULU Time *** \n");
            }
            else {
                printf("\n[DEBUG] Will skip setting ZULU time as we are in DEBUG mode\n");
            }

            // Save the situation
            SimConnect_TransmitClientEvent(hSimConnect, 0, EVENT_SITUATION_SAVE, 99, SIMCONNECT_GROUP_PRIORITY_HIGHEST, SIMCONNECT_EVENT_FLAG_GROUPID_IS_PRIORITY);
        }
        else {
            if (!DEBUG) {
                printf("\n[ALERT] Skipping SAVE and local ZULU set as this was a situation reset/reload.\n");
            }
            else {
				printf("\n[DEBUG] Will skip saving as we are in DEBUG mode (we were not saving any way, as this was a reset/reload)\n");
			}
        }
    }
    else {
        // Handle cases where saves are not allowed
        if (!DEBUG) {
            saveNotAllowed();
        }
        else {
			printf("\n[DEBUG] Will skip saving as we are in DEBUG mode (we were not saving here any way as the logic/flow prevented that here) \n");
		}
    }
}

void simStatus(bool running) {
    if (running) {
        if (currentFlight == "MAINMENU.FLT" || currentFlight == "") {

            // Green for "ON MENU SCREEN"
            printf("\n[SIM STATE] ********* \033[32m [ RUNNING ] \033[0m ********* -> (IS ON MENU SCREEN) \n");

            // Try to obtain Airports around me
            hr = SimConnect_RequestFacilitiesList(hSimConnect, SIMCONNECT_FACILITY_LIST_TYPE_AIRPORT, REQUEST_AIRPORT_LIST);

            // Set the flag to TRUE when the flight plan is activated and if one is loaded by the user
            if (currentFlight == "MAINMENU.FLT" && currentFlightPlan == "LAST.PLN")
                userLoadedPLN = TRUE;

            wasReset = FALSE; // Reset the flag when the sim is reset
            isFinalSave = FALSE; // Reset the flag when the sim is reset
        }
        else {
            // Green for "RUNNING"
            printf("\n[SIM STATE] ********* \033[32m [ RUNNING ] \033[0m *********\n");

            // Try to do a save and set ZULU time
            saveAndSetZULU();
        }
    }
    else {
        // Red for "STOPPED"
        printf("\n[SIM STATE] ********* \033[31m [ STOPPED ] \033[0m *********\n");
    }
}

// Its not an actual save, but a way to key track of what was loaded (e.g what type of flight)
void firstSave() {
    isFinalSave = FALSE;
    isFirstSave = TRUE;
    printf("\n[NOTICE] Starting flight... \n");
    if (currentFlight == "CUSTOMFLIGHT.FLT" && currentFlightPlan == "CUSTOMFLIGHT.PLN") {
        if (userLoadedPLN) {
            printf("\n[INFO] User loaded LAST.PLN file to start a flight with MSFS ATC active.\n");
            userLoadedPLN = FALSE; // Reset the flag
        }
        else {
            printf("\n[INFO] User selected BOTH a Departure and ARRIVAL airport to start a flight but no ATC Flight Plan.\n");
                SimConnect_FlightPlanLoad(hSimConnect, ""); // Deactivate the flight plan before saving
        }
    }
    // User is resuming a flight after originally starting a flight with a LAST.PLN file
    else if (currentFlight == "LAST.FLT" && currentFlightPlan == "CUSTOMFLIGHT.PLN") {
        printf("\n[INFO] User opened LAST.FLT after starting a flight with a LAST.PLN file. MSFS ATC will be active\n");
        SimConnect_FlightPlanLoad(hSimConnect, ""); // Activate the most current flight plan        
        SimConnect_FlightPlanLoad(hSimConnect, "LAST.PLN"); // Activate the most current flight plan
    }
    // User has been repeteadly opening LAST.FLT file after starting a flight with a LAST.PLN file (3 or more times)
    else if (currentFlight == "LAST.FLT" && currentFlightPlan == "LAST.PLN") {
        userLoadedPLN = FALSE; // Reset the flag again as there is a special case here
        printf("\n[INFO] User has opened LAST.FLT 3 or mores time after originally starting a flight with a LAST.PLN file.\nMSFS ATC will be active using LAST.PLN as your Flight Plan\n");
    }
    // For Flights Initiated or Resumed by selecting a DEPARTURE AIRPORT ONLY
    else if ((currentFlight == "LAST.FLT" || currentFlight == "CUSTOMFLIGHT.FLT") && currentFlightPlan == "") {
        if (currentFlight == "CUSTOMFLIGHT.FLT") {
            printf("\n[INFO] User selected a DEPARTURE airport and started a flight. No flight plan is loaded or needed\n");
        }
        else if (currentFlight == "LAST.FLT") {
            printf("\n[INFO] User RESUMED a flight with no Flight Plan and only DEPARTURE or DEPARTURE + ARRIVAL selected.\nNo flight plan is loaded or needed\n");
        }
        else {
            printf("An Unknown situation happened - ERROR CODE: 3\n"); // This is a random ERROR CODE just for tracking edge cases
        }
    }
    else { // Can't think of any other edge case
        printf("An Unknown situation happened - ERROR CODE: 4\n"); // This is a random ERROR CODE just for tracking edge cases
    }
}

void finalSave() {
    printf("\n[NOTICE] Saving... (check confirmation below)\n");
    isFinalSave = TRUE;
    isFirstSave = FALSE;

    // We save BOTH to avoid discrepancies when starting a flight from the menu as CUSTOMFLIGHT.FLT is used 
    // when selecting DEPARTURE/DESTINATION directly to start a flight. DO NOT CHANGE THIS

    if(!DEBUG) {        
        SimConnect_FlightSave(hSimConnect, szFileName, "My previous flight", "FSAutoSave Generated File", 0);
        SimConnect_FlightSave(hSimConnect, "LAST.FLT", "My previous flight", "FSAutoSave Generated File", 0);
	}
    else {
        printf("\n[DEBUG] Will skip saving as we are in DEBUG mode\n");
    }
}

void fixCustomFlight() {

    // If user loads a CustomFlight.FLT we assume he/she wants to start a flight from the GATE
    std::string narrowFile = "CustomFlight.FLT";
    std::string customFlightfile = pathToMonitor + "\\" + narrowFile;

    // Read the current setting from the file so we can set the one that corresponds to the actual state
    std::string gateSTATE = readConfigFile(customFlightfile, "FreeFlight", "FirstFlightState");
    if (gateSTATE.empty()) {
		gateSTATE = "PREFLIGHT_GATE"; // Set the default value
	}
    else if (gateSTATE == "LANDING_GATE" || gateSTATE == "WAITING") {
		gateSTATE = "PREFLIGHT_GATE"; // Set the default value
	}
    else if (gateSTATE == "PREFLIGHT_PUSHBACK") {
        gateSTATE = "PREFLIGHT_GATE"; // Set the default value
    }

    std::map<std::string, std::map<std::string, std::string>> fixState = {{
             {"LivingWorld", {{"AirportLife", enableAirportLife}}},
			 {"FreeFlight", {{"FirstFlightState", gateSTATE}}},
    }};

    std::string ffSTATEprev = readConfigFile(customFlightfile, "LivingWorld", "AirportLife");
    if (!ffSTATEprev.empty()) { // Here we handle the case where the key is found in the file
        // Before we modify the file, we check if the key is already set to the desired value
        if (ffSTATEprev != enableAirportLife) { // Here we handle the case where the key is found in the file but needs to be updated
            std::string ffSTATE = modifyConfigFile(customFlightfile, fixState);
            if (!ffSTATE.empty()) {
                printf("[INFO] File %s was *UPDATED*. Setting now is AirportLife=%s\n", narrowFile.c_str(), enableAirportLife.c_str());
            }
            else {
                printf("[ERROR] Could NOT update %s. Most likely file was in use when trying to modify it\n", narrowFile.c_str());
            }
        }
    }
    else { // Here we handle the case where the key is not found in the file
        std::string ffSTATE = modifyConfigFile(customFlightfile, fixState);
        if (!ffSTATE.empty()) {
            printf("[INFO] File %s now has a *NEW* setting added. AirportLife=%s\n", narrowFile.c_str(), enableAirportLife.c_str());
        }
        else {
            printf("[ERROR] Could NOT update %s. Most likely file was in use when trying to modify it\n", narrowFile.c_str());
        }
    }
}

int monitorCustomFlightChanges() {
    std::wstring widepathToMonitor(pathToMonitor.begin(), pathToMonitor.end());
    LPCWSTR directoryPath = widepathToMonitor.c_str();

    HANDLE hDir = CreateFile(directoryPath, FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);

    if (hDir == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to open directory for monitoring: " << GetLastError() << std::endl;
        return 1;
    }

    char buffer[1024];
    DWORD bytesReturned;
    while (TRUE) {
        if (ReadDirectoryChangesW(hDir, buffer, sizeof(buffer), FALSE, FILE_NOTIFY_CHANGE_LAST_WRITE, &bytesReturned, NULL, NULL)) {
            FILE_NOTIFY_INFORMATION* pNotify = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(buffer);
            do {

                std::wstring changedFileName(pNotify->FileName, pNotify->FileNameLength / sizeof(WCHAR));
                std::string narrowFile = wideToNarrow(changedFileName);

                // We can have different logic for different files here. This one is just for CustomFlight.FLT in the MSFSPathtoMonitor
                if (narrowFile == "CustomFlight.FLT") {
                    // Modify CustomFlight.FLT file to fix MSFS bug
                    fixCustomFlight();
                }

                pNotify = pNotify->NextEntryOffset ? reinterpret_cast<FILE_NOTIFY_INFORMATION*>((BYTE*)pNotify + pNotify->NextEntryOffset) : NULL;
            } while (pNotify != NULL);
        }
        else {
            std::cerr << "Failed to read directory changes: " << GetLastError() << std::endl;
            break; // Exit the loop on failure
        }
    }

    CloseHandle(hDir); // Ensure the directory handle is closed properly
    return 0;
}

void initApp() {

    // Run monitoring for CustomFlight.FLT file writes in a separate thread.
    std::thread fileMonitorThread(monitorCustomFlightChanges);
    fileMonitorThread.detach();  // Detach the thread to run independently

    // My Data Definitions (for data I might need from Simvars)
    hr = SimConnect_AddToDataDefinition(hSimConnect, DEFINITION_POSITION_DATA, "Plane Latitude", "degrees");
    hr = SimConnect_AddToDataDefinition(hSimConnect, DEFINITION_POSITION_DATA, "Plane Longitude", "degrees");
    hr = SimConnect_AddToDataDefinition(hSimConnect, DEFINITION_CAMERA_STATE, "CAMERA STATE", "number");
    hr = SimConnect_AddToDataDefinition(hSimConnect, DEFINITION_ZULU_TIME, "ZULU DAY OF YEAR", "number");

    // One request for the user aircraft position polls every second, the other request for the user aircraft position polls only once
    // hr = SimConnect_RequestDataOnSimObject(hSimConnect, REQUEST_POSITION, DEFINITION_POSITION_DATA, SIMCONNECT_OBJECT_ID_USER, SIMCONNECT_PERIOD_SECOND, SIMCONNECT_DATA_REQUEST_FLAG_CHANGED);
    hr = SimConnect_RequestDataOnSimObject(hSimConnect, REQUEST_POSITION, DEFINITION_POSITION_DATA, SIMCONNECT_OBJECT_ID_USER, SIMCONNECT_PERIOD_ONCE, SIMCONNECT_DATA_REQUEST_FLAG_DEFAULT);

    // Request data on specific Simvars (e.g. ZULU time or CAMERA STATE)
    hr = SimConnect_RequestDataOnSimObject(hSimConnect, REQUEST_CAMERA_STATE, DEFINITION_CAMERA_STATE, SIMCONNECT_OBJECT_ID_USER, SIMCONNECT_PERIOD_SECOND, SIMCONNECT_DATA_REQUEST_FLAG_CHANGED);

    // Read early any data I need (this is just a sample for ZULU time) 
    // hr = SimConnect_RequestDataOnSimObject(hSimConnect, REQUEST_ZULU_TIME, DEFINITION_ZULU_TIME, SIMCONNECT_OBJECT_ID_USER, SIMCONNECT_PERIOD_ONCE, SIMCONNECT_DATA_REQUEST_FLAG_DEFAULT);
    
    // hr = SimConnect_RequestDataOnSimObjectType(hSimConnect, REQUEST_TYPE_AIRPORTS, DEFINITION_TYPE_AIRPORTS, 50000, SIMCONNECT_SIMOBJECT_TYPE_AIRPORT);
    // hr = SimConnect_RequestDataOnSimObjectType(hSimConnect, REQUEST_TYPE_AIRPORTS, DEFINITION_TYPE_AIRPORTS, 0, SIMCONNECT_SIMOBJECT_TYPE_AIRPORT);

    

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
    // hr = SimConnect_SetSystemEventState(hSimConnect, EVENT_RECUR_FRAME, SIMCONNECT_STATE_ON); // Enable it when we need to analyze every frame
}

void CALLBACK Dispatcher(SIMCONNECT_RECV* pData, DWORD cbData, void* pContext)
{
    if(DEBUG)
        printf("Received callback with data size: %lu bytes\n", cbData); // General data size

    switch (pData->dwID)
    {


        /*

            case SIMCONNECT_FACILITY_LIST_TYPE_AIRPORT: // Corrected to the right identifier
    {
        printf("Facilities list received 2\n");

    case REQUEST_AIRPORT_LIST: {
        printf("Facilities list received\n");
        break;
    }

        // Assuming we get a facilities list back, not just a single airport
        SIMCONNECT_RECV_FACILITIES_LIST* pFacilities = (SIMCONNECT_RECV_FACILITIES_LIST*)pData;

        // This would usually be a loop to process multiple airports if pData contains an array
        for (unsigned int i = 0; i < pFacilities->dwArraySize; ++i) {
            SIMCONNECT_DATA_FACILITY_AIRPORT* pAirport = ((SIMCONNECT_DATA_FACILITY_AIRPORT*)&pFacilities->dwRequestID) + i;
            printf("Airport: Lat: %f, Long: %f\n", pAirport->Latitude, pAirport->Longitude);
        }
        break;
    }

        */


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
            if (pCS->state == 12) {
                std::thread(getFP).detach(); // Spawn a new thread to get the flight plan so we don't block the main thread. It will be cleaned up automatically
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
                printf("Our current position is Latitude: %f - Longitude: %f\n", pS->latitude, pS->longitude);
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
            if(DEBUG)
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

            // Not really needed as SimStart/SimStop are the same
            // hr = SimConnect_RequestSystemState(hSimConnect, REQUEST_SIM_STATE, "Sim"); // What is the current state of the sim?

            // Bugged. Its clearly not working
            // hr = SimConnect_RequestSystemState(hSimConnect, REQUEST_DIALOG_STATE, "DialogMode"); // What is the current state of the sim?

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

            if (currentSaveFlight != "") {
                printf("\n[SITUATION EVENT] Flight Saved: %s\n", currentSaveFlight.c_str());
            }
            else {
                printf("\n[SITUATION EVENT] No flight saved\n");
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

                    printf("\n[INFO] Before this activation (%d) DEACTIVATIONS ocurred. Will reset counter.\n", fpDisableCount);
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
            break;
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
                if (!isFinalSave && !isOnMenuScreen && isFirstSave) {
                    isPauseBeforeStart = TRUE;
                    printf("\n[STATUS] Simulator is in Briefing screen before start (Press READY TO FLY)\n");
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
            case EVENT_SITUATION_SAVE: { // CTRL+ALT+S or ESC triggered - Also for the automatic initial save (to set local ZULU time)

                // Only the following Flights are allowed to be saved
                if (currentFlight == "LAST.FLT" || currentFlight == "CUSTOMFLIGHT.FLT") {

                    if (evt->dwData == 99) { // INITIAL SAVE - Saves triggered by setZuluAndSave (we pass 99 as custom value)
                        firstSave();
                    }
                    else if (evt->dwData == 0) { // NORMAL SAVE (CTRL+ALT+S or ESC triggered)
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
                SimConnect_FlightLoad(hSimConnect, currentFlightPath.c_str());
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
                initialFLTchange();
                break;
            }
            case EVENT_SIM_STOP: {
                simStatus(0);
                break;
            }
            case EVENT_SIM_VIEW: {
                if (evt->dwData == 0) {
                    printf("\n[EVENT_SIM_VIEW] Entering Main Menu\n");
                }
                else if (evt->dwData == 2) {
                    printf("\n[EVENT_SIM_VIEW] Exiting Main Menu\n");
                }
                else {
                    printf("\n[EVENT_SIM_VIEW] Changed views\n");
                }
                break;
            }
            case EVENT_SIM_CRASHED: {
				printf("\n[EVENT_SIM_CRASHED] Aircraft Crashed\n");
				break;
			}
            case EVENT_SIM_CRASHRESET: {
				printf("\n[EVENT_SIM_CRASHRESET] Aircraft Crashed and Reset\n");
                break;
            }
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

    case SIMCONNECT_RECV_ID_OPEN:
    {
        SIMCONNECT_RECV_OPEN* openData = (SIMCONNECT_RECV_OPEN*)pData;
        printf("\n[SIMCONNECT] Connected to Flight Simulator! (%s Version %d.%d - Build %d)\n", openData->szApplicationName, openData->dwApplicationVersionMajor, openData->dwApplicationVersionMinor, openData->dwApplicationBuildMajor);
        break;
    }

    default:
        if(DEBUG)
            printf("Unhandled data ID: %lu\n", pData->dwID); // Log unhandled data IDs
        break;
    }
}

void waitForEnter() {
    printf("Press ENTER to exit...");
    char buffer[10];  // Larger buffer to accommodate Enter key and extra characters if needed
    do {
        fgets(buffer, sizeof(buffer), stdin);
    } while (buffer[0] != '\n'); // Check directly for newline character
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

    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        if (_tcscmp(argv[i], _T("-DEBUG")) == 0) {
            DEBUG = TRUE;
            printf("[INFO]  *** DEBUG MODE IS ON *** \n");
        }
        if (_tcscmp(argv[i], _T("-SILENT")) == 0) {
            minimizeOnStart = TRUE;
        }
        if (_tcscmp(argv[i], _T("-RESET")) == 0) {
            resetSaves = TRUE;
        }
        if (wcsncmp(argv[i], L"-SIMBRIEF:", 10) == 0) {
            SafeCopyPath(argv[i] + 10);  // Skip the "-SIMBRIEF:" (10 chars) part and copy the rest (the path) to the global variable GetFPpath
        }
    }

    MSFSPath = getMSFSdir();
    if (!MSFSPath.empty()) {     
        if (!isMSFSDirectoryWritable(MSFSPath)) {
            MSFSPath = "";
            printf("[INFO] MSFS is in a read-only directory. Program will not work, exiting.\n");
            waitForEnter();  // Ensure user presses Enter
            return 0;
        }
    }

    if (!MSFSPath.empty()) {  
		printf("[INFO] MSFS is Installed locally.\n");
        CommunityPath = getCommunityPath(MSFSPath + "\\UserCfg.opt");  // Assign directly to the global variable 
        pathToMonitor = MSFSPath + "\\Missions\\Custom\\CustomFlight"; // Path to monitor CustomFlight.FLT changes done by MSFS

        if (!CommunityPath.empty()) {
			printf("[INFO] Your MSFS Community Path is located at %s\n", CommunityPath.c_str());
		}
        else {
			printf("[ERROR] Your UserCfg.opt is corrupted. A Community Path could not be found. Will now exit\n");
            waitForEnter();  // Ensure user presses Enter
            return 0;
		}

        // Check if the user wants to reset the saved situations. We call the function to RESET the saves and then exit the program.
        if (resetSaves) {
            deleteAllSavedSituations();
            waitForEnter();  // Ensure user presses Enter
            return 0;
        }
	}
    else {
        // Check if the user wants to reset the saved situations. We call the function to RESET the saves and then exit the program.
        if (resetSaves) {
            printf("[RESET] To RESET saved situations run this program where MSFS is installed\n");
            waitForEnter();  // Ensure user presses Enter
            return 0;
        }
        else {
            printf("[INFO] MSFS NOT Installed locally, FSAutoSave will NOT run over the network. Will now exit\n");
            waitForEnter();  // Ensure user presses Enter
            return 0;
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
        printf("ANSI color is not supported.\n");
    }
    else {

        // Set console output to UTF-8
        SetConsoleOutputCP(CP_UTF8);

        // Ensure the console can handle Unicode characters
        SetConsoleCP(CP_UTF8);

        // Set the default console text color to bright white
        printf("\033[97m");

    }

    // Try to create a named mutex.
    HANDLE hMutex = CreateMutex(NULL, FALSE, _T("Global\\FSAutoSave"));

    // Check if the mutex was created successfully.
    if (hMutex == NULL) {
        printf("[ERROR] Could NOT create the mutex! Will now exit.\n");
        waitForEnter();  // Ensure user presses Enter
        return 0;
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