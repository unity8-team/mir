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
 * Authored by: Andreas Pokorny <andreas.pokorny@canonical.com>
 */

#ifndef MIR_SCENE_DEFAULT_INPUT_REGISTRAR_H_
#define MIR_SCENE_DEFAULT_INPUT_REGISTRAR_H_

#include "mir/scene/input_registrar.h"

#include <vector>
#include <mutex>

namespace mir
{
namespace scene
{

class DefaultInputRegistrar : public InputRegistrar
{
public:
    void input_channel_opened(std::shared_ptr<input::InputChannel> const& opened_channel,
                              std::shared_ptr<input::Surface> const& info,
                              input::InputReceptionMode input_mode) override;
    void input_channel_closed(std::shared_ptr<input::InputChannel> const& closed_channel) override;



    void add_observer(std::shared_ptr<InputRegistrarObserver> const& observer) override;
    void remove_observer(std::shared_ptr<InputRegistrarObserver> const& observer) override;

    DefaultInputRegistrar() = default;
private:
    DefaultInputRegistrar(InputRegistrar const&) = delete;
    DefaultInputRegistrar& operator=(InputRegistrar const&) = delete;
    std::mutex observers_mutex;
    std::vector<std::shared_ptr<InputRegistrarObserver>> observers;
};

}
} // namespace mir

#endif // MIR_SCENE_DEFAULT_INPUT_REGISTRAR_H_
