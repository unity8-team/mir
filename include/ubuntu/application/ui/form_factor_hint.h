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
#ifndef UBUNTU_APPLICATION_UI_FORM_FACTOR_HINT_H_
#define UBUNTU_APPLICATION_UI_FORM_FACTOR_HINT_H_

#include "ubuntu/application/ui/ubuntu_application_ui.h"

namespace ubuntu
{
namespace application
{
namespace ui
{
enum FormFactorHint
{
    desktop_form_factor = DESKTOP_FORM_FACTOR_HINT,
    phone_form_factor = PHONE_FORM_FACTOR_HINT,
    tablet_form_factor = TABLET_FORM_FACTOR_HINT
};

typedef unsigned int FormFactorHintFlags;

}
}
}

#endif // UBUNTU_APPLICATION_UI_FORM_FACTOR_HINT_H_
