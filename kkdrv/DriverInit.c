#include "DriverInit.h"

#include "FilteringEngine.h"
#include "InjectionEngine.h"
#include "UserModeBufferHandler.h"

DECLARE_CONST_UNICODE_STRING(
	SDDL_DEVOBJ_SYS_ALL_ADM_RWX_WORLD_R_RES_R,
	L"D:P(A;;GA;;;SY)(A;;GRGWGX;;;BA)(A;;GR;;;WD)(A;;GR;;;RC)"
	);
	
KKDRV_QUEUE_DATA gPacketQueue;
UINT64 gActiveFilterRangeInbound;
UINT64 gActiveFilterRangeOutbound;
UINT64 gActiveFilterLocal;
UINT32 gCalloutID;
WDFREQUEST gPendingRequest;
HANDLE gFilteringEngineHandle;
HANDLE gInjectionEngineHandle;
NDIS_HANDLE gPoolHandle;

VOID
kkVPNUnload(
	_In_ PDRIVER_OBJECT pDriverObject
	)
{
	UNREFERENCED_PARAMETER(pDriverObject);

	StopInjectionEngine(
		&gInjectionEngineHandle
		);

	StopFilterEngine(
		&gFilteringEngineHandle,
		&gCalloutID,
		&gActiveFilterRangeInbound,
		&gActiveFilterRangeOutbound,
		&gActiveFilterLocal
		);

	NdisFreeNetBufferListPool(gPoolHandle);
}

NTSTATUS
DriverEntry(
	_In_ PDRIVER_OBJECT  pDriverObject,
	_In_ PUNICODE_STRING pRegistryPath
	)
{
	NTSTATUS status;
	WDFDRIVER driver;
	WDF_DRIVER_CONFIG config;
	
	WDF_DRIVER_CONFIG_INIT(&config, kkdrvDriverDeviceAdd);

	status = WdfDriverCreate(
			pDriverObject, 
			pRegistryPath, 
			WDF_NO_OBJECT_ATTRIBUTES, 
			&config, 
			&driver
			);

	if (!NT_SUCCESS(status))
	{
		REPORT_ERROR(WdfDriverCreate, status);
		goto Exit;
	}

	pDriverObject->DriverUnload = kkVPNUnload;

	NET_BUFFER_LIST_POOL_PARAMETERS poolParams;

	RtlZeroMemory(&poolParams, sizeof(poolParams));
	poolParams.Header.Type = NDIS_OBJECT_TYPE_DEFAULT;
	poolParams.Header.Revision = NET_BUFFER_LIST_POOL_PARAMETERS_REVISION_1;
	poolParams.Header.Size = sizeof(poolParams);
	poolParams.fAllocateNetBuffer = TRUE;
	poolParams.PoolTag = KKDRV_TAG;
	poolParams.DataSize = 0;
	gPoolHandle = NdisAllocateNetBufferListPool(NULL, &poolParams);
	if (gPoolHandle == NULL)
	{
		status = STATUS_INSUFFICIENT_RESOURCES;
		REPORT_ERROR(NdisAllocateNetBufferListPool, status);
		goto Exit;
	}

	InitializePacketQueue(&gPacketQueue);

Exit:
	return status;
}

NTSTATUS 
kkdrvDriverDeviceAdd(
	_In_     WDFDRIVER Driver,
	_Inout_  PWDFDEVICE_INIT DeviceInit
	)
{
	UNREFERENCED_PARAMETER(Driver);

	NTSTATUS status = STATUS_SUCCESS;
	WDFDEVICE device;
	WDF_OBJECT_ATTRIBUTES deviceAttributes = { 0 };
	WDF_OBJECT_ATTRIBUTES_INIT(&deviceAttributes);

	deviceAttributes.EvtCleanupCallback = kkdrvCleanupCallback;

	WdfDeviceInitSetDeviceType(DeviceInit, FILE_DEVICE_NETWORK);
	WdfDeviceInitSetCharacteristics(DeviceInit, FILE_DEVICE_SECURE_OPEN, TRUE);
	status = WdfDeviceCreate(&DeviceInit, &deviceAttributes, &device);
	if (!NT_SUCCESS(status))
	{
		REPORT_ERROR(WdfDeviceCreate, status);
		WdfDeviceInitFree(DeviceInit);
		goto Exit;
	}

	status = CreateQueue(&device);
	if (!NT_SUCCESS(status))
	{
		REPORT_ERROR(CreateQueue, status);
		goto Exit;
	}

	DECLARE_CONST_UNICODE_STRING(dosDeviceName, DOS_DEVICE_NAME);
	status = WdfDeviceCreateSymbolicLink(
		device,
		&dosDeviceName
		);
	if (!NT_SUCCESS(status)) {
		REPORT_ERROR(WdfDeviceCreateSymbolicLink, status);
		goto Exit;
	}

	/*status = WdfDeviceCreateDeviceInterface(
		device,
		(LPGUID)&GUID_KKDRV_INTERFACE,
		NULL
		);
	if (!NT_SUCCESS(status)) {
		REPORT_ERROR(WdfDeviceCreateDeviceInterface, status);
		goto Exit;
	}*/

	WdfControlFinishInitializing(device);

	status = StartFilterEngine(
		&gFilteringEngineHandle,
		&gCalloutID,
		device
		);

	status = StartInjectionEngine(
		&gInjectionEngineHandle
		);

Exit:
	return status;
}

VOID 
InitializePacketQueue(
	_Inout_ KKDRV_QUEUE_DATA *packetQueue
	)
{
	InitializeListHead(&packetQueue->queue);
	KeInitializeSpinLock(&packetQueue->queueLock);

	packetQueue->queueLength = 0;
	packetQueue->queueLengthMax = KKDRV_MAX_PACKET_QUEUE_LENGTH;
}

VOID
ClearPacketQueue(
	_Inout_ KKDRV_QUEUE_DATA *packetQueue
	)
{
	while (packetQueue->queueLength > 0)
	{
		PLIST_ENTRY entry = RemoveHeadList(&packetQueue->queue);
		ExFreePoolWithTag(entry, KKDRV_TAG);
		packetQueue->queueLength--;
	}
}

NTSTATUS 
CreateQueue(
	_In_ WDFDEVICE *hDevice
	)
{
	WDFQUEUE writeQueue = NULL;
	WDFQUEUE readQueue = NULL;

	NTSTATUS status = STATUS_SUCCESS;
	WDF_IO_QUEUE_CONFIG ioWriteQueueConfig;
	WDF_IO_QUEUE_CONFIG ioReadQueueConfig;

	WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(
		&ioWriteQueueConfig,
		WdfIoQueueDispatchParallel
		);

	ioWriteQueueConfig.PowerManaged = FALSE;
	ioWriteQueueConfig.EvtIoDeviceControl = kkdrvIoDeviceControl;
	ioWriteQueueConfig.EvtIoWrite = kkdrvIoWrite;

	status = WdfIoQueueCreate(
		*hDevice,
		&ioWriteQueueConfig,
		WDF_NO_OBJECT_ATTRIBUTES,
		&writeQueue
		);
	if (!NT_SUCCESS(status)) 
	{
		REPORT_ERROR(WdfIoQueueCreate(Write), status);
		goto Exit;
	}

	WDF_IO_QUEUE_CONFIG_INIT(
		&ioReadQueueConfig,
		WdfIoQueueDispatchSequential
		);

	ioReadQueueConfig.PowerManaged = FALSE;
	ioReadQueueConfig.EvtIoRead = kkdrvIoRead;

	status = WdfIoQueueCreate(
		*hDevice,
		&ioReadQueueConfig,
		WDF_NO_OBJECT_ATTRIBUTES,
		&readQueue
		);
	if (!NT_SUCCESS(status)) 
	{
		REPORT_ERROR(WdfIoQueueCreate(Read), status);
		goto Exit;
	}

	status = WdfDeviceConfigureRequestDispatching(
		*hDevice,
		readQueue,
		WdfRequestTypeRead
		);
	if (!NT_SUCCESS(status)) 
	{
		REPORT_ERROR(WdfDeviceConfigureRequestDispatching(Read), status);
		goto Exit;
	}

Exit:

	if (!NT_SUCCESS(status))
	{
		if (writeQueue != NULL)
		{
			WdfObjectDelete(writeQueue);
		}

		if (readQueue != NULL)
		{
			WdfObjectDelete(readQueue);
		}
	}

	return status;
}

VOID 
kkdrvIoDeviceControl(
	_In_  WDFQUEUE Queue,
	_In_  WDFREQUEST Request,
	_In_  size_t OutputBufferLength,
	_In_  size_t InputBufferLength,
	_In_  ULONG IoControlCode
	)
{
	UNREFERENCED_PARAMETER(Queue);
	UNREFERENCED_PARAMETER(OutputBufferLength);
	UNREFERENCED_PARAMETER(InputBufferLength);

	NTSTATUS status = STATUS_SUCCESS;
	KKDRV_FILTER_DATA *data = NULL;
	size_t bytesRead = 0;

	switch (IoControlCode) 
	{
		case IOCTL_REGISTER:			
			status = WdfRequestRetrieveInputBuffer(
				Request,
				1,
				(void*)&data,
				&bytesRead
				);
			if (!NT_SUCCESS(status))
			{
				REPORT_ERROR(WdfRequestRetrieveInputBuffer, status);
				goto Complete;
			}

			ClearFilters(
				gFilteringEngineHandle,
				&gActiveFilterRangeInbound,
				&gActiveFilterRangeOutbound,
				&gActiveFilterLocal
				);
			
			ClearPacketQueue(&gPacketQueue);

			status = RegisterFilter(
				data,
				gFilteringEngineHandle,
				&gActiveFilterRangeInbound,
				&gActiveFilterRangeOutbound,
				&gActiveFilterLocal
				);
			if (!NT_SUCCESS(status))
			{
				REPORT_ERROR(RegisterFilter, status);
				goto Complete;
			}
			break;

		case IOCTL_RESTART:
			ClearFilters(
				gFilteringEngineHandle,
				&gActiveFilterRangeInbound,
				&gActiveFilterRangeOutbound,
				&gActiveFilterLocal
				);

			ClearPacketQueue(&gPacketQueue);
			break;

		default:
			status = STATUS_INVALID_DEVICE_REQUEST;
			break;
	}

Complete:
	WdfRequestCompleteWithInformation(Request, status, 0);
}

VOID 
kkdrvCleanupCallback(
	_In_  WDFOBJECT Object
	)
{
	UNREFERENCED_PARAMETER(Object);
}

VOID 
kkdrvIoWrite(
	_In_  WDFQUEUE Queue,
	_In_  WDFREQUEST Request,
	_In_  size_t Length
	)
{
	NTSTATUS status = STATUS_SUCCESS;
	PVOID data = NULL;
	size_t bytesRead = 0;

	UNREFERENCED_PARAMETER(Queue);
	UNREFERENCED_PARAMETER(Length);

	status = WdfRequestRetrieveInputBuffer(
		Request,
		1,
		&data,
		&bytesRead
		);
	if (!NT_SUCCESS(status))
	{
		REPORT_ERROR(WdfRequestRetrieveInputBuffer, status);
		WdfRequestCompleteWithInformation(Request, status, (ULONG_PTR)0);
		return;
	}

	InjectPacketReceive(
		gInjectionEngineHandle,
		data,
		bytesRead,
		&Request
		);
	if (!NT_SUCCESS(status))
	{
		REPORT_ERROR(InjectPacketReceive, status);
		WdfRequestCompleteWithInformation(Request, status, (ULONG_PTR)0);
		return;
	}
}

VOID
kkdrvIoRead(
	_In_  WDFQUEUE Queue,
	_In_  WDFREQUEST Request,
	_In_  size_t Length
	)
{
	UNREFERENCED_PARAMETER(Queue);
	UNREFERENCED_PARAMETER(Length);

	ULONG queueLength;
	KLOCK_QUEUE_HANDLE lockHandle;

	KeAcquireInStackQueuedSpinLock(
		&gPacketQueue.queueLock,
		&lockHandle
		);

	queueLength = gPacketQueue.queueLength;

	KeReleaseInStackQueuedSpinLock(
		&lockHandle
		);

	WdfRequestMarkCancelable(Request, kkdrvRequestCancel);

	if (gPacketQueue.queueLength > 0)
	{
		CompleteRequest(Request);
	}
	else
	{
		gPendingRequest = Request;
	}
}

VOID kkdrvRequestCancel(
	_In_  WDFREQUEST Request
	)
{
	ClearPacketQueue(&gPacketQueue);
	WdfRequestCompleteWithInformation(Request, STATUS_CANCELLED, 0);
}

VOID
CompleteRequest(
	_In_ WDFREQUEST Request
	)
{
	NTSTATUS status = STATUS_SUCCESS;
	BYTE *data = NULL;
	size_t bytesToWrite = 0;
	size_t bytesWritten = 0;
	ULONG packetsToRead = 0;
	size_t packetsToReadSize = 0;
	KLOCK_QUEUE_HANDLE lockHandle;
	PKKDRV_PACKET packet = NULL;
	PLIST_ENTRY packets[KKDRV_MAX_READ_PACKET_COUNT];

	WdfRequestUnmarkCancelable(Request);

	status = WdfRequestRetrieveOutputBuffer(
		Request,
		1,
		&data,
		&bytesToWrite
		);
	if (!NT_SUCCESS(status))
	{
		REPORT_ERROR(WdfRequestRetrieveOutputBuffer, status);
		goto Exit;
	}

	KeAcquireInStackQueuedSpinLock(
		&gPacketQueue.queueLock,
		&lockHandle
		);

	if (gPacketQueue.queueLength > 0)
	{
		PLIST_ENTRY entry = RemoveHeadList(&gPacketQueue.queue);
		PKKDRV_PACKET packet = CONTAINING_RECORD(entry, KKDRV_PACKET, entry);
		size_t packetSize = packet->dataLength;
		while ((packetsToReadSize + packetSize <= bytesToWrite)
			&& (packetsToRead < KKDRV_MAX_READ_PACKET_COUNT)
			&& (gPacketQueue.queueLength > 0))
		{
			packets[packetsToRead] = entry;

			gPacketQueue.queueLength--;
			packetsToReadSize += packetSize;
			packetsToRead++;

			entry = RemoveHeadList(&gPacketQueue.queue);
			packet = CONTAINING_RECORD(entry, KKDRV_PACKET, entry);
			packetSize = packet->dataLength;
		}

		if (packetsToRead != KKDRV_MAX_READ_PACKET_COUNT && gPacketQueue.queueLength > 0)
		{
			InsertHeadList(&gPacketQueue.queue, entry);
		}
	}

	KeReleaseInStackQueuedSpinLock(
		&lockHandle
		);

	if (packetsToRead > 0)
	{
		for (ULONG i = 0; i < packetsToRead; i++)
		{
			packet = CONTAINING_RECORD(packets[i], KKDRV_PACKET, entry);
			RtlCopyBytes(&data[bytesWritten], &packet->data, packet->dataLength);
			bytesWritten += packet->dataLength;

			ExFreePoolWithTag(packets[i], KKDRV_TAG);
		}
	}

Exit:
	WdfRequestCompleteWithInformation(Request, status, bytesWritten);
}