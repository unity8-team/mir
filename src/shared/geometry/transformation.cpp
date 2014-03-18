/*
 * Copyright © 2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Andreas Pokorny <andreas.pokorny@canonical.com>
 */

#include "mir/geometry/transformation.h"

#define GLM_FORCE_RADIANS
#include "glm/gtx/euler_angles.hpp"
#include "glm/gtx/transform.hpp"

#include <cmath>

namespace geom = mir::geometry;

//TODO
// * Consider supporting separate scale values for x and y co-ordinates.
// * Consider different forms of orientation / rotation descriptions.
// * clamp scales close to 1.0f and angles close to 0.0f when they cannot be noticed
//   on screen: i.e. sin(angle)*height/2 <1.0f or cos(angle)*width/2 < 1.0f

geom::Transformation::Transformation(Rectangle rectangle)
    : scale(1.0f),
    translation(rectangle.top_left - geom::Point()),
    size(rectangle.size),
    x_angle(0.0f),
    y_angle(0.0f),
    z_angle(0.0f),
    dirty(has_translation())
{
}
geom::Transformation::Transformation(Size rectangle_size)
    : Transformation(Rectangle{Point{0,0}, rectangle_size})
{
}

geom::Transformation::Transformation()
    : Transformation(Size(0,0) )
{}

bool geom::Transformation::has_scale() const
{
    return scale != 1.0f;
}

bool geom::Transformation::has_translation() const
{
    return translation != Displacement{};
}

bool geom::Transformation::has_rotation() const
{
    return !(x_angle == 0.0f && y_angle == 0.0f && z_angle == 0.0f);
}

bool geom::Transformation::is_identity() const
{
    return !has_translation() && !has_scale() && !has_rotation();
}

bool geom::Transformation::is_scaling() const
{
    return !has_translation() && has_scale() && !has_rotation();
}

bool geom::Transformation::is_translation() const
{
    return has_translation() && !has_scale() && !has_rotation();
}

bool geom::Transformation::is_scaling_translation() const
{
    return has_translation() && has_scale() && !has_rotation();
}

bool geom::Transformation::requires_matrix_transformation() const
{
    return has_rotation();
}

void geom::Transformation::update_size(Size size)
{
    if (this->size != size)
    {
        this->size = size;
        dirty = true;
    }
}

void geom::Transformation::set_scale(float scale)
{
    if (this->scale != scale)
    {
        dirty = true;
        this->scale = scale;
    }
}

void geom::Transformation::set_translation(Displacement const& movement)
{
    if (translation != movement)
    {
        dirty = true;
        translation = movement;
    }
}

glm::mat4 geom::Transformation::get_matrix() const
{
    update_matrices();
    return matrix;
}

glm::mat4 geom::Transformation::get_center_matrix() const
{
    update_matrices();
    return center_matrix;
}

void geom::Transformation::set_rotation(float y, float x, float z)
{
    if (y != y_angle ||
        x != x_angle ||
        z != z_angle)
    {
        dirty = true;
        y_angle = y;
        x_angle = x;
        z_angle = z;
    }
}

void geom::Transformation::set_rotation(float z)
{
    if (0.0f != y_angle ||
        0.0f != x_angle ||
        z != z_angle)
    {
        dirty = true;
        y_angle = 0.0f;
        x_angle = 0.0f;
        z_angle = z;
    }
}

void geom::Transformation::update_matrices() const
{
    if (!dirty)
        return;

    dirty = false;

    if (has_rotation())
    {
        glm::vec3 center{size.width.as_float()/2.0f, size.height.as_float()/2.0f, 0.0};

        if (x_angle == 0.0f && y_angle == 0.0f)
        {
            center_matrix = glm::eulerAngleZ(glm::radians(z_angle));
        }
        else
        {
            center_matrix = glm::eulerAngleYXZ(
                glm::radians(y_angle),
                glm::radians(x_angle),
                glm::radians(z_angle));
        }

        matrix = glm::translate(center) * center_matrix * glm::translate(-center);
        inverse_matrix = glm::translate(center) * glm::transpose(center_matrix) * glm::translate(-center);
    }
    else
    {
        center_matrix = glm::mat4{};
        matrix = glm::mat4{};
        inverse_matrix = glm::mat4{};
    }
    if (has_scale())
    {
        matrix = glm::scale(glm::vec3(scale)) * matrix;
        center_matrix = glm::scale(glm::vec3(scale)) * center_matrix;
        inverse_matrix *=  glm::scale(glm::vec3(1.0f/scale));
    }

    if (has_translation())
    {
        inverse_matrix *= glm::translate(glm::vec3{-translation.dx.as_float(), -translation.dy.as_float(), 0.0});

        matrix = glm::translate(glm::vec3{translation.dx.as_float(), translation.dy.as_float(), 0.0}) * matrix;
    }
}

glm::mat4 geom::Transformation::get_inverse_matrix() const
{
    update_matrices();

    return inverse_matrix;
}

geom::Point geom::Transformation::transform_to_local(geom::Point const& point) const
{
    if (is_identity())
        return point;
    if (is_translation())
        return point - translation;
    if (is_scaling())
        return point / scale;
    if (is_scaling_translation())
        return (point - translation)/ scale;

    glm::mat4 inv( get_inverse_matrix() );
    glm::vec4 pos = inv * glm::vec4{point.x.as_float(), point.y.as_float(), 0.0f, 1.0f};
    return geom::Point{ std::round(pos.x), std::round(pos.y) };
}

geom::Point geom::Transformation::transform_to_screen(geom::Point const& point) const
{
    if (is_identity())
        return point;
    if (is_translation())
        return point + translation;
    if (is_scaling())
        return point * scale;
    if (is_scaling_translation())
        return point * scale + translation;

    glm::mat4 trans( get_matrix() );
    glm::vec4 pos = trans * glm::vec4{point.x.as_float(), point.y.as_float(), 0.0f, 1.0f};
    return geom::Point{ std::round(pos.x), std::round(pos.y) };
}
