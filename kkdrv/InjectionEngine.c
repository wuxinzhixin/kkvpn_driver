#include <ntddk.h>
#include <wdf.h>

#pragma warning(push)
#pragma warning(disable:4201)       // unnamed struct/union

#include <fwpsk.h>

#pragma warning(pop)

#include <fwpmk.h>

#include "InjectionEngine.h"
#include "DriverInit.h"

NTSTATUS StartInjectionEngine(
	_In_ HANDLE *engineHandle
	)
{
	NTSTATUS status = STATUS_SUCCESS;

	status = FwpsInjectionHandleCreate(
		AF_INET, 
		FWPS_INJECTION_TYPE_TRANSPORT,
		engineHandle
		);
	if (!NT_SUCCESS(status))
	{
		REPORT_ERROR(FwpsInjectionHandleCreate, status);
		goto Exit;
	}

Exit:
	return status;
}

VOID StopInjectionEngine(
	_In_ HANDLE *engineHandle
	)
{
	NTSTATUS status = STATUS_SUCCESS;

	if (!(*engineHandle))
		return;

	status = FwpsInjectionHandleDestroy(*engineHandle);
	if (!NT_SUCCESS(status))
	{
		REPORT_ERROR(FwpsInjectionHandleDestroy, status);
		return;
	}

	*engineHandle = NULL;
}

NTSTATUS InjectPacketReceive(
	_In_ HANDLE *engineHandle,
	_In_ PVOID data,
	_In_ size_t length
	)
{
	NTSTATUS status = STATUS_SUCCESS;

	UNREFERENCED_PARAMETER(engineHandle);
	UNREFERENCED_PARAMETER(data);
	UNREFERENCED_PARAMETER(length);

	return status;
}

