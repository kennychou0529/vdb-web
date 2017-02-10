var ws = null;
var cvs = 0;
var ctx = 0;
var cmd_data = 0;
var has_connection = false;
var fps = 60;

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
        var num_line2 = view.getUint32(offset, little_endian); offset += 4;
        var num_line3 = view.getUint32(offset, little_endian); offset += 4;
        var num_point2 = view.getUint32(offset, little_endian); offset += 4;
        var num_point3 = view.getUint32(offset, little_endian); offset += 4;

        ctx.fillStyle="#1a1a1a";
        ctx.fillRect(0, 0, cvs.width, cvs.height);
        ctx.fill();

        for (var i = 0; i < num_line2; i++)
        {
            var color = view.getUint8(offset, little_endian); offset += 1;
            var x1_ndc = view.getFloat32(offset, little_endian); offset += 4;
            var y1_ndc = view.getFloat32(offset, little_endian); offset += 4;
            var x2_ndc = view.getFloat32(offset, little_endian); offset += 4;
            var y2_ndc = view.getFloat32(offset, little_endian); offset += 4;

            var a = cvs.height/cvs.width;
            var x1 = (0.5+0.5*x1_ndc*a)*cvs.width;
            var y1 = (0.5+0.5*y1_ndc)*cvs.height;
            var x2 = (0.5+0.5*x2_ndc*a)*cvs.width;
            var y2 = (0.5+0.5*y2_ndc)*cvs.height;

            ctx.strokeStyle = "#fff";
            ctx.beginPath();
            ctx.moveTo(x1, y1);
            ctx.lineTo(x2, y2);
            ctx.stroke();
        }

        for (var i = 0; i < num_line3; i++)
        {
            var color = view.getUint8(offset, little_endian); offset += 1;
            var x1_ndc = view.getFloat32(offset, little_endian); offset += 4;
            var y1_ndc = view.getFloat32(offset, little_endian); offset += 4;
            var z1_ndc = view.getFloat32(offset, little_endian); offset += 4;
            var x2_ndc = view.getFloat32(offset, little_endian); offset += 4;
            var y2_ndc = view.getFloat32(offset, little_endian); offset += 4;
            var z2_ndc = view.getFloat32(offset, little_endian); offset += 4;
        }

        for (var i = 0; i < num_point2; i++)
        {
            var color = view.getUint8(offset, little_endian); offset += 1;
            var x_ndc = view.getFloat32(offset, little_endian); offset += 4;
            var y_ndc = view.getFloat32(offset, little_endian); offset += 4;

                 if (color == 0) ctx.fillStyle = "#990343";
            else if (color == 1) ctx.fillStyle = "#D33F4D";
            else if (color == 2) ctx.fillStyle = "#F56D47";
            else if (color == 3) ctx.fillStyle = "#FDAE5F";
            else if (color == 4) ctx.fillStyle = "#FFDE8D";
            else if (color == 5) ctx.fillStyle = "#FFFFBE";
            else if (color == 6) ctx.fillStyle = "#FEDF8C";
            else if (color == 7) ctx.fillStyle = "#E3F69C";
            else if (color == 8) ctx.fillStyle = "#65C39F";
            else if (color == 9) ctx.fillStyle = "#3486BE";
            else                 ctx.fillStyle = "#5E509F";
            ctx.beginPath();

            var a = cvs.height/cvs.width;
            var x = (0.5+0.5*x_ndc*a)*cvs.width;
            var y = (0.5+0.5*y_ndc)*cvs.height;
            ctx.fillRect(x-4,y-4,8,8);
            // ctx.moveTo(x, y);
            // ctx.arc(x, y, 6, 0, Math.PI*2.0);

            ctx.fill();
        }

        for (var i = 0; i < num_point3; i++)
        {
            var color = view.getUint8(offset, little_endian); offset += 1;
            var x = view.getFloat32(offset, little_endian); offset += 4;
            var y = view.getFloat32(offset, little_endian); offset += 4;
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
