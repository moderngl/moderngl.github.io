import math

import glm
import js
import pyodide_js
from pyodide.ffi import create_proxy

import moderngl
import toys

canvas = js.document.getElementById('canvas')
gl = canvas.getContext('webgl2', antialias=False, depth=True, stencil=True, alpha=True)
symbols = js.window.eval(moderngl._extern_gl)(pyodide_js._module, gl)
pyodide_js._module.mergeLibSymbols(symbols)

ctx = moderngl.get_context()

toys.init()

pointer = {'x': 0.0, 'y': 0.0, 'pressed': False}


def on_move(e):
    pointer['x'] = e.offsetX / max(canvas.clientWidth, 1) * 2.0 - 1.0
    pointer['y'] = 1.0 - e.offsetY / max(canvas.clientHeight, 1) * 2.0


def on_down(e):
    on_move(e)
    pointer['pressed'] = True
    canvas.setPointerCapture(e.pointerId)


def on_up(e):
    pointer['pressed'] = False


listeners = [create_proxy(on_move), create_proxy(on_down), create_proxy(on_up)]
canvas.addEventListener('pointermove', listeners[0])
canvas.addEventListener('pointerdown', listeners[1])
canvas.addEventListener('pointerup', listeners[2])
canvas.addEventListener('pointercancel', listeners[2])

tile = 256
colors = ['#e63946', '#f4a261', '#e9c46a', '#2a9d8f', '#457b9d', '#8e7dbe', '#ef476f', '#06d6a0']
painter = js.document.createElement('canvas')
painter.width = tile
painter.height = tile * (len(colors) + 1)
g = painter.getContext('2d')
g.textAlign = 'center'
g.textBaseline = 'middle'
g.font = f'900 {int(tile * 0.7)}px system-ui, sans-serif'
for i, color in enumerate(colors):
    y = i * tile
    g.fillStyle = color
    g.fillRect(0, y, tile, tile)
    g.strokeStyle = 'rgba(0, 0, 0, 0.15)'
    g.lineWidth = 16
    g.strokeRect(8, y + 8, tile - 16, tile - 16)
    g.fillStyle = '#ffffff'
    g.fillText(toys.letters[i], tile / 2, y + tile * 0.54)

y = len(colors) * tile
g.fillStyle = '#141414'
g.fillRect(0, y, tile, tile)
g.fillStyle = '#1e1e1e'
g.fillRect(0, y, tile / 2, tile / 2)
g.fillRect(tile / 2, y + tile / 2, tile / 2, tile / 2)

texture = ctx.texture((tile, tile), 'rgba8unorm', bytes(g.getImageData(0, 0, tile, painter.height).data.to_py()), array=len(colors) + 1, levels=0)
texture.mipmaps()

ctx.shaders['mesh.vert'] = '''
    #version 330 core

    layout (std140) uniform Common {
        mat4 camera;
    };

    layout (location = 0) in vec3 in_vertex;
    layout (location = 1) in vec3 in_normal;
    layout (location = 2) in vec3 in_uvw;

    out vec3 v_vertex;
    out vec3 v_normal;
    out vec3 v_uvw;

    void main() {
        gl_Position = camera * vec4(in_vertex, 1.0);
        v_vertex = in_vertex;
        v_normal = in_normal;
        v_uvw = in_uvw;
    }
'''

ctx.shaders['mesh.frag'] = '''
    #version 330 core

    uniform sampler2DArray Texture;

    in vec3 v_vertex;
    in vec3 v_normal;
    in vec3 v_uvw;

    layout (location = 0) out vec4 out_color;

    void main() {
        vec3 light = normalize(vec3(0.3, -0.6, 1.0));
        float lum = 0.55 + 0.45 * max(dot(normalize(v_normal), light), 0.0);
        vec3 color = texture(Texture, v_uvw).rgb * lum;
        float fog = smoothstep(12.0, 30.0, length(v_vertex.xy));
        out_color = vec4(mix(color, vec3(0.05), fog), 1.0);
    }
'''

uniform_buffer = ctx.buffer(size=64)
vertex_buffer = ctx.buffer(size=toys.vertices * 36)

ctx.screen.clear_color = (0.05, 0.05, 0.05, 1.0)

pipeline = ctx.pipeline(
    'mesh.vert',
    'mesh.frag',
    samplers={'Texture': 0},
    uniform_buffers={'Common': 0},
    vertex_array=ctx.vertex_array(vertex_buffer.bind('3f 3f 3f', 0, 1, 2), vertices=toys.vertices),
    framebuffer=ctx.screen,
    resources=ctx.resources(samplers={0: ctx.sampler(texture)}, uniform_buffers={0: uniform_buffer}),
    depth_test=True,
    cull_face='back',
)

last = 0.0


def render(timestamp):
    global last
    if canvas.style.display == 'none':
        canvas.style.display = ''
    ratio = min(js.window.devicePixelRatio, 2.0)
    size = (max(int(canvas.clientWidth * ratio), 1), max(int(canvas.clientHeight * ratio), 1))
    if size != ctx.screen.size:
        canvas.width, canvas.height = size
        ctx.screen.size = size
    dt = min((timestamp - last) / 1000.0, 0.1) if last else 0.0
    last = timestamp

    aspect = size[0] / size[1]
    distance = max(7.0 / (math.tan(math.radians(22.5)) * aspect), 10.0)
    target = glm.vec3(0.0, 0.0, 0.5)
    eye = target + glm.vec3(0.0, -math.cos(0.45), math.sin(0.45)) * distance
    camera = glm.perspective(math.radians(45.0), aspect, 0.1, 1000.0) * glm.lookAt(eye, target, glm.vec3(0.0, 0.0, 1.0))
    inverse = glm.inverse(camera)
    near = inverse * glm.vec4(pointer['x'], pointer['y'], -1.0, 1.0)
    far = inverse * glm.vec4(pointer['x'], pointer['y'], 1.0, 1.0)
    origin = near.xyz / near.w
    direction = glm.normalize(far.xyz / far.w - origin)

    toys.update(dt, tuple(origin), tuple(direction), pointer['pressed'])
    vertex_buffer.write(toys.mesh())
    uniform_buffer.write(camera.to_bytes())

    ctx.start_frame()
    ctx.screen.clear()
    pipeline.run()
    ctx.end_frame()
    js.requestAnimationFrame(render_proxy)


render_proxy = create_proxy(render)
js.requestAnimationFrame(render_proxy)
