/*
 * Copyright (C) 2013 Canonical Ltd
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
 * Authored by: Thomas Voss <thomas.voss@canonical.com>
 */
#ifndef REF_COUNTED_H_
#define REF_COUNTED_H_

#include <atomic>

namespace detail
{
class RefCounted
{
  public:
    RefCounted(const RefCounted&) = delete;
    virtual ~RefCounted() = default;

    RefCounted& operator=(const RefCounted&) = delete;
    bool operator==(const RefCounted&) const = delete;

    void ref() { counter.fetch_add(1); }
    void unref() { if (1 == counter.fetch_sub(1)) { delete this; } }

  protected:
    RefCounted() : counter(1)
    {
    }

  private:
    std::atomic<int> counter;
};
}

#endif // REF_COUNTED_H_
