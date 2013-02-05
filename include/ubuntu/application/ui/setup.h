/*
 * Copyright © 2012 Canonical Ltd.
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
 * Authored by: Thomas Voß <thomas.voss@canonical.com>
 */
#ifndef UBUNTU_APPLICATION_UI_SETUP_H_
#define UBUNTU_APPLICATION_UI_SETUP_H_

#include "ubuntu/application/ui/stage_hint.h"
#include "ubuntu/application/ui/form_factor_hint.h"
#include "ubuntu/platform/shared_ptr.h"

namespace ubuntu
{
namespace application
{
namespace ui
{
class Setup : public ubuntu::platform::ReferenceCountedBase
{
public:
    typedef ubuntu::platform::shared_ptr<Setup> Ptr;

    static const Ptr& instance();

    virtual StageHint stage_hint() = 0;

    virtual FormFactorHintFlags form_factor_hint() = 0;

    virtual const char* desktop_file_hint() = 0;

protected:
    Setup() {}
    virtual ~Setup() {}

    Setup(const Setup&) = delete;
    Setup& operator=(const Setup&) = delete;
};
}
}
}

#endif // UBUNTU_APPLICATION_UI_SETUP_H_
