/*
 * Copyright © 2013 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Alan Griffiths <alan@octopull.co.uk>
 */


#ifndef MIR_GRAPHICS_MESA_CURSOR_H_
#define MIR_GRAPHICS_MESA_CURSOR_H_

#include "mir/graphics/cursor.h"
#include "mir_toolkit/common.h"

#include <gbm.h>
#include <memory>

namespace mir
{
namespace geometry
{
struct Rectangle;
}
namespace graphics
{
class CursorImage;

namespace mesa
{
class KMSOutputContainer;
class KMSOutput;
class KMSDisplayConfiguration;
class GBMPlatform;

class CurrentConfiguration
{
public:
    virtual ~CurrentConfiguration() = default;

    virtual void with_current_configuration_do(
        std::function<void(KMSDisplayConfiguration const&)> const& exec) = 0;

protected:
    CurrentConfiguration() = default;
    CurrentConfiguration(CurrentConfiguration const&) = delete;
    CurrentConfiguration& operator=(CurrentConfiguration const&) = delete;
};

class Cursor : public graphics::Cursor
{
public:
    Cursor(
        gbm_device* device,
        KMSOutputContainer& output_container,
        std::shared_ptr<CurrentConfiguration> const& current_configuration,
        std::shared_ptr<CursorImage> const& cursor_image);

    ~Cursor() noexcept;

    void show(CursorImage const& cursor_image) override;
    void hide() override;

    void move_to(geometry::Point position);

    void show_at_last_known_position();

private:
    enum ForceCursorState { UpdateState, ForceState };
    void for_each_used_output(std::function<void(KMSOutput&, geometry::Rectangle const&, MirOrientation orientation)> const& f);
    void place_cursor_at(geometry::Point position, ForceCursorState force_state);

    KMSOutputContainer& output_container;
    geometry::Point current_position;

    struct GBMBOWrapper
    {
        GBMBOWrapper(gbm_device* gbm);
        operator gbm_bo*();
        ~GBMBOWrapper();
    private:
        gbm_bo* buffer;
        GBMBOWrapper(GBMBOWrapper const&) = delete;
        GBMBOWrapper& operator=(GBMBOWrapper const&) = delete;
    } buffer;

    std::shared_ptr<CurrentConfiguration> const current_configuration;
};
}
}
}


#endif /* MIR_GRAPHICS_MESA_CURSOR_H_ */
