#ifndef QPA_MIR_INPUT_CONFIGURATION_H
#define QPA_MIR_INPUT_CONFIGURATION_H

#include <mir/input/input_configuration.h>
#include <mir/input/input_report.h>
#include <mir/input/input_region.h>

#include "mirinputmanager.h"

class MirInputConfiguration : public mir::input::InputConfiguration
{
public:
    MirInputConfiguration(std::shared_ptr<mir::input::InputReport> inputReport,
                          std::shared_ptr<mir::input::InputRegion> inputRegion);
    virtual ~MirInputConfiguration() {}

    std::shared_ptr<mir::scene::InputRegistrar> the_input_registrar() override;
    std::shared_ptr<mir::shell::InputTargeter> the_input_targeter() override;
    std::shared_ptr<mir::input::InputManager> the_input_manager() override;

    void set_input_targets(std::shared_ptr<mir::input::InputTargets> const& targets) override;

private:
    std::shared_ptr<MirInputManager> mInputManager;
};

#endif // QPA_MIR_INPUT_CONFIGURATION_H
