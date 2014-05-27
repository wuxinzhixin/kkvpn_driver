#ifndef _COMMONSTRUCTURES_H_
#define _COMMONSTRUCTURES_H_

#define IOCTL_REGISTER			CTL_CODE(FILE_DEVICE_NETWORK, 0x1, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_RESTART			CTL_CODE(FILE_DEVICE_NETWORK, 0x2, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct KKDRV_FILTER_DATA_
{
	unsigned __int32 low;
	unsigned __int32 high;
	unsigned __int64 event_receive;
	unsigned __int64 event_completed;
} KKDRV_FILTER_DATA;

typedef struct KKDRV_NET_BUFFER_FLAT_
{
	unsigned __int32 length;
	char buffer;
} KKDRV_NET_BUFFER_FLAT;

#endif //!_COMMONSTRUCTURES_H_