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
// this pushes data to the buffer that is sent on each vdb_end.
// You can use this in conjunction with your own parser at the
// browser-side to implement custom rendering. See app.js for
// an example parser, and the any of the vdb_point/line/...
// for an example render command.
void *vdb_push_bytes(const void *data, int count);

#endif
