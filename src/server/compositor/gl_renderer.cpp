/* Copyright © 2013 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored By: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#include "mir/compositor/gl_renderer.h"
#include "mir/compositor/buffer_stream.h"
#include "mir/graphics/renderable.h"
#include "mir/graphics/buffer.h"

#define GLM_FORCE_RADIANS
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <boost/throw_exception.hpp>
#include <stdexcept>
#include <cmath>
#include <mutex>

namespace mg = mir::graphics;
namespace mc = mir::compositor;
namespace geom = mir::geometry;

namespace
{

const GLchar* vertex_shader_src =
{
    "attribute vec3 position;\n"
    "attribute vec2 texcoord;\n"
    "uniform mat4 screen_to_gl_coords;\n"
    "uniform mat4 display_transform;\n"
    "uniform mat4 transform;\n"
    "uniform vec2 centre;\n"
    "varying vec2 v_texcoord;\n"
    "void main() {\n"
    "   vec4 mid = vec4(centre, 0.0, 0.0);\n"
    "   vec4 transformed = (transform * (vec4(position, 1.0) - mid)) + mid;\n"
    "   gl_Position = display_transform * screen_to_gl_coords * transformed;\n"
    "   v_texcoord = texcoord;\n"
    "}\n"
};

const GLchar* fragment_shader_src =
{
    "precision mediump float;\n"
    "uniform sampler2D tex;\n"
    "uniform float alpha;\n"
    "varying vec2 v_texcoord;\n"
    "void main() {\n"
    "   vec4 frag = texture2D(tex, v_texcoord);\n"
    "   gl_FragColor = vec4(frag.xyz, frag.a * alpha);\n"
    "}\n"
};

typedef void(*MirGLGetObjectInfoLog)(GLuint, GLsizei, GLsizei *, GLchar *);
typedef void(*MirGLGetObjectiv)(GLuint, GLenum, GLint *);

void GetObjectLogAndThrow(MirGLGetObjectInfoLog getObjectInfoLog,
                          MirGLGetObjectiv      getObjectiv,
                          std::string const &   msg,
                          GLuint                object)
{
    GLint object_log_length = 0;
    (*getObjectiv)(object, GL_INFO_LOG_LENGTH, &object_log_length);

    const GLuint object_log_buffer_length = object_log_length + 1;
    std::string  object_info_log;

    object_info_log.resize(object_log_buffer_length);
    (*getObjectInfoLog)(object, object_log_length, NULL,
                        const_cast<GLchar *>(object_info_log.data()));

    std::string object_info_err(msg + "\n");
    object_info_err += object_info_log;

    BOOST_THROW_EXCEPTION(std::runtime_error(object_info_err));
}

}

mc::GLRenderer::GLRenderer(geom::Rectangle const& display_area) :
    vertex_shader(0),
    fragment_shader(0),
    program(0),
    position_attr_loc(0),
    texcoord_attr_loc(0),
    centre_uniform_loc(0),
    transform_uniform_loc(0),
    alpha_uniform_loc(0),
    rotation(NAN) // ensure the first set_rotation succeeds
{
    /*
     * We need to serialize renderer creation because some GL calls used
     * during renderer construction that create unique resource ids
     * (e.g. glCreateProgram) are not thread-safe when the threads are
     * have the same or shared EGL contexts.
     */
    static std::mutex mutex;
    std::lock_guard<std::mutex> lock(mutex);

    GLint param = 0;

    /* Create shaders and program */
    vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &vertex_shader_src, 0);
    glCompileShader(vertex_shader);
    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &param);
    if (param == GL_FALSE)
    {
        GetObjectLogAndThrow(glGetShaderInfoLog,
            glGetShaderiv,
            "Failed to compile vertex shader:",
            vertex_shader);
    }

    fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &fragment_shader_src, 0);
    glCompileShader(fragment_shader);
    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &param);
    if (param == GL_FALSE)
    {
        GetObjectLogAndThrow(glGetShaderInfoLog,
            glGetShaderiv,
            "Failed to compile fragment shader:",
            fragment_shader);
    }

    program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &param);
    if (param == GL_FALSE)
    {
        GetObjectLogAndThrow(glGetProgramInfoLog,
            glGetProgramiv,
            "Failed to link program:",
            program);
    }

    glUseProgram(program);

    /* Set up program variables */
    GLint tex_loc = glGetUniformLocation(program, "tex");
    display_transform_uniform_loc = glGetUniformLocation(program, "display_transform");
    transform_uniform_loc = glGetUniformLocation(program, "transform");
    alpha_uniform_loc = glGetUniformLocation(program, "alpha");
    position_attr_loc = glGetAttribLocation(program, "position");
    texcoord_attr_loc = glGetAttribLocation(program, "texcoord");
    centre_uniform_loc = glGetUniformLocation(program, "centre");

    glUniform1i(tex_loc, 0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUseProgram(0);

    set_viewport(display_area);
    set_rotation(0.0f);
}

mc::GLRenderer::~GLRenderer() noexcept
{
    if (vertex_shader)
        glDeleteShader(vertex_shader);
    if (fragment_shader)
        glDeleteShader(fragment_shader);
    if (program)
        glDeleteProgram(program);
    for (auto& t : textures)
        glDeleteTextures(1, &t.second.id);
}

void mc::GLRenderer::tessellate(std::vector<Primitive>& primitives,
                                graphics::Renderable const& renderable,
                                geometry::Size const& buf_size) const
{
    auto const& rect = renderable.screen_position();
    GLfloat left = rect.top_left.x.as_int();
    GLfloat right = left + rect.size.width.as_int();
    GLfloat top = rect.top_left.y.as_int();
    GLfloat bottom = top + rect.size.height.as_int();

    primitives.resize(1);
    auto& client = primitives[0];
    client.tex_id = 0;
    client.type = GL_TRIANGLE_STRIP;

    GLfloat tex_right = static_cast<GLfloat>(rect.size.width.as_int()) /
                        buf_size.width.as_int();
    GLfloat tex_bottom = static_cast<GLfloat>(rect.size.height.as_int()) /
                         buf_size.height.as_int();

    auto& vertices = client.vertices;
    vertices.resize(4);
    vertices[0] = {{left,  top,    0.0f}, {0.0f,      0.0f}};
    vertices[1] = {{left,  bottom, 0.0f}, {0.0f,      tex_bottom}};
    vertices[2] = {{right, top,    0.0f}, {tex_right, 0.0f}};
    vertices[3] = {{right, bottom, 0.0f}, {tex_right, tex_bottom}};
}

void mc::GLRenderer::render(mg::Renderable const& renderable, mg::Buffer& buffer) const
{
    glUseProgram(program);

    if (renderable.shaped() || renderable.alpha() < 1.0f)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    else
    {
        glDisable(GL_BLEND);
    }
    glActiveTexture(GL_TEXTURE0);

    auto const& rect = renderable.screen_position();
    GLfloat centrex = rect.top_left.x.as_int() +
                      rect.size.width.as_int() / 2.0f;
    GLfloat centrey = rect.top_left.y.as_int() +
                      rect.size.height.as_int() / 2.0f;
    glUniform2f(centre_uniform_loc, centrex, centrey);

    glUniformMatrix4fv(transform_uniform_loc, 1, GL_FALSE,
                       glm::value_ptr(renderable.transformation()));
    glUniform1f(alpha_uniform_loc, renderable.alpha());

    GLuint surface_tex = load_texture(renderable, buffer);

    /* Draw */
    glEnableVertexAttribArray(position_attr_loc);
    glEnableVertexAttribArray(texcoord_attr_loc);

    std::vector<Primitive> primitives;
    tessellate(primitives, renderable, buffer.size());
   
    for (auto const& p : primitives)
    {
        // Note a primitive tex_id of zero means use the surface texture,
        // which is what you normally want. Other textures could be used
        // in decorations etc.

        glBindTexture(GL_TEXTURE_2D, p.tex_id ? p.tex_id : surface_tex);

        glVertexAttribPointer(position_attr_loc, 3, GL_FLOAT,
                              GL_FALSE, sizeof(Vertex),
                              &p.vertices[0].position);
        glVertexAttribPointer(texcoord_attr_loc, 2, GL_FLOAT,
                              GL_FALSE, sizeof(Vertex),
                              &p.vertices[0].texcoord);

        glDrawArrays(p.type, 0, p.vertices.size());
    }

    glDisableVertexAttribArray(texcoord_attr_loc);
    glDisableVertexAttribArray(position_attr_loc);
}

GLuint mc::GLRenderer::load_texture(mg::Renderable const& renderable,
                                    mg::Buffer& buffer) const
{
    auto& tex = textures[renderable.id()];
    bool changed = true;
    auto const& buf_id = buffer.id();
    if (!tex.id)
    {
        glGenTextures(1, &tex.id);
        glBindTexture(GL_TEXTURE_2D, tex.id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    else
    {
        glBindTexture(GL_TEXTURE_2D, tex.id);
        changed = (tex.origin != buf_id) || skipped;
    }
    tex.origin = buf_id;
    tex.used = true;
    if (changed)  // Don't upload a new texture unless the surface has changed
        buffer.bind_to_texture();

    return tex.id;
}

void mc::GLRenderer::set_viewport(geometry::Rectangle const& rect)
{
    if (rect == viewport)
        return;

    /*
     * Create and set screen_to_gl_coords transformation matrix.
     * The screen_to_gl_coords matrix transforms from the screen coordinate system
     * (top-left is (0,0), bottom-right is (W,H)) to the normalized GL coordinate system
     * (top-left is (-1,1), bottom-right is (1,-1))
     */
    glm::mat4 screen_to_gl_coords = glm::translate(glm::mat4(1.0f), glm::vec3{-1.0f, 1.0f, 0.0f});

    /*
     * Perspective division is one thing that can't be done in a matrix
     * multiplication. It happens after the matrix multiplications. GL just
     * scales {x,y} by 1/w. So modify the final part of the projection matrix
     * to set w ([3]) to be the incoming z coordinate ([2]).
     */
    screen_to_gl_coords[2][3] = -1.0f;

    float const vertical_fov_degrees = 30.0f;
    float const near =
        (rect.size.height.as_float() / 2.0f) /
        std::tan((vertical_fov_degrees * M_PI / 180.0f) / 2.0f);
    float const far = -near;

    screen_to_gl_coords = glm::scale(screen_to_gl_coords,
            glm::vec3{2.0f / rect.size.width.as_float(),
                      -2.0f / rect.size.height.as_float(),
                      2.0f / (near - far)});
    screen_to_gl_coords = glm::translate(screen_to_gl_coords,
            glm::vec3{-rect.top_left.x.as_float(),
                      -rect.top_left.y.as_float(),
                      0.0f});

    glUseProgram(program);
    GLint mat_loc = glGetUniformLocation(program, "screen_to_gl_coords");
    glUniformMatrix4fv(mat_loc, 1, GL_FALSE, glm::value_ptr(screen_to_gl_coords));
    glUseProgram(0);

    viewport = rect;
}

void mc::GLRenderer::set_rotation(float degrees)
{
    if (degrees == rotation)
        return;

    float rad = degrees * M_PI / 180.0f;
    GLfloat cos = cosf(rad);
    GLfloat sin = sinf(rad);
    GLfloat rot[16] = {cos,  sin,  0.0f, 0.0f,
                       -sin, cos,  0.0f, 0.0f,
                       0.0f, 0.0f, 1.0f, 0.0f,
                       0.0f, 0.0f, 0.0f, 1.0f};
    glUseProgram(program);
    glUniformMatrix4fv(display_transform_uniform_loc, 1, GL_FALSE, rot);
    glUseProgram(0);

    rotation = degrees;
}

void mc::GLRenderer::begin() const
{
    glClear(GL_COLOR_BUFFER_BIT);
}

void mc::GLRenderer::end() const
{
    auto t = textures.begin();
    while (t != textures.end())
    {
        auto& tex = t->second;
        if (tex.used)
        {
            tex.used = false;
            ++t;
        }
        else
        {
            glDeleteTextures(1, &tex.id);
            t = textures.erase(t);
        }
    }
    skipped = false;
}

void mc::GLRenderer::suspend()
{
    skipped = true;
}
