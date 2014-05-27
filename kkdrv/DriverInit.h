#include <ntddk.h>
#include <wdf.h>
#include <initguid.h>

#ifndef _DRIVERINIT_H_
#define _DRIVERINIT_H_

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
//#define IOCTL_SET_EVENT_HANDLE	CTL_CODE(FILE_DEVICE_NETWORK, 0x3, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define _DRVNAME "kkVPN: "
#define _DRVVER "0.1.0"
#define DOS_DEVICE_NAME  L"\\DosDevices\\kkdrv"

#define UM_BUFFER_PAGE_COUNT (16)
#define UM_BUFFER_PAGE_SIZE (PAGE_SIZE*UM_BUFFER_PAGE_COUNT)

#define REPORT_ERROR(func, status) DbgPrint(_DRVNAME "Error at " #func " - status: 0x%08x.\n", status);

typedef struct KKDRV_FILTER_DATA_
{
	unsigned __int32 low;
	unsigned __int32 high;
	HANDLE event_receive;
	HANDLE event_completed;
} KKDRV_FILTER_DATA;

#pragma pack (1)
typedef struct KKDRV_NB_METADATA_
{
	UINT16 length;
	UINT8 protocol;
} KKDRV_NB_METADATA;
#pragma pack ()

typedef struct KKDRV_NET_BUFFER_FLAT_
{
	UINT32 length;
	char buffer;
	//KKDRV_NB_METADATA metadata;
} KKDRV_NET_BUFFER_FLAT;

typedef struct KKDRV_NBL_QUEUE_
{
	KSPIN_LOCK lock;

	BOOLEAN awake;

	PNET_BUFFER_LIST nblHead;
	PNET_BUFFER_LIST nblTail;

	size_t length;
	UINT32 flags;
}
KKDRV_NBL_QUEUE, *PKKDRV_NBL_QUEUE;

typedef struct KKDRV_WORKER_DATA_
{
	KEVENT event;
	PKEVENT userevent_receive;
	PKEVENT userevent_complete;
	PVOID stoppingThread;
	PVOID mem;
	PKKDRV_NBL_QUEUE queue;
} 
KKDRV_WORKER_DATA;

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD kkdrvDriverDeviceAdd;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL kkdrvIoDeviceControl;
EVT_WDF_IO_QUEUE_IO_READ kkdrvIoRead;
EVT_WDF_IO_QUEUE_IO_WRITE kkdrvIoWrite;
EVT_WDF_OBJECT_CONTEXT_CLEANUP kkdrvCleanupCallback;

VOID 
kkVPNUnload();

NTSTATUS 
CreateQueue();

NTSTATUS 
CreateAndMapMemory(
	PMDL* PMemMdl, 
	PVOID* UserVa
	);

void 
UnMapAndFreeMemory(
	PMDL PMdl, 
	PVOID UserVa
	);

extern BOOLEAN gStoppingThread;
extern KKDRV_WORKER_DATA gParams;

extern PVOID gUASharedMem;
extern PVOID gSharedMem;
extern HANDLE gFilteringEngineHandle;
extern HANDLE gInjectionEngineHandle;

extern UINT64 gActiveFilter;
extern UINT32 gCalloutID;

extern PKEVENT gBufferEvent;
extern PKEVENT gUserModeEventReceive;
extern PKEVENT gUserModeEventComplete;
extern PMDL gMdl;

extern KKDRV_NBL_QUEUE gNblQueue;

#endif // !_DRIVERINIT_H_