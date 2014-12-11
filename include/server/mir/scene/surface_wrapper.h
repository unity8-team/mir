/*
 * Copyright © 2014 Canonical Ltd.
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
 * Authored by: Daniel van Vugt <daniel.van.vugt@canonical.com>
 */

#ifndef MIR_SCENE_SURFACE_WRAPPER_H_
#define MIR_SCENE_SURFACE_WRAPPER_H_

#include "mir/scene/surface.h"
#include <memory>

namespace mir { namespace scene {

class SurfaceWrapper : public scene::Surface
{
public:
    SurfaceWrapper(std::shared_ptr<Surface> const& impl);
    virtual ~SurfaceWrapper();

    virtual std::shared_ptr<mir::input::InputChannel> input_channel() const override;
    virtual mir::input::InputReceptionMode reception_mode() const override;
    virtual std::string name() const override;
    virtual geometry::Point top_left() const override;
    virtual geometry::Size client_size() const override;
    virtual geometry::Size size() const override;
    virtual geometry::Rectangle input_bounds() const override;
    virtual bool input_area_contains(mir::geometry::Point const&) const override;
    virtual std::unique_ptr<graphics::Renderable> compositor_snapshot(void const*) const override;
    virtual float alpha() const override;
    virtual MirSurfaceType type() const override;
    virtual MirSurfaceState state() const override;
    virtual void hide() override;
    virtual void show() override;
    virtual bool visible() const override;
    virtual void move_to(geometry::Point const&) override;
    virtual void take_input_focus(std::shared_ptr<shell::InputTargeter> const&) override;
    virtual void set_input_region(std::vector<geometry::Rectangle> const&) override;
    virtual void allow_framedropping(bool) override;
    virtual void resize(geometry::Size const&) override;
    virtual void set_transformation(glm::mat4 const&) override;
    virtual void set_alpha(float) override;
    virtual void set_orientation(MirOrientation) override;
    virtual void force_requests_to_complete() override;
    virtual void add_observer(std::shared_ptr<SurfaceObserver> const&) override;
    virtual void remove_observer(std::weak_ptr<SurfaceObserver> const&) override;
    virtual void set_reception_mode(input::InputReceptionMode mode) override;
    virtual void consume(MirEvent const&) override;
    virtual void set_cursor_image(std::shared_ptr<graphics::CursorImage> const&) override;
    virtual std::shared_ptr<graphics::CursorImage> cursor_image() const override;
    virtual void request_client_surface_close() override;
    virtual MirPixelFormat pixel_format() const override;
    virtual void swap_buffers(graphics::Buffer*, std::function<void(graphics::Buffer*)>) override;
    virtual bool supports_input() const override;
    virtual int client_input_fd() const override;
    virtual int configure(MirSurfaceAttrib, int) override;
    virtual int query(MirSurfaceAttrib) override;
    virtual void with_most_recent_buffer_do(std::function<void(graphics::Buffer&)> const& ) override;

protected:
    std::shared_ptr<Surface> const raw_surface;
};

}} // namespace mir::scene

#endif // MIR_SCENE_SURFACE_WRAPPER_H_
