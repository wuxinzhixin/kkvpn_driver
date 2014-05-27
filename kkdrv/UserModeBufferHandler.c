#include <ntddk.h>
#include <wdf.h>

#pragma warning(push)
#pragma warning(disable:4201)       // unnamed struct/union

#include <fwpsk.h>

#pragma warning(pop)

#include <fwpmk.h>

#include "UserModeBufferHandler.h"
#include "DriverInit.h"

HANDLE gWorkerThreadHandle;
PVOID gWorkerThreadObj;

VOID 
WorkerRoutine(
	_In_ PVOID params
	)
{
	NTSTATUS status = STATUS_SUCCESS;
	KKDRV_WORKER_DATA* threadParams = (KKDRV_WORKER_DATA*)params;

	DbgPrint(_DRVNAME "Starting worker thread");

	for (;;)
	{
		KLOCK_QUEUE_HANDLE lockHandle;
		NET_BUFFER_LIST *nbl = NULL;
		size_t length = 0;
		UINT32 flags = 0;
		KKDRV_NET_BUFFER_FLAT *nbflat = NULL;

		KeWaitForSingleObject(
			(PKEVENT)&threadParams->event,
			Executive,
			KernelMode,
			FALSE,
			NULL
			);

		if (*(BOOLEAN*)(threadParams->stoppingThread))
		{
			break;
		}

		nbflat = (KKDRV_NET_BUFFER_FLAT*)threadParams->mem;

		threadParams->queue->awake = TRUE;

		KeAcquireInStackQueuedSpinLock(&(threadParams->queue->lock), &lockHandle);

		nbl = threadParams->queue->nblHead;
		length = threadParams->queue->length;
		flags = threadParams->queue->flags;

		threadParams->queue->nblHead = NULL;
		threadParams->queue->nblTail = NULL;
		threadParams->queue->length = 0;

		KeReleaseInStackQueuedSpinLock(&lockHandle);

		KeClearEvent(&threadParams->event);

		if (!nbl)
			continue;

		KeWaitForSingleObject(
			(PKEVENT)threadParams->userevent_complete,
			Executive,
			KernelMode,
			FALSE,
			NULL
			);

		nbflat->length = (UINT32)length;
		status = CopyNblDataToBuffer(
			nbl,
			length,
			flags,
			&(nbflat->buffer)
			);
		if (!NT_SUCCESS(status))
		{
			REPORT_ERROR(CopyNblDataToBuffer, status);
			continue;
		}

		FwpsFreeCloneNetBufferList(nbl, 0);

		if (threadParams->userevent_receive)
			KeSetEvent(threadParams->userevent_receive, IO_NO_INCREMENT, FALSE);
		else
			DbgPrint(_DRVNAME "Usermode event empty\n");

		threadParams->queue->awake = FALSE;
	}

	DbgPrint(_DRVNAME "Exiting worker thread\n");
}

NTSTATUS 
StartWorker(
	_Out_ PKEVENT *NtfyEvent
	)
{
	NTSTATUS status = STATUS_SUCCESS;
	//HANDLE sharedEvent = NULL;
	//PKEVENT event = NULL;
	gStoppingThread = FALSE;

	/*UNICODE_STRING eventName;
	RtlInitUnicodeString(&eventName, L"\\BaseNamedObjects\\kkdrvBufferEvent");
	event = IoCreateNotificationEvent(&eventName, &sharedEvent);
	if (event == NULL)
	{
		REPORT_ERROR(IoCreateNotificationEvent, 0);
		return STATUS_UNSUCCESSFUL;
	}*/

	KeInitializeEvent(
		&gParams.event,
		NotificationEvent,
		FALSE
		);

	*NtfyEvent = &gParams.event;
	gParams.stoppingThread = &gStoppingThread;
	gParams.userevent_receive = gUserModeEventReceive;
	gParams.userevent_complete = gUserModeEventComplete;
	//gParams.event = event;
	gParams.mem = gSharedMem;
	gParams.queue = &gNblQueue;

	//KeClearEvent(event);

	status = PsCreateSystemThread(
		&gWorkerThreadHandle,
		THREAD_ALL_ACCESS,
		NULL,
		NULL,
		NULL,
		WorkerRoutine,
		&gParams
		);
	if (!NT_SUCCESS(status))
	{
		REPORT_ERROR(PsCreateSystemThread, status);
		return status;
	}

	status = ObReferenceObjectByHandle(
		gWorkerThreadHandle,
		0,
		NULL,
		KernelMode,
		&gWorkerThreadObj,
		NULL
		);
	NT_ASSERT(NT_SUCCESS(status));

	ZwClose(gWorkerThreadHandle);

	return status;
}

VOID 
StopWorker()
{
	KeWaitForSingleObject(
		gWorkerThreadObj,
		Executive,
		KernelMode,
		FALSE,
		NULL
		);

	ObDereferenceObject(gWorkerThreadObj);
}

NTSTATUS 
CopyNblDataToBuffer(
	_Inout_ NET_BUFFER_LIST *netBufferListChain,
	_In_ size_t length,
	_In_ UINT32 flags,
	_Out_ PVOID buffer
	)
{
	NTSTATUS status = STATUS_SUCCESS;

	UNREFERENCED_PARAMETER(netBufferListChain);
	UNREFERENCED_PARAMETER(buffer);

	if (length > (UM_BUFFER_PAGE_SIZE))
	{
		REPORT_ERROR(CopyNblDataToBuffer, 0);
		return STATUS_UNSUCCESSFUL;
	}

	FWPS_STREAM_DATA streamData;
	size_t bytesCopied = 0;

	streamData.dataLength = length;
	streamData.flags = flags;
	streamData.netBufferListChain = netBufferListChain;

	streamData.dataOffset.netBufferList = netBufferListChain;
	streamData.dataOffset.netBuffer =
		NET_BUFFER_LIST_FIRST_NB(streamData.dataOffset.netBufferList);
	streamData.dataOffset.mdl =
		NET_BUFFER_CURRENT_MDL(streamData.dataOffset.netBuffer);
	streamData.dataOffset.mdlOffset =
		NET_BUFFER_CURRENT_MDL_OFFSET(streamData.dataOffset.netBuffer);

	FwpsCopyStreamDataToBuffer(
			&streamData,
			buffer,
			length,
			&bytesCopied
			);
	NT_ASSERT(bytesCopied == length);
	/*DbgPrint(_DRVNAME "Bytes copied to flat buffer: 0x%08x, NBLs in chain: %d, NBs in chain: %d\n", 
		bytesCopied, 
		NBLsInChain(netBufferListChain),
		NBsInChain(netBufferListChain)
		);*/

	return status;
}

__inline UINT32
NBLsInChain(
	_In_ NET_BUFFER_LIST * nbl
	)
{
	UINT32 res = 0;
	NET_BUFFER_LIST *out = nbl;

	while (out)
	{
		out = out->Next;
		res++;
	}

	return res;
}

__inline UINT32
NBsInChain(
	_In_ NET_BUFFER_LIST * nbl
	)
{
	UINT32 res = 0;
	NET_BUFFER_LIST *out = nbl;

	while (out)
	{
		NET_BUFFER *nb = out->FirstNetBuffer;

		while (nb)
		{
			res++;
			nb = nb->Next;
		}

		out = out->Next;
	}

	return res;
}