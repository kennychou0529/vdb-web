#define vdb_color_mode_primary  0
#define vdb_color_mode_ramp     1

#define vdb_mode_point2         1
#define vdb_mode_point3         2
#define vdb_mode_line2          3
#define vdb_mode_line3          4
#define vdb_mode_fill_rect      5
#define vdb_mode_circle         6
#define vdb_mode_image_rgb8     7
#define vdb_mode_slider         254

static unsigned char vdb_current_color_mode = 0;
static unsigned char vdb_current_color = 0;
static unsigned char vdb_current_alpha = 0;

static float *vdb_point_size = 0;
static float *vdb_line_size = 0;
static float *vdb_alpha_value = 0;
static unsigned char *vdb_nice_points = 0;

static float vdb_xrange_left = -1.0f;
static float vdb_xrange_right = +1.0f;
static float vdb_yrange_bottom = -1.0f;
static float vdb_yrange_top = +1.0f;
static float vdb_zrange_far = -1.0f;
static float vdb_zrange_near = +1.0f;

// This function is automatically called on a successful vdb_begin call
// to let you set up whatever state before beginning to submit commands.
void vdb_begin_submission()
{
    vdb_current_color_mode = vdb_color_mode_primary;
    vdb_current_alpha = 0;
    vdb_xrange_left = -1.0f;
    vdb_xrange_right = +1.0f;
    vdb_yrange_bottom = -1.0f;
    vdb_yrange_top = +1.0f;
    vdb_zrange_far = -1.0f;
    vdb_zrange_near = +1.0f;

    // Reserve the immediately first portion of the work buffer
    // for geometry-global variables
    vdb_point_size = vdb_push_r32(4.0f);
    vdb_line_size = vdb_push_r32(2.0f);
    vdb_alpha_value = vdb_push_r32(0.5f);
    vdb_nice_points = vdb_push_u08(0);
}

// This function is automatically called on vdb_end, right before
// the workload is sent off the the network thread.
void vdb_end_submission()
{
    // Mark events as handled
    vdb_shared->status.mouse_click = 0;
}

void vdb_color_primary(int primary, int shade)
{
    if (primary < 0) primary = 0;
    if (primary > 4) primary = 4;
    if (shade < 0) shade = 0;
    if (shade > 2) shade = 2;
    vdb_current_color_mode = vdb_color_mode_primary;
    vdb_current_color = (unsigned char)(3*primary + shade);
}

void vdb_color_rampf(float value)
{
    int i = (int)(value*63.0f);
    if (i < 0) i = 0;
    if (i > 63) i = 63;
    vdb_current_color_mode = vdb_color_mode_ramp;
    vdb_current_color = (unsigned char)i;
}

void vdb_color_ramp(int i)
{
    vdb_current_color_mode = vdb_color_mode_ramp;
    vdb_current_color = (unsigned char)(i % 63);
}

void vdb_color_red(int shade)   { vdb_color_primary(0, shade); }
void vdb_color_green(int shade) { vdb_color_primary(1, shade); }
void vdb_color_blue(int shade)  { vdb_color_primary(2, shade); }
void vdb_color_black(int shade) { vdb_color_primary(3, shade); }
void vdb_color_white(int shade) { vdb_color_primary(4, shade); }
void vdb_translucent()          { vdb_current_alpha = 1; }
void vdb_opaque()               { vdb_current_alpha = 0; }

void vdb_setPointSize(float radius)   { if (vdb_point_size)  *vdb_point_size = radius; }
void vdb_setLineSize(float radius)    { if (vdb_line_size)   *vdb_line_size = radius; }
void vdb_setNicePoints(int enabled)   { if (vdb_nice_points) *vdb_nice_points = (unsigned char)enabled; }
void vdb_setTranslucency(float alpha) { if (vdb_alpha_value) *vdb_alpha_value = alpha; }

void vdb_xrange(float left, float right)
{
    vdb_xrange_left = left;
    vdb_xrange_right = right;
}

void vdb_yrange(float bottom, float top)
{
    vdb_yrange_bottom = bottom;
    vdb_yrange_top = top;
}

void vdb_zrange(float z_near, float z_far)
{
    vdb_zrange_near = z_near;
    vdb_zrange_far = z_far;
}

float vdb__unmap_x(float x) { return vdb_xrange_left + (0.5f+0.5f*x)*(vdb_xrange_right-vdb_xrange_left); }
float vdb__unmap_y(float y) { return vdb_yrange_bottom + (0.5f+0.5f*y)*(vdb_yrange_top-vdb_yrange_bottom); }
float vdb__map_x(float x) { return -1.0f + 2.0f*(x-vdb_xrange_left)/(vdb_xrange_right-vdb_xrange_left); }
float vdb__map_y(float y) { return -1.0f + 2.0f*(y-vdb_yrange_bottom)/(vdb_yrange_top-vdb_yrange_bottom); }
float vdb__map_z(float z) { return +1.0f - 2.0f*(z-vdb_zrange_near)/(vdb_zrange_far-vdb_zrange_near); }

void vdb_push_style()
{
    unsigned char opacity = ((vdb_current_alpha & 0x01)      << 7);
    unsigned char mode    = ((vdb_current_color_mode & 0x01) << 6);
    unsigned char value   = ((vdb_current_color & 0x3F)      << 0);
    unsigned char style   = opacity | mode | value;
    vdb_push_u08(style);
}

void vdb_point(float x, float y)
{
    vdb_push_u08(vdb_mode_point2);
    vdb_push_style();
    vdb_push_r32(vdb__map_x(x));
    vdb_push_r32(vdb__map_y(y));
}

void vdb_point3d(float x, float y, float z)
{
    vdb_push_u08(vdb_mode_point3);
    vdb_push_style();
    vdb_push_r32(vdb__map_x(x));
    vdb_push_r32(vdb__map_y(y));
    vdb_push_r32(vdb__map_z(z));
}

void vdb_line(float x1, float y1, float x2, float y2)
{
    vdb_push_u08(vdb_mode_line2);
    vdb_push_style();
    vdb_push_r32(vdb__map_x(x1));
    vdb_push_r32(vdb__map_y(y1));
    vdb_push_r32(vdb__map_x(x2));
    vdb_push_r32(vdb__map_y(y2));
}

void vdb_line3d(float x1, float y1, float z1, float x2, float y2, float z2)
{
    vdb_push_u08(vdb_mode_line3);
    vdb_push_style();
    vdb_push_r32(vdb__map_x(x1));
    vdb_push_r32(vdb__map_y(y1));
    vdb_push_r32(vdb__map_z(z1));
    vdb_push_r32(vdb__map_x(x2));
    vdb_push_r32(vdb__map_y(y2));
    vdb_push_r32(vdb__map_z(z2));
}

void vdb_fillRect(float x, float y, float w, float h)
{
    vdb_push_u08(vdb_mode_fill_rect);
    vdb_push_style();
    vdb_push_r32(vdb__map_x(x));
    vdb_push_r32(vdb__map_y(y));
    vdb_push_r32(vdb__map_x(x+w));
    vdb_push_r32(vdb__map_y(y+h));
}

void vdb_circle(float x, float y, float r)
{
    vdb_push_u08(vdb_mode_circle);
    vdb_push_style();
    vdb_push_r32(vdb__map_x(x));
    vdb_push_r32(vdb__map_y(y));
    vdb_push_r32(vdb__map_x(r) - vdb__map_x(0.0f));
    vdb_push_r32(vdb__map_y(r) - vdb__map_y(0.0f));
}

void vdb_imageRGB8(const void *data, int w, int h)
{
    vdb_push_u08(vdb_mode_image_rgb8);
    vdb_push_style();
    vdb_push_u32(w);
    vdb_push_u32(h);
    vdb_push_bytes(data, w*h*3);
}

void vdb_slider1f(const char *in_label, float *x, float min_value, float max_value)
{
    int i = 0;
    vdb_label_t label = {0};
    vdb_copy_label(&label, in_label);
    vdb_push_u08(vdb_mode_slider);
    vdb_push_style();
    vdb_push_bytes(label.chars, VDB_LABEL_LENGTH);
    vdb_push_r32(*x);
    vdb_push_r32(min_value);
    vdb_push_r32(max_value);
    vdb_push_r32(0.01f);

    // Update variable
    // @ ROBUSTNESS @ RACE CONDITION: Mutex on latest message
    for (i = 0; i < vdb_shared->status.var_count; i++)
    {
        if (vdb_cmp_label(&vdb_shared->status.var_label[i], &label))
        {
            float v = vdb_shared->status.var_value[i];
            if (v < min_value) v = min_value;
            if (v > max_value) v = max_value;
            *x = v;
        }
    }
}

void vdb_slider1i(const char *in_label, int *x, int min_value, int max_value)
{
    int i = 0;
    vdb_label_t label = {0};
    vdb_copy_label(&label, in_label);
    vdb_push_u08(vdb_mode_slider);
    vdb_push_style();
    vdb_push_bytes(label.chars, VDB_LABEL_LENGTH);
    vdb_push_r32((float)*x);
    vdb_push_r32((float)min_value);
    vdb_push_r32((float)max_value);
    vdb_push_r32(1.0f);

    // Update variable
    // @ ROBUSTNESS @ RACE CONDITION: Mutex on latest message
    for (i = 0; i < vdb_shared->status.var_count; i++)
    {
        if (vdb_cmp_label(&vdb_shared->status.var_label[i], &label))
        {
            int v = (int)vdb_shared->status.var_value[i];
            if (v < min_value) v = min_value;
            if (v > max_value) v = max_value;
            *x = v;
        }
    }
}

void vdb_checkbox(const char *in_label, int *x)
{
    vdb_slider1i(in_label, x, 0, 1);
}

int vdb_mouse_click(float *x, float *y)
{
    if (vdb_shared->status.mouse_click)
    {
        *x = vdb__unmap_x(vdb_shared->status.mouse_click_x);
        *y = vdb__unmap_y(vdb_shared->status.mouse_click_y);
        return 1;
    }
    return 0;
}
