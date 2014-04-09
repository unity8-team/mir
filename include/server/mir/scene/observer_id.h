/*
 * Copyright © 2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored By: Robert Carr <racarr@canonical.com>
 */

#ifndef MIR_SCENE_OBSERVER_NOTIFICATION_ID_H_
#define MIR_SCENE_OBSERVER_NOTIFICATION_ID_H_

#include "mir/int_wrapper.h"

namespace mir
{
namespace scene
{
namespace detail { struct ObserverIDTag; }

typedef IntWrapper<detail::ObserverIDTag> ObserverID;
}
} // namespace mir

#endif // MIR_SCENE_OBSERVER_NOTIFICATION_ID_H_
