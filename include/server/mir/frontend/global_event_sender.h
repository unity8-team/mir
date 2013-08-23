/*
 * Copyright © 2013 Canonical Ltd.
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
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */

#ifndef MIR_FRONTEND_GLOBAL_EVENT_SENDER_H_
#define MIR_FRONTEND_GLOBAL_EVENT_SENDER_H_

#include "event_sink.h"
#include <memory>

namespace mir
{
namespace shell
{
class SessionContainer;
}
namespace frontend
{
class GlobalEventSender : public EventSink
{
public:
    GlobalEventSender(std::shared_ptr<shell::SessionContainer> const&);

    void handle_event(MirEvent const& e);
    void handle_lifecycle_event(MirLifecycleState state);
    void handle_display_config_change(graphics::DisplayConfiguration const& config);

private:
    std::shared_ptr<shell::SessionContainer> const sessions;
};
}
}

#endif /* MIR_FRONTEND_GLOBAL_EVENT_SENDER_H_ */
