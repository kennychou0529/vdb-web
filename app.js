var ws = null;
var cvs = 0;
var ctx = 0;
var cmd_data = 0;
var fps = 60;

function render()
{
    var status = document.getElementById("status");

    if (ws && cmd_data != 0)
    {
        status.innerHTML = "Connected!";

        // @ Maybe TypedArray has better perf for many points?
        // @ Negotiate endianness with server
        var little_endian = true;
        var view = new DataView(cmd_data);
        var count = view.getInt32(0, little_endian);

        ctx.fillStyle="#AA4949";
        ctx.fillRect(0, 0, cvs.width, cvs.height);
        ctx.fill();

        ctx.fillStyle = "#E6D2C2";
        // ctx.strokeStyle = "#000000";
        ctx.beginPath();

        for (var i = 0; i < count; i++)
        {
            var x_ndc = view.getFloat32(4 + 4*(2*i+0), little_endian);
            var y_ndc = view.getFloat32(4 + 4*(2*i+1), little_endian);
            var a = cvs.height/cvs.width;
            var x = (0.5+0.5*x_ndc*a)*cvs.width;
            var y = (0.5+0.5*y_ndc)*cvs.height;
            ctx.fillRect(x,y,5,5);
            // ctx.moveTo(x, y);
            // ctx.arc(x, y, 6, 0, Math.PI*2.0);
        }

        // ctx.stroke();
        ctx.fill();
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
            }

            ws.onclose = function()
            {
                // console.log("Socket closed");
                ws = null;
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
