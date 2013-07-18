#include "mir/graphics/gl_renderer.h"
#include "mir/graphics/compositing_criteria.h"
#include "mir/surfaces/buffer_stream.h"
#include "mir/graphics/buffer.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <boost/throw_exception.hpp>
#include <stdexcept>

namespace mg=mir::graphics;
namespace geom=mir::geometry;

namespace
{

const GLchar* vertex_shader_src =
{
    "attribute vec3 position;\n"
    "attribute vec2 texcoord;\n"
    "uniform mat4 screen_to_gl_coords;\n"
    "uniform mat4 transform;\n"
    "varying vec2 v_texcoord;\n"
    "void main() {\n"
    "   gl_Position = screen_to_gl_coords * transform * vec4(position, 1.0);\n"
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

struct VertexAttributes
{
    glm::vec3 position;
    glm::vec2 texcoord;
};

/*
 * The texture coordinates are y-inverted to account for the difference in the
 * texture and renderable pixel data row order. In particular, GL textures
 * expect pixel data in rows starting from the bottom and moving up the image,
 * whereas our renderables provide data in rows starting from the top and
 * moving down the image.
 */
VertexAttributes vertex_attribs[4] =
{
    {
        glm::vec3{-0.5f, -0.5f, 0.0f},
        glm::vec2{0.0f, 0.0f}
    },
    {
        glm::vec3{-0.5f, 0.5f, 0.0f},
        glm::vec2{0.0f, 1.0f},
    },
    {
        glm::vec3{0.5f, -0.5f, 0.0f},
        glm::vec2{1.0f, 0.0f},
    },
    {
        glm::vec3{0.5f, 0.5f, 0.0f},
        glm::vec2{1.0f, 1.0f}
    }
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
    (*getObjectInfoLog)(object, object_log_length, NULL, const_cast<GLchar *>(object_info_log.data()));

    std::string object_info_err(msg + "\n");
    object_info_err += object_info_log;

    BOOST_THROW_EXCEPTION(std::runtime_error(object_info_err));
}

}

mg::GLRenderer::Resources::Resources() :
    vertex_shader(0),
    fragment_shader(0),
    program(0),
    position_attr_loc(0),
    texcoord_attr_loc(0),
    transform_uniform_loc(0),
    alpha_uniform_loc(0),
    vertex_attribs_vbo(0),
    texture(0)
{
}

mg::GLRenderer::Resources::~Resources()
{
    if (vertex_shader)
        glDeleteShader(vertex_shader);
    if (fragment_shader)
        glDeleteShader(fragment_shader);
    if (program)
        glDeleteProgram(program);
    if (vertex_attribs_vbo)
        glDeleteBuffers(1, &vertex_attribs_vbo);
    if (texture)
        glDeleteTextures(1, &texture);
}

void mg::GLRenderer::Resources::setup(const geometry::Size& display_size)
{
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
    GLint mat_loc = glGetUniformLocation(program, "screen_to_gl_coords");
    GLint tex_loc = glGetUniformLocation(program, "tex");
    transform_uniform_loc = glGetUniformLocation(program, "transform");
    alpha_uniform_loc = glGetUniformLocation(program, "alpha");
    position_attr_loc = glGetAttribLocation(program, "position");
    texcoord_attr_loc = glGetAttribLocation(program, "texcoord");

    /*
     * Create and set screen_to_gl_coords transformation matrix.
     * The screen_to_gl_coords matrix transforms from the screen coordinate system
     * (top-left is (0,0), bottom-right is (W,H)) to the normalized GL coordinate system
     * (top-left is (-1,1), bottom-right is (1,-1))
     */
    glm::mat4 screen_to_gl_coords = glm::translate(glm::mat4{1.0f}, glm::vec3{-1.0f, 1.0f, 0.0f});
    screen_to_gl_coords = glm::scale(screen_to_gl_coords,
            glm::vec3{2.0f / display_size.width.as_uint32_t(),
            -2.0f / display_size.height.as_uint32_t(),
            1.0f});
    glUniformMatrix4fv(mat_loc, 1, GL_FALSE, glm::value_ptr(screen_to_gl_coords));

    /* Create the texture (temporary workaround until we can use the Renderable's texture) */
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glUniform1i(tex_loc, 0);

    /* Create VBO */
    glGenBuffers(1, &vertex_attribs_vbo);

    glBindBuffer(GL_ARRAY_BUFFER, vertex_attribs_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_attribs),
            glm::value_ptr(vertex_attribs[0].position), GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUseProgram(0);
}

mg::GLRenderer::GLRenderer(const geom::Size& display_size)
{
    resources.setup(display_size);
}

void mg::GLRenderer::render(std::function<void(std::shared_ptr<void> const&)> save_resource,
                            mg::CompositingCriteria const& criteria, mir::surfaces::BufferStream& stream)
{
    glUseProgram(resources.program);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glActiveTexture(GL_TEXTURE0);

    glUniformMatrix4fv(resources.transform_uniform_loc, 1, GL_FALSE, glm::value_ptr(criteria.transformation()));
    glUniform1f(resources.alpha_uniform_loc, criteria.alpha());

    /* Set up vertex attribute data */
    glBindBuffer(GL_ARRAY_BUFFER, resources.vertex_attribs_vbo);
    glVertexAttribPointer(resources.position_attr_loc, 3, GL_FLOAT, GL_FALSE, sizeof(VertexAttributes), 0);
    glVertexAttribPointer(resources.texcoord_attr_loc, 2, GL_FLOAT, GL_FALSE, sizeof(VertexAttributes),
                          reinterpret_cast<void*>(sizeof(glm::vec3)));

    /* Use the renderable's texture */
    glBindTexture(GL_TEXTURE_2D, resources.texture);

    auto region_resource = stream.lock_back_buffer();
    region_resource->bind_to_texture();
    save_resource(region_resource);

    /* Draw */
    glEnableVertexAttribArray(resources.position_attr_loc);
    glEnableVertexAttribArray(resources.texcoord_attr_loc);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray(resources.texcoord_attr_loc);
    glDisableVertexAttribArray(resources.position_attr_loc);
}

void mg::GLRenderer::clear()
{
    glClear(GL_COLOR_BUFFER_BIT);
}

