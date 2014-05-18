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

#define _DRVNAME "kkVPN: "
#define _DRVVER "0.1.0"
#define DOS_DEVICE_NAME  L"\\DosDevices\\kkdrv"

#define REPORT_ERROR(func, status) DbgPrint(_DRVNAME "Error at " #func " - status: 0x%08x.\n", status);

typedef struct
{
	PKEVENT event;
	PKEVENT userevent;
	PVOID driverUnloading;
	PVOID mem;
} 
KKDRV_WORKER_DATA;

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD kkdrvDriverDeviceAdd;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL kkdrvIoDeviceControl;
EVT_WDF_IO_QUEUE_IO_READ kkdrvIoRead;
EVT_WDF_IO_QUEUE_IO_WRITE kkdrvIoWrite;
EVT_WDF_OBJECT_CONTEXT_CLEANUP kkdrvCleanupCallback;
VOID kkVPNUnload();
NTSTATUS CreateQueue();
NTSTATUS CreateAndMapMemory(PMDL* PMemMdl, PVOID* UserVa);
void UnMapAndFreeMemory(PMDL PMdl, PVOID UserVa);

extern BOOLEAN gDriverStopping;
extern KKDRV_WORKER_DATA gParams;

extern PVOID gUASharedMem;
extern PVOID gSharedMem;
extern HANDLE gFilteringEngineHandle;
extern HANDLE gInjectionEngineHandle;

extern UINT64 gActiveFilter;
extern UINT32 gCalloutID;

extern PKEVENT gBufferEvent;
extern PKEVENT gUserModeEvent;
extern PMDL gMdl;

#endif // !_DRIVERINIT_H_