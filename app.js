var ws = null;
var cvs = 0;
var ctx = 0;
var cmd_data = 0;
var has_connection = false;
var fps = 60;
var palette_index = 0;

var viewport_xl = 0;
var viewport_xr = 0;
var viewport_yb = 0;
var viewport_yt = 0;

function set_viewport(mode)
{
    if (mode === "letterbox")
    {
        if (cvs.width > cvs.height)
        {
            viewport_xl = (cvs.width-cvs.height)/2.0;
            viewport_xr = (cvs.width+cvs.height)/2.0;
            viewport_yb = 0;
            viewport_yt = cvs.height;
        }
        else
        {
            viewport_xl = 0;
            viewport_xr = cvs.width;
            viewport_yb = (cvs.height-cvs.width)/2.0;
            viewport_yt = (cvs.height+cvs.width)/2.0;
        }
    }
    if (mode === "stretch")
    {
        viewport_xl = 0;
        viewport_xr = cvs.width;
        viewport_yb = 0;
        viewport_yt = cvs.height;
    }
}

function onchange_select_viewport()
{
    var element = document.getElementById("select_viewport");
    var mode = element.options[element.selectedIndex].value;
    set_viewport(mode);
}

function palette(color)
{
    var i = palette_index;
    var palettes = [
        // Spectral
        ["#9E0142", "#D53E4F", "#F46D43", "#FDAE61", "#FEE08B", "#FFFFBF", "#E6F598", "#ABDDA4", "#66C2A5", "#3288BD", "#5E4FA2"],

        // BrownBlueGreen
        ["#003C30", "#01665E", "#35978F", "#80CDC1", "#C7EAE5", "#F5F5F5", "#F6E8C3", "#DFC27D", "#BF812D", "#8C510A", "#543005"],

        // Blue
        ["#08306B", "#08519C", "#2171B5", "#4292C6", "#6BAED6", "#9ECAE1", "#C6DBEF", "#DEEBF7", "#F7FBFF"],

        // GreenBlue
        ["#084081", "#0868AC", "#2B8CBE", "#4EB3D3", "#7BCCC4", "#A8DDB5", "#CCEBC5", "#E0F3DB", "#F7FCF0"],

        // Set 1
        ["#E41A1C", "#377EB8", "#4DAF4A", "#984EA3", "#FF7F00", "#FFFF33", "#A65628", "#F781BF"]
    ];
    return palettes[i][color % (palettes[i].length)];
}

function x_ndc_to_viewport(x_ndc) { return viewport_xl + (viewport_xr-viewport_xl)*(0.5+0.5*x_ndc); }
function y_ndc_to_viewport(y_ndc) { return viewport_yb + (viewport_yt-viewport_yb)*(0.5+0.5*y_ndc); }

function render()
{
    var status = document.getElementById("status");

    if (has_connection && cmd_data != 0)
    {
        status.innerHTML = "Connected!";

        var select_color = document.getElementById("color_palette");
        palette_index = parseInt(select_color.options[select_color.selectedIndex].value);

        // @ Maybe TypedArray has better perf for many points?
        // @ Negotiate endianness with server
        var little_endian = true;
        var view = new DataView(cmd_data);
        var offset = 0;

        ctx.fillStyle="#2a2a2a";
        // ctx.fillRect(viewport_xl, viewport_yb, viewport_xr-viewport_xl, viewport_yt-viewport_yb);
        ctx.fillRect(0, 0, cvs.width, cvs.height);
        ctx.fill();

        while (offset < view.byteLength)
        {
            var a = cvs.height/cvs.width;
            var mode = view.getUint8(offset, little_endian); offset += 1;
            if (mode == 1) // point2
            {
                var color = view.getUint8(offset, little_endian); offset += 1;
                var x_ndc = view.getFloat32(offset, little_endian); offset += 4;
                var y_ndc = view.getFloat32(offset, little_endian); offset += 4;
                var x = x_ndc_to_viewport(x_ndc);
                var y = y_ndc_to_viewport(y_ndc);

                ctx.fillStyle = palette(color);
                ctx.beginPath();
                ctx.fillRect(x-3,y-3,6,6);
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
                var x1 = x_ndc_to_viewport(x1_ndc);
                var y1 = y_ndc_to_viewport(y1_ndc);
                var x2 = x_ndc_to_viewport(x2_ndc);
                var y2 = y_ndc_to_viewport(y2_ndc);

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
                var x = x_ndc_to_viewport(x_ndc);
                var y = y_ndc_to_viewport(y_ndc);
                var w = x_ndc_to_viewport(x_ndc+w_ndc)-x;
                var h = x_ndc_to_viewport(x_ndc+w_ndc)-y;

                ctx.fillStyle = palette(color);
                ctx.beginPath();
                ctx.fillRect(x,y,w,h);
                ctx.fill();
            }
            // else if (mode == 6) // circle
            // {
            //     var color = view.getUint8(offset, little_endian); offset += 1;
            //     var x_ndc = view.getFloat32(offset, little_endian); offset += 4;
            //     var y_ndc = view.getFloat32(offset, little_endian); offset += 4;
            //     var r_ndc = view.getFloat32(offset, little_endian); offset += 4;

            //     ctx.fillStyle = palette(color);
            //     ctx.beginPath();

            //     // var a = cvs.height/cvs.width;
            //     var a = 1.0;
            //     var x = (0.5+0.5*x_ndc*a)*cvs.width;
            //     var y = (0.5+0.5*y_ndc)*cvs.height;
            //     ctx.moveTo(x, y);
            //     ctx.arc(x, y, 6, 0, Math.PI*2.0);

            //     ctx.fill();
            // }
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
        set_viewport("letterbox");
    }
}

function ws_shutdown_server()
{
    ws.send("shutdown");
}
