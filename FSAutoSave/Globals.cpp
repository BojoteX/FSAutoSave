#include <Windows.h>
#include "FSAutoSave.h"
#include "Globals.h"

std::atomic<bool> isModifyingFile(false);
SIMCONNECT_DATA_REQUEST_ID FACILITY_DATA_DEF_REQUEST_START	= 100;
unsigned g_RequestCount										= 0;
HANDLE hSimConnect											= NULL;
HANDLE g_hEvent												= NULL;
HRESULT hr													= NULL;

bool DEBUG				= FALSE;
bool minimizeOnStart	= FALSE;
bool resetSaves			= FALSE;
bool isBUGfixed			= FALSE;
bool isBUGfixedCustom	= FALSE;
bool isSteam			= FALSE;
bool isMSStore			= FALSE;

int startCounter		= 0;
int quit				= 0;
int fpDisableCount		= 0;
int parkingIndex		= 0;
int countJetways		= 0;
int countTaxiParking	= 0;

const std::string DELETE_MARKER			= "!DELETE!";
const std::string DELETE_SECTION_MARKER = "!DELETE_SECTION!";

const char* szFileName		 = "Missions\\Custom\\CustomFlight\\CustomFlight";
const char* szTitle			 = "FSAutoSave generated file";
const char* szDescription	 = "This is a save of your last flight so you can resume exactly where you left.";

double myLatitude		= 0.0;
double myLongitude		= 0.0;
double myAltitude		= 0.0;
double myIASinFPS		= 0.0;
double myAirspeed		= 0.0;
double myHeading		= 0.0;
double isSimOnGround	= 0.0;
double JetwayDistance	= 0.0;
double JetwayBearing	= 0.0;

std::string airportName;
std::string airportICAO;
std::string parkingGate;
std::string parkingGateSuffix;
unsigned parkingNumber;

std::string currentAircraft;
std::string currentSaveFlight;
std::string currentSaveFlightPath;
std::string currentFlightPlan;
std::string currentFlightPlanPath;
std::string currentFlight;
std::string currentFlightPath;
std::string MSFSPath;
std::string CommunityPath;
std::string pathToMonitor;
std::string customFlightmod;
std::string lastMOD;
std::string localStatePath;

wchar_t GetFPpath[1024];

std::string firstFlightState	= "PREFLIGHT_GATE";
std::string enableAirportLife	= "False";

bool aircraftCrashed = FALSE;
bool isOnMenuScreen = FALSE;
bool isFlightPlanActive = FALSE;
bool wasReset = FALSE;
bool wasSoftPaused = FALSE;
bool wasFullyPaused = FALSE;
bool isFinalSave = FALSE;
bool isFirstSave = FALSE;
bool isPauseBeforeStart = FALSE;
bool userLoadedPLN = FALSE;
bool flightInitialized = FALSE;

DWORD isSimRunning = 0;