#include <ntddk.h>
#include <wdf.h>

#ifndef _INJECTIONENGINE_H_
#define _INJECTIONENGINE_H_

NTSTATUS 
StartInjectionEngine(
	_In_ HANDLE *engineHandle
	);

VOID 
StopInjectionEngine(
	_In_ HANDLE *engineHandle
	);

NTSTATUS 
InjectPacketReceive(
	_In_ HANDLE *engineHandle,
	_In_ PVOID data,
	_In_ size_t length
	);

#endif // !_INJECTIONENGINE_H_