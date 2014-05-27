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
	UNREFERENCED_PARAMETER(flowContext);

	NTSTATUS status;

	BOOLEAN awake = FALSE;

	RtlZeroMemory(classifyOut, sizeof(FWPS_CLASSIFY_OUT));
	classifyOut->flags = FWPS_CLASSIFY_OUT_FLAG_ABSORB;

	NT_ASSERT(gBufferEvent != NULL);

	if (layerData)		// && (classifyOut->rights & FWPS_RIGHT_ACTION_WRITE))
	{
		NET_BUFFER_LIST *nbl = (NET_BUFFER_LIST*)layerData;

		status = InsertNBL(
			&gNblQueue,
			nbl, //(NET_BUFFER_LIST*)layerData,
			inMetaValues,
			&awake
			);
		if (!NT_SUCCESS(status))
		{
			REPORT_ERROR(InsertNBL, status);
		}

		NT_ASSERT(nbl->FirstNetBuffer->Next == NULL);

		classifyOut->actionType = FWP_ACTION_BLOCK;
		classifyOut->rights &= ~FWPS_RIGHT_ACTION_WRITE;
		
		DbgPrint(_DRVNAME "Callout!");
		KeSetEvent(gBufferEvent, IO_NO_INCREMENT, FALSE);
	}
	/*else
	{
		classifyOut->actionType = FWP_ACTION_PERMIT;
		classifyOut->rights &= ~FWPS_RIGHT_ACTION_WRITE;
	}*/
		
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
InsertNBL(
	_Inout_ KKDRV_NBL_QUEUE *queue,
	_In_ NET_BUFFER_LIST *nbl,
	_In_ const FWPS_INCOMING_METADATA_VALUES0* inMetaValues,
	_Out_ BOOLEAN *awake
	)
{
	NTSTATUS status = STATUS_SUCCESS;
	KLOCK_QUEUE_HANDLE lockHandle;

	NET_BUFFER_LIST *clone;

	NdisRetreatNetBufferDataStart(
		NET_BUFFER_LIST_FIRST_NB(nbl),
		inMetaValues->ipHeaderSize,
		0,
		NULL
		);

	status = FwpsAllocateCloneNetBufferList(
		nbl,
		NULL,
		NULL,
		0,
		&clone
		);
	if (!NT_SUCCESS(status))
	{
		goto Exit;
	}

	NdisAdvanceNetBufferDataStart(
		NET_BUFFER_LIST_FIRST_NB(nbl),
		inMetaValues->ipHeaderSize,
		FALSE,
		NULL);

	KeAcquireInStackQueuedSpinLock(
		&(queue->lock),
		&lockHandle
		);

	if (queue->nblTail != NULL)
	{
		queue->nblTail->Next = clone;
	}
	else
	{
		queue->nblHead = clone;
	}

	queue->nblTail = TailOfNetBufferListChain(clone);
	queue->length += GetNBLLength(clone);
	queue->flags = inMetaValues->flags;
	*awake = queue->awake;

	KeReleaseInStackQueuedSpinLock(
		&lockHandle
		);

Exit:
	return status;
}

__inline NET_BUFFER_LIST* 
TailOfNetBufferListChain(
	_In_ NET_BUFFER_LIST* nbl
	)
{
	NET_BUFFER_LIST* out = nbl;

	while (out->Next != NULL)
	{
		out = out->Next;
	}

	return out;
}

__inline size_t
GetNBLLength(
	_In_ NET_BUFFER_LIST* nbl
	)
{
	NET_BUFFER_LIST* out = nbl;
	size_t length = 0;

	while (out)
	{
		NET_BUFFER *nb = out->FirstNetBuffer;

		while (nb)
		{
			length += NET_BUFFER_DATA_LENGTH(nb);
			nb = nb->Next;
		}

		out = out->Next;
	}

	return length;
}

//{
//	NET_BUFFER* out = nbl->FirstNetBuffer;
//	size_t length = 0;
//
//	while (out != NULL)
//	{
//		length += NET_BUFFER_DATA_LENGTH(out); // ->DataLength;
//		out = out->Next;
//	}
//
//	return length;
//}