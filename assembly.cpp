#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif
#include <stdio.h>
#include <stdlib.h>

// @ This can be really huge... Maybe compress vdb_cmd_t?
#define vdb_max_cmds_per_frame (1280*720)

struct vdb_cmd_t
{
    int palette;
    int width;
    int color;
    int mode;
    union
    {
        struct point2_t { float x, y; } point2;
        struct point3_t { float x, y, z; } point3;
        struct line2_t  { float x1, y1, x2, y2; } line2;
        struct line3_t  { float x1, y1, z1, x2, y2, z2; } line3;
        struct tri2_t   { float x1, y1, x2, y2, x3, y3; } tri2;
        struct tri3_t   { float x1, y1, z1, x2, y2, z2, x3, y3, z3; } tri3;
        struct text_t   { char *buffer; } text;
    };
};

static vdb_cmd_t *vdb_cmds = 0;

void vdb_point(float x, float y, int width, int color, int palette)
{

}

void vdb_line(float x1, float y1, float x2, float y2, int width, int color, int palette)
{

}

int main()
{
    vdb_cmds = (vdb_cmd_t*)calloc(vdb_max_cmds_per_frame, sizeof(vdb_cmd_t));
    if (!vdb_cmds)
    {
        printf("[vdb] Not enough memory! Lower vdb_max_cmds_per_frame constant.\n");
        return 0;
    }
    printf("[vdb] Got %.2f megabytes.\n", vdb_max_cmds_per_frame*sizeof(vdb_cmd_t)/(float)(1024*1024));
    free(vdb_cmds);
    return 0;
}
