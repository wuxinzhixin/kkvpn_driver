#include "InjectionEngine.h"

#define PACKET_IPV4_LENGTH_OFFSET (ULONG)0x2

NTSTATUS 
StartInjectionEngine(
	_In_ HANDLE *engineHandle
	)
{
	NTSTATUS status = STATUS_SUCCESS;

	status = FwpsInjectionHandleCreate(
		AF_INET, 
		FWPS_INJECTION_TYPE_NETWORK,
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

VOID 
StopInjectionEngine(
	_In_ HANDLE *engineHandle
	)
{
	NTSTATUS status = STATUS_SUCCESS;

	if (!(*engineHandle))
	{
		return;
	}

	status = FwpsInjectionHandleDestroy(*engineHandle);
	if (!NT_SUCCESS(status))
	{
		REPORT_ERROR(FwpsInjectionHandleDestroy, status);
		return;
	}

	*engineHandle = NULL;
}

NTSTATUS 
InjectPacketReceive(
	_In_ HANDLE *engineHandle,
	_In_ PVOID data,
	_In_ size_t length,
	_In_ WDFREQUEST *request
	)
{
	NTSTATUS status = STATUS_SUCCESS;
	PNET_BUFFER_LIST nbl;
	ULONG offset = 0;

	while (offset < length)
	{
		CHAR *currentAddr = (CHAR*)data + offset;
		USHORT packetLength = *((USHORT*)(currentAddr + PACKET_IPV4_LENGTH_OFFSET));
		InvertBytes(&packetLength);

		status = InsertDataToNBL(
			(PVOID)currentAddr,
			packetLength,
			&nbl
			);
		if (!NT_SUCCESS(status))
		{
			REPORT_ERROR(InsertDataToNBL, status);
			goto Exit;
		}

		status = FwpsInjectNetworkSendAsync(
			engineHandle,
			NULL,
			0,
			UNSPECIFIED_COMPARTMENT_ID,
			nbl,
			InjectComplete,
			NULL
			);
		if (!NT_SUCCESS(status))
		{
			REPORT_ERROR(FwpsInjectNetworkSendAsync, status);
			goto Exit;
		}

		offset += (ULONG)packetLength;
	}

Exit:
	WdfRequestCompleteWithInformation(*request, status, length);
	return status;
}

NTSTATUS
InsertDataToNBL(
	_In_ PVOID data,
	_In_ size_t length,
	_Out_ NET_BUFFER_LIST **nbl
	)
{
	PNET_BUFFER_LIST nblOut = NULL;
	NTSTATUS status = STATUS_SUCCESS;
	PMDL mdl = NULL;

	PVOID buffer = ExAllocatePoolWithTag(
		NonPagedPool, 
		length,
		KKDRV_TAG
		);
	if (!buffer)
	{
		status = STATUS_INSUFFICIENT_RESOURCES;
		REPORT_ERROR(ExAllocatePoolWithTag, status);		
		goto Exit;
	}

	RtlCopyBytes(buffer, data, length);

	mdl = IoAllocateMdl(
		buffer,
		(ULONG)length,
		FALSE,
		FALSE,
		NULL
		);
	if (!mdl)
	{
		status = STATUS_INSUFFICIENT_RESOURCES;
		REPORT_ERROR(IoAllocateMdl, status);
		goto Exit;
	}

	MmBuildMdlForNonPagedPool(mdl);

	status = FwpsAllocateNetBufferAndNetBufferList(
		gPoolHandle,
		0,
		0,
		mdl,
		0,
		length,
		&nblOut
		);
	if (!NT_SUCCESS(status))
	{
		REPORT_ERROR(FwpsAllocateNetBufferAndNetBufferList, status);
		goto Exit;
	}

	*nbl = nblOut;

Exit:

	if (!NT_SUCCESS(status))
	{
		if (nblOut)
		{
			FwpsFreeNetBufferList(nblOut);
		}
		if (mdl)
		{
			IoFreeMdl(mdl);
		}
		if (buffer)
		{
			ExFreePoolWithTag(buffer, KKDRV_TAG);
		}
	}

	return status;
}


void NTAPI 
InjectComplete(
	_In_     VOID *context,
	_Inout_  NET_BUFFER_LIST *nbl,
	_In_     BOOLEAN dispatchLevel
	)
{
	UNREFERENCED_PARAMETER(dispatchLevel);
	UNREFERENCED_PARAMETER(context);

	NET_BUFFER *nb = NET_BUFFER_LIST_FIRST_NB(nbl);
	PMDL mdl = NET_BUFFER_FIRST_MDL(nb);
	PVOID buffer = MmGetSystemAddressForMdlSafe(mdl, NormalPagePriority);
	
	FwpsFreeNetBufferList(nbl);
	IoFreeMdl(mdl);
	ExFreePoolWithTag(buffer, KKDRV_TAG);
}

__inline
VOID
InvertBytes(
	USHORT *data
	)
{
	USHORT temp = *data;
	*data = (temp << 8) + (temp >> 8);
}

/*status = FwpsInjectNetworkReceiveAsync(
engineHandle,
NULL,
0,
UNSPECIFIED_COMPARTMENT_ID,
10,
0,
nbl,
InjectComplete,
*request
);*/