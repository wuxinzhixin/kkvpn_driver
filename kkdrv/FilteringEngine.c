#include "FilteringEngine.h"

#include "DriverMain.h"
#include "Callout.h"
#include "UserModeBufferHandler.h"

NTSTATUS 
StartFilterEngine(
	_Inout_ HANDLE *engineHandle,
	_Inout_ UINT32 *calloutID,
	_Inout_ WDFDEVICE device
	)
{
	NTSTATUS status = STATUS_SUCCESS;
	BOOLEAN engineOpened = FALSE;
	BOOLEAN transactionStarted = FALSE;

	FWPM_SESSION session = { 0 };
	
	session.flags = FWPM_SESSION_FLAG_DYNAMIC;

	status = FwpmEngineOpen(
		NULL,
		RPC_C_AUTHN_WINNT,
		NULL,
		&session,
		engineHandle
		);
	if (!NT_SUCCESS(status))
	{
		REPORT_ERROR(FwpmEngineOpen, status);
		goto Exit;
	}
	engineOpened = TRUE;

	status = FwpmTransactionBegin(*engineHandle, 0);
	if (!NT_SUCCESS(status))
	{
		REPORT_ERROR(StartFilterEngine->FwpmTransactionBegin, status);
		goto Exit;
	}
	transactionStarted = TRUE;

	//register callout

	status = RegisterCallout(
		device,
		*engineHandle,
		calloutID
		);
	if (!NT_SUCCESS(status))
	{
		REPORT_ERROR(RegisterCallout, status);
		goto Exit;
	}

	status = FwpmTransactionCommit(*engineHandle);
	if (!NT_SUCCESS(status))
	{
		REPORT_ERROR(FwpmTransactionCommit, status);
		goto Exit;
	}
	transactionStarted = FALSE;

Exit:

	if (!NT_SUCCESS(status))
	{
		if (transactionStarted)
		{
			FwpmTransactionAbort(*engineHandle);
		}

		if (engineOpened)
		{
			FwpmEngineClose(*engineHandle);
			*engineHandle = NULL;
		}
	}

	return status;
}

VOID 
StopFilterEngine(
	_In_ HANDLE *engineHandle,
	_In_ UINT32 *calloutID,
	_In_ UINT64 *activeFilterRangeInbound,
	_In_ UINT64 *activeFilterRangeOutbound,
	_In_ UINT64 *activeFilterLocal
	)
{
	ClearFilters(
		engineHandle,
		activeFilterRangeInbound,
		activeFilterRangeOutbound,
		activeFilterLocal
		);

	if (*calloutID)
	{
		if (*engineHandle)
		{
			FwpmCalloutDeleteById(*engineHandle, *calloutID);
		}
		FwpsCalloutUnregisterById(*calloutID);
		*calloutID = 0;
	}	

	if (*engineHandle)
	{
		FwpmEngineClose(*engineHandle);
		*engineHandle = NULL;
	}
}

NTSTATUS
ClearFilters(
	_In_ HANDLE *engineHandle,
	_In_ UINT64 *activeFilterRangeInbound,
	_In_ UINT64 *activeFilterRangeOutbound,
	_In_ UINT64 *activeFilterLocal
	)
{
	BOOL transactionStarted = FALSE;

	NTSTATUS status = FwpmTransactionBegin(engineHandle, 0);
	if (!NT_SUCCESS(status))
	{
		REPORT_ERROR(ClearFilters->FwpmTransactionBegin, status);
		goto Exit;
	}
	transactionStarted = TRUE;

	if (*activeFilterRangeInbound)
	{
		status = FwpmFilterDeleteById(*engineHandle, *activeFilterRangeInbound);
		*activeFilterRangeInbound = 0;
	}

	if (*activeFilterRangeOutbound)
	{
		status = FwpmFilterDeleteById(*engineHandle, *activeFilterRangeOutbound);
		*activeFilterRangeOutbound = 0;
	}

	if (*activeFilterLocal)
	{
		status = FwpmFilterDeleteById(*engineHandle, *activeFilterLocal);
		*activeFilterLocal = 0;
	}

	status = FwpmTransactionCommit(engineHandle);
	if (!NT_SUCCESS(status))
	{
		REPORT_ERROR(FwpmTransactionCommit, status);
		goto Exit;
	}
	transactionStarted = FALSE;

Exit:

	if (transactionStarted)
	{
		FwpmTransactionAbort(engineHandle);
	}

	return status;
}

NTSTATUS
RegisterCallout(
	_Inout_ WDFDEVICE device,
	_In_ HANDLE engineHandle,
	_Out_ UINT32* calloutId
	)
{
	NTSTATUS status = STATUS_SUCCESS;
	BOOLEAN calloutRegistered = FALSE;

	FWPS_CALLOUT sCallout = { 0 };
	FWPM_CALLOUT mCallout = { 0 };
	FWPM_DISPLAY_DATA displayData = { 0 };
	PDEVICE_OBJECT deviceObject = WdfDeviceWdmGetDeviceObject(device);

	sCallout.calloutKey = GUID_KKDRV_CALLOUT;
	sCallout.classifyFn = CalloutClassifyFunction;
	sCallout.notifyFn = CalloutNotifyFunction;

	FwpmCalloutDeleteByKey(
		engineHandle,
		&GUID_KKDRV_CALLOUT
		);

	FwpsCalloutUnregisterByKey(&GUID_KKDRV_CALLOUT);

	status = FwpsCalloutRegister(
		deviceObject,
		&sCallout,
		calloutId
		);
	if (!NT_SUCCESS(status))
	{
		REPORT_ERROR(FwpsCalloutRegister, status);
		goto Exit;
	}
	calloutRegistered = TRUE;

	displayData.name = L"kkdrv IPv4 Callout";
	displayData.description = L"Callout that extracts network data.";

	mCallout.calloutKey = GUID_KKDRV_CALLOUT;
	mCallout.displayData = displayData;
	mCallout.applicableLayer = FWPM_LAYER_OUTBOUND_IPPACKET_V4;

	status = FwpmCalloutAdd(
		engineHandle,
		&mCallout,
		NULL,
		NULL
		);
	if (!NT_SUCCESS(status))
	{
		REPORT_ERROR(FwpmCalloutAdd, status);
		goto Exit;
	}

Exit:
	if (!NT_SUCCESS(status))
	{
		if (calloutRegistered)
		{
			FwpsCalloutUnregisterById(*calloutId);
		}
	}

	return status;
}

NTSTATUS
RegisterFilter(
	_In_ KKDRV_FILTER_DATA *ipRange,
	_In_ HANDLE engineHandle,
	_Inout_ UINT64 *activeFilterRangeInbound,
	_Inout_ UINT64 *activeFilterRangeOutbound,
	_Inout_ UINT64 *activeFilterLocal
	)
{
	NTSTATUS status = STATUS_SUCCESS;

	FWPM_FILTER filterInbound = { 0 };
	FWPM_FILTER filterOutbound = { 0 };
	FWPM_FILTER filterLocal = { 0 };
	FWPM_FILTER_CONDITION filterInboundCondition[1] = { 0 };
	FWPM_FILTER_CONDITION filterOutboundCondition[1] = { 0 };
	FWPM_FILTER_CONDITION filterLocalCondition[1] = { 0 };
	FWP_RANGE filterConditionRange = { 0 };

	BOOLEAN transactionStarted = FALSE;

	filterConditionRange.valueLow.type = FWP_UINT32;
	filterConditionRange.valueLow.uint32 = (UINT32)(ipRange->low);

	filterConditionRange.valueHigh.type = FWP_UINT32;
	filterConditionRange.valueHigh.uint32 = (UINT32)(ipRange->high);

	status = FwpmTransactionBegin(engineHandle, 0);
	if (!NT_SUCCESS(status))
	{
		REPORT_ERROR(RegisterFilter->FwpmTransactionBegin, status);
		goto Exit;
	}
	transactionStarted = TRUE;

	/*	
		Outbound Filter
	*/

	RtlZeroMemory(&filterOutbound, sizeof(filterOutbound));
	filterOutbound.layerKey = FWPM_LAYER_OUTBOUND_IPPACKET_V4;
	filterOutbound.displayData.name = L"kkdrv IPv4 Outbound Filter";
	filterOutbound.displayData.description = L"Filter that checks for packets addressed inside\
											   VPN subnetwork.";

	filterOutbound.action.type = FWP_ACTION_CALLOUT_TERMINATING;
	filterOutbound.action.calloutKey = GUID_KKDRV_CALLOUT;
	filterOutbound.filterCondition = filterOutboundCondition;
	filterOutbound.numFilterConditions = 1;
	filterOutbound.subLayerKey = FWPM_SUBLAYER_UNIVERSAL;
	filterOutbound.weight.type = FWP_EMPTY;

	filterOutboundCondition[0].fieldKey = FWPM_CONDITION_IP_REMOTE_ADDRESS;
	filterOutboundCondition[0].matchType = FWP_MATCH_RANGE;
	filterOutboundCondition[0].conditionValue.type = FWP_RANGE_TYPE;
	filterOutboundCondition[0].conditionValue.rangeValue = &filterConditionRange;

	status = FwpmFilterAdd(
		engineHandle,
		&filterOutbound,
		NULL,
		activeFilterRangeOutbound
		);
	if (!NT_SUCCESS(status))
	{
		REPORT_ERROR(FwpmFilterAdd(outbound), status);
		goto Exit;
	}

	/*
		Inbound Filter
	*/

	RtlZeroMemory(&filterInbound, sizeof(filterInbound));
	filterInbound.layerKey = FWPM_LAYER_INBOUND_IPPACKET_V4;
	filterInbound.displayData.name = L"kkdrv IPv4 Inbound Filter";
	filterInbound.displayData.description = L"Filter that checks for packets addressed inside\
											  VPN subnetwork.";

	filterInbound.action.type = FWP_ACTION_PERMIT; //FWP_ACTION_BLOCK;
	filterInbound.filterCondition = filterInboundCondition;
	filterInbound.numFilterConditions = 1;
	filterInbound.subLayerKey = FWPM_SUBLAYER_UNIVERSAL;
	filterInbound.weight.type = FWP_UINT8;
	filterInbound.weight.uint8 = 0xF;

	filterInboundCondition[0].fieldKey = FWPM_CONDITION_IP_REMOTE_ADDRESS;
	filterInboundCondition[0].matchType = FWP_MATCH_RANGE;
	filterInboundCondition[0].conditionValue.type = FWP_RANGE_TYPE;
	filterInboundCondition[0].conditionValue.rangeValue = &filterConditionRange;

	status = FwpmFilterAdd(
		engineHandle,
		&filterInbound,
		NULL,
		activeFilterRangeInbound
		);
	if (!NT_SUCCESS(status))
	{
		REPORT_ERROR(FwpmFilterAdd(inbound), status);
		goto Exit;
	}

	/*
		Local Filter
	*/
	UNREFERENCED_PARAMETER(activeFilterLocal);
	if (ipRange->local != 0)
	{
		RtlZeroMemory(&filterLocal, sizeof(filterLocal));
		filterLocal.layerKey = FWPM_LAYER_OUTBOUND_IPPACKET_V4;
		filterLocal.displayData.name = L"kkdrv IPv4 Outbound Filter (Local)";
		filterLocal.displayData.description = L"Filter that checks for packets addressed inside\
											   	VPN subnetwork.";

		filterLocal.action.type = FWP_ACTION_PERMIT;
		filterLocal.filterCondition = filterLocalCondition;
		filterLocal.numFilterConditions = 1;
		filterLocal.subLayerKey = FWPM_SUBLAYER_UNIVERSAL;
		filterLocal.weight.type = FWP_EMPTY;

		filterLocalCondition[0].fieldKey = FWPM_CONDITION_IP_REMOTE_ADDRESS;
		filterLocalCondition[0].matchType = FWP_MATCH_EQUAL;
		filterLocalCondition[0].conditionValue.type = FWP_UINT32;
		filterLocalCondition[0].conditionValue.uint32 = ipRange->local;

		status = FwpmFilterAdd(
			engineHandle,
			&filterLocal,
			NULL,
			activeFilterLocal
			);
		if (!NT_SUCCESS(status))
		{
			REPORT_ERROR(FwpmFilterAdd(local), status);
			goto Exit;
		}
	}
	/*
		Commit
	*/

	status = FwpmTransactionCommit(engineHandle);
	if (!NT_SUCCESS(status))
	{
		REPORT_ERROR(FwpmTransactionCommit, status);
		goto Exit;
	}
	transactionStarted = FALSE;

Exit:

	if (transactionStarted)
	{
		FwpmTransactionAbort(engineHandle);
	}

	return status;
}

NTSTATUS
RestartEngine(
	_In_ HANDLE *engineHandle,
	_In_ UINT32 *calloutID,
	_In_ UINT64 *activeFilterRangeInbound,
	_In_ UINT64 *activeFilterRangeOutbound,
	_In_ UINT64 *activeFilterLocal,
	_Inout_ WDFDEVICE device
	)
{
	NTSTATUS status = STATUS_SUCCESS;

	StopFilterEngine(
		engineHandle,
		calloutID,
		activeFilterRangeInbound,
		activeFilterRangeOutbound,
		activeFilterLocal
		);

	StartFilterEngine(
		engineHandle,
		calloutID,
		device
		);
	if (!NT_SUCCESS(status))
	{
		REPORT_ERROR(StartFilterEngine, status);
		goto Exit;
	}

Exit:
	return status;
}