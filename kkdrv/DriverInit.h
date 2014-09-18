#ifndef _DRIVERINIT_H_
#define _DRIVERINIT_H_

#include <ntddk.h>
#include <wdf.h>
#include <initguid.h>


#pragma warning(push)
#pragma warning(disable:4201)       // unnamed struct/union
#include <fwpsk.h>
#pragma warning(pop)

#include <fwpmk.h>

#define KKDRV_TAG 'vdkK'

// {4F2ADB03-E310-42B5-BEA7-4EB8C00BCA0C}
DEFINE_GUID(GUID_KKDRV_INTERFACE,
	0x4f2adb03, 0xe310, 0x42b5, 0xbe, 0xa7, 0x4e, 0xb8, 0xc0, 0xb, 0xca, 0xc);

// {E6477825-63A3-4AE7-A9A6-7FC298E33F1A}
DEFINE_GUID(GUID_KKDRV_CALLOUT,
	0xe6477825, 0x63a3, 0x4ae7, 0xa9, 0xa6, 0x7f, 0xc2, 0x98, 0xe3, 0x3f, 0x1a);

// {124BB092-E631-4B92-AD76-BB9389CE4BB3}
DEFINE_GUID(GUID_KKDRV_SUBLAYER,
	0x124bb092, 0xe631, 0x4b92, 0xad, 0x76, 0xbb, 0x93, 0x89, 0xce, 0x4b, 0xb3);


#define IOCTL_REGISTER			CTL_CODE(FILE_DEVICE_NETWORK, 0x1, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_RESTART			CTL_CODE(FILE_DEVICE_NETWORK, 0x2, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define KKDRV_DIRECTION_RECEIVE		0
#define KKDRV_DIRECTION_SEND		1

#define KKDRV_MAX_PACKET_QUEUE_LENGTH 1024
#define KKDRV_MAX_READ_PACKET_COUNT 512

#define _DRVNAME "kkVPN: "
#define _DRVVER "0.1.0"
#define DOS_DEVICE_NAME  L"\\DosDevices\\kkdrv"

#define UM_BUFFER_PAGE_COUNT (16)
#define UM_BUFFER_PAGE_SIZE (PAGE_SIZE*UM_BUFFER_PAGE_COUNT)
#define KKDRV_PACKET_SIZE               (sizeof(KKDRV_PACKET))

#define REPORT_ERROR(func, status) DbgPrint(_DRVNAME "Error at " #func " - status: 0x%08x.\n", status);

typedef struct KKDRV_FILTER_DATA_
{
	unsigned __int32 low;
	unsigned __int32 high;
}
KKDRV_FILTER_DATA;

#pragma warning(push)
#pragma warning(disable:4201) 
typedef struct KKDRV_NET_BUFFER_FLAT_
{
	union
	{
		UINT32 length;
		UINT32 direction;
	};
	char buffer;
} 
KKDRV_NET_BUFFER_FLAT;
#pragma warning(pop)

typedef struct KKDRV_PACKET_ KKDRV_PACKET, *PKKDRV_PACKET;

typedef struct KKDRV_PACKET_
{
	LIST_ENTRY entry;
	size_t dataLength;
	char data;
} 
KKDRV_PACKET, *PKKDRV_PACKET;

typedef struct KKDRV_QUEUE_DATA_
{
	KSPIN_LOCK queueLock;
	ULONG queueLength;
	ULONG queueLengthMax;
	LIST_ENTRY queue;
} 
KKDRV_QUEUE_DATA;

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD kkdrvDriverDeviceAdd;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL kkdrvIoDeviceControl;
EVT_WDF_IO_QUEUE_IO_WRITE kkdrvIoWrite;
EVT_WDF_IO_QUEUE_IO_READ kkdrvIoRead;
EVT_WDF_OBJECT_CONTEXT_CLEANUP kkdrvCleanupCallback;

VOID 
kkVPNUnload(
	_In_ PDRIVER_OBJECT pDriverObject
	);

NTSTATUS
CreateQueue(
	_In_ WDFDEVICE *hDevice
	);

VOID
InitializePacketQueue(
	_Inout_ KKDRV_QUEUE_DATA *packetQueue
	);

VOID
ClearPacketQueue(
	_Inout_ KKDRV_QUEUE_DATA *packetQueue
	);

VOID
CompleteRequest(
	_In_ WDFREQUEST Request
	);

extern NDIS_HANDLE gPoolHandle;
extern HANDLE gFilteringEngineHandle;
extern HANDLE gInjectionEngineHandle;
extern WDFREQUEST gPendingRequest;
extern UINT64 gActiveFilter;
extern UINT32 gCalloutID;
extern KKDRV_QUEUE_DATA gPacketQueue;

#endif // !_DRIVERINIT_H_