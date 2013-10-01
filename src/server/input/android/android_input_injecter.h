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
 * Authored by: Robert Carr <robert.carr@canonical.com>
 */

#ifndef MIR_INPUT_ANDROID_INJECTER_H_
#define MIR_INPUT_ANDROID_INJECTER_H_

#include "mir/shell/input_injecter.h"

#include <utils/StrongPointer.h>

#include <mutex>

namespace android
{
class InputDispatcherInterface;
class InputWindowHandle;
}

namespace droidinput = android;

namespace mir
{
namespace input
{
namespace android
{
class InputConfiguration;
class WindowHandleRepository;

class InputInjecter : public shell::InputInjecter
{
public:
    explicit InputInjecter(droidinput::sp<droidinput::InputDispatcherInterface> const& input_dispatcher,
                           std::shared_ptr<WindowHandleRepository> const& repository);
    virtual ~InputInjecter() noexcept(true);

    void inject_input(std::shared_ptr<input::InputChannel const> const& target,
                      MirEvent const& ev);

protected:
    InputInjecter(const InputInjecter&) = delete;
    InputInjecter& operator=(const InputInjecter&) = delete;

private:
    droidinput::sp<droidinput::InputDispatcherInterface> input_dispatcher;

    std::shared_ptr<WindowHandleRepository> const repository;
};

}
}
} // namespace mir

#endif // MIR_INPUT_ANDROID_INJECTER_H_
