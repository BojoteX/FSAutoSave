#pragma once
#include <atomic>
#include <string>
#include <filesystem>

// Namespace for filesystem operations
namespace fs = std::filesystem;

// External declarations of global variables
extern std::atomic<bool> isModifyingFile;
extern SIMCONNECT_DATA_REQUEST_ID FACILITY_DATA_DEF_REQUEST_START;
extern unsigned g_RequestCount;
extern HANDLE hSimConnect;
extern HANDLE g_hEvent;
extern HRESULT hr;

extern bool DEBUG;
extern bool minimizeOnStart;
extern bool resetSaves;
extern bool isBUGfixed;
extern bool isSteam;
extern bool isMSStore;

extern int startCounter;
extern int quit;
extern int fpDisableCount;
extern int parkingIndex;
extern int countJetways;
extern int countTaxiParking;

extern const std::string DELETE_MARKER;
extern const std::string DELETE_SECTION_MARKER;

extern const char* szFileName;
extern const char* szTitle;
extern const char* szDescription;

extern double myLatitude;
extern double myLongitude;
extern double myAltitude;
extern double myAirspeed;
extern double isSimOnGround;

extern std::string airportICAO;
extern std::string airportName;
extern std::string parkingGate;
extern std::string parkingGateSuffix;
extern unsigned parkingNumber;

extern std::string currentAircraft;
extern std::string currentSaveFlight;
extern std::string currentSaveFlightPath;
extern std::string currentFlightPlan;
extern std::string currentFlightPlanPath;
extern std::string currentFlight;
extern std::string currentFlightPath;
extern std::string MSFSPath;
extern std::string CommunityPath;
extern std::string pathToMonitor;
extern wchar_t GetFPpath[1024];

extern std::string enableAirportLife;

extern bool isOnMenuScreen;
extern bool isFlightPlanActive;
extern bool wasReset;
extern bool wasSoftPaused;
extern bool wasFullyPaused;
extern bool isFinalSave;
extern bool isFirstSave;
extern bool isPauseBeforeStart;
extern bool userLoadedPLN;
extern bool flightInitialized;
extern DWORD isSimRunning;

// Pause state flags
#define PAUSE_STATE_FLAG_OFF 0
#define PAUSE_STATE_FLAG_PAUSE 1
#define PAUSE_STATE_FLAG_PAUSE_WITH_SOUND 2
#define PAUSE_STATE_FLAG_ACTIVE_PAUSE 4
#define PAUSE_STATE_FLAG_SIM_PAUSE 8

// Structs and Enums
// (Include definitions as they do not create multiple definition errors and are useful across files)
#pragma pack(push, 1)
struct sAirport { char name[64]; char icao[8]; };
struct sJetways { int PARKING_GATE; int PARKING_SUFFIX; int PARKING_SPOT; };
struct sTaxiParkings { int NAME; int SUFFIX; unsigned NUMBER; };
struct GateInfo { std::string friendlyName; std::string gateString; };
struct SimDayOfYear { double dayOfYear; };
struct AircraftPosition { double latitude; double longitude; double altitude; double airspeed; double sim_on_ground; };
struct CameraState { double state; };
#pragma pack(pop)

enum INPUT_ID { INPUT0 };
enum GROUP_ID { GROUP0, GROUP1 };
enum DATA_DEFINE_ID {
    DEFINITION_ZULU_TIME,
    DEFINITION_POSITION_DATA,
    DEFINITION_CAMERA_STATE,
    DEFINITION_FACILITY_AIRPORT,
};
enum DATA_REQUEST_ID {
    REQUEST_SIM_STATE,
    REQUEST_AIRCRAFT_STATE,
    REQUEST_FLIGHTLOADED_STATE,
    REQUEST_FLIGHTPLAN_STATE,
    REQUEST_ZULU_TIME,
    REQUEST_POSITION,
    REQUEST_POSITION_ONCE,
    REQUEST_DIALOG_STATE,
    REQUEST_CAMERA_STATE,
    REQUEST_CLOSEST_AIRPORT,
    REQUEST_JETWAY_DATA,
};
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
    EVENT_CLOSEST_AIRPORT,
};