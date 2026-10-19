#include <assert.h>
#include <string.h>
#include <math.h>
#include "renderer.h"
#include "atlas.inl"

#define RGL_LOAD_IMPLEMENTATION
#include "rglLoad.h" 

#define BUFFER_SIZE 16384

typedef struct {
    float x, y;
    float u, v;
    unsigned char r, g, b, a;
} Vertex;

static Vertex vertices[BUFFER_SIZE * 4];
static unsigned int indices[BUFFER_SIZE * 6];

static int width  = 800;
static int height = 600;
static float pixelRatio = 1.0f;
static int buf_idx = 0;

static GLuint vao, vbo, ebo;
static GLuint shader_program;
static GLuint atlas_texture_id;
static GLint u_projection_loc;
static GLint u_texture_loc;
static float projection[16];

static void create_ortho_matrix(float* mat, float left, float right, float bottom, float top) {
    memset(mat, 0, 16 * sizeof(float));
    mat[0] = 2.0f / (right - left);
    mat[5] = 2.0f / (top - bottom);
    mat[10] = -1.0f;
    mat[12] = -(right + left) / (right - left);
    mat[13] = -(top + bottom) / (top - bottom);
    mat[15] = 1.0f;
}

static GLuint compile_shader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        printf("Shader Compilation Failed:\n%s\n", infoLog);
        assert(0);
    }
    return shader;
}

static const char* vertex_shader_src = 
    "#version 330 core\n"
    "layout (location = 0) in vec2 aPos;\n"
    "layout (location = 1) in vec2 aTexCoord;\n"
    "layout (location = 2) in vec4 aColor;\n"
    "out vec2 TexCoord;\n"
    "out vec4 Color;\n"
    "uniform mat4 uProjection;\n"
    "void main() {\n"
    "    gl_Position = uProjection * vec4(aPos, 0.0, 1.0);\n"
    "    TexCoord = aTexCoord;\n"
    "    Color = aColor;\n"
    "}\n";

static const char* fragment_shader_src = 
    "#version 330 core\n"
    "in vec2 TexCoord;\n"
    "in vec4 Color;\n"
    "out vec4 FragColor;\n"
    "uniform sampler2D uTexture;\n"
    "void main() {\n"
    "    vec4 texColor = texture(uTexture, TexCoord);\n"
    "    FragColor = texColor * Color;\n"
    "}\n";

void r_init(void) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST);

    GLuint vs = compile_shader(GL_VERTEX_SHADER, vertex_shader_src);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fragment_shader_src);
    shader_program = glCreateProgram();
    glAttachShader(shader_program, vs);
    glAttachShader(shader_program, fs);
    glLinkProgram(shader_program);
    glDeleteShader(vs);
    glDeleteShader(fs);

    u_projection_loc = glGetUniformLocation(shader_program, "uProjection");
    u_texture_loc = glGetUniformLocation(shader_program, "uTexture");

    glGenTextures(1, &atlas_texture_id);
    glBindTexture(GL_TEXTURE_2D, atlas_texture_id);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, ATLAS_WIDTH, ATLAS_HEIGHT, 0,
                 GL_RED, GL_UNSIGNED_BYTE, atlas_texture);
                 
    GLint swizzleMask[] = { GL_RED, GL_RED, GL_RED, GL_RED };
    glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);
    
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), NULL, GL_DYNAMIC_DRAW);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), NULL, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, x));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, u));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void*)offsetof(Vertex, r));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);

    create_ortho_matrix(projection, 0.0f, (float)width, (float)height, 0.0f);
}

static void flush(void) {
    if (buf_idx == 0) return;

    glUseProgram(shader_program);
    glUniformMatrix4fv(u_projection_loc, 1, GL_FALSE, projection);
    glUniform1i(u_texture_loc, 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, atlas_texture_id);

    glBindVertexArray(vao);
    
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, buf_idx * 4 * sizeof(Vertex), vertices);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, buf_idx * 6 * sizeof(unsigned int), indices);

    glDrawElements(GL_TRIANGLES, buf_idx * 6, GL_UNSIGNED_INT, 0);

    glBindVertexArray(0);
    buf_idx = 0;
}

static void push_quad(mu_Rect dst, mu_Rect src, mu_Color color) {
    if (buf_idx == BUFFER_SIZE) flush();

    int v_idx = buf_idx * 4;
    float u1 = (src.x + 0.01f) / (float)ATLAS_WIDTH;
    float v1 = (src.y + 0.01f) / (float)ATLAS_HEIGHT;
    float u2 = (src.x + src.w - 0.01f) / (float)ATLAS_WIDTH;
    float v2 = (src.y + src.h - 0.01f) / (float)ATLAS_HEIGHT;

    vertices[v_idx + 0] = (Vertex){ dst.x,         dst.y,         u1, v1, color.r, color.g, color.b, color.a };
    vertices[v_idx + 1] = (Vertex){ dst.x + dst.w, dst.y,         u2, v1, color.r, color.g, color.b, color.a };
    vertices[v_idx + 2] = (Vertex){ dst.x,         dst.y + dst.h, u1, v2, color.r, color.g, color.b, color.a };
    vertices[v_idx + 3] = (Vertex){ dst.x + dst.w, dst.y + dst.h, u2, v2, color.r, color.g, color.b, color.a };

    int i_idx = buf_idx * 6;
    indices[i_idx + 0] = v_idx + 0;
    indices[i_idx + 1] = v_idx + 1;
    indices[i_idx + 2] = v_idx + 2;
    indices[i_idx + 3] = v_idx + 2;
    indices[i_idx + 4] = v_idx + 3;
    indices[i_idx + 5] = v_idx + 1;

    buf_idx++;
}

void r_draw_rect(mu_Rect rect, mu_Color color) {
    push_quad(rect, atlas[ATLAS_WHITE], color);
}

void r_draw_text(const char *text, mu_Vec2 pos, mu_Color color) {
    mu_Rect dst = { pos.x, pos.y, 0, 0 };
    for (const char *p = text; *p; p++) {
        if ((*p & 0xc0) == 0x80) continue;
        int chr = mu_min((unsigned char)*p, 127);
        mu_Rect src = atlas[ATLAS_FONT + chr];
        dst.w = src.w;
        dst.h = src.h;
        push_quad(dst, src, color);
        dst.x += dst.w;
    }
}

void r_draw_icon(int id, mu_Rect rect, mu_Color color) {
    mu_Rect src = atlas[id];
    int x = rect.x + (rect.w - src.w) / 2;
    int y = rect.y + (rect.h - src.h) / 2;
    push_quad(mu_rect(x, y, src.w, src.h), src, color);
}

int r_get_text_width(const char *text, int len) {
    int res = 0;
    for (const char *p = text; *p && len--; p++) {
        if ((*p & 0xc0) == 0x80) continue;
        int chr = mu_min((unsigned char)*p, 127);
        res += atlas[ATLAS_FONT + chr].w;
    }
    return res;
}

int r_get_text_height(void) {
    return 18;
}

void r_set_clip_rect(mu_Rect rect) {
    flush();
    GLsizei sx = (GLsizei)((float)rect.x * pixelRatio);
    GLsizei sy = (GLsizei)((float)(height - (rect.y + rect.h)) * pixelRatio);
    GLsizei sw = (GLsizei)((float)rect.w * pixelRatio);
    GLsizei sh = (GLsizei)((float)rect.h * pixelRatio);
    glScissor(sx, sy, sw, sh);
}

void r_clear(mu_Color clr) {
    flush();
    glViewport(0, 0, (int)((float)width * pixelRatio), (int)((float)height * pixelRatio));
    glClearColor(clr.r / 255.0f, clr.g / 255.0f, clr.b / 255.0f, clr.a / 255.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void r_present(void) {
    flush();
}