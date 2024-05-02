#pragma once
#include <Windows.h>  // Ensure to include this for types like HANDLE
#include <string>
#include <chrono>
#include <map>
#include <vector>

// Declare utility functions
bool enableANSI();
bool isMSFSDirectoryWritable(const std::string& directoryPath);

int monitorCustomFlightChanges();
int calculateClockPosition(double bearing, double heading);

std::string formatDuration(int totalSeconds);
std::string get_env_variable(const char* env_var);
std::string getMSFSdir();
std::string getCommunityPath(const std::string& user_cfg_path);
std::string NormalizePath(const std::string& fullPath);
std::string wideToNarrow(const std::wstring& wstr);
std::string GetVersionInfo(const std::string& info);
std::string WideCharToUTF8(const wchar_t* wideChars);

void copyFile(const std::string& source, const std::string& destination);
void handleGroundOperations(const char* airportIdent);
void simStatus(bool running);
void SafeCopyPath(const wchar_t* source);
void fixMSFSbug(const std::string& filePath);
void fixLASTflight(const std::string& filePath);
void sendText(HANDLE hSimConnect, const std::string& text);
void getFP();
void deleteAllSavedSituations();
void initialFLTchange();
void saveNotAllowed();
void currentStatus();
void saveAndSetZULU();
void firstSave();
void finalSave();
void fixCustomFlight();
void waitForEnter();

double metersToFeet(double meters);

struct DistanceAndBearing { double distance; double bearing; };
DistanceAndBearing calculateDistanceAndBearing(double lat1, double lon1, double lat2, double lon2);

// Time structure to represent Zulu time
struct ZuluTime {
    DWORD hour;
    DWORD minute;
    DWORD dayOfYear;
    DWORD year;
};

GateInfo formatGateName(int name);
ZuluTime getZuluTime();
