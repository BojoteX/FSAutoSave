#include <Windows.h>
#include <regex>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include "SimConnect.h"
#include "Globals.h"
#include "Utility.h"

namespace fs = std::filesystem;

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

std::string WideCharToUTF8(const wchar_t* wideChars) {
    if (!wideChars) return ""; // Safety check

    // Calculate the required buffer size for the target multi-byte string
    int bufferSize = WideCharToMultiByte(CP_UTF8, 0, wideChars, -1, nullptr, 0, nullptr, nullptr);
    if (bufferSize == 0) {
        // Handle the error, possibly with GetLastError()
        return "";
    }

    std::string utf8String(bufferSize - 1, 0); // Allocate string with required buffer size minus null terminator
    WideCharToMultiByte(CP_UTF8, 0, wideChars, -1, &utf8String[0], bufferSize, nullptr, nullptr);

    return utf8String;
}

void copyFile(const std::string& source, const std::string& destination) {
    std::ifstream src(source, std::ios::binary);
    std::ofstream dest(destination, std::ios::binary);
    dest << src.rdbuf();
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
        wprintf(L"\nDownloading Flight Plan from Simbrief using %s\n", programPath.c_str());
    }
    else {
        // wprintf(L"\nEnable integration with Simbrief using GetFP from Github. To activate download GetFP and then use -SIMBRIEF:\"C:\\PATH_TO_PROGRAM\\GetFP.exe\"\n");
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

    // FSAutoSave generated files
    {"FSAutoSave generated Situation Files", {
        "LAST.FLT",
        "LAST.PLN",
        "LAST.WX",
        "LAST.SPB",
    }},
    // Additional sets can be added here
};

// Function to delete all files from all sets or simulate the deletion process
void deleteAllSavedSituations() {

    printf("\n[RESET] Deleting your LAST flight files from %s\n", localStatePath.c_str());
    for (const auto& pair : fileSets) {
        const std::string& setName = pair.first;
        const auto& files = pair.second;

        printf("\n[RESET] Processing set: %s\n", setName.c_str());
        for (const auto& file : files) {
            fs::path fullPath = fs::path(localStatePath) / file;
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

    std::string fspath;
    // Check if the directories exist
    if (fs::exists(ms_store_dir)) {
        fspath = ms_store_dir;
        isMSStore = true;
        // Regular expression for the pattern to be searched
        std::regex pattern("LocalCache");
        localStatePath = std::regex_replace(fspath, pattern, "LocalState");
    }
    else if (fs::exists(steam_dir)) {
        fspath = steam_dir;
        isSteam = true;
        localStatePath = fspath;
    }
    else {
        printf("MSFS directory not found.\n");
        return "";
    }

	// Make sure only one version is installed
	if (isSteam && isMSStore) {
		printf("Both MS Store and Steam versions of MSFS are installed. Please uninstall one of them.\n");
		return "";
	}

    // Set lastMOD and customFlightmod based on the installation type above
    customFlightmod = localStatePath + "\\" + szFileName + ".FLT";
    lastMOD         = localStatePath + "\\LAST.FLT";

    return fspath;
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

std::string wideToNarrow(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(sizeNeeded, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], sizeNeeded, NULL, NULL);
    return strTo;
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

    if (!DEBUG) {
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
    }
    else {
        printf("\n[DEBUG] ********* [ %s READ OK, NO modifications were made as we are in DEBUG mode ] *********\n", filePath.c_str());
        return "";
    }
    return filePath;  // Return the file path if all operations are successful
}

void fixLASTflight(const std::string& filePath) {
    // Removes the LocalVars section entirely 

    // Regex pattern
    std::regex pattern("PMDG 7\\d{2}-\\d{3}\\w*");

    // Check condition
    if (std::regex_match(currentAircraft, pattern)) {
        std::string MODfile = filePath;
        if (!DEBUG) {
            std::map<std::string, std::map<std::string, std::string>> fixLAST = {
                {"LocalVars.0", {{"!DELETE_SECTION!", "!DELETE!"}}},    // Used to DELETE entire section. 
            };
            std::string applyFIX = modifyConfigFile(MODfile, fixLAST);
            MODfile = NormalizePath(MODfile);
            if (!applyFIX.empty()) {
                printf("\n[FIX] Removed [LocalVars.0] section from LAST.FLT\n");
            }
            else {
                printf("\n[ERROR] ********* [ %s READ OK, BUT FAILED TO FIX BUG ] *********\n", MODfile.c_str());
            }
            return;
        }
        else {
            printf("\n[DEBUG] ********* [ %s READ OK - NO modifications were made as we are in DEBUG mode ] *********\n", MODfile.c_str());
        }

        MODfile = NormalizePath(MODfile);
        printf("\n[ERROR] ********* [ FAILED TO READ %s ] *********\n", MODfile.c_str());

    }
    else {
		printf("\n[INFO] Skipping [LocalVars.0] removal from LAST.FLT for %s aircraft\n", currentAircraft.c_str());
	}
}

void fixMSFSbug(const std::string& filePath) {
    // Check if the last flight state is set to LANDING_TAXI or LANDING_GATE and FIX it. Also we change PREFLIGHT_TAXI to firstFlightState* for consistency 

    std::string MODfile = filePath;
    std::string ffSTATE;

    ffSTATE = readConfigFile(MODfile, "FreeFlight", "FirstFlightState");
    if (!DEBUG) {
        if (ffSTATE == "LANDING_TAXI" || ffSTATE == "LANDING_GATE" || ffSTATE == "PREFLIGHT_PUSHBACK" || ffSTATE.empty() ) {
            std::map<std::string, std::map<std::string, std::string>> fixState;

            if (ffSTATE == "PREFLIGHT_PUSHBACK") { // If the FirstFlightState is set to PREFLIGHT_PUSHBACK
                fixState = {
                    {"FreeFlight", {{"FirstFlightState", firstFlightState}} }, // Change the FirstFlightState to firstFlightState* to avoid the MSFS bug/crash
                };
            }
            else if (ffSTATE == "LANDING_TAXI" || ffSTATE == "LANDING_GATE") { // If the FirstFlightState is set to LANDING_TAXI or LANDING_GATE
                fixState = {
                    {"Arrival", {{"!DELETE_SECTION!", "!DELETE!"}}},    // Used to DELETE entire section. 
                    {"FreeFlight", {{"FirstFlightState", firstFlightState}} }, // Change the FirstFlightState to firstFlightState* to avoid the MSFS bug/crash
                    {"Main", {{"OriginalFlight", ""}} },
                };
                fixLASTflight(MODfile); // Remove the LocalVars section entirely but only if ffSTATE is LANDING_TAXI or LANDING_GATE
            }
            else { // If the FirstFlightState is empty
                fixState = {
					{"FreeFlight", {{"FirstFlightState", firstFlightState}} }, // Change the FirstFlightState to firstFlightState* to avoid the MSFS bug/crash
				};
			}

            std::string applyFIX = modifyConfigFile(MODfile, fixState);
            MODfile = NormalizePath(MODfile);
            if (!applyFIX.empty()) {

                if (ffSTATE.empty()) {
                    printf("\n[FIX] FirstFlightState was empty, setting it to %s in %s\n", firstFlightState.c_str(), MODfile.c_str());
                }
                else {
                    printf("\n[FIX] Replacing %s with %s in %s\n", ffSTATE.c_str(), firstFlightState.c_str(), MODfile.c_str());
                }

                if (MODfile == "LAST.FLT" && isFinalSave) {
                    isBUGfixed = TRUE;
                    // printf("Setting isBUGfixed to TRUE\n");
				}
				else if (MODfile == "CUSTOMFLIGHT.FLT" && isFinalSave) {
					isBUGfixedCustom = TRUE;
                    // printf("Setting isBUGfixedCustom to TRUE\n");
				}
                else {
					// printf("NOT setting isBUGfixed or isBUGfixedCustom, this was fixed in the World Map\n");
				}
            }
            else {
                printf("\n[ERROR] ********* [ %s READ OK, BUT FAILED TO FIX BUG ] *********\n", MODfile.c_str());
            }
        }
        return;
    }
    else {
        printf("\n[DEBUG] ********* [ %s READ OK, FirstFlightState: %s - NO modifications were made as we are in DEBUG mode ] *********\n", MODfile.c_str(), ffSTATE.c_str());
    }
    MODfile = NormalizePath(MODfile);
    printf("\n[ERROR] ********* [ FAILED TO READ %s ] *********\n", MODfile.c_str());
}

std::string formatDuration(int totalSeconds) {
    int hours = totalSeconds / 3600; // Calculate total hours
    int minutes = (totalSeconds % 3600) / 60; // Calculate remaining minutes
    int seconds = totalSeconds % 60; // Calculate remaining seconds

    std::stringstream ss;
    if (hours > 0) {
        ss << hours << " hour" << (hours > 1 ? "s" : "");
    }
    if (minutes > 0) {
        if (ss.tellp() > 0) ss << ", "; // Add a comma if there's already something in ss
        ss << minutes << " minute" << (minutes > 1 ? "s" : "");
    }
    if (seconds > 0 || totalSeconds == 0) { // Include seconds if it's the only value or not zero
        if (ss.tellp() > 0) ss << ", "; // Add a comma if there's already something in ss
        ss << seconds << " second" << (seconds > 1 ? "s" : "");
    }

    return ss.str();
}

void finalFLTchange() {
    // This will ALSO execute on the first run of the program to set the initial state of the .FLT files or when exiting a flight, so check for MAINMENU.FLT or empty string 
    // if you want to skip any of the conditions below 

    std::string local_customFlightmod;
    std::string local_lastMOD;

    std::string flightVersion = readConfigFile(lastMOD, "Main", "FlightVersion"); // Autoincremented version of the flight
    if (flightVersion.empty() || flightVersion == "0") {
		flightVersion = "1";
	}

    std::string elapsedTimeLeg = readConfigFile(lastMOD, "SimScheduler", "SimTime"); // Elapsed time in seconds (String)
    std::string aircraftSignature = readConfigFile(lastMOD, "Sim.0", "Sim"); // PMDG 737 - 800 American Airlines (N666JA)

    elapsedTimeLeg = formatDuration(std::stoi(elapsedTimeLeg));

    std::string dynamicTitle = "Resume your flight";
    std::string description = "Welcome back! ready to resume your flight?";

    std::map<std::string, std::map<std::string, std::string>> finalsave;
    std::map<std::string, std::map<std::string, std::string>> finalsave1;
    std::map<std::string, std::map<std::string, std::string>> finalsave2;

    // Define or compute your variable
    std::string dynamicBrief = "Welcome back! ready to resume your " + aircraftSignature + " flight? Currently " + elapsedTimeLeg + " of flight time since your original flight.";
    std::string missionLocation;
    std::string isSimOnGround = readConfigFile(lastMOD, "SimVars.0", "SimOnGround"); // True or False (String)

    std::string ZVelBodyAxis = readConfigFile(lastMOD, "SimVars.0", "ZVelBodyAxis"); // Double represented as String
    if (!isSimOnGround.empty()) {
        if (isSimOnGround == "False") {
            // Adjust IAS
            std::ostringstream stream;
            stream << std::fixed << std::setprecision(17) << myIASinFPS;
            ZVelBodyAxis = stream.str();
        }
	}

    if (!airportName.empty()) {
        if (isSimOnGround == "True") {
            // printf("You are at %s (%s) | %s %u (Suffix is %s)\n", airportName.c_str(), airportICAO.c_str(), parkingGate.c_str(), parkingNumber, parkingGateSuffix.c_str());
            missionLocation = airportName;
        }
        else {
            // printf("You are currently flying near %s (%s)\n", airportName.c_str(), airportICAO.c_str());
            missionLocation = "Enroute, close to " + airportName;
            dynamicBrief =  "You are enroute, close to " + airportName + ". Ready to resume your flight?";
        }
    }

    // Create a map of changes if the bug was fixed (we also delete the entire Departure and Arrival sections as your flight has been completed)

        //               THIS MAP IS FOR BOTH LAST.FLT AND CUSTOMFLIGHT.FLT FILES WHEN ENDING A FLIGHT              //

    finalsave = {
        {"Departure", {
            {"ICAO", airportICAO },
			{"GateName", parkingGate },
			{"GateNumber", std::to_string(parkingNumber) },
			{"GateSuffix", parkingGateSuffix }
        }},
        {"Arrival", {{"!DELETE_SECTION!", "!DELETE!"}}},    // Used to DELETE entire section. 
        {"LivingWorld", {
            {"AirportLife", enableAirportLife },
        }},
        {"ResourcePath", {
            {"Path", "Missions\\Asobo\\FreeFlights\\FreeFlight\\FreeFlight" },
        }},
        {"ObjectFile", {
            {"File", "Missions\\Asobo\\FreeFlights\\FreeFlight\\FreeFlight" },
        }},
        {"Weather", {
            {"UseLiveWeather", "True" },
            {"WeatherCanBeLive", "True" },
            {"UseWeatherFile", "False" },
            {"WeatherPresetFile", "" },
        }},
        {"Main", {
            {"Title", dynamicTitle },
            {"Description", description },
            {"MissionLocation", missionLocation },
            {"AppVersion", "10.0.61355" },
            {"FlightVersion", std::to_string(std::stoi(flightVersion) + 1) },
            {"OriginalFlight", "" },
            {"FlightType", "SAVE" },
        }},
        {"SimVars.0", {
           {"ZVelBodyAxis", ZVelBodyAxis },
        }},
        {"Briefing", {
            {"BriefingText", dynamicBrief },
        }},
    };

    // Create a map of changes if the bug was not fixed (or not present) for the LAST.FLT file

            //               THIS MAP IS FOR LAST.FLT FOR A REGULAR SAVE              //

    finalsave1 = {
        {"LivingWorld", {{"AirportLife", enableAirportLife}}},
        {"Main", {
            {"Title", dynamicTitle },
            {"MissionLocation", missionLocation },
            {"Description", description },
            {"AppVersion", "10.0.61355" },
            {"FlightVersion", std::to_string(std::stoi(flightVersion) + 1) },
            {"FlightType", "SAVE" },
        }},
        {"SimVars.0", {
           {"ZVelBodyAxis", ZVelBodyAxis },
        }},
        {"ResourcePath", {
            {"Path", "Missions\\Asobo\\FreeFlights\\FreeFlight\\FreeFlight" },
        }},
        {"ObjectFile", {
            {"File", "Missions\\Asobo\\FreeFlights\\FreeFlight\\FreeFlight" },
        }},
        {"Weather", {
            {"UseLiveWeather", "True" },
            {"WeatherCanBeLive", "True" },
            {"UseWeatherFile", "False" },
            {"WeatherPresetFile", "" },
        }},
        {"Briefing", {
            {"BriefingText", dynamicBrief },
        }},
    };

    // Create a map of changes if the bug was not fixed (or not present) for the CUSTOMFLIGHT.FLT file

                //               THIS MAP IS FOR CUSTOMFLIGHT.FLT FOR A REGULAR SAVE              //

    finalsave2 = { 
        {"LivingWorld", {{"AirportLife", enableAirportLife}}},
        {"Main", {
            {"Title", dynamicTitle },
            {"MissionLocation", missionLocation },
            {"Description", description },
            {"AppVersion", "10.0.61355" },
            {"FlightVersion", std::to_string(std::stoi(flightVersion) + 1) },
            {"FlightType", "SAVE" },
        }},
        {"SimVars.0", {
           {"ZVelBodyAxis", ZVelBodyAxis },
        }},
        {"ResourcePath", {
            {"Path", "Missions\\Asobo\\FreeFlights\\FreeFlight\\FreeFlight" },
        }},
        {"ObjectFile", {
            {"File", "Missions\\Asobo\\FreeFlights\\FreeFlight\\FreeFlight" },
        }},
        {"Weather", {
            {"UseLiveWeather", "True" },
            {"WeatherCanBeLive", "True" },
            {"UseWeatherFile", "False" },
            {"WeatherPresetFile", "" },
        }},
        {"Briefing", {
            {"BriefingText", dynamicBrief },
        }},
    };

    // Fix the MSFS bug where the FirstFlightState is set to LANDING_TAXI or LANDING_GATE in LAST.FLT
    fixMSFSbug(lastMOD);

    // Remove [LocalVars.0] section from LAST.FLT
    // fixLASTflight(lastMOD);

    if (isBUGfixed && isFinalSave) {
        isBUGfixed = FALSE; // Reset the flag
        local_lastMOD = modifyConfigFile(lastMOD, finalsave);
    }
    else {
        local_lastMOD = modifyConfigFile(lastMOD, finalsave1);
    }

    // Fix the MSFS bug where the FirstFlightState is set to LANDING_TAXI or LANDING_GATE in CUSTOMFLIGHT.FLT
    fixMSFSbug(customFlightmod);
    if (isBUGfixedCustom && isFinalSave) {
        isBUGfixedCustom = FALSE; // Reset the flag
        local_customFlightmod = modifyConfigFile(customFlightmod, finalsave);
    }
    else {
        local_customFlightmod = modifyConfigFile(customFlightmod, finalsave2);
    }

    local_lastMOD = NormalizePath(lastMOD);
    if (!local_lastMOD.empty())
        printf("\n[FLIGHT SITUATION] ********* \033[35m [ UPDATED %s ] \033[0m *********\n", local_lastMOD.c_str());
    else
        printf("\n[ERROR] ********* \033[31m [ %s UPDATE FAILED ] \033[0m *********\n", local_lastMOD.c_str());

    local_customFlightmod = NormalizePath(customFlightmod);
    if (!local_customFlightmod.empty())
        printf("\n[FLIGHT SITUATION] ********* \033[35m [ UPDATED %s ] \033[0m *********\n", local_customFlightmod.c_str());
    else
        printf("\n[ERROR] ********* \033[31m [ %s UPDATE FAILED ] \033[0m *********\n", local_customFlightmod.c_str());

    // Reset the variables as we are done with them
    airportICAO.clear();        // Reset the airport name
    airportName.clear();        // Reset the airport name
    parkingGate.clear();        // Reset the parking gate
    parkingGateSuffix.clear();  // Reset the parking suffix
    parkingNumber = 0;          // Reset the parking number
}

void initialFLTchange() { // We just wrap the finalFLTchange() function here as we only need to call it once
    // We use the counter to track how many times the sim engine is running while on the menu screen
    if (currentFlight == "MAINMENU.FLT" || currentFlight == "") {
        if (startCounter == 0) { // Only modify the file once, the first time the sim engine is running while on the menu screen

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

        // MODIFY the .FLT file to set the FirstFlightState to firstFlightState* but only do it for the final save and when flight is LAST.FLT
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
    
    // printf("\n[INFO] *** Attempting to save situation and set local ZULU time *** \n");

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
            // printf("\n[INFO] *** executing saveNotAllowed() *** \n");
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
            printf("\n[SIM STATE] ********* \033[32m [ RUNNING ] \033[0m ********* -> (ON MENU SCREEN) \n");

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

// Function to calculate the clock position based on current bearing and heading
int calculateClockPosition(double bearing, double heading) {
    // Calculate the relative angle
    double relativeAngle = bearing - heading;

    // Normalize the angle to be within the range of 0 to 360 degrees
    relativeAngle = fmod(relativeAngle + 360.0, 360.0);

    // Calculate the clock position
    int clockPosition = static_cast<int>(round(relativeAngle / 30.0)) % 12;

    // Adjust 0 o'clock to 12 o'clock
    if (clockPosition == 0) {
        clockPosition = 12;
    }

    return clockPosition;
}

double metersToFeet(double meters) {
    const double metersToFeetConversionFactor = 3.28084;
    return meters * metersToFeetConversionFactor;
}

DistanceAndBearing calculateDistanceAndBearing(double lat1, double lon1, double lat2, double lon2) {
    const double R = 6371000; // Earth's radius in meters
    const double PI = 3.14159265358979323846;

    double latRad1 = lat1 * (PI / 180);
    double latRad2 = lat2 * (PI / 180);
    double deltaLat = (lat2 - lat1) * (PI / 180);
    double deltaLon = (lon2 - lon1) * (PI / 180);

    // Calculate the distance
    double a = sin(deltaLat / 2) * sin(deltaLat / 2) +
        cos(latRad1) * cos(latRad2) * sin(deltaLon / 2) * sin(deltaLon / 2);
    double c = 2 * atan2(sqrt(a), sqrt(1 - a));
    double distance = R * c; // Distance in meters

    // Calculate the bearing
    double y = sin(deltaLon) * cos(latRad2);
    double x = cos(latRad1) * sin(latRad2) - sin(latRad1) * cos(latRad2) * cos(deltaLon);
    double bearingRadians = atan2(y, x);
    double bearingDegrees = fmod((bearingRadians * 180 / PI + 360), 360); // Convert to degrees and normalize

    // Return both distance and bearing
    DistanceAndBearing result;
    result.distance = distance;
    result.bearing = bearingDegrees;
    return result;
}

void handleGroundOperations(const char* airportIdent) {
    hr = SimConnect_RequestJetwayData(hSimConnect, airportIdent, 0, nullptr);
    if (hr != S_OK) {
        printf("Failed to request jetway data\n");
    }
}

// We save LAST.FLT to force some features to work properly
void firstSave() {

    isFinalSave = FALSE;
    isFirstSave = TRUE;

    printf("\n[NOTICE] Starting flight... \n");
    if (currentFlight == "CUSTOMFLIGHT.FLT" && currentFlightPlan == "CUSTOMFLIGHT.PLN") {
        if (userLoadedPLN) {
            printf("\n[INFO] User loaded LAST.PLN file to start a NEW flight with MSFS ATC active.\n");
            userLoadedPLN = FALSE; // Reset the flag
        }
        else {
            printf("\n[INFO] User selected BOTH a Departure and ARRIVAL airport to start a NEW flight with no MSFS ATC Flight Plan.\n");
            if (!DEBUG) {
                SimConnect_FlightPlanLoad(hSimConnect, ""); // Deactivate the flight plan before saving
            }
            else {
                printf("\n[DEBUG] Will skip deactivating any flight plans (if active) as we are in DEBUG mode\n");
            }
        }
    }
    // User is resuming a flight after originally starting a flight with a LAST.PLN file
    else if (currentFlight == "LAST.FLT" && currentFlightPlan == "CUSTOMFLIGHT.PLN") {
        printf("\n[INFO] User opened LAST.FLT to RESUME a flight originally started loading the LAST.PLN file.\nMSFS ATC will be active using the current LAST.PLN file as your active Flight Plan\n");
        if (!DEBUG) {
            SimConnect_FlightPlanLoad(hSimConnect, ""); // Activate the most current flight plan        
            SimConnect_FlightPlanLoad(hSimConnect, "LAST.PLN"); // Activate the most current flight plan
        }
        else {
            printf("\n[DEBUG] Will skip saving LAST.FLT and Loading the flightplan LAST.PLN as we are in DEBUG mode\n");
        }
    }
    // User has been repeteadly opening LAST.FLT file after starting a flight with a LAST.PLN file (3 or more times)
    else if (currentFlight == "LAST.FLT" && currentFlightPlan == "LAST.PLN") {
        userLoadedPLN = FALSE; // Reset the flag again as there is a special case here
        printf("\n[INFO] User has opened LAST.FLT (for 3 or more legs) to RESUME a flight originally started loading the LAST.PLN file.\nMSFS ATC will be active using the current LAST.PLN file as your active Flight Plan\n");
        if (!DEBUG) {
            // printf("\n[INFO] Initiating first SAVE...\n");
        }
        else {
            printf("\n[DEBUG] Will skip saving LAST.FLT as we are in DEBUG mode\n");
        }
    }
    // For Flights Initiated or Resumed by selecting a DEPARTURE AIRPORT ONLY
    else if ((currentFlight == "LAST.FLT" || currentFlight == "CUSTOMFLIGHT.FLT") && currentFlightPlan == "") {
        if (currentFlight == "CUSTOMFLIGHT.FLT") {
            printf("\n[INFO] User selected a DEPARTURE airport to start a NEW flight. No flight plan is loaded or needed\n");
        }
        else if (currentFlight == "LAST.FLT") {
            printf("\n[INFO] User RESUMED a flight with only DEPARTURE or DEPARTURE + ARRIVAL selected.\nNo flight plan is loaded or needed\n");
            if (!DEBUG) {
                // printf("\n[INFO] Initiating first SAVE...\n");
            }
            else {
                printf("\n[DEBUG] Will skip saving LAST.FLT as we are in DEBUG mode\n");
            }
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
    // printf("\n[NOTICE] Saving... (check confirmation below)\n");
    isFinalSave = TRUE;
    isFirstSave = FALSE;

    // We ONLY save LAST.FLT as CustomFlight.FLT is used to start only FRESH flights 
    if (!DEBUG) {
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
        gateSTATE = firstFlightState; // Set the default value
    }
    else if (gateSTATE == "LANDING_TAXI" || gateSTATE == "LANDING_GATE") {
        gateSTATE = firstFlightState; // Set the default value
    }
    else if (gateSTATE == "PREFLIGHT_PUSHBACK") {
        gateSTATE = firstFlightState; // Set the default value
    }

    std::map<std::string, std::map<std::string, std::string>> fixState = { {
             {"Assistance", {{"!DELETE_SECTION!", "!DELETE!"}}},    // Used to DELETE entire section.
             {"LivingWorld", {{"AirportLife", enableAirportLife }}},
             {"FreeFlight", {{"FirstFlightState", gateSTATE }}},
             {"SimScheduler", {{"SimTime", "1.0" }}},
    }};

    std::string ffSTATEprev = readConfigFile(customFlightfile, "LivingWorld", "AirportLife");
    if (!ffSTATEprev.empty()) { // Here we handle the case where the key is found in the file
        // Before we modify the file, we check if the key is already set to the desired value
        if (ffSTATEprev != enableAirportLife) { // Here we handle the case where the key is found in the file but needs to be updated
            std::string ffSTATE = modifyConfigFile(customFlightfile, fixState);
            if (!ffSTATE.empty()) {
                // printf("[INFO] File %s was *UPDATED*. Setting now is AirportLife=%s\n", narrowFile.c_str(), enableAirportLife.c_str());
            }
            else {
                printf("[ERROR] Could NOT update %s. Most likely file was in use when trying to modify it\n", narrowFile.c_str());
            }
        }
    }
    else { // Here we handle the case where the key is not found in the file
        std::string ffSTATE = modifyConfigFile(customFlightfile, fixState);
        if (!ffSTATE.empty()) {
            // printf("[INFO] File %s now has a *NEW* setting added. AirportLife=%s\n", narrowFile.c_str(), enableAirportLife.c_str());
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
                else {
					// printf("\n[INFO] File %s changed but no action taken\n", narrowFile.c_str());
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

GateInfo formatGateName(int name) {
    GateInfo result;

    if (name >= 12 && name <= 37) {
        // Calculate the gate letter, 'A' is 12, so subtract 12 from name and add to ASCII value of 'A'
        char gateLetter = 'A' + (name - 12);
        result.gateString = "GATE_" + std::string(1, gateLetter);
        result.friendlyName = "GATE " + std::string(1, gateLetter);
    }
    else {
        // Handle other cases if necessary
        switch (name) {
        case 0: result.friendlyName = result.gateString = "NONE"; break;
        case 1: result.friendlyName = result.gateString = "PARKING"; break;
        case 2: result.friendlyName = result.gateString = "N_PARKING"; break;
        case 3: result.friendlyName = result.gateString = "NE_PARKING"; break;
        case 4: result.friendlyName = result.gateString = "E_PARKING"; break;
        case 5: result.friendlyName = result.gateString = "SE_PARKING"; break;
        case 6: result.friendlyName = result.gateString = "S_PARKING"; break;
        case 7: result.friendlyName = result.gateString = "SW_PARKING"; break;
        case 8: result.friendlyName = result.gateString = "W_PARKING"; break;
        case 9: result.friendlyName = result.gateString = "NW_PARKING"; break;
        case 10: result.friendlyName = result.gateString = "GATE"; break;
        case 11: result.friendlyName = result.gateString = "DOCK"; break;
        default: result.friendlyName = result.gateString = "UNKNOWN"; // Or handle this case differently if needed
        }
    }

    return result;
}

void waitForEnter() {
    printf("Press ENTER to exit...");
    char buffer[10];  // Larger buffer to accommodate Enter key and extra characters if needed
    do {
        fgets(buffer, sizeof(buffer), stdin);
    } while (buffer[0] != '\n'); // Check directly for newline character
}

std::string GetVersionInfo(const std::string& info) {
    UINT size = 0;
    LPBYTE lpBuffer = NULL;
    std::string result;

    DWORD bufferSize = GetFileVersionInfoSizeA("FSAutoSave.exe", NULL); // 'handle' is not needed, pass NULL instead
    if (bufferSize == 0) {
        std::cerr << "Error in GetFileVersionInfoSize: " << GetLastError() << std::endl;
        return {};
    }

    // Allocate the buffer
    std::vector<char> buffer(bufferSize);

    // Get the version information.
    if (!GetFileVersionInfoA("FSAutoSave.exe", 0, bufferSize, buffer.data())) {
        std::cerr << "Error in GetFileVersionInfo: " << GetLastError() << std::endl;
        return {};
    }

    // Get the desired value.
    if (!VerQueryValueA(buffer.data(), ("\\StringFileInfo\\040904b0\\" + info).c_str(), (LPVOID*)&lpBuffer, &size)) {
        std::cerr << "Error in VerQueryValue: " << GetLastError() << std::endl;
        return {};
    }

    // Copy the value to a string if we found something.
    if (size > 0) {
        result.assign((char*)lpBuffer, size - 1); // size - 1 to exclude the null-terminating character
    }
    return result;
}