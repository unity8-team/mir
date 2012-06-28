/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Alan Griffiths <alan@octopull.co.uk>
 */

#ifndef MIR_GEOMETRY_DIMENSIONS_H_
#define MIR_GEOMETRY_DIMENSIONS_H_

#include <cstdint>
#include <iosfwd>

namespace mir
{
namespace geometry
{

namespace detail
{
enum DimensionTag { width, height, x, y, dx, dy };

template<DimensionTag Tag>
class IntWrapper
{
public:
    IntWrapper() : value(0) {}
    IntWrapper(uint32_t value) : value(value) {}

    uint32_t as_uint32_t() const
    {
        return value;
    }

private:
    uint32_t value;
};

template<DimensionTag Tag>
std::ostream& operator<<(std::ostream& out, IntWrapper<Tag> const& value)
{
    out << value.as_uint32_t();
    return out;
}

template<DimensionTag Tag>
inline bool operator == (IntWrapper<Tag> const& lhs, IntWrapper<Tag> const& rhs)
{
    return lhs.as_uint32_t() == rhs.as_uint32_t();
}

template<DimensionTag Tag>
inline bool operator != (IntWrapper<Tag> const& lhs, IntWrapper<Tag> const& rhs)
{
    return lhs.as_uint32_t() != rhs.as_uint32_t();
}

template<DimensionTag Tag>
inline bool operator <= (IntWrapper<Tag> const& lhs, IntWrapper<Tag> const& rhs)
{
    return lhs.as_uint32_t() <= rhs.as_uint32_t();
}

template<DimensionTag Tag>
inline bool operator >= (IntWrapper<Tag> const& lhs, IntWrapper<Tag> const& rhs)
{
    return lhs.as_uint32_t() >= rhs.as_uint32_t();
}

template<DimensionTag Tag>
inline bool operator < (IntWrapper<Tag> const& lhs, IntWrapper<Tag> const& rhs)
{
    return lhs.as_uint32_t() < rhs.as_uint32_t();
}

template<DimensionTag Tag>
inline bool operator > (IntWrapper<Tag> const& lhs, IntWrapper<Tag> const& rhs)
{
    return lhs.as_uint32_t() > rhs.as_uint32_t();
}
} // namespace detail

typedef detail::IntWrapper<detail::width> Width;
typedef detail::IntWrapper<detail::height> Height;
typedef detail::IntWrapper<detail::x> X;
typedef detail::IntWrapper<detail::y> Y;
typedef detail::IntWrapper<detail::dx> DeltaX;
typedef detail::IntWrapper<detail::dy> DeltaY;

// Adding deltas is fine
DeltaX operator+(DeltaX lhs, DeltaX rhs) { return DeltaX(lhs.as_uint32_t() + rhs.as_uint32_t()); }
DeltaY operator+(DeltaY lhs, DeltaX rhs) { return DeltaY(lhs.as_uint32_t() + rhs.as_uint32_t()); }
DeltaX operator-(DeltaX lhs, DeltaX rhs) { return DeltaX(lhs.as_uint32_t() - rhs.as_uint32_t()); }
DeltaY operator-(DeltaY lhs, DeltaX rhs) { return DeltaY(lhs.as_uint32_t() - rhs.as_uint32_t()); }

// Adding deltas to co-ordinates is fine
X operator+(X lhs, DeltaX rhs) { return X(lhs.as_uint32_t() + rhs.as_uint32_t()); }
Y operator+(Y lhs, DeltaY rhs) { return Y(lhs.as_uint32_t() + rhs.as_uint32_t()); }
X operator-(X lhs, DeltaX rhs) { return X(lhs.as_uint32_t() - rhs.as_uint32_t()); }
Y operator-(Y lhs, DeltaY rhs) { return Y(lhs.as_uint32_t() - rhs.as_uint32_t()); }

// Subtracting coordinates is fine
DeltaX operator-(X lhs, X rhs) { return DeltaX(lhs.as_uint32_t() - rhs.as_uint32_t()); }
DeltaY operator-(Y lhs, Y rhs) { return DeltaY(lhs.as_uint32_t() - rhs.as_uint32_t()); }

template<typename Target, typename Source>
Target dim_cast(Source s) { return Target(s.as_uint32_t()); }
}
}

#endif /* MIR_GEOMETRY_DIMENSIONS_H_ */
