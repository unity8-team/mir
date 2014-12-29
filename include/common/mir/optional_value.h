/*
 * Copyright © 2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Alberto Aguirre <alberto.aguirre@canonical.com>
 */

#ifndef MIR_OPTIONAL_VALUE_H_
#define MIR_OPTIONAL_VALUE_H_

namespace mir
{
template<typename T>
class optional_value
{
public:
    optional_value& operator=(T const& value)
    {
        value_ = value;
        is_set_ = true;
        return *this;
    }

    bool is_set() const { return is_set_; }
    T value() const { return value_; }

private:
    T value_;
    bool is_set_{false};
};


template<typename T>
inline bool operator == (optional_value<T> const& lhs, optional_value<T> const& rhs)
{
    return lhs.is_set() == rhs.is_set() &&
           (lhs.is_set() ? lhs.value() == rhs.value() : true);
}

template<typename T>
inline bool operator != (optional_value<T> const& lhs, optional_value<T> const& rhs)
{
    return !(lhs == rhs);
}

template<typename T>
inline bool operator == (optional_value<T> const& lhs, T const& rhs)
{
    return lhs.is_set() && (lhs.value() == rhs);
}

template<typename T>
inline bool operator != (optional_value<T> const& lhs, T const& rhs)
{
    return !(lhs == rhs);
}

template<typename T>
inline bool operator == (T const& lhs, optional_value<T> const& rhs)
{
    return rhs == lhs;
}

template<typename T>
inline bool operator != (T const& lhs, optional_value<T> const& rhs)
{
    return rhs != lhs;
}
}

#endif /* MIR_OPTIONAL_VALUE_H_ */
