/*
 * Copyright © 2013 Canonical Ltd.
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
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */

#ifndef MIR_SCENE_SURFACE_DATA_STORAGE_H_
#define MIR_SCENE_SURFACE_DATA_STORAGE_H_

#include "mir/compositor/compositing_criteria.h"
#include "mir/input/surface.h"
#include "mutable_surface_state.h"

#include <vector>
#include <functional>
#include <mutex>

namespace mir
{
namespace scene
{

class SurfaceData : public compositor::CompositingCriteria, public input::Surface,
                    public MutableSurfaceState
{
public:
    SurfaceData(std::string const& name, geometry::Rectangle rect,
                std::function<void()> change_cb,
                bool nonrectangular);
    ~SurfaceData();

    //mc::CompositingCriteria
    glm::mat4 const& transformation() const;
    float alpha() const;
    bool should_be_rendered_in(geometry::Rectangle const& rect) const;
    bool shaped() const override;

    //mi::Surface
    std::string const& name() const;
    geometry::Point position() const;
    geometry::Size size() const;
    bool contains(geometry::Point const& point) const;

    //ms::MutableSurfaceState
    void move_to(geometry::Point);
    void resize(geometry::Size const& size);
    void frame_posted();
    void set_hidden(bool hidden);
    void apply_alpha(float alpha);
    void apply_rotation(float degrees, glm::vec3 const&);
    void set_input_region(std::vector<geometry::Rectangle> const& input_rectangles);

    void set_shell_data(void* shell_data, std::function<void()> const& destroy_shell_data);
    void* shell_data() const;

private:
    std::mutex mutable guard;
    std::function<void()> notify_change;
    std::string surface_name;
    geometry::Rectangle surface_rect;
    glm::mat4 rotation_matrix;
    mutable glm::mat4 transformation_matrix;
    mutable geometry::Size transformation_size;
    mutable bool transformation_dirty;
    float surface_alpha;
    bool first_frame_posted;
    bool hidden;
    const bool nonrectangular;
    std::vector<geometry::Rectangle> input_rectangles;
    
    void *shell_data_;
    std::function<void()> destroy_shell_data;
    
    void cleanup_shell_data_locked(std::unique_lock<std::mutex> const&);
};

}
}
#endif /* MIR_SCENE_SURFACE_DATA_STORAGE_H_ */
