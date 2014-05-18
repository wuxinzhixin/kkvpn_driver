#include <ntddk.h>
#include <wdf.h>

//#pragma warning(push)
//#pragma warning(disable:4201)       // unnamed struct/union
//
//#include <fwpsk.h>
//
//#pragma warning(pop)
//
//#include <fwpmk.h>

NTSTATUS StartInjectionEngine(
	_In_ HANDLE *engineHandle
	);
VOID StopInjectionEngine(
	_In_ HANDLE *engineHandle
	);
NTSTATUS InjectPacketReceive(
	_In_ HANDLE *engineHandle,
	_In_ PVOID data,
	_In_ size_t length
	);