#pragma once

#include "SimConnect.h"

void initApp();
void CALLBACK Dispatcher(SIMCONNECT_RECV* pData, DWORD cbData, void* pContext);
void sc();