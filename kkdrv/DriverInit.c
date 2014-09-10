#include "DriverInit.h"

#include "FilteringEngine.h"
#include "InjectionEngine.h"
#include "UserModeBufferHandler.h"

DECLARE_CONST_UNICODE_STRING(
	SDDL_DEVOBJ_SYS_ALL_ADM_RWX_WORLD_R_RES_R,
	L"D:P(A;;GA;;;SY)(A;;GRGWGX;;;BA)(A;;GR;;;WD)(A;;GR;;;RC)"
	);
	
KKDRV_QUEUE_DATA gPacketQueue;
UINT64 gActiveFilter;
UINT32 gCalloutID;
WDFQUEUE gQueue;
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
		&gActiveFilter
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

	status = CreateQueue(&device, &gQueue);
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
		&gActiveFilter,
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
	_In_ WDFDEVICE *hDevice,
	_Out_ WDFQUEUE *hQueue
	)
{
	hQueue = NULL;

	NTSTATUS status = STATUS_SUCCESS;
	WDF_IO_QUEUE_CONFIG ioQueueConfig;

	WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(
		&ioQueueConfig,
		WdfIoQueueDispatchSequential
		);

	ioQueueConfig.PowerManaged = FALSE;
	ioQueueConfig.EvtIoDeviceControl = kkdrvIoDeviceControl;
	ioQueueConfig.EvtIoWrite = kkdrvIoWrite;
	ioQueueConfig.EvtIoRead = kkdrvIoRead;

	status = WdfIoQueueCreate(
		*hDevice,
		&ioQueueConfig,
		WDF_NO_OBJECT_ATTRIBUTES,
		hQueue
		);
	if (!NT_SUCCESS(status)) {
		REPORT_ERROR(WdfIoQueueCreate, status);
		goto Exit;
	}

	/*WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(
		&ioQueueConfigRead,
		WdfIoQueueDispatchSequential
		);

	ioQueueConfigRead.PowerManaged = FALSE;
	ioQueueConfigRead.EvtIoRead = kkdrvIoRead;

	status = WdfIoQueueCreate(
		*hDevice,
		&ioQueueConfigRead,
		WDF_NO_OBJECT_ATTRIBUTES,
		hQueueRead
		);
	if (!NT_SUCCESS(status)) {
		REPORT_ERROR(WdfIoQueueCreate, status);
		goto Exit;
	}
*/
Exit:

	if (!NT_SUCCESS(status))
	{
		if (hQueue != NULL)
		{
			WdfObjectDelete(hQueue);
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
	UNREFERENCED_PARAMETER(OutputBufferLength);
	UNREFERENCED_PARAMETER(InputBufferLength);

	NTSTATUS status = STATUS_SUCCESS;
	WDFDEVICE device = WdfIoQueueGetDevice(Queue);
	KKDRV_FILTER_DATA *data = NULL;
	size_t bytes_read = 0;
	size_t bytes_returned = 0;

	switch (IoControlCode) 
	{
		case IOCTL_REGISTER:
			status = WdfRequestRetrieveInputBuffer(
				Request,
				1,
				(void*)&data,
				&bytes_read
				);
			if (!NT_SUCCESS(status))
			{
				REPORT_ERROR(WdfRequestRetrieveInputBuffer, status);
				goto Complete;
			}

			ClearPacketQueue(&gPacketQueue);

			if (gActiveFilter)
			{
				FwpmFilterDeleteById(gFilteringEngineHandle, gActiveFilter);
				gActiveFilter = 0;
			}
			
			status = RegisterFilter(
				data,
				gFilteringEngineHandle,
				&gActiveFilter
				);
			if (!NT_SUCCESS(status))
			{
				REPORT_ERROR(RegisterFilter, status);
				goto Complete;
			}
			break;

		case IOCTL_RESTART:
			bytes_returned = 0;

			status = RestartEngine(
				&gFilteringEngineHandle,
				&gCalloutID,
				&gActiveFilter,
				device
				);
			break;

		default:
			status = STATUS_INVALID_DEVICE_REQUEST;
			break;
	}

Complete:
	WdfRequestCompleteWithInformation(Request, status, bytes_returned);
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

	NTSTATUS status = STATUS_SUCCESS;
	BYTE *data = NULL;
	size_t bytesToWrite = 0;
	size_t bytesWritten = 0;
	ULONG packetsToRead = 0;
	size_t packetsToReadSize = 0;
	KLOCK_QUEUE_HANDLE lockHandle;
	PKKDRV_PACKET packet = NULL;
	PLIST_ENTRY packets[KKDRV_MAX_READ_PACKET_COUNT];

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
		//PLIST_ENTRY entry = gPacketQueue.queue.Flink;
		//PKKDRV_PACKET packet = CONTAINING_RECORD(entry, KKDRV_PACKET, entry);//((KKDRV_PACKET*)entry)->dataLength;
		//size_t packetSize = packet->dataLength;
		//while ((packetsToReadSize + packetSize <= bytesToWrite) 
		//	&& (packetsToRead < KKDRV_MAX_READ_PACKET_COUNT) 
		//	&& (gPacketQueue.queueLength > 0))
		//{
		//	packets[packetsToRead] = RemoveHeadList(&gPacketQueue.queue);

		//	gPacketQueue.queueLength--;
		//	packetsToReadSize += packetSize;
		//	packetsToRead++;

		//	packet = CONTAINING_RECORD(packets[packetsToRead], KKDRV_PACKET, entry);
		//	packetSize = packet->dataLength;
		//}

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