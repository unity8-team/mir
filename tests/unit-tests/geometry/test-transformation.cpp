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
 * Authored by: Andreas Pokorny <andreas.pokorny@canonical.com>
 */

#include "mir/geometry/transformation.h"

#include <gtest/gtest.h>

#define GLM_FORCE_RADIANS
#include "glm/gtx/euler_angles.hpp"
#include "glm/gtx/transform.hpp"

namespace geom = mir::geometry;

::testing::AssertionResult MatricesMatch(glm::mat4 const& expected, glm::mat4 const& tested)
{
    for (int i = 0; i != 4; ++i)
    {
        for (int j = 0; j != 4; ++j)
        {
            float diff = std::abs(expected[i][j] - tested[i][j]);
            if (diff > 1.0E-7f)
            {
                return ::testing::AssertionFailure() << "item (" << i << ", "<< j << ") differs:"
                    << tested[i][j] << " instead of " << expected[i][j] << " off by " << diff;
            }
        }
    }

    return ::testing::AssertionSuccess();
}

namespace
{

glm::vec4 to_vec4(geom::Point const& p)
{
    return glm::vec4{p.x.as_float(), p.y.as_float(), 0.0f, 1.0f};
}

geom::Point to_point(glm::vec4 const& p)
{
    return {std::round(p.x), std::round(p.y)};
}

}

TEST(geometry_transformation, identity_on_creation)
{
    using namespace geom;
    Transformation identity;
    EXPECT_TRUE(identity.is_identity());
    EXPECT_FALSE(identity.is_translation());
    EXPECT_FALSE(identity.is_scaling_translation());
    EXPECT_EQ(1.0f, identity.get_scale());
    EXPECT_EQ(0.0f, identity.get_z_rotation());
    EXPECT_EQ(0.0f, identity.get_x_rotation());
    EXPECT_EQ(0.0f, identity.get_y_rotation());
    EXPECT_EQ(geom::Displacement(), identity.get_translation());
}

TEST(geometry_transformation, scale_may_destroy_identity)
{
    using namespace geom;
    Transformation identity;
    identity.set_scale(1230.f);
    EXPECT_FALSE(identity.is_identity());
    EXPECT_FALSE(identity.is_scaling_translation());
    EXPECT_TRUE(identity.is_scaling());
    identity.set_scale(1.0f);
    EXPECT_TRUE(identity.is_identity());
}

TEST(geometry_transformation, translation)
{
    using namespace geom;
    Transformation movement;
    movement.set_translation(geom::Displacement{13,14});
    EXPECT_TRUE(movement.is_translation());
    EXPECT_FALSE(movement.is_scaling_translation());
    EXPECT_FALSE(movement.requires_matrix_transformation());
    EXPECT_TRUE(movement.is_translation());

    geom::Displacement mov{13,14};
    EXPECT_EQ(mov, movement.get_translation());
}

TEST(geometry_transformation, identity_matrices)
{
    using namespace geom;
    Transformation identity;
    glm::mat4 expected;
    EXPECT_TRUE(MatricesMatch(expected, identity.get_matrix()));
    EXPECT_TRUE(MatricesMatch(expected, identity.get_inverse_matrix()));
    EXPECT_TRUE(MatricesMatch(expected, identity.get_center_matrix()));

    Point test_p(4124,1231);
    EXPECT_EQ(test_p, identity.transform_to_local(test_p));
    EXPECT_EQ(test_p, identity.transform_to_screen(test_p));
    EXPECT_EQ(test_p, to_point(identity.get_matrix() * to_vec4(test_p)));
    EXPECT_EQ(test_p, to_point(identity.get_inverse_matrix() * to_vec4(test_p)));
}

TEST(geometry_transformation, translation_matrices)
{
    using namespace geom;
    Transformation translation;
    Displacement amount{13,42};
    translation.set_translation(amount);
    glm::mat4 expected_center, expected;

    expected[3] = glm::vec4{amount.dx.as_float(), amount.dy.as_float(), 0.0f, 1.0f};

    EXPECT_TRUE(MatricesMatch(expected, translation.get_matrix()));
    EXPECT_TRUE(MatricesMatch(glm::inverse(expected), translation.get_inverse_matrix()));
    EXPECT_TRUE(MatricesMatch(expected_center, translation.get_center_matrix()));

    Point test_p(4124, 1147);
    Point screen_p(4137, 1189);
    EXPECT_EQ(screen_p, translation.transform_to_screen(test_p));
    EXPECT_EQ(test_p, translation.transform_to_local(screen_p));
    EXPECT_EQ(screen_p, to_point(translation.get_matrix() * to_vec4(test_p)));
    EXPECT_EQ(test_p, to_point(translation.get_inverse_matrix() * to_vec4(screen_p)));
}

TEST(geometry_transformation, scaling_matrices)
{
    using namespace geom;
    Transformation scaling{Size{120,60}};
    scaling.set_scale(3.5f);
    glm::mat4 inv_expected, expected;
    expected[0].x = expected[1].y = expected[2].z = 3.5f;
    inv_expected[0].x = inv_expected[1].y = inv_expected[2].z = 1.0f/3.5f;

    EXPECT_TRUE(MatricesMatch(expected, scaling.get_matrix()));
    EXPECT_TRUE(MatricesMatch(expected, scaling.get_center_matrix()));
    EXPECT_TRUE(MatricesMatch(inv_expected, scaling.get_inverse_matrix()));

    Point test_p(10, 20);
    Point screen_p(35, 70);
    EXPECT_EQ(screen_p, scaling.transform_to_screen(test_p));
    EXPECT_EQ(test_p, scaling.transform_to_local(screen_p));
    EXPECT_EQ(screen_p, to_point(scaling.get_matrix() * to_vec4(test_p)));
    EXPECT_EQ(test_p, to_point(scaling.get_inverse_matrix() * to_vec4(screen_p)));
}

TEST(geometry_transformation, scale_and_translate_matrices)
{
    using namespace geom;
    Transformation scale_and_translate{Size{30,10}};
    float scale = 1.33f;
    float x_pos = 60.0f;
    float y_pos = 50.0f;
    scale_and_translate.set_scale(scale);
    scale_and_translate.set_translation(Displacement{x_pos, y_pos});
    glm::mat4 inv_expected, expected;
    expected[0].x = expected[1].y = expected[2].z = scale;
    expected[3].x = x_pos;
    expected[3].y = y_pos;
    inv_expected = glm::scale(glm::vec3(1.0f/scale))* glm::translate(-glm::vec3{x_pos,y_pos,0});

    EXPECT_TRUE(MatricesMatch(expected, scale_and_translate.get_matrix()));
    EXPECT_TRUE(MatricesMatch(inv_expected, scale_and_translate.get_inverse_matrix()));

    Point test_p(10, 20);
    Point screen_p(73, 77);
    EXPECT_EQ(screen_p, scale_and_translate.transform_to_screen(test_p));
    EXPECT_EQ(test_p, scale_and_translate.transform_to_local(screen_p));
    EXPECT_EQ(screen_p, to_point(scale_and_translate.get_matrix() * to_vec4(test_p)));
    EXPECT_EQ(test_p, to_point(scale_and_translate.get_inverse_matrix() * to_vec4(screen_p)));
}

TEST(geometry_transformation, rotate_by_90_matrices)
{
    using namespace geom;
    Transformation rotation(Size(10,10));
    rotation.set_rotation(90.0f);
    glm::mat4 expected_center;
    expected_center[0] = glm::vec4{0.0f, 1.0f, 0.0f, 0.0f};
    expected_center[1] = glm::vec4{-1.0f, 0.0f, 0.0f, 0.0f};
    glm::mat4 expected_matrix = expected_center;
    expected_matrix[3] = glm::vec4{10.0f, 0.0f, 0.0f, 1.0f};
    glm::mat4 expected_inverse = glm::inverse(expected_matrix);

    EXPECT_TRUE(MatricesMatch(expected_matrix, rotation.get_matrix()));
    EXPECT_TRUE(MatricesMatch(expected_center, rotation.get_center_matrix()));
    EXPECT_TRUE(MatricesMatch(expected_inverse, rotation.get_inverse_matrix()));

    Point test_p(0, 0);
    Point screen_p(10, 0);
    EXPECT_EQ(screen_p, rotation.transform_to_screen(test_p));
    EXPECT_EQ(test_p, rotation.transform_to_local(screen_p));
    EXPECT_EQ(screen_p, to_point(rotation.get_matrix() * to_vec4(test_p)));
    EXPECT_EQ(test_p, to_point(rotation.get_inverse_matrix() * to_vec4(screen_p)));
}

TEST(geometry_transformation, all_combined_matrices)
{
    using namespace geom;
    glm::vec3 center(10,5,0);
    float angle = 20.0f;
    float scale = 3.65f;
    glm::vec3 translation(100,100,0);

    Transformation all_combined(Size(center.x*2,center.y*2));
    all_combined.set_rotation(angle);
    all_combined.set_scale(scale);
    all_combined.set_translation(Displacement(translation.x,translation.y));

    glm::mat4 expected =
        glm::translate(translation) *
        glm::scale(glm::vec3(scale)) *
        glm::translate(center) *
        glm::eulerAngleZ(glm::radians(angle)) *
        glm::translate(-center);

    glm::mat4 inverse =
        glm::translate(center) *
        glm::eulerAngleZ(glm::radians(-angle)) *
        glm::translate(-center) *
        glm::scale(glm::vec3(1.0f / scale)) *
        glm::translate(-translation);

    EXPECT_TRUE(MatricesMatch(expected, all_combined.get_matrix()));
    EXPECT_TRUE(MatricesMatch(inverse, all_combined.get_inverse_matrix()));

    Point test_p(20, 10);
    Point screen_p(165, 148);
    EXPECT_EQ(screen_p, all_combined.transform_to_screen(test_p));
    EXPECT_EQ(test_p, all_combined.transform_to_local(screen_p));
    EXPECT_EQ(screen_p, to_point(all_combined.get_matrix() * to_vec4(test_p)));
    EXPECT_EQ(test_p, to_point(all_combined.get_inverse_matrix() * to_vec4(screen_p)));
}
