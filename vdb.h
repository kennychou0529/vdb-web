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

// You probably don't want to mess with this, but if you do,
// these push data to the buffer that is sent on each vdb_end.
// You can use these in conjunction with your own parser at the
// browser-side to implement custom rendering.
int vdb_push_s32(int x);
int vdb_push_r32(float x);

void vdb_sleep(int milliseconds); // @ Removeme

#endif
