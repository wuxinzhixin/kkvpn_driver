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
	_Inout_ HANDLE *engineHandle,
	_Inout_ UINT32 *calloutID,
	_Inout_ UINT64 *activeFilter,
	_Inout_ WDFDEVICE device
	);
NTSTATUS UnregisterCallouts();
VOID StopFilterEngine(
	_In_ HANDLE *engineHandle,
	_In_ UINT32 *calloutID,
	_In_ UINT64 *activeFilter
	);
NTSTATUS RegisterCallout(
	_Inout_ void* deviceObject,
	_In_ HANDLE engineHandle,
	_Out_ UINT32* calloutId
	);
NTSTATUS RegisterFilter(
	_Inout_ WDFDEVICE* deviceObject,
	_In_ KKDRV_FILTER_DATA *ipRange,
	_In_ HANDLE engineHandle,
	_Inout_ UINT64 *activeFilter,
	_Out_ UINT32* calloutId
	);
NTSTATUS RestartEngine(
	_In_ HANDLE *engineHandle,
	_In_ UINT32 *calloutID,
	_In_ UINT64 *activeFilter,
	_Inout_ WDFDEVICE device
	);

#endif // !_FILTERINGENGINE_H_