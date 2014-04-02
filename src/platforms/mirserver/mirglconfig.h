#ifndef QPA_MIR_GL_CONFIG_H
#define QPA_MIR_GL_CONFIG_H

#include <mir/graphics/gl_config.h>

class MirGLConfig : public mir::graphics::GLConfig
{
public:
    int depth_buffer_bits() const override { return 24; }
    int stencil_buffer_bits() const override { return 8; }
};

#endif // QPA_MIR_GL_CONFIG_H
