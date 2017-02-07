var ws = 0;
var cvs = 0;
var ctx = 0;

// function doc_onkeydown(e)
// {
//     if (e.key == "Escape")
//         ws.send("shutdown");
// }
// document.onkeydown = doc_onkeydown;

function ws_onmessage(e)
{
    // console.log(e.data);

    // @ Maybe TypedArray has better perf for many points?

    // @ negotiate endianness with server
    var little_endian = true;
    var view = new DataView(e.data);
    var count = view.getInt32(0, little_endian);

    ctx.fillStyle="#AA4949";
    ctx.fillRect(0, 0, cvs.width, cvs.height);
    ctx.fill();

    ctx.fillStyle = "#E6D2C2";
    // ctx.strokeStyle = "#000000";
    ctx.beginPath();

    for (var i = 0; i < count; i++)
    {
        var x_ndc = view.getFloat32(2 + 4*(2*i+0), little_endian);
        var y_ndc = view.getFloat32(2 + 4*(2*i+1), little_endian);
        var a = cvs.height/cvs.width;
        var x = (0.5+0.5*x_ndc*a)*cvs.width;
        var y = (0.5+0.5*y_ndc)*cvs.height;
        ctx.moveTo(x, y);
        ctx.arc(x, y, 6, 0, Math.PI*2.0);
    }

    // ctx.stroke();
    ctx.fill();
}

function ws_connect()
{
    if (!("WebSocket" in window))
        alert("Your browser does not support WebSockets! Sorry, good luck!");
    ws = new WebSocket("ws://localhost:8000");
    ws.binaryType = 'arraybuffer';
    ws.onmessage = ws_onmessage;

    ws.onopen = function()
    {
        console.log("Sending data");
        ws.send("Hello from browser!");
    }

    ws.onclose = function()
    {
        console.log("Socket closed");
    }
}

function app_onload()
{
    var cvs = document.getElementById("canvas");
    var ctx = canvas.getContext("2d");
    if (ws == 0)
    {
        ws_connect();
    }
}

function ws_shutdown_server()
{
    ws.send("shutdown");
}
