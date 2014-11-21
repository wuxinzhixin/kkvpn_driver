#include "Callout.h"

#include "DriverInit.h"
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
	)
{
	UNREFERENCED_PARAMETER(inFixedValues);
	UNREFERENCED_PARAMETER(classifyContext);
	UNREFERENCED_PARAMETER(filter);
	UNREFERENCED_PARAMETER(inMetaValues);
	UNREFERENCED_PARAMETER(flowContext);

	WDFREQUEST request = gPendingRequest;
	gPendingRequest = NULL;

	NTSTATUS status;

	if (layerData == NULL)
	{
		return;
	}

	FWPS_PACKET_INJECTION_STATE injection = FwpsQueryPacketInjectionState(
		gInjectionEngineHandle,
		(NET_BUFFER_LIST*)layerData,
		NULL
		);

	RtlZeroMemory(classifyOut, sizeof(FWPS_CLASSIFY_OUT));

	if (injection == FWPS_PACKET_INJECTED_BY_SELF ||
		injection == FWPS_PACKET_PREVIOUSLY_INJECTED_BY_SELF)
	{
		classifyOut->actionType = FWP_ACTION_PERMIT;
		return;
	}		
	else
	{
		classifyOut->actionType = FWP_ACTION_BLOCK;
		classifyOut->flags = FWPS_CLASSIFY_OUT_FLAG_ABSORB;
	}

	if (layerData) // && (classifyOut->rights & FWPS_RIGHT_ACTION_WRITE))
	{
		NET_BUFFER_LIST *nbl = (NET_BUFFER_LIST*)layerData;

		status = InsertNBs(
			&gPacketQueue,
			nbl
			);
		if (!NT_SUCCESS(status))
		{
			REPORT_ERROR(InsertNBL, status);
		}	

		if (request != NULL)
		{
			CompleteRequest(request);
		}
	}
		
}

NTSTATUS NTAPI 
CalloutNotifyFunction(
	_In_ FWPS_CALLOUT_NOTIFY_TYPE notifyType,
	_In_ const GUID* filterKey,
	_Inout_ FWPS_FILTER* filter
	)
{
	UNREFERENCED_PARAMETER(notifyType);
	UNREFERENCED_PARAMETER(filterKey);
	UNREFERENCED_PARAMETER(filter);

	return STATUS_SUCCESS;
}

NTSTATUS
InsertNBs(
	_Inout_ KKDRV_QUEUE_DATA *queueData,
	_In_ NET_BUFFER_LIST *head
	)
{
	NTSTATUS status = STATUS_SUCCESS;
	KLOCK_QUEUE_HANDLE lockHandle;

	NET_BUFFER_LIST *nbl = head;
	NET_BUFFER *nb;

	while (nbl)
	{
		nb = NET_BUFFER_LIST_FIRST_NB(nbl);

		while (nb)
		{
			PVOID data;
			ULONG dataLength = NET_BUFFER_DATA_LENGTH(nb);
			PKKDRV_PACKET packet = (PKKDRV_PACKET)ExAllocatePoolWithTag(
				NonPagedPool,
				KKDRV_PACKET_SIZE + dataLength,
				KKDRV_TAG
				);
			if (packet == NULL)
			{
				return STATUS_INSUFFICIENT_RESOURCES;
			};

			packet->dataLength = dataLength;
			data = NdisGetDataBuffer(nb, dataLength, NULL, 1, 0);
			if (data == NULL)
			{
				NdisGetDataBuffer(nb, dataLength, &packet->data, 1, 0);
			}
			else
			{
				RtlCopyMemory(&(packet->data), data, dataLength);
			}

			KeAcquireInStackQueuedSpinLockAtDpcLevel(
				&queueData->queueLock,
				&lockHandle
				);

			InsertTailList(&queueData->queue, &packet->entry);
			queueData->queueLength++;

			if (queueData->queueLength > queueData->queueLengthMax)
			{
				PLIST_ENTRY entry = RemoveHeadList(&queueData->queue);
				ExFreePoolWithTag(entry, KKDRV_TAG);
				queueData->queueLength--;
			}

			KeReleaseInStackQueuedSpinLockFromDpcLevel(
				&lockHandle
				);

			nb = nb->Next;
		}

		nbl = nbl->Next;
	}

	return status;
}