#ifndef QPA_MIR_INPUT_MANAGER_H
#define QPA_MIR_INPUT_MANAGER_H

#include <mir/input/input_manager.h>
#include <mir/scene/input_registrar.h>
#include <mir/shell/input_targeter.h>

class MirInputManager : public mir::input::InputManager,
                        public mir::scene::InputRegistrar,
                        public mir::shell::InputTargeter
{
public:
    // From mir::input::InputManager
    void start() override;
    void stop() override;
    std::shared_ptr<mir::input::InputChannel> make_input_channel() override;

    // From mir::scene::InputRegistrar
    void input_channel_opened(std::shared_ptr<mir::input::InputChannel> const& opened_channel,
                              std::shared_ptr<mir::input::Surface> const& info,
                              mir::input::InputReceptionMode input_mode) override;

    void input_channel_closed(std::shared_ptr<mir::input::InputChannel> const& closed_channel) override;

    // From mir::shell::InputTargeter
    void focus_changed(std::shared_ptr<mir::input::InputChannel const> const& focus_channel) override;
    void focus_cleared() override;
};

#endif // QPA_MIR_INPUT_MANAGER_H
