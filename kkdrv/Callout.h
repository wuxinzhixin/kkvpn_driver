#ifndef _CALLOUT_H_
#define _CALLOUT_H_

#include "DriverMain.h"
#include "UserModeBufferHandler.h"

void NTAPI 
CalloutClassifyFunction(
	_In_ const FWPS_INCOMING_VALUES* inFixedValues,
	_In_ const FWPS_INCOMING_METADATA_VALUES* inMetaValues,
	_Inout_opt_ void* layerData,
	_In_opt_  const void *classifyContext,
	_In_ const FWPS_FILTER* filter,
	_In_ UINT64 flowContext,
	_Inout_ FWPS_CLASSIFY_OUT* classifyOut
	);

NTSTATUS NTAPI 
CalloutNotifyFunction(
	_In_ FWPS_CALLOUT_NOTIFY_TYPE notifyType,
	_In_ const GUID* filterKey,
	_Inout_ FWPS_FILTER* filter
	);

NTSTATUS
InsertNBs(
	_Inout_ KKDRV_QUEUE_DATA *queueData,
	_In_ NET_BUFFER_LIST *head
	);

#endif // !_CALLOUT_H_