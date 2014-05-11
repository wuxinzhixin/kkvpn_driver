//#include <ntifs.h>
#include <ntddk.h>
#include <wdf.h>
#include "CommonStructures.h"

#pragma warning(push)
#pragma warning(disable:4201)       // unnamed struct/union

#include <fwpsk.h>

#pragma warning(pop)

#include <fwpmk.h>

#ifndef _FILTERINGENGINE_H_
#define _FILTERINGENGINE_H_

NTSTATUS StartFilterEngine(
	_Inout_ WDFDEVICE *device
	);
NTSTATUS UnregisterCallouts();
VOID StopFilterEngine();
NTSTATUS RegisterCallout(
	_Inout_ void* deviceObject,
	_Out_ UINT32* calloutId
	);
NTSTATUS RegisterFilter(
	_Inout_ void* deviceObject,
	_In_ FILTER_IP_RANGE *ipRange,
	_Out_ UINT32* calloutId
	);
NTSTATUS RestartEngine();

#endif // !_FILTERINGENGINE_H_