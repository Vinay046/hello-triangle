#include <wayland-client-core.h>
#include <wayland-client.h>
#include <wayland-server.h>
#include <wayland-client-protocol.h>
#include <wayland-egl.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <GLES2/gl2.h>
#include <EGL/egl.h>
#include <EGL/eglplatform.h>
#include <linux/input-event-codes.h>

#include "xdg-shell-client-protocol.h"
#include "glm_wrapper.h"

#if defined(DEBUG)
#define LOG(...) fprintf(stderr, __VA_ARGS__)
#else
#define LOG(...)
#endif

struct _escontext {
    EGLNativeDisplayType native_display;
    EGLNativeWindowType native_window;
    uint16_t window_width, window_height;
    EGLDisplay display;
    EGLContext context;
    EGLSurface surface;
};

void CreateNativeWindow(char* title, int width, int height);
EGLBoolean CreateEGLContext();
EGLBoolean CreateWindowWithEGLContext(char *title, int width, int height);
void RefreshWindow();

struct wl_compositor *compositor = NULL;
struct wl_surface *surface;
struct wl_egl_window *egl_window;
struct wl_region *region;
struct wl_seat *seat = NULL;
struct wl_keyboard *keyboard = NULL;

struct xdg_wm_base *XDGWMBase;
struct xdg_surface *XDGSurface;
struct xdg_toplevel *XDGToplevel;

struct _escontext ESContext = {
    .native_display = NULL,
    .window_width = 0,
    .window_height = 0,
    .native_window  = 0,
    .display = NULL,
    .context = NULL,
    .surface = NULL
};

#define TRUE 1
#define FALSE 0

#define WINDOW_WIDTH 1920
#define WINDOW_HEIGHT 1080

bool program_alive;
int32_t old_w, old_h;

static void xdg_toplevel_handle_configure(void *data __attribute__((unused)),
        struct xdg_toplevel *xdg_toplevel __attribute__((unused)), int32_t w, int32_t h,
        struct wl_array *states __attribute__((unused))) {

    if(w == 0 && h == 0) return;

    if(old_w != w && old_h != h) {
        old_w = w;
        old_h = h;

        wl_egl_window_resize(ESContext.native_window, w, h, 0, 0);
        wl_surface_commit(surface);
    }
}

static void xdg_toplevel_handle_close(void *data __attribute__((unused)),
        struct xdg_toplevel *xdg_toplevel __attribute__((unused))) {
    program_alive = false;
}

struct xdg_toplevel_listener xdg_toplevel_listener = {
    .configure = xdg_toplevel_handle_configure,
    .close = xdg_toplevel_handle_close,
};

static void xdg_surface_configure(void *data __attribute__((unused)), struct xdg_surface *xdg_surface,
        uint32_t serial) {
    xdg_surface_ack_configure(xdg_surface, serial);
}

const struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_configure,
};

static void xdg_wm_base_ping(void *data __attribute__((unused)), struct xdg_wm_base *xdg_wm_base,
        uint32_t serial) {
    xdg_wm_base_pong(xdg_wm_base, serial);
}

const struct xdg_wm_base_listener xdg_wm_base_listener = {
    .ping = xdg_wm_base_ping,
};

static void keyboard_handle_keymap(void *data __attribute__((unused)), struct wl_keyboard *keyboard __attribute__((unused)), uint32_t format __attribute__((unused)), int fd __attribute__((unused)), uint32_t size __attribute__((unused))) {
    // This function can be left empty
}

static void keyboard_handle_enter(void *data __attribute__((unused)), struct wl_keyboard *keyboard __attribute__((unused)), uint32_t serial __attribute__((unused)), struct wl_surface *surface __attribute__((unused)), struct wl_array *keys __attribute__((unused))) {
    // This function can be left empty
}

static void keyboard_handle_leave(void *data __attribute__((unused)), struct wl_keyboard *keyboard __attribute__((unused)), uint32_t serial __attribute__((unused)), struct wl_surface *surface __attribute__((unused))) {
    // This function can be left empty
}

static void keyboard_handle_key(void *data __attribute__((unused)), struct wl_keyboard *keyboard __attribute__((unused)), uint32_t serial __attribute__((unused)), uint32_t time __attribute__((unused)), uint32_t key, uint32_t state) {
    if (state == WL_KEYBOARD_KEY_STATE_PRESSED && key == KEY_Q) {
        program_alive = false;
    }
}

static void keyboard_handle_modifiers(void *data __attribute__((unused)), struct wl_keyboard *keyboard __attribute__((unused)), uint32_t serial __attribute__((unused)), uint32_t mods_depressed __attribute__((unused)), uint32_t mods_latched __attribute__((unused)), uint32_t mods_locked __attribute__((unused)), uint32_t group __attribute__((unused))) {
    // This function can be left empty
}

static void keyboard_handle_repeat_info(void *data __attribute__((unused)), struct wl_keyboard *keyboard __attribute__((unused)), int32_t rate __attribute__((unused)), int32_t delay __attribute__((unused))) {
    // This function can be left empty
}

static const struct wl_keyboard_listener keyboard_listener = {
    .keymap = keyboard_handle_keymap,
    .enter = keyboard_handle_enter,
    .leave = keyboard_handle_leave,
    .key = keyboard_handle_key,
    .modifiers = keyboard_handle_modifiers,
    .repeat_info = keyboard_handle_repeat_info
};

static void seat_handle_capabilities(void *data __attribute__((unused)), struct wl_seat *seat, uint32_t capabilities) {
    if (capabilities & WL_SEAT_CAPABILITY_KEYBOARD) {
        keyboard = wl_seat_get_keyboard(seat);
        wl_keyboard_add_listener(keyboard, &keyboard_listener, NULL);
    }
}

static const struct wl_seat_listener seat_listener = {
    .capabilities = seat_handle_capabilities,
    .name = NULL
};

void CreateNativeWindow(char *title __attribute__((unused)), int width, int height) {
    old_w = WINDOW_WIDTH;
    old_h = WINDOW_HEIGHT;

    region = wl_compositor_create_region(compositor);

    wl_region_add(region, 0, 0, width, height);
    wl_surface_set_opaque_region(surface, region);

    egl_window = wl_egl_window_create(surface, width, height);

    if (egl_window == EGL_NO_SURFACE) {
        LOG("No window !?\n");
        exit(1);
    } else {
        LOG("Window created !\n");
    }

    ESContext.window_width = width;
    ESContext.window_height = height;
    ESContext.native_window = egl_window;
}

EGLBoolean CreateEGLContext () {
    EGLint numConfigs;
    EGLint majorVersion;
    EGLint minorVersion;
    EGLContext context;
    EGLSurface surface;
    EGLConfig config;
    EGLint fbAttribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_NONE
    };
    EGLint contextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE, EGL_NONE };
    EGLDisplay display = eglGetDisplay(ESContext.native_display);

    if (display == EGL_NO_DISPLAY) {
        LOG("No EGL Display...\n");
        return EGL_FALSE;
    }

    if (!eglInitialize(display, &majorVersion, &minorVersion)) {
        LOG("No Initialization...\n");
        return EGL_FALSE;
    }

    if ((eglGetConfigs(display, NULL, 0, &numConfigs) != EGL_TRUE) || (numConfigs == 0)) {
        LOG("No configuration...\n");
        return EGL_FALSE;
    }

    if ((eglChooseConfig(display, fbAttribs, &config, 1, &numConfigs) != EGL_TRUE) || (numConfigs != 1)) {
        LOG("No configuration...\n");
        return EGL_FALSE;
    }

    surface = eglCreateWindowSurface(display, config, ESContext.native_window, NULL);
    if (surface == EGL_NO_SURFACE) {
        LOG("No surface...\n");
        return EGL_FALSE;
    }

    context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttribs);
    if (context == EGL_NO_CONTEXT) {
        LOG("No context...\n");
        return EGL_FALSE;
    }

    if (!eglMakeCurrent(display, surface, surface, context)) {
        LOG("Could not make the current window current !\n");
        return EGL_FALSE;
    }

    ESContext.display = display;
    ESContext.surface = surface;
    ESContext.context = context;
    return EGL_TRUE;
}

EGLBoolean CreateWindowWithEGLContext(char *title, int width, int height) {
    CreateNativeWindow(title, width, height);
    return CreateEGLContext();
}

GLuint program;
GLuint vbo;
GLint pos_location;


// Vertex and Fragment shader sources
const char *vertex_shader_source =
    "#version 100\n"
    "attribute vec3 vPosition;\n"
    "attribute vec3 vColor;\n"
    "varying vec3 fColor;\n"
    "void main() {\n"
    "    gl_Position = vec4(vPosition, 1.0);\n"
    "    fColor = vColor;\n"
    "}\n";

const char *fragment_shader_source =
    "#version 100\n"
    "precision mediump float;\n"
    "varying vec3 fColor;\n"
    "void main() {\n"
    "    gl_FragColor = vec4(fColor, 1.0);\n"
    "}\n";

GLuint create_shader(const char *source, GLenum shader_type) {
    GLuint shader = glCreateShader(shader_type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    GLint compiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        GLint info_len = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_len);
        if (info_len > 1) {
            char *info_log = malloc(info_len);
            glGetShaderInfoLog(shader, info_len, NULL, info_log);
            fprintf(stderr, "Error compiling shader:\n%s\n", info_log);
            free(info_log);
        }
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

void setup_graphics() {
    GLuint vertex_shader = create_shader(vertex_shader_source, GL_VERTEX_SHADER);
    if (!vertex_shader) {
        fprintf(stderr, "Error creating vertex shader\n");
        exit(1);
    }

    GLuint fragment_shader = create_shader(fragment_shader_source, GL_FRAGMENT_SHADER);
    if (!fragment_shader) {
        fprintf(stderr, "Error creating fragment shader\n");
        exit(1);
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);

    GLint linked;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        GLint info_len = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &info_len);
        if (info_len > 1) {
            char *info_log = malloc(info_len);
            glGetProgramInfoLog(program, info_len, NULL, info_log);
            fprintf(stderr, "Error linking program:\n%s\n", info_log);
            free(info_log);
        }
        glDeleteProgram(program);
        exit(1);
    }

    glUseProgram(program);

    // Set up vertex data and buffers and configure vertex attributes
    GLfloat vertices[] = {
        // Positions       // Colors
         0.0f,  0.5f, 0.0f,  1.0f, 0.0f, 0.0f,  // Top vertex (red)
        -0.5f, -0.5f, 0.0f,  0.0f, 1.0f, 0.0f,  // Bottom-left vertex (green)
         0.5f, -0.5f, 0.0f,  0.0f, 0.0f, 1.0f   // Bottom-right vertex (blue)
    };

    GLuint indices[] = {
        0, 1, 2  // First triangle
    };

    unsigned int VBO, VAO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);
    // Color attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    glBindVertexArray(VAO);
}

void draw() {
    glClearColor(0.0, 0.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // glDrawArrays(GL_TRIANGLES, 0, 3);
    glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, 0);
}

void RefreshWindow() {
    eglSwapBuffers(ESContext.display, ESContext.surface);
}

static void global_registry_handler(void *data __attribute__((unused)), struct wl_registry *registry, uint32_t id,
                                    const char *interface, uint32_t version __attribute__((unused))) {
    LOG("Got a registry event for %s id %d\n", interface, id);
    if (strcmp(interface, "wl_compositor") == 0)
        compositor = wl_registry_bind(registry, id, &wl_compositor_interface, 1);
    else if(strcmp(interface, xdg_wm_base_interface.name) == 0) {
        XDGWMBase = wl_registry_bind(registry, id, &xdg_wm_base_interface, 1);
        xdg_wm_base_add_listener(XDGWMBase, &xdg_wm_base_listener, NULL);
    } else if (strcmp(interface, "wl_seat") == 0) {
        seat = wl_registry_bind(registry, id, &wl_seat_interface, 1);
        wl_seat_add_listener(seat, &seat_listener, NULL);
    }
}

static void global_registry_remover(void *data __attribute__((unused)), struct wl_registry *registry __attribute__((unused)), uint32_t id __attribute__((unused))) {
    LOG("Got a registry losing event for %d\n", id);
}

const struct wl_registry_listener listener = {
    global_registry_handler,
    global_registry_remover
};

static void get_server_references() {
    struct wl_display *display = wl_display_connect(NULL);
    if (display == NULL) {
        LOG("Can't connect to wayland display !?\n");
        exit(1);
    }
    LOG("Got a display !\n");

    struct wl_registry *wl_registry = wl_display_get_registry(display);
    wl_registry_add_listener(wl_registry, &listener, NULL);

    wl_display_dispatch(display);
    wl_display_roundtrip(display);

    if (compositor == NULL || XDGWMBase == NULL || seat == NULL) {
        LOG("No compositor !? No XDG !! There's NOTHING in here !\n");
        exit(1);
    } else {
        LOG("Okay, we got a compositor, XDG and a seat... That's something !\n");
        ESContext.native_display = display;
    }
}

void destroy_window() {
    eglDestroySurface(ESContext.display, ESContext.surface);
    wl_egl_window_destroy(ESContext.native_window);
    xdg_toplevel_destroy(XDGToplevel);
    xdg_surface_destroy(XDGSurface);
    wl_surface_destroy(surface);
    eglDestroyContext(ESContext.display, ESContext.context);
}

int main() {
    get_server_references();

    surface = wl_compositor_create_surface(compositor);
    if (surface == NULL) {
        LOG("No Compositor surface ! Yay....\n");
        exit(1);
    } else {
        LOG("Got a compositor surface !\n");
    }

    XDGSurface = xdg_wm_base_get_xdg_surface(XDGWMBase, surface);
    xdg_surface_add_listener(XDGSurface, &xdg_surface_listener, NULL);

    XDGToplevel = xdg_surface_get_toplevel(XDGSurface);
    xdg_toplevel_set_title(XDGToplevel, "Wayland EGL example");
    xdg_toplevel_add_listener(XDGToplevel, &xdg_toplevel_listener, NULL);

    wl_surface_commit(surface);

    if (!CreateWindowWithEGLContext("Nya", WINDOW_WIDTH, WINDOW_HEIGHT)) {
        LOG("Failed to create window with EGL context\n");
        exit(1);
    }

    setup_graphics();
    glEnable(GL_DEPTH_TEST);
    program_alive = true;

    while (program_alive) {
        while (wl_display_prepare_read(ESContext.native_display) != 0) {
            wl_display_dispatch_pending(ESContext.native_display);
        }
        wl_display_flush(ESContext.native_display);
        wl_display_read_events(ESContext.native_display);
        wl_display_dispatch_pending(ESContext.native_display);

        draw();
        RefreshWindow();
    }

    destroy_window();
    wl_display_disconnect(ESContext.native_display);
    LOG("Display disconnected !\n");

    return 0;
}
