#ifndef _COMMONSTRUCTURES_H_
#define _COMMONSTRUCTURES_H_

#define IOCTL_REGISTER			0x1
#define IOCTL_RESTART			0x2
#define IOCTL_SET_EVENT_HANDLE	0x3

typedef struct FILTER_IP_RANGE_
{
	unsigned __int32 low;
	unsigned __int32 high;
} FILTER_IP_RANGE;

#endif //!_COMMONSTRUCTURES_H_