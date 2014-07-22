/*
 * Copyright (C) 2014 Canonical, Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License version 3, as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranties of MERCHANTABILITY,
 * SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

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
