#ifndef PERFIOCTL_H
#define PERFIOCTL_H

#include <linux/types.h>
#include <linux/ioctl.h>

enum  {
    SWUI = 0,
    HWUI,
    GLSURFACE
};

typedef struct _FPSGO_PACKAGE {
    __u32 tid;
    union {
        __u64 frame_time;
        __u64 bufID;
    };
    union {
        __u32 start;
        __u32 connectedAPI;
        __u32 render_method;
    };
} FPSGO_PACKAGE;

#define FPSGO_QUEUE                  _IOW('g', 1, FPSGO_PACKAGE)
#define FPSGO_DEQUEUE                _IOW('g', 3, FPSGO_PACKAGE)
#define FPSGO_VSYNC                  _IOW('g', 5, FPSGO_PACKAGE)
#define FPSGO_ACT_SWITCH             _IOW('g', 8, FPSGO_PACKAGE)
#define FPSGO_GAME                   _IOW('g', 9, FPSGO_PACKAGE)
#define FPSGO_TOUCH                  _IOW('g', 10, FPSGO_PACKAGE)
#define FPSGO_FRAME_COMPLETE         _IOW('g', 11, FPSGO_PACKAGE)
#define FPSGO_INTENDED_VSYNC         _IOW('g', 12, FPSGO_PACKAGE)
#define FPSGO_NO_RENDER              _IOW('g', 13, FPSGO_PACKAGE)
#define FPSGO_SWAP_BUFFER            _IOW('g', 14, FPSGO_PACKAGE)
#define FPSGO_QUEUE_CONNECT          _IOW('g', 15, FPSGO_PACKAGE)


#endif
