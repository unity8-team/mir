#include "mirinputmanager.h"
#include "mirinputchannel.h"

using mir::input::InputChannel;
using namespace mir;

void MirInputManager::start()
{
}

void MirInputManager::stop()
{
}

std::shared_ptr<input::InputChannel> MirInputManager::make_input_channel()
{
    return std::shared_ptr<input::InputChannel>(new MirInputChannel);
}

void MirInputManager::input_channel_opened(
        std::shared_ptr<InputChannel> const& opened_channel,
        std::shared_ptr<input::Surface> const& info,
        input::InputReceptionMode input_mode)
{
    (void)opened_channel;
    (void)info;
    (void)input_mode;
}

void MirInputManager::input_channel_closed(
        std::shared_ptr<InputChannel> const& closed_channel)
{
    (void)closed_channel;
}

void MirInputManager::focus_changed(std::shared_ptr<InputChannel const> const& focus_channel)
{
    (void)focus_channel;
}

void MirInputManager::focus_cleared()
{
}
