/*
 * Copyright © 2015 Canonical Ltd.
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
 * Authored By: Andreas Pokorny <andreas.pokorny@canonical.com>
 */

#ifndef MIR_FLAGS_H_
#define MIR_FLAGS_H_

#include <type_traits>

namespace mir
{

/*!
 * Treat an enumeration, scoped and unscoped, like a set of flags
 */
template<typename Enum>
struct Flags
{
    using value_type = typename std::underlying_type<Enum>::type;
    value_type value;

    explicit constexpr Flags(value_type value = 0) noexcept
        : value{value} {}
    constexpr Flags(Enum value) noexcept
        : value{static_cast<value_type>(value)} {}

    constexpr Flags<Enum> operator|(Flags<Enum> other) const noexcept
    {
        return Flags<Enum>(value|other.value);
    }

    constexpr Flags<Enum> operator&(Flags<Enum> other) const noexcept
    {
        return Flags<Enum>(value & other.value);
    }

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wconstexpr-not-const"
#endif
    constexpr Flags<Enum>& operator|=(Flags<Enum> other) noexcept
    {
        return value |= other.value, *this;
    }

    constexpr Flags<Enum> operator&=(Flags<Enum> other) noexcept
    {
        return value &= other.value, *this;
    }
#ifdef __clang__
#pragma clang diagnostic pop
#endif

    constexpr bool operator==(Flags<Enum> other) const noexcept
    {
        return value == other.value;
    }
};

template<typename Enum>
constexpr Flags<Enum> operator|(Flags<Enum> flags, Enum e) noexcept
{
    return Flags<Enum>(flags.value | static_cast<decltype(flags.value)>(e));
}

template<typename Enum>
constexpr Flags<Enum> operator|(Enum e, Flags<Enum> flags) noexcept
{
    return Flags<Enum>(flags.value | static_cast<decltype(flags.value)>(e));
}

template<typename Enum>
constexpr Enum operator&(Enum e, Flags<Enum> flags) noexcept
{
    return static_cast<Enum>(flags.value & static_cast<decltype(flags.value)>(e));
}

template<typename Enum>
constexpr Enum operator&(Flags<Enum> flags, Enum e) noexcept
{
    return static_cast<Enum>(flags.value & static_cast<decltype(flags.value)>(e));
}

template<typename Enum>
constexpr bool operator==(Flags<Enum> flags, Enum e) noexcept
{
    return e == static_cast<Enum>(flags.value);
}

template<typename Enum>
constexpr bool operator==(Enum e, Flags<Enum> flags) noexcept
{
    return e == static_cast<Enum>(flags.value);
}

template<typename Enum>
constexpr bool contains(Flags<Enum> flags, Enum e) noexcept
{
    return e == static_cast<Enum>(flags.value & static_cast<decltype(flags.value)>(e));
}

}

#endif
