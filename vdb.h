// vdb - Version 1
// Changelog
// (1) Works on unix and windows

#ifndef VDB_HEADER_INCLUDE
#define VDB_HEADER_INCLUDE

// You can specify which port vdb uses to host the visualization by calling this
// function before the first vdb_begin, or writing #define VDB_LISTEN_PORT port
// before #including the implementation.
int vdb_set_listen_port(int port);
int vdb_begin();
void vdb_end();

void vdb_color1i(int c);
void vdb_color1f(float c);
void vdb_point2(float x, float y);
void vdb_point3(float x, float y, float z);
void vdb_line2(float x1, float y1, float x2, float y2);
void vdb_line3(float x1, float y1, float z1, float x2, float y2, float z2);

// You probably don't want to mess with this, but if you do,
// these push data to the buffer that is sent on each vdb_end.
// You can use these in conjunction with your own parser at the
// browser-side to implement custom rendering.
void *vdb_push_bytes(const void *data, int count);

void vdb_sleep(int milliseconds); // @ Removeme

#endif
