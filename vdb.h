// vdb - Version 3
// Changelog
// (3) Float and int sliders and checkboxes
// (2) Message passing from browser to vdb
// (1) Works on unix and windows

#ifndef VDB_HEADER_INCLUDE
#define VDB_HEADER_INCLUDE

// STREAM MODE - Run a block of code without blocking the caller.
// EXAMPLE -
//     if (vdb_begin()) {
//         vdb_point()
//         vdb_end();
//     }
int  vdb_begin(); // Returns true if vdb is not already busy sending data
void vdb_end();

// LOOP MODE - Run a block of code at a specified framerate until
// we receive a 'continue' signal from the client.
// EXAMPLE -
//     while (vdb_loop(60)) {
//         static float t = 0.0f; t += 1.0f/60.0f;
//         vdb_point(cosf(t), sinf(t));
//     }
int vdb_loop(int fps);

// These functions assign a RGB color to all subsequent draw calls
// The ramp functions will map the input to a smooth gradient, while
// the primary functions (red/green/blue/...) will color the element
// a specified shade of the given primary color.
void vdb_color_rampf(float value);
void vdb_color_ramp(int i);
void vdb_color_red(int shade);
void vdb_color_green(int shade);
void vdb_color_blue(int shade);
void vdb_color_black(int shade);
void vdb_color_white(int shade);

// These functions make the next elements semi- or fully opaque,
// with an opacity that can be adjusted in the browser.
void vdb_translucent();
void vdb_opaque();

// These functions maps your input coordinates from the specified
// range to the corresponding edges of the viewport.
void vdb_xrange(float left, float right);
void vdb_yrange(float bottom, float top);
void vdb_zrange(float z_near, float z_far);

// These are your basic 2D draw commands
void vdb_point(float x, float y);
void vdb_line(float x1, float y1, float x2, float y2);
void vdb_fillRect(float x, float y, float w, float h);
void vdb_circle(float x, float y, float r);

// This will send a densely packed array of (w x h x 3) bytes and
// render it as an image of RGB values, each one byte.
void vdb_imageRGB8(const void *data, int w, int h);

// These functions let you modify variables in a vdb_begin or vdb_loop block.
// You can build a simple graphical user interface with sliders and checkboxes.
void vdb_slider1f(const char *in_label, float *x, float min_value, float max_value);
void vdb_slider1i(const char *in_label, int *x, int min_value, int max_value);
void vdb_checkbox(const char *in_label, int *x);

// You probably don't want to mess with this, but if you do,
// this pushes data to the buffer that is sent on each vdb_end.
// You can use this in conjunction with your own parser at the
// browser-side to implement custom rendering. See app.js for
// an example parser, and the any of the vdb_point/line/...
// for an example render command.
void *vdb_push_bytes(const void *data, int count);

#endif // VDB_HEADER_INCLUDE
