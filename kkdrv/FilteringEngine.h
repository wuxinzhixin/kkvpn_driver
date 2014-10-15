#ifndef _FILTERINGENGINE_H_
#define _FILTERINGENGINE_H_

#include "DriverInit.h"

NTSTATUS 
StartFilterEngine(
	_Inout_ HANDLE *engineHandle,
	_Inout_ UINT32 *calloutID,
	_Inout_ WDFDEVICE device
	);

NTSTATUS 
UnregisterCallouts();

VOID 
StopFilterEngine(
	_In_ HANDLE *engineHandle,
	_In_ UINT32 *calloutID,
	_In_ UINT64 *activeFilterRangeInbound,
	_In_ UINT64 *activeFilterRangeOutbound,
	_In_ UINT64 *activeFilterLocal
	);

NTSTATUS
ClearFilters(
	_In_ HANDLE *engineHandle,
	_In_ UINT64 *activeFilterRangeInbound,
	_In_ UINT64 *activeFilterRangeOutbound,
	_In_ UINT64 *activeFilterLocal
	);

NTSTATUS 
RegisterCallout(
	_Inout_ void* deviceObject,
	_In_ HANDLE engineHandle,
	_Out_ UINT32* calloutId
	);

NTSTATUS 
RegisterFilter(
	_In_ KKDRV_FILTER_DATA *ipRange,
	_In_ HANDLE engineHandle,
	_Inout_ UINT64 *activeFilterRangeInbound,
	_Inout_ UINT64 *activeFilterRangeOutbound,
	_Inout_ UINT64 *activeFilterLocal
	);

NTSTATUS RestartEngine(
	_In_ HANDLE *engineHandle,
	_In_ UINT32 *calloutID,
	_In_ UINT64 *activeFilterRangeInbound,
	_In_ UINT64 *activeFilterRangeOutbound,
	_In_ UINT64 *activeFilterLocal,
	_Inout_ WDFDEVICE device
	);

#endif // !_FILTERINGENGINE_H_