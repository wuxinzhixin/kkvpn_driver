#include <ntddk.h>
#include <wdf.h>

#pragma warning(push)
#pragma warning(disable:4201)       // unnamed struct/union

#include <fwpsk.h>

#pragma warning(pop)

#include <fwpmk.h>

#include "InjectionEngine.h"
#include "DriverInit.h"

NTSTATUS 
StartInjectionEngine(
	_In_ HANDLE *engineHandle
	)
{
	NTSTATUS status = STATUS_SUCCESS;

	status = GetInterfaceData();
	if (!NT_SUCCESS(status))
	{
		REPORT_ERROR(GetInterfaceData, status);
		goto Exit;
	}

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
		return;

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

	NET_BUFFER_LIST *nbl = NULL;
	
	status = InsertDataToNBL(
		data,
		length,
		&nbl
		);
	if (!NT_SUCCESS(status))
	{
		REPORT_ERROR(InsertDataToNBL, status);
		return status;
	}

	//IF_INDEX If = gIfIndex;
	//IF_INDEX subIf = gSubIfIndex;

	//status = FwpsInjectNetworkReceiveAsync(
	//	engineHandle,
	//	NULL,
	//	0,
	//	UNSPECIFIED_COMPARTMENT_ID,
	//	If, //gIfIndex,
	//	subIf, //gSubIfIndex,
	//	nbl,
	//	InjectComplete,
	//	request
	//	);

	status = FwpsInjectNetworkSendAsync(
		engineHandle,
		NULL,
		0,
		UNSPECIFIED_COMPARTMENT_ID,
		nbl,
		InjectComplete,
		*request
		);

	/*status = FwpsInjectTransportReceiveAsync0(
		engineHandle,
		NULL,
		NULL,
		0,
		AF_INET,
		UNSPECIFIED_COMPARTMENT_ID,
		gIfIndex,
		gSubIfIndex,
		nbl,
		InjectComplete,
		request
		);*/
		
	if (!NT_SUCCESS(status))
	{
		REPORT_ERROR(FwpsInjectNetworkSendAsync, status);
		return status;
	}

	return status;
}

NTSTATUS
InsertDataToNBL(
	_In_ PVOID data,
	_In_ size_t length,
	_Out_ NET_BUFFER_LIST **nbl
	)
{
	NTSTATUS status = STATUS_SUCCESS;
	PMDL mdl = NULL;
	//PIP4HDR ip4header = (PIP4HDR)data;
	//ULONG headerSize = ip4header->HdrLength * 4; //(((char*)buffer)[0] & 0xF) * 4;

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

	status = FwpsAllocateNetBufferAndNetBufferList0(
		gPoolHandle,
		0,
		0,
		mdl,
		0,
		length,
		nbl
		);
	if (!NT_SUCCESS(status))
	{
		REPORT_ERROR(FwpsAllocateNetBufferAndNetBufferList, status);
		goto Exit;
	}

	/*FwpsConstructIpHeaderForTransportPacket0(
		*nbl,
		headerSize,
		AF_INET,
		(UCHAR*)&ip4header->SrcAddr,
		(UCHAR*)&ip4header->DstAddr,
		(IPPROTO)ip4header->Protocol,
		0,
		NULL,
		0,
		0,
		NULL,
		gIfIndex,
		gSubIfIndex
		);*/

	/*NdisAdvanceNetBufferListDataStart(
		*nbl,
		headerSize,
		FALSE,
		NULL
		);*/

Exit:

	if (!NT_SUCCESS(status))
	{
		if (nbl)
		{
			FwpsFreeNetBufferList(*nbl);
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
	WDFREQUEST request = (WDFREQUEST)context;

	NET_BUFFER *nb = NET_BUFFER_LIST_FIRST_NB(nbl);
	PMDL mdl = NET_BUFFER_FIRST_MDL(nb);
	PVOID buffer = MmGetSystemAddressForMdlSafe(mdl, NormalPagePriority);
	
	FwpsFreeNetBufferList(nbl);
	IoFreeMdl(mdl);
	ExFreePoolWithTag(buffer, KKDRV_TAG);

	WdfRequestCompleteWithInformation(request, NET_BUFFER_LIST_STATUS(nbl), NET_BUFFER_DATA_LENGTH(nb));
}

NTSTATUS
GetInterfaceData()
{
	NTSTATUS status = STATUS_SUCCESS;

	gIfIndex = 0;
	gSubIfIndex = 0;

	return status;
}