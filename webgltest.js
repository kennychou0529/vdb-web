var cvs = null;
var gl = null;
var program = null;

var loc_coord = null;
var loc_color = null;
var loc_texel = null;
var loc_chan0 = null;

var vbo_user_coord = null;
var vbo_user_color = null;

var vbo_user_texel = null;
var tex_white = null;
var tex_view0 = null;

var mode_line = 1;
var mode_point = 2;
var mode_image = 3;
var palette_index = 0;

var vbo_quad_coord = null;
var vbo_quad_color = null;
var vbo_quad_texel = null;
var tex_view0_active = false;

function createShader(gl, type, source)
{
    var shader = gl.createShader(type);
    gl.shaderSource(shader, source);
    gl.compileShader(shader);
    var success = gl.getShaderParameter(shader, gl.COMPILE_STATUS);
    if (success)
        return shader;
    console.log(gl.getShaderInfoLog(shader));
    gl.deleteShader(shader);
}

function createProgram(gl, vs, fs)
{
    var program = gl.createProgram();
    gl.attachShader(program, vs);
    gl.attachShader(program, fs);
    gl.linkProgram(program);
    var success = gl.getProgramParameter(program, gl.LINK_STATUS);
    if (success)
        return program;
    console.log(gl.getProgramInfoLog(program));
    gl.deleteProgram(program);
}

function colorPalette(color)
{
    var palettes = [
        [0x9E0142FF,0xD53E4FFF,0xF46D43FF,0xFDAE61FF,0xFEE08BFF,0xFFFFBFFF,0xE6F598FF,0xABDDA4FF,0x66C2A5FF,0x3288BDFF,0x5E4FA2FF],
        [0x003C30FF,0x01665EFF,0x35978FFF,0x80CDC1FF,0xC7EAE5FF,0xF5F5F5FF,0xF6E8C3FF,0xDFC27DFF,0xBF812DFF,0x8C510AFF,0x543005FF],
        [0x08306BFF,0x08519CFF,0x2171B5FF,0x4292C6FF,0x6BAED6FF,0x9ECAE1FF,0xC6DBEFFF,0xDEEBF7FF,0xF7FBFFFF],
        [0x084081FF,0x0868ACFF,0x2B8CBEFF,0x4EB3D3FF,0x7BCCC4FF,0xA8DDB5FF,0xCCEBC5FF,0xE0F3DBFF,0xF7FCF0FF],
        [0xE41A1CFF,0x377EB8FF,0x4DAF4AFF,0x984EA3FF,0xFF7F00FF,0xFFFF33FF,0xA65628FF,0xF781BFFF]
    ];
    return palettes[palette_index][color % (palettes[palette_index].length)];
}

function toNDCX(x) { return x*cvs.height/cvs.width; }
function toNDCY(y) { return y; }

function generateTriangles(commands)
{
    tex_view0_active = false;

    var coords = [];
    var colors = [];
    var i = 0;
    var num_elements = 0;
    while (i < commands.length)
    {
        var mode = commands[i]; i++;
        var color_index = commands[i]; i++;
        var color = colorPalette(color_index);
        var color_r = (color >> 24) & 0xFF;
        var color_g = (color >> 16) & 0xFF;
        var color_b = (color >>  8) & 0xFF;
        var color_a = (color >>  0) & 0xFF;
        if (mode == mode_line)
        {
            var x1 = commands[i]; i++;
            var y1 = commands[i]; i++;
            var x2 = commands[i]; i++;
            var y2 = commands[i]; i++;

            var ln = Math.sqrt((x2-x1)*(x2-x1) + (y2-y1)*(y2-y1));
            var nx = -(y2-y1) / ln;
            var ny = (x2-x1) / ln;

            var w = 0.1;
            var x11 = toNDCX(x1 - nx*w);
            var y11 = toNDCY(y1 - ny*w);
            var x21 = toNDCX(x2 - nx*w);
            var y21 = toNDCY(y2 - ny*w);
            var x12 = toNDCX(x1 + nx*w);
            var y12 = toNDCY(y1 + ny*w);
            var x22 = toNDCX(x2 + nx*w);
            var y22 = toNDCY(y2 + ny*w);

            coords.push(x11,y11, x21,y21, x22,y22, x22,y22, x12,y12, x11,y11);
            colors.push(color_r,color_g,color_b,color_a,
                        color_r,color_g,color_b,color_a,
                        color_r,color_g,color_b,color_a,
                        color_r,color_g,color_b,color_a,
                        color_r,color_g,color_b,color_a,
                        color_r,color_g,color_b,color_a);
            num_elements += 6;
        }
        else if (mode == mode_point)
        {
            var x = commands[i]; i++;
            var y = commands[i]; i++;

            var x1 = toNDCX(x-0.05);
            var x2 = toNDCX(x+0.05);
            var y1 = toNDCY(y-0.05);
            var y2 = toNDCY(y+0.05);
            coords.push(x1,y1, x2,y1, x2,y2, x2,y2, x1,y2, x1,y1);
            colors.push(color_r,color_g,color_b,color_a,
                        color_r,color_g,color_b,color_a,
                        color_r,color_g,color_b,color_a,
                        color_r,color_g,color_b,color_a,
                        color_r,color_g,color_b,color_a,
                        color_r,color_g,color_b,color_a);
            num_elements += 6;
        }
        else if (mode == mode_image)
        {
            var width = commands[i]; i++;
            var height = commands[i]; i++;
            // channels, gray, etc.
            var data = new Uint8Array(width*height*4);
            data.set(commands.slice(i, i+width*height*4));
            i += width*height*4;

            gl.bindTexture(gl.TEXTURE_2D, tex_view0);
            gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, width, height, 0, gl.RGBA, gl.UNSIGNED_BYTE, data);
            gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
            gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
            gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
            gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
            gl.bindTexture(gl.TEXTURE_2D, null);

            tex_view0_active = true;
        }
    }

    gl.bindBuffer(gl.ARRAY_BUFFER, vbo_user_coord);
    gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(coords), gl.STATIC_DRAW);

    gl.bindBuffer(gl.ARRAY_BUFFER, vbo_user_color);
    gl.bufferData(gl.ARRAY_BUFFER, new Uint8Array(colors), gl.STATIC_DRAW);

    return num_elements;
}

function init()
{
    // Compile the ubershader
    var shader_vs_src = document.getElementById("shader_vs").text;
    var shader_fs_src = document.getElementById("shader_fs").text;
    var shader_vs = createShader(gl, gl.VERTEX_SHADER, shader_vs_src);
    var shader_fs = createShader(gl, gl.FRAGMENT_SHADER, shader_fs_src);
    program = createProgram(gl, shader_vs, shader_fs);

    // Get attribute locations
    loc_coord = gl.getAttribLocation(program, "coord");
    loc_color = gl.getAttribLocation(program, "color");
    loc_texel = gl.getAttribLocation(program, "texel");
    loc_chan0 = gl.getUniformLocation(program, "chan0");

    // These vertex buffers are updated in generateTriangles
    vbo_user_coord = gl.createBuffer();
    vbo_user_color = gl.createBuffer();
    vbo_user_texel = gl.createBuffer();

    // These vertex buffers are used to draw background textures
    vbo_quad_coord = gl.createBuffer();
    vbo_quad_color = gl.createBuffer();
    vbo_quad_texel = gl.createBuffer();
    {
        var coords = [ -1,-1, +1,-1, +1,+1, +1,+1, -1,+1, -1,-1 ];
        var texels = [ 0,0, 1,0, 1,1, 1,1, 0,1, 0,0 ];
        var colors = [ 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255 ];
        gl.bindBuffer(gl.ARRAY_BUFFER, vbo_quad_coord); gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(coords), gl.STATIC_DRAW);
        gl.bindBuffer(gl.ARRAY_BUFFER, vbo_quad_color); gl.bufferData(gl.ARRAY_BUFFER, new   Uint8Array(colors), gl.STATIC_DRAW);
        gl.bindBuffer(gl.ARRAY_BUFFER, vbo_quad_texel); gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(texels), gl.STATIC_DRAW);
        gl.bindBuffer(gl.ARRAY_BUFFER, null);
    }

    // This texture is for drawing non-textured geometry
    tex_white = gl.createTexture();
    {
        var data = new Uint8Array([255,255,255,255]);
        gl.bindTexture(gl.TEXTURE_2D, tex_white);
        gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, 1, 1, 0, gl.RGBA, gl.UNSIGNED_BYTE, data);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
        gl.bindTexture(gl.TEXTURE_2D, null);
    }

    // These textures can be assigned data by the user (from generateTriangles)
    tex_view0 = gl.createTexture();
}

function tick(t, dt)
{
    // Fill some mock commands
    var commands = [
        mode_line, 0, -0.1,-0.1+0.0*(0.5+0.5*Math.sin(t)), +0.1,+0.2+0.0*(0.5+0.5*Math.sin(t)),
        mode_line, 1, -0.1,-0.1+0.1*(0.5+0.5*Math.sin(t)), +0.1,+0.2+0.1*(0.5+0.5*Math.sin(t)),
        mode_line, 2, -0.1,-0.1+0.2*(0.5+0.5*Math.sin(t)), +0.1,+0.2+0.2*(0.5+0.5*Math.sin(t)),
        mode_line, 3, -0.1,-0.1+0.3*(0.5+0.5*Math.sin(t)), +0.1,+0.2+0.3*(0.5+0.5*Math.sin(t)),
        mode_line, 4, -0.1,-0.1+0.4*(0.5+0.5*Math.sin(t)), +0.1,+0.2+0.4*(0.5+0.5*Math.sin(t)),
        mode_line, 5, -0.1,-0.1+0.5*(0.5+0.5*Math.sin(t)), +0.1,+0.2+0.5*(0.5+0.5*Math.sin(t)),
        mode_point, 0, 0.5*Math.cos(t),0.5*Math.sin(t),
    ];
    var width = 64;
    var height = 64;
    commands.push(mode_image);
    commands.push(0);
    commands.push(width);
    commands.push(height);
    for (var y = 0; y < height; y++)
    for (var x = 0; x < width; x++)
    {
        commands.push((x%8)*4+150, (y%8)*8+100, 128, 255);
    }

    // Generate geometry every frame because we might decide to change it that often
    // (as opposed to only generating it when we parse commands)
    num_elements = generateTriangles(commands);

    // Resize framebuffer resolution to match size of displayed window
    if (cvs.width  != cvs.clientWidth || cvs.height != cvs.clientHeight)
    {
        cvs.width  = cvs.clientWidth;
        cvs.height = cvs.clientHeight;
    }

    gl.enable(gl.BLEND);
    gl.blendEquation(gl.FUNC_ADD);
    gl.blendFunc(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA);

    gl.viewport(0, 0, gl.drawingBufferWidth, gl.drawingBufferHeight);
    gl.clearColor(1, 1, 1, 1);
    gl.clear(gl.COLOR_BUFFER_BIT);
    gl.useProgram(program);

    // Draw background texture (if any)
    if (tex_view0_active)
    {
        gl.activeTexture(gl.TEXTURE0 + 0);
        gl.bindTexture(gl.TEXTURE_2D, tex_view0);
        gl.enableVertexAttribArray(loc_coord);
        gl.enableVertexAttribArray(loc_color);
        gl.enableVertexAttribArray(loc_texel);
        gl.bindBuffer(gl.ARRAY_BUFFER, vbo_quad_coord);
        gl.vertexAttribPointer(loc_coord, 2, gl.FLOAT, false, 0, 0);
        gl.bindBuffer(gl.ARRAY_BUFFER, vbo_quad_color);
        gl.vertexAttribPointer(loc_color, 4, gl.UNSIGNED_BYTE, true, 0, 0);
        gl.bindBuffer(gl.ARRAY_BUFFER, vbo_quad_texel);
        gl.vertexAttribPointer(loc_texel, 2, gl.FLOAT, false, 0, 0);
        gl.uniform1i(loc_chan0, 0);
        gl.drawArrays(gl.TRIANGLES, 0, 6);
        gl.bindTexture(gl.TEXTURE_2D, null);
    }

    // Draw user geometry
    gl.activeTexture(gl.TEXTURE0 + 0);
    gl.bindTexture(gl.TEXTURE_2D, tex_white);
    gl.enableVertexAttribArray(loc_coord);
    gl.enableVertexAttribArray(loc_color);
    gl.disableVertexAttribArray(loc_texel);
    gl.bindBuffer(gl.ARRAY_BUFFER, vbo_user_coord);
    gl.vertexAttribPointer(loc_coord, 2, gl.FLOAT, false, 0, 0);
    gl.bindBuffer(gl.ARRAY_BUFFER, vbo_user_color);
    gl.vertexAttribPointer(loc_color, 4, gl.UNSIGNED_BYTE, true, 0, 0);
    gl.uniform1i(loc_chan0, 0);
    gl.drawArrays(gl.TRIANGLES, 0, num_elements);
    gl.bindTexture(gl.TEXTURE_2D, null);
}

var animation_frame_t_first = null;
var animation_frame_t_prev = null;
function anim(t)
{
    var delta = 1.0/60.0;
    var elapsed = 0.0;
    if (t)
    {
        if (!animation_frame_t_first) animation_frame_t_first = t;
        if (!animation_frame_t_prev) animation_frame_t_prev = t;
        delta = (t - animation_frame_t_prev)/1000.0;
        elapsed = (t - animation_frame_t_first)/1000.0;
        animation_frame_t_prev = t;
    }
    tick(elapsed, delta);

    var p_fps = document.getElementById("fps");
    p_fps.innerText = "FPS: " + (1.0/delta).toPrecision(4);

    requestAnimationFrame(anim);
}

function load()
{
    cvs = document.getElementById("canvas");
    // gl = cvs.getContext("webgl");
    gl = cvs.getContext("webgl", {antialias: false});
    if (!gl)
    {
        console.log("Fuck");
        return;
    }

    init();
    anim();
}