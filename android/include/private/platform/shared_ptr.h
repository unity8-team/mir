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
#ifndef UBUNTU_PLATFORM_SHARED_PTR_H_
#define UBUNTU_PLATFORM_SHARED_PTR_H_

#ifdef ANDROID
#include <utils/RefBase.h>
#include <utils/StrongPointer.h>
#else
#include <memory>
#endif

#include "empty_base.h"

namespace ubuntu
{
namespace platform
{

#ifdef ANDROID
typedef android::RefBase ReferenceCountedBase;

template<typename T>
struct shared_ptr : public android::sp<T>
{
    shared_ptr() : android::sp<T>()
    {
    }

    template<typename Y>
    shared_ptr(Y* p) : android::sp<T>(p)
    {
    }
};
#else
typedef ubuntu::platform::EmptyBase ReferenceCountedBase;
using std::shared_ptr;
#endif

}
}

#endif // UBUNTU_PLATFORM_SHARED_PTR_H_
