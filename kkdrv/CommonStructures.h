#ifndef _COMMONSTRUCTURES_H_
#define _COMMONSTRUCTURES_H_

#define IOCTL_REGISTER			CTL_CODE(0x87DC, 0x1, METHOD_BUFFERED, FILE_ANY_ACCESS)	//FILE_DEVICE_NETWORK
#define IOCTL_RESTART			CTL_CODE(0x87DC, 0x2, METHOD_BUFFERED, FILE_ANY_ACCESS)
//#define IOCTL_SET_EVENT_HANDLE	CTL_CODE(0x87DC, 0x3, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct KKDRV_FILTER_DATA_
{
	unsigned __int32 low;
	unsigned __int32 high;
	HANDLE event;
} KKDRV_FILTER_DATA;

#endif //!_COMMONSTRUCTURES_H_