
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

#ifndef MIR_GEOMETRY_TRANSFORMATION_H_
#define MIR_GEOMETRY_TRANSFORMATION_H_

#include "mir/geometry/displacement.h"
#include "mir/geometry/size.h"
#include "mir/geometry/rectangle.h"

#include "glm/glm.hpp"

namespace mir
{

namespace geometry
{

/*!
 * \brief Transformation describes transformations applied to a rectangle.
 */
class Transformation
{
public:
    Transformation();
    explicit Transformation(Size rectangle_size);
    explicit Transformation(Rectangle rectangle);

    void update_size(Size rectangle_size);
    Size get_size() const
    {
        return size;
    }
    /*!
     * \brief Returns true when the transformation does not affect the co-ordinates of an object.
     */
    bool is_identity() const;
    /*!
     * \brief Returns true when the transformation only moves the object.
     */
    bool is_translation() const;
    /*!
     * \brief Returns true when the transformation only scales the object.
     */
    bool is_scaling() const;
    /*!
     * \brief Returns true when the transformation only moves the object and scales it with a single factor.
     */
    bool is_scaling_translation() const;
    /*!
     * \brief Returns true when the transformation contains at least a rotation,
     * and might also contain movements and scaling operations.
     */
    bool requires_matrix_transformation() const;

    /*!
     * \brief Changes the scaling factor by the given \a scale.
     */
    void set_scale(float scale);
    void set_translation(Displacement const& movement);
    /*!
     * \brief Rotation of the surface described as yxz angles
     * \param y angle around the rotated y axis (yaw)
     * \param x angle around the rotated x axis (pitch)
     * \param z angle around the original z axis (roll)
     */
    void set_rotation(float y, float x, float z);
    /*!
     * \brief Rotation of the surface described as rotation around the z axis
     */
    void set_rotation(float z);

    float get_z_rotation() const
    {
        return z_angle;
    }

    float get_x_rotation() const
    {
        return x_angle;
    }

    float get_y_rotation() const
    {
        return y_angle;
    }

    Displacement get_translation() const
    {
        return translation;
    }

    float get_scale() const
    {
        return scale;
    }

    /*!
     * \brief Retuns the complete transformation matrix
     *
     * The transformation is defined as:
     *   translate(translation) * translate(size*scale/2) * rotation(y,x,z) * scale * translate(-size*scale/2)
     */
    glm::mat4 get_matrix() const;
    /*!
     * \brief Retuns the inverted transformation matrix
     */
    glm::mat4 get_inverse_matrix() const;
    /*!
     * \brief Returns a transformation matrix lacking the translation.
     */
    glm::mat4 get_center_matrix() const;

    Point transform_to_local(Point const& point) const;
    Point transform_to_screen(Point const& point) const;
private:
    float scale;
    Displacement translation;
    Size size;

    float x_angle, y_angle, z_angle;
    mutable glm::mat4 matrix;
    // center_matrix is just Rotate*Scale
    mutable glm::mat4 center_matrix;
    mutable glm::mat4 inverse_matrix;
    mutable bool dirty;

    void update_matrices() const;
    bool has_scale() const;
    bool has_translation() const;
    bool has_rotation() const;
};
}

}

#endif

