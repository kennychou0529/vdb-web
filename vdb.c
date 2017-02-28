// implementation

#define vdb_assert(EXPR)  { if (!(EXPR)) { printf("[error]\n\tAssert failed at line %d in file %s:\n\t'%s'\n", __LINE__, __FILE__, #EXPR); return 0; } }
#ifdef VDB_LOG_DEBUG
#define vdb_log(...)      { printf("[vdb] %s@L%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); }
#else
#define vdb_log(...)      { }
#endif
#define vdb_log_once(...) { static int first = 1; if (first) { printf("[vdb] "); printf(__VA_ARGS__); first = 0; } }
#define vdb_err_once(...) { static int first = 1; if (first) { printf("[vdb] Error at line %d in file %s:\n[vdb] ", __LINE__, __FILE__); printf(__VA_ARGS__); first = 0; } }
#define vdb_critical(EXPR) if (!(EXPR)) { printf("[vdb] Something went wrong at line %d in file %s\n", __LINE__, __FILE__); vdb_shared->critical_error = 1; return 0; }

#if defined(_WIN32) || defined(_WIN64)
#define VDB_WINDOWS
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#define VDB_UNIX
#include <sys/mman.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#endif
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "tcp.c"
#include "websocket.c"

// Draw commands are stored in a work buffer that is allocated
// once on the first vdb_begin call, and stays a fixed size that
// is given below in number-of-bytes. If you are memory constrained,
// or if you need more memory than allocated by default, you can
// define your own work buffer size before #including vdb.
#ifndef VDB_WORK_BUFFER_SIZE
#define VDB_WORK_BUFFER_SIZE (32*1024*1024)
#endif

// Messages received from the browser are stored in a buffer that
// is allocated once on the first vdb_begin call, and stays a fixed
// size is given below in number-of-bytes. If you are sending large
// messages from the browser back to the application you can define
// your own recv buffer size before #including vdb.
#ifndef VDB_RECV_BUFFER_SIZE
#define VDB_RECV_BUFFER_SIZE (1024*1024)
#endif

#ifndef VDB_LISTEN_PORT
#define VDB_LISTEN_PORT 8000
#define VDB_USING_DEFAULT_LISTEN_PORT
#endif

#define VDB_MAX_S32_VARIABLES 1024
#define VDB_MAX_R32_VARIABLES 1024

typedef struct
{
    #ifdef VDB_WINDOWS
    volatile HANDLE send_semaphore;
    volatile LONG busy;
    volatile int bytes_to_send;
    #else
    int bytes_to_send;
    pid_t send_pid;
    // These pipes are used for flow control between the main thread and the sending thread
    // The sending thread blocks on a read on pipe_ready, until the main thread signals the
    // pipe by write on pipe_ready. The main thread checks if sending is complete by polling
    // (non-blocking) pipe_done, which is signalled by the sending thread.
    int ready[2]; // [0]: read, [1]: send
    int done[2];// [0]: read, [1]: send
    #endif

    int has_send_thread;
    int critical_error;
    int has_connection;
    int work_buffer_used;
    char swapbuffer1[VDB_WORK_BUFFER_SIZE];
    char swapbuffer2[VDB_WORK_BUFFER_SIZE];
    char *work_buffer;
    char *send_buffer;

    char recv_buffer[VDB_RECV_BUFFER_SIZE];

    // These hold the latest state from the browser
    uint64_t msg_var_r32_address[VDB_MAX_R32_VARIABLES];
    float    msg_var_r32_value[VDB_MAX_R32_VARIABLES];
    int      msg_var_r32_count;
    // uint64_t msg_var_s32_address[VDB_MAX_S32_VARIABLES];
    // int32_t  msg_var_s32_value[VDB_MAX_S32_VARIABLES];
    // int      msg_var_s32_count;
    int      msg_flag_continue;
    // float    app_mouse_x;
    // float    app_mouse_y;
} vdb_shared_t;

static vdb_shared_t *vdb_shared = 0;
static int vdb_listen_port = VDB_LISTEN_PORT;

int vdb_set_listen_port(int port)
{
    #ifndef VDB_USING_DEFAULT_LISTEN_PORT
    printf("[vdb] Warning: You are setting the port with both vdb_set_listen_port and #define VDB_LISTEN_PORT.\nAre you sure this is intentional?\n");
    #endif
    vdb_listen_port = port;
    return 1;
}

#ifdef VDB_WINDOWS
int vdb_wait_data_ready()
{
    WaitForSingleObject(vdb_shared->send_semaphore, INFINITE);
    while (InterlockedCompareExchange(&vdb_shared->busy, 1, 0) == 1)
    {
        vdb_log("CompareExchange blocked\n");
    }
    return 1;
}
int vdb_signal_data_sent()  { vdb_shared->busy = 0; return 1; }
int vdb_poll_data_sent()    { return (InterlockedCompareExchange(&vdb_shared->busy, 1, 0) == 0); }
int vdb_signal_data_ready() { vdb_shared->busy = 0; ReleaseSemaphore(vdb_shared->send_semaphore, 1, 0); return 1; } // @ mfence, writefence
void vdb_sleep(int ms)      { Sleep(ms); }
#else
int vdb_wait_data_ready()   { int val = 0; return  read(vdb_shared->ready[0], &val, sizeof(val)) == sizeof(val); }
int vdb_poll_data_sent()    { int val = 0; return   read(vdb_shared->done[0], &val, sizeof(val)) == sizeof(val); }
int vdb_signal_data_ready() { int one = 1; return write(vdb_shared->ready[1], &one, sizeof(one)) == sizeof(one); }
int vdb_signal_data_sent()  { int one = 1; return  write(vdb_shared->done[1], &one, sizeof(one)) == sizeof(one); }
void vdb_sleep(int ms)      { usleep(ms*1000); }
#endif

int vdb_send_thread()
{
    vdb_shared_t *vs = vdb_shared;
    unsigned char *frame; // @ UGLY: form_frame should modify a char *?
    int frame_len;
    vdb_log("Created send thread\n");
    vdb_sleep(100); // @ RACECOND: Let the parent thread set has_send_thread to 1
    while (!vs->critical_error)
    {
        // blocking until data is signalled ready from main thread
        vdb_critical(vdb_wait_data_ready());

        // send frame header
        vdb_form_frame(vs->bytes_to_send, &frame, &frame_len);
        if (!tcp_sendall(frame, frame_len))
        {
            vdb_log("Failed to send frame\n");
            vdb_critical(vdb_signal_data_sent());
            break;
        }

        // send the payload
        if (!tcp_sendall(vs->send_buffer, vs->bytes_to_send))
        {
            vdb_log("Failed to send payload\n");
            vdb_critical(vdb_signal_data_sent());
            break;
        }

        // signal to main thread that data has been sent
        vdb_critical(vdb_signal_data_sent());
    }
    vs->has_send_thread = 0;
    return 0;
}

#ifdef VDB_WINDOWS
DWORD WINAPI vdb_win_send_thread(void *vdata) { (void)(vdata); return vdb_send_thread(); }
#endif

int vdb_recv_thread()
{
    vdb_shared_t *vs = vdb_shared;
    int read_bytes;
    vdb_msg_t msg;
    vdb_log("Created read thread\n");
    while (!vs->critical_error)
    {
        if (vs->critical_error)
        {
            return 0;
        }
        if (!tcp_has_listen_socket)
        {
            if (vdb_listen_port == 0)
            {
                vdb_log_once("You have not set the listening port for vdb. Using 8000.\n"
                       "You can use a different port by calling vdb_set_listen_port(<port>),\n"
                       "or by #define VDB_LISTEN_PORT <port> before #including vdb.c.\n"
                       "Valid ports are between 1024 and 65535.\n");
                vdb_listen_port = 8000;
            }
            vdb_log("Creating listen socket\n");
            if (!tcp_listen(vdb_listen_port))
            {
                vdb_log("listen failed\n");
                vdb_sleep(1000);
                continue;
            }
            vdb_log_once("Visualization is live at <Your IP>:%d\n", vdb_listen_port);
        }
        if (!tcp_has_client_socket)
        {
            vdb_log("Waiting for client\n");
            if (!tcp_accept())
            {
                vdb_sleep(1000);
                continue;
            }
        }
        if (!vs->has_connection)
        {
            char *response;
            int response_len;
            vdb_log("Waiting for handshake\n");
            if (!tcp_recv(vs->recv_buffer, VDB_RECV_BUFFER_SIZE, &read_bytes))
            {
                vdb_log("lost connection during handshake\n");
                tcp_shutdown();
                vdb_sleep(1000);
                continue;
            }

            vdb_log("Generating handshake\n");
            if (!vdb_generate_handshake(vs->recv_buffer, read_bytes, &response, &response_len))
            {
                static char http_response[1024*1024];
                // const char *content = "<html>In the future I'll also send the vdb application to you over the browser.<br>For now, you need to open the app.html file in your browser.</html>";
                const char *content = "<title>vdebug</title><style>body{font-family:roboto,sans-serif;font-size:13px;background:#f1f1f1;margin:0}#canvas{border-radius:4px;background:#fff;box-shadow:0 2px 2px #a7a7a7;display:block;margin-left:auto;margin-right:auto;margin-top:12px;margin-bottom:12px;width:600px;height:300px}#container{width:640;height:480;display:block;margin:auto}#fps,#status{margin-top:6px;text-align:center}a{color:#fff;background:#b54;padding:6px;box-shadow:0 2px 2px #a7a7a7;border-radius:2px;text-decoration:none}a:hover{background:#c65}a:active{background:#d76}</style><script id=shader_vs type=notjs>attribute vec4 coord;attribute vec4 color;attribute vec2 texel;varying vec4 v_color;varying vec2 v_texel;void main(){v_color = color;v_texel = texel;gl_Position = coord;}</script><script id=shader_fs type=notjs>precision mediump float;varying vec4 v_color;varying vec2 v_texel;uniform sampler2D channel0;void main(){gl_FragColor = v_color*texture2D(channel0, v_texel);}</script><script>function colorPalette(e){var t=[[2650882815,3577630719,4100801535,4256063999,4276128767,4294950911,3874855167,2883429631,1724032511,847822335,1582277375],[3944703,23486207,899125247,2160968191,3354060287,4126537215,4142449663,3754065407,3212914175,2354121471,1412433407],[137391103,139566335,561100287,1116915455,1806620415,2664096255,3336302591,3740006399,4160487423],[138445311,141077759,730644223,1320408063,2077017343,2833102335,3438003711,3774077951,4160549119],[3826916607,931051775,1303333631,2555290623,4286513407,4294915071,2790664447,4152475647]];return t[palette_index][e%t[palette_index].length]}function alphaPalette(e){return 0==e?255:1==e?128:void 0}function generateTriangles(e){tex_view0_active=!1;for(var t=[],l=[],r=0,a=!0,o=new DataView(e),g=0;g<o.byteLength;){var n=o.getUint8(g,a);g+=1;var _=o.getUint8(g,a);g+=1;var i=127&_,c=_>>7&1,u=colorPalette(i),E=u>>24&255,T=u>>16&255,d=u>>8&255,v=255&alphaPalette(c);if(1==n){var R=o.getFloat32(g,a);g+=4;var s=o.getFloat32(g,a);g+=4;for(var A=point_radius/(cvs.width/2),f=point_radius/(cvs.height/2),h=32,b=(cvs.width/2,cvs.height/2,0);b<h;b++){var m=6.2831852*b/h,x=6.2831852*(b+1)/h,F=R,U=s,w=F+A*Math.cos(m),B=U+f*Math.sin(m),D=F+A*Math.cos(x),P=U+f*Math.sin(x);t.push(F,U,w,B,D,P),l.push(E,T,d,v,E,T,d,v,E,T,d,v),r+=3}}else if(2==n){o.getFloat32(g,a);g+=4;o.getFloat32(g,a);g+=4;o.getFloat32(g,a);g+=4}else if(3==n){var F=o.getFloat32(g,a);g+=4;var U=o.getFloat32(g,a);g+=4;var w=o.getFloat32(g,a);g+=4;var B=o.getFloat32(g,a);g+=4;var S=Math.sqrt((w-F)*(w-F)+(B-U)*(B-U)),p=-(B-U)/S,X=(w-F)/S,A=line_width/2/(cvs.width/2),f=line_width/2/(cvs.height/2),I=F-p*A,y=U-X*f,L=w-p*A,N=B-X*f,M=F+p*A,G=U+X*f,Y=w+p*A,C=B+X*f;t.push(I,y,L,N,Y,C,Y,C,M,G,I,y),l.push(E,T,d,v,E,T,d,v,E,T,d,v,E,T,d,v,E,T,d,v,E,T,d,v),r+=6}else if(4==n){o.getFloat32(g,a);g+=4;o.getFloat32(g,a);g+=4;o.getFloat32(g,a);g+=4;o.getFloat32(g,a);g+=4;o.getFloat32(g,a);g+=4;o.getFloat32(g,a);g+=4}else if(5==n){var R=o.getFloat32(g,a);g+=4;var s=o.getFloat32(g,a);g+=4;var W=o.getFloat32(g,a);g+=4;var q=o.getFloat32(g,a);g+=4,t.push(R,s,R+W,s,R+W,s+q,R+W,s+q,R,s+q,R,s),l.push(E,T,d,v,E,T,d,v,E,T,d,v,E,T,d,v,E,T,d,v,E,T,d,v),r+=6}else if(6==n){var R=o.getFloat32(g,a);g+=4;var s=o.getFloat32(g,a);g+=4;var O=o.getFloat32(g,a);g+=4;for(var h=32,b=0;b<h;b++){var m=6.2831852*b/h,x=6.2831852*(b+1)/h,F=R,U=s,w=F+O*Math.cos(m),B=U+O*Math.sin(m),D=F+O*Math.cos(x),P=U+O*Math.sin(x);t.push(F,U,w,B,D,P),l.push(E,T,d,v,E,T,d,v,E,T,d,v),r+=3}}else if(7==n){var H=o.getUint32(g,a);g+=4;var k=o.getUint32(g,a);g+=4;var V=H*k*3,K=new Uint8Array(e,g,V);g+=V,tex_view0_width!=H||tex_view0_height!=k?(tex_view0_width=H,tex_view0_height=k,gl.bindTexture(gl.TEXTURE_2D,tex_view0),gl.texImage2D(gl.TEXTURE_2D,0,gl.RGB,H,k,0,gl.RGB,gl.UNSIGNED_BYTE,K),gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_WRAP_S,gl.CLAMP_TO_EDGE),gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_WRAP_T,gl.CLAMP_TO_EDGE),gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_MAG_FILTER,gl.NEAREST),gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_MIN_FILTER,gl.NEAREST),gl.bindTexture(gl.TEXTURE_2D,null)):(gl.bindTexture(gl.TEXTURE_2D,tex_view0),gl.texSubImage2D(gl.TEXTURE_2D,0,0,0,H,k,gl.RGB,gl.UNSIGNED_BYTE,K),gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_WRAP_S,gl.CLAMP_TO_EDGE),gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_WRAP_T,gl.CLAMP_TO_EDGE),gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_MAG_FILTER,gl.NEAREST),gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_MIN_FILTER,gl.NEAREST),gl.bindTexture(gl.TEXTURE_2D,null)),tex_view0_active=!0,K=null}else if(255==n){var W=o.getFloat32(g,a);g+=4;var q=o.getFloat32(g,a);g+=4;var j=cvs.clientHeight*W/q,z=document.getElementById(\"canvas\");z.style.width=j+\"px\"}}return gl.bindBuffer(gl.ARRAY_BUFFER,vbo_user_coord),gl.bufferData(gl.ARRAY_BUFFER,new Float32Array(t),gl.STATIC_DRAW),gl.bindBuffer(gl.ARRAY_BUFFER,vbo_user_color),gl.bufferData(gl.ARRAY_BUFFER,new Uint8Array(l),gl.STATIC_DRAW),t=null,l=null,o=null,r}function createShader(e,t,l){var r=e.createShader(t);e.shaderSource(r,l),e.compileShader(r);var a=e.getShaderParameter(r,e.COMPILE_STATUS);return a?r:(console.log(e.getShaderInfoLog(r)),void e.deleteShader(r))}function createProgram(e,t,l){var r=e.createProgram();e.attachShader(r,t),e.attachShader(r,l),e.linkProgram(r);var a=e.getProgramParameter(r,e.LINK_STATUS);return a?r:(console.log(e.getProgramInfoLog(r)),void e.deleteProgram(r))}function init(){var e=document.getElementById(\"shader_vs\").text,t=document.getElementById(\"shader_fs\").text,l=createShader(gl,gl.VERTEX_SHADER,e),r=createShader(gl,gl.FRAGMENT_SHADER,t);program=createProgram(gl,l,r),loc_coord=gl.getAttribLocation(program,\"coord\"),loc_color=gl.getAttribLocation(program,\"color\"),loc_texel=gl.getAttribLocation(program,\"texel\"),loc_chan0=gl.getUniformLocation(program,\"chan0\"),vbo_user_coord=gl.createBuffer(),vbo_user_color=gl.createBuffer(),vbo_user_texel=gl.createBuffer(),vbo_quad_coord=gl.createBuffer(),vbo_quad_color=gl.createBuffer(),vbo_quad_texel=gl.createBuffer();var a=[-1,-1,1,-1,1,1,1,1,-1,1,-1,-1],o=[0,0,1,0,1,1,1,1,0,1,0,0],g=[255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255];gl.bindBuffer(gl.ARRAY_BUFFER,vbo_quad_coord),gl.bufferData(gl.ARRAY_BUFFER,new Float32Array(a),gl.STATIC_DRAW),gl.bindBuffer(gl.ARRAY_BUFFER,vbo_quad_color),gl.bufferData(gl.ARRAY_BUFFER,new Uint8Array(g),gl.STATIC_DRAW),gl.bindBuffer(gl.ARRAY_BUFFER,vbo_quad_texel),gl.bufferData(gl.ARRAY_BUFFER,new Float32Array(o),gl.STATIC_DRAW),gl.bindBuffer(gl.ARRAY_BUFFER,null),tex_white=gl.createTexture();var n=new Uint8Array([255,255,255,255]);gl.bindTexture(gl.TEXTURE_2D,tex_white),gl.texImage2D(gl.TEXTURE_2D,0,gl.RGBA,1,1,0,gl.RGBA,gl.UNSIGNED_BYTE,n),gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_WRAP_S,gl.CLAMP_TO_EDGE),gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_WRAP_T,gl.CLAMP_TO_EDGE),gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_MAG_FILTER,gl.NEAREST),gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_MIN_FILTER,gl.NEAREST),gl.bindTexture(gl.TEXTURE_2D,null),tex_view0=gl.createTexture(),tex_view0_active=!1}function draw(){line_width=parseFloat(document.getElementById(\"line_width\").value),point_radius=parseFloat(document.getElementById(\"point_radius\").value),cvs.width==cvs.clientWidth&&cvs.height==cvs.clientHeight||(cvs.width=cvs.clientWidth,cvs.height=cvs.clientHeight);var e=0;null!=cmd_data&&(e=generateTriangles(cmd_data)),gl.enable(gl.BLEND),gl.blendEquation(gl.FUNC_ADD),gl.blendFunc(gl.SRC_ALPHA,gl.ONE_MINUS_SRC_ALPHA),gl.viewport(0,0,gl.drawingBufferWidth,gl.drawingBufferHeight),gl.clearColor(0,0,0,0),gl.clear(gl.COLOR_BUFFER_BIT),gl.useProgram(program),tex_view0_active&&(gl.activeTexture(gl.TEXTURE0+0),gl.bindTexture(gl.TEXTURE_2D,tex_view0),gl.enableVertexAttribArray(loc_coord),gl.enableVertexAttribArray(loc_color),gl.enableVertexAttribArray(loc_texel),gl.bindBuffer(gl.ARRAY_BUFFER,vbo_quad_coord),gl.vertexAttribPointer(loc_coord,2,gl.FLOAT,!1,0,0),gl.bindBuffer(gl.ARRAY_BUFFER,vbo_quad_color),gl.vertexAttribPointer(loc_color,4,gl.UNSIGNED_BYTE,!0,0,0),gl.bindBuffer(gl.ARRAY_BUFFER,vbo_quad_texel),gl.vertexAttribPointer(loc_texel,2,gl.FLOAT,!1,0,0),gl.uniform1i(loc_chan0,0),gl.drawArrays(gl.TRIANGLES,0,6),gl.bindTexture(gl.TEXTURE_2D,null)),e>0&&(gl.activeTexture(gl.TEXTURE0+0),gl.bindTexture(gl.TEXTURE_2D,tex_white),gl.enableVertexAttribArray(loc_coord),gl.enableVertexAttribArray(loc_color),gl.disableVertexAttribArray(loc_texel),gl.bindBuffer(gl.ARRAY_BUFFER,vbo_user_coord),gl.vertexAttribPointer(loc_coord,2,gl.FLOAT,!1,0,0),gl.bindBuffer(gl.ARRAY_BUFFER,vbo_user_color),gl.vertexAttribPointer(loc_color,4,gl.UNSIGNED_BYTE,!0,0,0),gl.uniform1i(loc_chan0,0),gl.drawArrays(gl.TRIANGLES,0,e),gl.bindTexture(gl.TEXTURE_2D,null))}function connect(){setInterval(function(){ws||(ws=new WebSocket(\"ws://localhost:8000\"),ws.binaryType=\"arraybuffer\",ws.onopen=function(){ws.send(\"Hello from browser!\"),has_connection=!0},ws.onclose=function(){ws=null,has_connection=!1},ws.onmessage=function(e){cmd_data=e.data})},250)}function anim(e){var t=1/60,l=0;e&&(animation_frame_t_first||(animation_frame_t_first=e),animation_frame_t_prev||(animation_frame_t_prev=e),t=(e-animation_frame_t_prev)/1e3,l=(e-animation_frame_t_first)/1e3,animation_frame_t_prev=e);var r=document.getElementById(\"status\");has_connection?(r.innerHTML=\"Connected\",draw()):(r.innerHTML=\"No connection\",draw());var a=document.getElementById(\"fps\");a.innerText=\"FPS: \"+(1/t).toPrecision(4),requestAnimationFrame(anim)}function onKeyUp(e){13==event.keyCode&&ws.send(\"c\")}function load(){return\"WebSocket\"in window?(cvs=document.getElementById(\"canvas\"),(gl=cvs.getContext(\"webgl\",{antialias:!1}))?(document.onkeyup=onKeyUp,connect(),init(),void anim()):void console.log(\"Your browser does not support WebGL! Sorry, good luck!\")):void alert(\"Your browser does not support WebSockets! Sorry, good luck!\")}function sendContinue(){ws.send(\"c\")}var ws=null,has_connection=!1,cmd_data=null,cvs=null,gl=null,program=null,loc_coord=null,loc_color=null,loc_texel=null,loc_chan0=null,vbo_user_coord=null,vbo_user_color=null,vbo_user_texel=null,vbo_quad_coord=null,vbo_quad_color=null,vbo_quad_texel=null,tex_white=null,tex_view0=null,palette_index=0,tex_view0_active=!1,tex_view0_width=0,tex_view0_height=0,line_width=4,point_radius=4,palette_index=0,fuck=0,animation_frame_t_first=null,animation_frame_t_prev=null</script><body onload=load()><div id=container><p id=status><p id=fps></p><canvas id=canvas></canvas><input id=line_width max=32.0 min=1.0 type=range value=4.0> <input id=point_radius max=32.0 min=1.0 type=range value=4.0> <a class=button href=javascript:sendContinue() style=float:right>Continue</a></div>";

                // If we failed to generate a handshake, it means that either
                // we did something wrong, or the browser did something wrong,
                // or the request was an ordinary HTTP request. If the latter
                // we'll send an HTTP response containing the vdb.html page.
                int len = sprintf(http_response,
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Length: %d\r\n"
                    "Content-Type: text/html\r\n"
                    "Connection: Closed\r\n\r\n%s",
                    (int)strlen(content),
                    content
                    );

                vdb_log("Failed to generate handshake. Sending HTML page.\n");
                if (!tcp_sendall(http_response, len))
                {
                    vdb_log("Lost connection while sending HTML page\n");
                    tcp_shutdown();
                    vdb_sleep(1000);
                    continue;
                }

                // Sending 'Connection: Closed' allows us to close the socket immediately
                tcp_close_client();

                vdb_sleep(1000);
                continue;
            }

            vdb_log("Sending handshake\n");
            if (!tcp_sendall(response, response_len))
            {
                vdb_log("Failed to send handshake\n");
                tcp_shutdown();
                vdb_sleep(1000);
                continue;
            }

            vs->has_connection = 1;
        }
        #ifdef VDB_UNIX
        // The send thread is allowed to return on unix, because if the connection
        // goes down, the recv thread needs to respawn the process after a new
        // client connection has been acquired (to share the file descriptor).
        // fork() shares the open file descriptors with the child process, but
        // if a file descriptor is _then_ opened in the parent, it will _not_
        // be shared with the child.
        if (!vs->has_send_thread)
        {
            vs->send_pid = fork();
            vdb_critical(vs->send_pid != -1);
            if (vs->send_pid == 0)
            {
                vdb_send_thread(); // vdb_send_thread sets has_send_thread to 0 upon returning
                _exit(0);
            }
            vs->has_send_thread = 1;
        }
        #else
        // Because we allow it to return on unix, we allow it to return on windows
        // as well, even though file descriptors are shared anyway.
        if (!vs->has_send_thread)
        {
            CreateThread(0, 0, vdb_win_send_thread, NULL, 0, 0); // vdb_send_thread sets has_send_thread to 0 upon returning
            vs->has_send_thread = 1;
        }
        #endif
        if (!tcp_recv(vs->recv_buffer, VDB_RECV_BUFFER_SIZE, &read_bytes)) // @ INCOMPLETE: Assemble frames
        {
            vdb_log("Connection went down\n");
            vs->has_connection = 0;
            tcp_shutdown();
            #ifdef VDB_UNIX
            if (vs->has_send_thread)
            {
                kill(vs->send_pid, SIGUSR1);
                vs->has_send_thread = 0;
            }
            #endif
            vdb_sleep(100);
            continue;
        }
        if (!vdb_parse_message(vs->recv_buffer, read_bytes, &msg))
        {
            vdb_log("Got a bad message\n");
            continue;
        }
        if (!msg.fin)
        {
            vdb_log("Got an incomplete message (%d): '%s'\n", msg.length, msg.payload);
            continue;
        }
        // vdb_log("Got a message (%d): '%s'\n", msg.length, msg.payload);

        if (msg.length == 1 && msg.payload[0] == 'c') // asynchronous 'continue' event
        {
            vs->msg_flag_continue = 1;
        }

        // @ todo: mutex on latest message?
        if (msg.length > 1 && msg.payload[0] == 's') // status update
        {
            char *ptr = msg.payload+1;
            {
                int n;
                int read_bytes;
                sscanf(ptr, "%d%n", &n, &read_bytes);
                ptr += read_bytes;

                if (n >= 0 && n <= VDB_MAX_R32_VARIABLES)
                {
                    int i = 0;
                    for (i = 0; i < n; i++)
                    {
                        uint32_t addr_low, addr_high; float value;
                        sscanf(ptr, "%u %u %f%n", &addr_low, &addr_high, &value, &read_bytes);
                        ptr += read_bytes;

                        uint64_t addr = ((uint64_t)addr_high << 32) | (uint64_t)addr_low;
                        vs->msg_var_r32_address[i] = addr;
                        vs->msg_var_r32_value[i] = value;
                    }
                    vs->msg_var_r32_count = n;
                }
            }
        }

    }
    return 0;
}

#ifdef VDB_WINDOWS
DWORD WINAPI vdb_win_recv_thread(void *vdata) { (void)(vdata); return vdb_recv_thread(); }
#endif

void *vdb_push_bytes(const void *data, int count)
{
    if (vdb_shared->work_buffer_used + count <= VDB_WORK_BUFFER_SIZE)
    {
        const char *src = (const char*)data;
              char *dst = vdb_shared->work_buffer + vdb_shared->work_buffer_used;
        if (src) memcpy(dst, src, count);
        else     memset(dst, 0, count);
        vdb_shared->work_buffer_used += count;
        return (void*)dst;
    }
    return NULL;
}

#define _vdb_push_type(VALUE, TYPE)                                                  \
    if (vdb_shared->work_buffer_used + sizeof(TYPE) <= VDB_WORK_BUFFER_SIZE)         \
    {                                                                                \
        TYPE *ptr = (TYPE*)(vdb_shared->work_buffer + vdb_shared->work_buffer_used); \
        vdb_shared->work_buffer_used += sizeof(TYPE);                                \
        *ptr = VALUE;                                                                \
        return ptr;                                                                  \
    }                                                                                \
    return NULL;

uint8_t  *vdb_push_u08(uint8_t x)  { _vdb_push_type(x, uint8_t);  }
uint32_t *vdb_push_u32(uint32_t x) { _vdb_push_type(x, uint32_t); }
float    *vdb_push_r32(float x)    { _vdb_push_type(x, float);    }

void vdb_init_drawstate();

int vdb_begin()
{
    if (vdb_listen_port < 1024 || vdb_listen_port > 65535)
    {
        vdb_err_once("You have requested a vdb_listen_port (%d) outside the valid range 1024-65535.\n", vdb_listen_port);
        return 0;
    }
    if (!vdb_shared)
    {
        #ifdef VDB_UNIX
        vdb_shared = (vdb_shared_t*)mmap(NULL, sizeof(vdb_shared_t),
                                         PROT_READ|PROT_WRITE,
                                         MAP_SHARED|MAP_ANONYMOUS, -1, 0);
        if (vdb_shared == MAP_FAILED)
            vdb_shared = 0;
        #else
        vdb_shared = (vdb_shared_t*)calloc(sizeof(vdb_shared_t),1);
        #endif

        if (!vdb_shared)
        {
            vdb_err_once("Tried to allocate too much memory, try lowering VDB_RECV_BUFFER_SIZE and VDB_SEND_BUFFER_SIZE\n");
            vdb_shared->critical_error = 1;
            return 0;
        }

        #ifdef VDB_UNIX
        vdb_critical(pipe(vdb_shared->ready) != -1);
        vdb_critical(pipe2(vdb_shared->done, O_NONBLOCK) != -1);
        {
            pid_t pid = fork();
            vdb_critical(pid != -1);
            if (pid == 0) { vdb_recv_thread(); _exit(0); }
        }
        vdb_signal_data_sent(); // Needed for first vdb_end call
        #else
        vdb_shared->send_semaphore = CreateSemaphore(0, 0, 1, 0);
        CreateThread(0, 0, vdb_win_recv_thread, NULL, 0, 0);
        #endif

        vdb_shared->work_buffer = vdb_shared->swapbuffer1;
        vdb_shared->send_buffer = vdb_shared->swapbuffer2;
        // Remaining parameters should be initialized to zero by calloc or mmap
    }
    if (vdb_shared->critical_error)
    {
        vdb_err_once("You must restart your program to use vdb.\n");
        return 0;
    }
    if (!vdb_shared->has_connection)
    {
        return 0;
    }
    vdb_shared->work_buffer_used = 0;
    vdb_init_drawstate();
    return 1;
}

void vdb_end()
{
    vdb_shared_t *vs = vdb_shared;
    if (vdb_poll_data_sent()) // Check if send_thread has finished sending data
    {
        char *new_work_buffer = vs->send_buffer;
        vs->send_buffer = vs->work_buffer;
        vs->bytes_to_send = vs->work_buffer_used;
        vs->work_buffer = new_work_buffer;
        vs->work_buffer_used = 0;

        // Notify sending thread that data is available
        vdb_signal_data_ready();
    }
}

int vdb_loop(int fps)
{
    static int entry = 1;
    if (entry)
    {
        while (!vdb_begin())
        {
        }
        entry = 0;
    }
    else
    {
        vdb_end();
        vdb_sleep(1000/fps);
        if (vdb_shared->msg_flag_continue)
        {
            vdb_shared->msg_flag_continue = 0;
            entry = 1;
            return 0;
        }
        while (!vdb_begin())
        {
        }
    }
    return 1;
}

// Public API implementation

#define vdb_color_mode_primary  0
#define vdb_color_mode_ramp     1

#define vdb_mode_point2         1
#define vdb_mode_point3         2
#define vdb_mode_line2          3
#define vdb_mode_line3          4
#define vdb_mode_fill_rect      5
#define vdb_mode_circle         6
#define vdb_mode_image_rgb8     7
#define vdb_mode_slider_float   254

static unsigned char vdb_current_color_mode = 0;
static unsigned char vdb_current_color = 0;
static unsigned char vdb_current_alpha = 0;

static float vdb_xrange_left = -1.0f;
static float vdb_xrange_right = +1.0f;
static float vdb_yrange_bottom = -1.0f;
static float vdb_yrange_top = +1.0f;
static float vdb_zrange_far = -1.0f;
static float vdb_zrange_near = +1.0f;

void vdb_init_drawstate()
{
    vdb_current_color_mode = vdb_color_mode_primary;
    vdb_current_alpha = 0;
    vdb_xrange_left = -1.0f;
    vdb_xrange_right = +1.0f;
    vdb_yrange_bottom = -1.0f;
    vdb_yrange_top = +1.0f;
    vdb_zrange_far = -1.0f;
    vdb_zrange_near = +1.0f;
}

void vdb_translucent() { vdb_current_alpha = 1; }
void vdb_opaque()      { vdb_current_alpha = 0; }

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

float vdb_map_x(float x) { return -1.0f + 2.0f*(x-vdb_xrange_left)/(vdb_xrange_right-vdb_xrange_left); }
float vdb_map_y(float y) { return -1.0f + 2.0f*(y-vdb_yrange_bottom)/(vdb_yrange_top-vdb_yrange_bottom); }
float vdb_map_z(float z) { return +1.0f - 2.0f*(z-vdb_zrange_near)/(vdb_zrange_far-vdb_zrange_near); }

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
    vdb_push_r32(vdb_map_x(x));
    vdb_push_r32(vdb_map_y(y));
}

void vdb_point3d(float x, float y, float z)
{
    vdb_push_u08(vdb_mode_point3);
    vdb_push_style();
    vdb_push_r32(vdb_map_x(x));
    vdb_push_r32(vdb_map_y(y));
    vdb_push_r32(vdb_map_z(z));
}

void vdb_line(float x1, float y1, float x2, float y2)
{
    vdb_push_u08(vdb_mode_line2);
    vdb_push_style();
    vdb_push_r32(vdb_map_x(x1));
    vdb_push_r32(vdb_map_y(y1));
    vdb_push_r32(vdb_map_x(x2));
    vdb_push_r32(vdb_map_y(y2));
}

void vdb_line3d(float x1, float y1, float z1, float x2, float y2, float z2)
{
    vdb_push_u08(vdb_mode_line3);
    vdb_push_style();
    vdb_push_r32(vdb_map_x(x1));
    vdb_push_r32(vdb_map_y(y1));
    vdb_push_r32(vdb_map_z(z1));
    vdb_push_r32(vdb_map_x(x2));
    vdb_push_r32(vdb_map_y(y2));
    vdb_push_r32(vdb_map_z(z2));
}

void vdb_fillRect(float x, float y, float w, float h)
{
    vdb_push_u08(vdb_mode_fill_rect);
    vdb_push_style();
    vdb_push_r32(vdb_map_x(x));
    vdb_push_r32(vdb_map_y(y));
    vdb_push_r32(vdb_map_x(x+w));
    vdb_push_r32(vdb_map_y(y+h));
}

void vdb_circle(float x, float y, float r)
{
    vdb_push_u08(vdb_mode_circle);
    vdb_push_style();
    vdb_push_r32(vdb_map_x(x));
    vdb_push_r32(vdb_map_y(y));
    vdb_push_r32(r);
}

void vdb_imageRGB8(const void *data, int w, int h)
{
    vdb_push_u08(vdb_mode_image_rgb8);
    vdb_push_style();
    vdb_push_u32(w);
    vdb_push_u32(h);
    vdb_push_bytes(data, w*h*3);
}

void vdb_slider1f(float *x, float min_value, float max_value)
{
    uint64_t addr = (uint64_t)x;
    vdb_push_u08(vdb_mode_slider_float);
    vdb_push_style();
    vdb_push_u32(addr & 0xFFFFFFFF); // low-bits
    vdb_push_u32((addr >> 32) & 0xFFFFFFFF); // high-bits
    vdb_push_r32(*x);
    vdb_push_r32(min_value);
    vdb_push_r32(max_value);

    {
        // Update variable
        uint64_t *msg_addr = vdb_shared->msg_var_r32_address;
        float *msg_value = vdb_shared->msg_var_r32_value;
        int n = vdb_shared->msg_var_r32_count;
        int i = 0;
        for (i = 0; i < n; i++)
        {
            if (msg_addr[i] == addr)
            {
                *x = msg_value[i];
            }
        }
    }
}
