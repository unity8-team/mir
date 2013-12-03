#include "mirinputconfiguration.h"

using namespace mir;

MirInputConfiguration::MirInputConfiguration(std::shared_ptr<input::InputReport> inputReport,
                                             std::shared_ptr<input::InputRegion> inputRegion)
    : mInputManager(std::make_shared<MirInputManager>(inputReport, inputRegion))
{
}

std::shared_ptr<scene::InputRegistrar> MirInputConfiguration::the_input_registrar()
{
    return std::static_pointer_cast<scene::InputRegistrar>(mInputManager);
}

std::shared_ptr<shell::InputTargeter> MirInputConfiguration::the_input_targeter()
{
    return std::static_pointer_cast<shell::InputTargeter>(mInputManager);
}

std::shared_ptr<input::InputManager> MirInputConfiguration::the_input_manager()
{
    return std::static_pointer_cast<input::InputManager>(mInputManager);
}

void MirInputConfiguration::set_input_targets(std::shared_ptr<input::InputTargets> const& targets)
{
    (void)targets;
}
