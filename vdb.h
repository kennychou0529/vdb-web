#include <stdint.h>

#include "tcp.h"
#define vdb_sv_port 27042

enum vdb_cmd_type_t
{
    vdb_cmd_type_begin = 1, // signal the beginning of a frame (incoming drawcalls)
    vdb_cmd_type_end,       // signal the end of the frame (no more drawcalls)

    vdb_cmd_type_point,
    vdb_cmd_type_line,

    vdb_cmd_type_color,
    vdb_cmd_type_clear
};

struct vdb_cmd_point_t
{
    float x;
    float y;
};

struct vdb_cmd_line_t
{
    float x1;
    float y1;
    float x2;
    float y2;
};

struct vdb_cmd_t
{
    uint8_t type;
    union
    {
        vdb_cmd_point_t point;
        vdb_cmd_line_t line;
    };
};

enum vdb_event_type_t
{
    vdb_event_type_step_once = 1,
    vdb_event_type_modify
};

struct vdb_event_t
{
    uint8_t type;
    // ...
    // uint32_t value;
    // int32_t value;
    // float value;
};
