#include <ntddk.h>
#include <wdf.h>

#pragma warning(push)
#pragma warning(disable:4201)       // unnamed struct/union

#include <fwpsk.h>

#pragma warning(pop)

#include <fwpmk.h>

#include "Callout.h"
#include "DriverInit.h"
#include "UserModeBufferHandler.h"

void NTAPI 
CalloutClasifyFunction(
	_In_ const FWPS_INCOMING_VALUES0* inFixedValues,
	_In_ const FWPS_INCOMING_METADATA_VALUES0* inMetaValues,
	_Inout_opt_ void* layerData,
	_In_opt_ const void* classifyContext,
	_In_ const FWPS_FILTER2* filter,
	_In_ UINT64 flowContext,
	_Inout_ FWPS_CLASSIFY_OUT0* classifyOut
	)
{
	UNREFERENCED_PARAMETER(inFixedValues);
	UNREFERENCED_PARAMETER(classifyContext);
	UNREFERENCED_PARAMETER(filter);
	UNREFERENCED_PARAMETER(inMetaValues);
	UNREFERENCED_PARAMETER(flowContext);

	NTSTATUS status;

	BOOLEAN awake = FALSE;

	NT_ASSERT(gBufferEvent != NULL);

	FWPS_PACKET_INJECTION_STATE injection = FwpsQueryPacketInjectionState(
		gInjectionEngineHandle,
		(NET_BUFFER_LIST*)layerData,
		NULL
		);

	if (injection == FWPS_PACKET_INJECTED_BY_SELF ||
		injection == FWPS_PACKET_PREVIOUSLY_INJECTED_BY_SELF)
	{
		classifyOut->actionType = FWP_ACTION_PERMIT;
		return;
	}		

	gIfIndex = inFixedValues->incomingValue[
		FWPS_FIELD_INBOUND_IPPACKET_V4_INTERFACE_INDEX].value.uint32;
	gSubIfIndex = inFixedValues->incomingValue[
		FWPS_FIELD_INBOUND_IPPACKET_V4_SUB_INTERFACE_INDEX].value.uint32;

	RtlZeroMemory(classifyOut, sizeof(FWPS_CLASSIFY_OUT));
	classifyOut->flags = FWPS_CLASSIFY_OUT_FLAG_ABSORB;

	if (layerData) // && (classifyOut->rights & FWPS_RIGHT_ACTION_WRITE))
	{
		NET_BUFFER_LIST *nbl = (NET_BUFFER_LIST*)layerData;

		status = InsertNBs(
			&gPacketQueue,
			nbl,
			&awake
			);
		if (!NT_SUCCESS(status))
		{
			REPORT_ERROR(InsertNBL, status);
		}

		NT_ASSERT(nbl->FirstNetBuffer->Next == NULL);

		classifyOut->actionType = FWP_ACTION_BLOCK;
		classifyOut->rights &= ~FWPS_RIGHT_ACTION_WRITE;

		KeSetEvent(gBufferEvent, IO_NO_INCREMENT, FALSE);
	}
		
}

NTSTATUS NTAPI 
CalloutNotifyFunction(
	_In_ FWPS_CALLOUT_NOTIFY_TYPE notifyType,
	_In_ const GUID* filterKey,
	_Inout_ FWPS_FILTER2* filter
	)
{
	UNREFERENCED_PARAMETER(notifyType);
	UNREFERENCED_PARAMETER(filterKey);
	UNREFERENCED_PARAMETER(filter);

	return STATUS_SUCCESS;
}

NTSTATUS
InsertNBs(
	_Inout_ KKDRV_PACKET_QUEUE *queue,
	_In_ NET_BUFFER_LIST *head,
	_Out_ BOOLEAN *awake
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
				NdisGetDataBuffer(nb, dataLength, &(packet->data), 1, 0);
			}
			else
			{
				RtlCopyMemory(&(packet->data), data, dataLength);
			}

			KeAcquireInStackQueuedSpinLockAtDpcLevel(
				&(queue->lock),
				&lockHandle
				);

			if (queue->nblTail != NULL)
			{
				queue->nblTail->Next = packet;
			}
			else
			{
				queue->nblHead = packet;
			}

			queue->nblTail = TailOfNetPacketChain(packet);
			queue->length += dataLength;
			*awake = queue->awake;

			KeReleaseInStackQueuedSpinLockFromDpcLevel(
				&lockHandle
				);

			nb = nb->Next;
		}

		nbl = nbl->Next;
	}

//Exit:
	return status;
}

__inline PKKDRV_PACKET
TailOfNetPacketChain(
	_In_ PKKDRV_PACKET packet
	)
{
	PKKDRV_PACKET out = packet;

	while (out->Next != NULL)
	{
		out = out->Next;
	}

	return out;
}