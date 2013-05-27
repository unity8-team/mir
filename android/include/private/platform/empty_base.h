/*
 * Copyright © 2012 Canonical Ltd.
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
 * Authored by: Thomas Voß <thomas.voss@canonical.com>
 */
#ifndef UBUNTU_PLATFORM_EMPTY_BASE_H_
#define UBUNTU_PLATFORM_EMPTY_BASE_H_

namespace ubuntu
{
namespace platform
{
class EmptyBase
{
public:
    virtual ~EmptyBase() {}
protected:
    EmptyBase() {}
    EmptyBase(const EmptyBase&) = delete;
    EmptyBase& operator=(const EmptyBase&) = delete;
};
}
}

#endif // UBUNTU_PLATFORM_EMPTY_BASE_H_
