/*
 * Copyright © 2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Daniel van Vugt <daniel.van.vugt@canonical.com>
 */

#ifndef MIR_EXAMPLES_DEMO_RENDERER_H_
#define MIR_EXAMPLES_DEMO_RENDERER_H_

#include "mir/compositor/gl_renderer.h"

namespace mir
{
namespace examples
{

class DemoRenderer : public compositor::GLRenderer
{
public:
    DemoRenderer(geometry::Rectangle const& display_area);
    void begin() const override;
};

} // namespace examples
} // namespace mir

#endif // MIR_EXAMPLES_DEMO_RENDERER_H_
