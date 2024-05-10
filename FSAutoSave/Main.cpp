// Copyright (c) Jesus "Bojote" Altuve, All rights reserved.
// FSAutoSave - A simple program to automatically save your last flight so you can resume exactly where you left.
// 
// CTRL+ALT+S  =  to save the current flight 
// CTRL+ALT+P  =  to request the closest GATE/Jetway
// ---------------------------------------------------------

#include <windows.h>
#include <tchar.h>
#include "FSAutoSave.h"
#include "Globals.h"
#include "Utility.h"

int __cdecl _tmain(int argc, _TCHAR* argv[])
{
    // Initialize App Version Info
    std::string companyName = GetVersionInfo("CompanyName");
    std::string fileDescription = GetVersionInfo("FileDescription");
    std::string fileVersion = GetVersionInfo("FileVersion");
    std::string internalName = GetVersionInfo("InternalName");
    std::string legalCopyright = GetVersionInfo("LegalCopyright");
    std::string originalFilename = GetVersionInfo("OriginalFilename");
    std::string productName = GetVersionInfo("ProductName");
    std::string productVersion = GetVersionInfo("ProductVersion");

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

    // Print the startup banner.
    printf("\033[36m\n%s v%s - %s\n%s by %s\n\033[0m", productName.c_str(), productVersion.c_str(), fileDescription.c_str(), legalCopyright.c_str(), companyName.c_str());

    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        if (_tcscmp(argv[i], _T("-DEBUG")) == 0) {
            DEBUG = TRUE;
            printf("[INFO]  *** DEBUG MODE IS ON *** \n");
        }
        if (_tcscmp(argv[i], _T("-ENABLEAIRPORTLIFE")) == 0) {
            enableAirportLife = "True";
            printf("[INFO]  *** AirportLife is ENABLED *** \n");
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
        if (_tcsncmp(argv[i], _T("-FFSTATE:"), 9) == 0) {
            // Set the firstFlightState based on the argument provided
            firstFlightState = WideCharToUTF8(argv[i] + 9); // Convert from TCHAR* to std::string
            printf("Using %s as FirstFlightState in [FreeFlight]\n", firstFlightState.c_str());
        }
    }

    MSFSPath = getMSFSdir();
    if (!MSFSPath.empty()) {
        if (!isMSFSDirectoryWritable(MSFSPath)) {
            MSFSPath = "";
            printf("[INFO] MSFS is in a read-only directory. Program will not run.\n");
            waitForEnter();  // Ensure user presses Enter
            return 0;
        }
    }

    if (!MSFSPath.empty()) {
        // printf("[INFO] MSFS is Installed locally.\n");

        if (isSteam) {
            CommunityPath = getCommunityPath(MSFSPath + "\\UserCfg.opt");  // Assign directly to the global variable 
            pathToMonitor = MSFSPath + "\\Missions\\Custom\\CustomFlight"; // Path to monitor CustomFlight.FLT changes done by MSFS
        }
        else if (isMSStore) {
            CommunityPath = getCommunityPath(MSFSPath + "\\UserCfg.opt");  // Assign directly to the global variable
            pathToMonitor = localStatePath + "\\Missions\\Custom\\CustomFlight"; // Path to monitor CustomFlight.FLT changes done by MSFS
        }
        else {
            printf("[ERROR] Could not determine if MSFS is Steam or MS Store version. Only one version needs to be installed for FSAutoSave to function. Will now exit\n");
            waitForEnter();  // Ensure user presses Enter
            return 0;
        }

        if (!CommunityPath.empty()) {
            // printf("[INFO] Your MSFS Community Path is located at %s\n", CommunityPath.c_str());
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
            printf("[RESET] In order to RESET saved situations you need to run this program where MSFS is installed\n");
            waitForEnter();  // Ensure user presses Enter
            return 0;
        }
        else {
            printf("[INFO] MSFS is NOT Installed locally, FSAutoSave will RUN over the network, but WILL NOT be able to FIX some of the MSFS Save system bugs.\n");
        }
    }

    // Minimize the console window if the flag is set
    if (minimizeOnStart) {
        HWND hWnd = GetConsoleWindow();
        if (hWnd != NULL) {
            ShowWindow(hWnd, SW_MINIMIZE);
        }
    }

    // Try to create a named mutex.
    HANDLE hMutex = CreateMutex(NULL, FALSE, _T("Global\\FSAutoSave"));

    // Check if the mutex was created successfully.
    if (hMutex == NULL) {
        printf("[ERROR] Could NOT create the mutex! Will now exit.\n");
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