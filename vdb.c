// Example usage:
// #define VDB_LISTEN_PORT 8000 // Set listen port at compile time
// #include "vdb.c"
// int main()
// {
//     Alternatively, you can set the listen port at runtime
//     vdb_set_listen_port(8000);
// }

#include <stdint.h>

// interface
int vdb_set_listen_port(int port);
int vdb_push_s32(int32_t x);
int vdb_push_r32(float x);
int vdb_begin();
void vdb_end();
void vdb_sleep(int milliseconds);

// implementation
#define vdb_assert(EXPR) if (!(EXPR)) { printf("[error]\n\tAssert failed at line %d in file %s:\n\t'%s'\n", __LINE__, __FILE__, #EXPR); return 0; }
#define vdb_logd(...) { printf("[vdb] l%d in %s: ", __LINE__, __FILE__); printf(__VA_ARGS__); }
#include "tcp.c"
#include "websocket.c"
#if defined(_WIN32) || defined(_WIN64)
#include "vdb_win.c"
#else
#include "vdb_unix.c"
#endif
