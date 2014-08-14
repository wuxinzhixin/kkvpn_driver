#include <ntddk.h>
#include <wdf.h>

#ifndef _INJECTIONENGINE_H_
#define _INJECTIONENGINE_H_

#pragma warning(push)
#pragma warning(disable:4214)       // bit field types other than int

typedef struct IP4HDR_
{
	UINT8 HdrLength : 4;
	UINT8 Version : 4;
	UINT8 TOS;
	UINT16 Length;
	UINT16 Id;
	UINT16 FragOff0;
	UINT8 TTL;
	UINT8 Protocol;
	UINT16 Checksum;
	UINT32 SrcAddr;
	UINT32 DstAddr;
}IP4HDR, *PIP4HDR;

#pragma warning(pop)

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
	_In_ size_t length,
	_In_ WDFREQUEST *request
	);

NTSTATUS
InsertDataToNBL(
	_In_ PVOID data,
	_In_ size_t length,
	_Out_ NET_BUFFER_LIST **nbl
	);

void NTAPI 
InjectComplete(
	_In_     VOID *context,
	_Inout_  NET_BUFFER_LIST *nbl,
	_In_     BOOLEAN dispatchLevel
	);

NTSTATUS
GetInterfaceData();

#endif // !_INJECTIONENGINE_H_