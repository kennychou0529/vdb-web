var ws = null;
var cvs = 0;
var ctx = 0;
var cmd_data = 0;
var has_connection = false;
var fps = 60;

function palette(color)
{
         if (color == 0) return "#990343";
    else if (color == 1) return "#D33F4D";
    else if (color == 2) return "#F56D47";
    else if (color == 3) return "#FDAE5F";
    else if (color == 4) return "#FFDE8D";
    else if (color == 5) return "#FFFFBE";
    else if (color == 6) return "#FEDF8C";
    else if (color == 7) return "#E3F69C";
    else if (color == 8) return "#65C39F";
    else if (color == 9) return "#3486BE";
    else                 return "#5E509F";
}

function render()
{
    var status = document.getElementById("status");

    if (has_connection && cmd_data != 0)
    {
        status.innerHTML = "Connected!";

        // @ Maybe TypedArray has better perf for many points?
        // @ Negotiate endianness with server
        var little_endian = true;
        var view = new DataView(cmd_data);

        var offset = 0;
        var count = view.getUint32(offset, little_endian); offset += 4;

        ctx.fillStyle="#2a2a2a";
        ctx.fillRect(0, 0, cvs.width, cvs.height);
        ctx.fill();
        for (var i = 0; i < count; i++)
        {
            var a = cvs.height/cvs.width;
            var mode = view.getUint8(offset, little_endian); offset += 1;
            if (mode == 1) // point2
            {
                var color = view.getUint8(offset, little_endian); offset += 1;
                var x_ndc = view.getFloat32(offset, little_endian); offset += 4;
                var y_ndc = view.getFloat32(offset, little_endian); offset += 4;

                ctx.fillStyle = palette(color);
                ctx.beginPath();

                var a = cvs.height/cvs.width;
                var x = (0.5+0.5*x_ndc*a)*cvs.width;
                var y = (0.5+0.5*y_ndc)*cvs.height;
                ctx.fillRect(x-3,y-3,6,6);
                // ctx.moveTo(x, y);
                // ctx.arc(x, y, 6, 0, Math.PI*2.0);

                ctx.fill();
            }
            else if (mode == 2) // point3
            {
                var color = view.getUint8(offset, little_endian); offset += 1;
                var x_ndc = view.getFloat32(offset, little_endian); offset += 4;
                var y_ndc = view.getFloat32(offset, little_endian); offset += 4;
                var z_ndc = view.getFloat32(offset, little_endian); offset += 4;
            }
            else if (mode == 3) // line2
            {
                var color = view.getUint8(offset, little_endian); offset += 1;
                var x1_ndc = view.getFloat32(offset, little_endian); offset += 4;
                var y1_ndc = view.getFloat32(offset, little_endian); offset += 4;
                var x2_ndc = view.getFloat32(offset, little_endian); offset += 4;
                var y2_ndc = view.getFloat32(offset, little_endian); offset += 4;

                var x1 = (0.5+0.5*x1_ndc*a)*cvs.width;
                var y1 = (0.5+0.5*y1_ndc)*cvs.height;
                var x2 = (0.5+0.5*x2_ndc*a)*cvs.width;
                var y2 = (0.5+0.5*y2_ndc)*cvs.height;

                ctx.strokeStyle = palette(color);

                ctx.lineWidth = 4;

                ctx.beginPath();
                ctx.moveTo(x1, y1);
                ctx.lineTo(x2, y2);
                ctx.stroke();
            }
            else if (mode == 4) // line3
            {
                var color = view.getUint8(offset, little_endian); offset += 1;
                var x1_ndc = view.getFloat32(offset, little_endian); offset += 4;
                var y1_ndc = view.getFloat32(offset, little_endian); offset += 4;
                var z1_ndc = view.getFloat32(offset, little_endian); offset += 4;
                var x2_ndc = view.getFloat32(offset, little_endian); offset += 4;
                var y2_ndc = view.getFloat32(offset, little_endian); offset += 4;
                var z2_ndc = view.getFloat32(offset, little_endian); offset += 4;
            }
            else if (mode == 5) // rect
            {
                var color = view.getUint8(offset, little_endian); offset += 1;
                var x_ndc = view.getFloat32(offset, little_endian); offset += 4;
                var y_ndc = view.getFloat32(offset, little_endian); offset += 4;
                var w_ndc = view.getFloat32(offset, little_endian); offset += 4;
                var h_ndc = view.getFloat32(offset, little_endian); offset += 4;

                ctx.fillStyle = palette(color);
                ctx.beginPath();

                // var a = cvs.height/cvs.width;
                var a = 1.0;
                var x = (0.5+0.5*x_ndc*a)*cvs.width;
                var y = (0.5+0.5*y_ndc)*cvs.height;
                var w = w_ndc*a*cvs.width;
                var h = h_ndc*cvs.height;
                ctx.fillRect(x,y,w,h);
                // ctx.moveTo(x, y);
                // ctx.arc(x, y, 6, 0, Math.PI*2.0);

                ctx.fill();
            }
        }
    }
    else
    {
        status.innerHTML = "No connection";
    }
}

function loop()
{
    setTimeout(function()
    {
        requestAnimationFrame(loop);
        render();
    }, 1000 / fps);
}

function try_connect()
{
    setInterval(function()
    {
        if (!ws)
        {
            // console.log("Retry!");
            ws = new WebSocket("ws://localhost:8000");
            ws.binaryType = 'arraybuffer';

            ws.onopen = function()
            {
                // console.log("Sending data");
                ws.send("Hello from browser!");
                has_connection = true;
            }

            ws.onclose = function()
            {
                // console.log("Socket closed");
                ws = null;
                has_connection = false;
            }

            ws.onmessage = function(e)
            {
                cmd_data = e.data;
                // @ TODO: Parse and unpack data, convert to correct endian, etc
            }
        }
    }, 1000);
}

function app_onload()
{
    if (!("WebSocket" in window))
    {
        alert("Your browser does not support WebSockets! Sorry, good luck!");
    }
    else
    {
        cvs = document.getElementById("canvas");
        ctx = canvas.getContext("2d");
        loop();
        try_connect();
    }
}

function ws_shutdown_server()
{
    ws.send("shutdown");
}
