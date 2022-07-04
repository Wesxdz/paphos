import contextlib, glfw, skia
from OpenGL import GL
import time
import math

WIDTH, HEIGHT = 640, 480

class Game:
    play_rate = 1.0
    start_time = time.time()
    loop_progress = 0.0
    last_time = time.time()
    mouse_icon = skia.Image.open("mouse.png")
    mouse_event_locations = []
    indicator_pos = skia.Point(0,0)

    def __init__(self) -> None:
        pass

    def progress(self):
        t = time.time()
        self.loop_progress += t - self.last_time
        self.last_time = t

    def test(self, window, x, y):
        pass
        # self.mouse_event_locations.append(self.indicator_pos)


def render_skia(game, surface):
    with surface as canvas:
        canvas.clear(0)
        canvas.drawColor(skia.ColorBLACK)
        paint = skia.Paint(
            Style=skia.Paint.kFill_Style,
            AntiAlias=True,
            StrokeWidth=4,
            Color=0xFF4285F4)

        paint.setColor(0xFF0F9D58)
        rad = 128# + math.sin(time.time() * 2) * 2

        paint.setStyle(skia.Paint.kStroke_Style)
        paint.setStrokeWidth(1.0)
        path = skia.Path() # TODO: Editor panel in Blender
        path.moveTo(WIDTH/2, HEIGHT/2)
        a = 1
        k = 0.2
        mat = skia.Matrix()
        mat.setRotate(game.loop_progress * -360/8, WIDTH/2, HEIGHT/2)

        spiral_points = []
        for i in range(500):
            phi = 1 + i * 0.1
            spiral_x = WIDTH/2 + a * math.pow(math.e, k * phi) * math.cos(phi)
            spiral_y = HEIGHT/2 + a * math.pow(math.e, k * phi) * math.sin(phi)
            spiral_points.append(skia.Point(spiral_x, spiral_y))

        # evaluate which point in spiral is closest to the loop radius
        
        rot_spiral_points = mat.mapPoints(spiral_points)

        paint.setColor(int("22666666", 16))
        for i, point in enumerate(rot_spiral_points):
            path.lineTo(point)
        canvas.drawPath(path, paint)
        
        paint.setStyle(skia.Paint.kFill_Style)
        canvas.drawCircle(rot_spiral_points[230], 8, paint)
        game.indicator_pos = rot_spiral_points[230]
        # ind = int(game.loop_progress * 10.0) % 500
        for pos in game.mouse_event_locations:
            # canvas.drawCircle(pos, 14, paint)
            canvas.drawImage(game.mouse_icon, pos.x() - game.mouse_icon.width()/2, pos.y() - game.mouse_icon.height()/2)

        # colors[i].get_hex()[1:]
        if game.play_rate > 0.0:
            paint.setColor(int("CC666666", 16))
        else:
            paint.setColor(int("66666666", 16))

        paint2 = paint
        paint2.setPathEffect(skia.DashPathEffect.Make([4.0, 2.0], 0.0))
        paint.setStyle(skia.Paint.kStroke_Style)
        paint.setStrokeWidth(1.0)
        for i in range(2):
            canvas.drawCircle(WIDTH/2, HEIGHT/2, rad + i * 32, paint)
        # for i in range(8):
        cursor_pos = glfw.get_cursor_pos(window) # Get angle of cursor
        cursor_pos = skia.Point(cursor_pos[0], cursor_pos[1])
        line_points = [skia.Point(WIDTH/2, (HEIGHT/2) - 160), skia.Point(WIDTH/2, HEIGHT/2 - 128)]
        path = skia.Path()
        mat = skia.Matrix()
        center_cursor_offset = cursor_pos - skia.Point(WIDTH/2, HEIGHT/2)
        center_cursor_offset.normalize()
        od = 90 + math.degrees(math.atan2(center_cursor_offset.y(), center_cursor_offset.x()))
        mat.setRotate(od, WIDTH/2, HEIGHT/2)
        rotated_line_points = mat.mapPoints(line_points)
        path.moveTo(rotated_line_points[0])
        path.lineTo(rotated_line_points[1])
        # canvas.drawPath(path, paint2)
        
        arc_outliner = skia.Path()
        arc_outliner.moveTo(rotated_line_points[0])
        paint.setColor(0xFF0e5de0)
        arc_outliner.addArc(skia.Rect(WIDTH/2 - 160, HEIGHT/2 - 160, WIDTH/2 + 160, HEIGHT/2 + 160), od - 90 - 15, 30)
        paint.setPathEffect(skia.DashPathEffect.Make([1.0, 0.0], 0.0))
        paint.setStrokeWidth(2.0)
        canvas.drawPath(arc_outliner, paint)

        arc_outliner = skia.Path()
        arc_outliner.moveTo(rotated_line_points[1])
        paint.setColor(0xFF0e5de0)
        arc_outliner.addArc(skia.Rect(WIDTH/2 - 128, HEIGHT/2 - 128, WIDTH/2 + 128, HEIGHT/2 + 128), od - 90 - 15, 30)
        paint.setPathEffect(skia.DashPathEffect.Make([1.0, 0.0], 0.0))
        canvas.drawPath(arc_outliner, paint)

        # paint.setStyle(skia.Paint.kFill_Style)
        # paint.setColor(0xFFFFFFFF)
        # canvas.drawString('paphos', 10, 32, skia.Font(None, 36), paint)



@contextlib.contextmanager
def glfw_window():
    if not glfw.init():
        raise RuntimeError('glfw.init() failed')
    glfw.window_hint(glfw.STENCIL_BITS, 8)
    # glfw.window_hint(glfw.DECORATED, False)
    window = glfw.create_window(WIDTH, HEIGHT, 'Paphos Visual Prototype 0.0.1', None, None)
    glfw.make_context_current(window)
    yield window
    glfw.terminate()

@contextlib.contextmanager
def skia_surface(window):
    context = skia.GrDirectContext.MakeGL()
    (fb_width, fb_height) = glfw.get_framebuffer_size(window)
    backend_render_target = skia.GrBackendRenderTarget(
        fb_width,
        fb_height,
        0,  # sampleCnt
        0,  # stencilBits
        skia.GrGLFramebufferInfo(0, GL.GL_RGBA8))
    surface = skia.Surface.MakeFromBackendRenderTarget(
        context, backend_render_target, skia.kBottomLeft_GrSurfaceOrigin,
        skia.kRGBA_8888_ColorType, skia.ColorSpace.MakeSRGB())
    assert surface is not None
    yield surface
    context.abandonContext()

with glfw_window() as window:
    GL.glClearColor(255, 0, 0, 0)
    GL.glClear(GL.GL_COLOR_BUFFER_BIT)

    game = Game()
    glfw.set_cursor_pos_callback(window, game.test)
    while not glfw.window_should_close(window):
        game.progress()
        with skia_surface(window) as surface:
            render_skia(game, surface)
            surface.flushAndSubmit()
            glfw.swap_buffers(window)
            glfw.poll_events()
glfw.terminate()