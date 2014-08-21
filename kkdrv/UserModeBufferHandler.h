#ifndef _USERMODEBUFFERHANDLER_H_
#define _USERMODEBUFFERHANDLER_H_

#include "DriverInit.h"

extern HANDLE gWorkerThreadHandle;
extern PVOID gWorkerThreadObj;

NTSTATUS 
StartWorker(
	_Out_ PKEVENT *NtfyEvent
	);

VOID 
WorkerRoutine(
	_In_ PVOID params
	);

VOID 
StopWorker();

NTSTATUS
CopyPacketDataToBuffer(
	_Inout_ PKKDRV_PACKET head,
	_In_ size_t length,
	_Out_ PVOID buffer
	);

#endif // !_USERMODEBUFFERHANDLER_H_