#include <ntddk.h>
#include <wdf.h>

#ifndef _USERMODEBUFFERHANDLER_H_
#define _USERMODEBUFFERHANDLER_H_

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
CopyNblDataToBuffer(
_Inout_ NET_BUFFER_LIST *netBufferListChain,
	_In_ size_t length,
	_In_ UINT32 flags,
	_Out_ PVOID buffer
	);

__inline UINT32
NBLsInChain(
	_In_ NET_BUFFER_LIST * nbl
	);

__inline UINT32
NBsInChain(
	_In_ NET_BUFFER_LIST * nbl
	);

#endif // !_USERMODEBUFFERHANDLER_H_