#ifndef QPA_MIR_INPUT_MANAGER_H
#define QPA_MIR_INPUT_MANAGER_H

#include <mir/input/input_manager.h>
#include <mir/scene/input_registrar.h>
#include <mir/input/input_region.h>
#include <mir/shell/input_targeter.h>
#include <mir/input/input_report.h>

// android-input
#include <InputReader.h>
#include <EventHub.h>

// local
#include "qteventfeeder.h"
#include "mirinputdispatcher.h"

class MirInputManager : public mir::input::InputManager,
                        public mir::scene::InputRegistrar,
                        public mir::shell::InputTargeter
{
public:
    MirInputManager(std::shared_ptr<mir::input::InputReport> inputReport,
                    std::shared_ptr<mir::input::InputRegion> inputRegion);

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

private:
    android::sp<android::EventHubInterface> mEventHub;
    android::sp<android::InputReaderPolicyInterface> mInputReaderPolicy;
    android::sp<android::InputReaderInterface> mInputReader;
    android::sp<android::InputReaderThread> mReaderThread;
    android::sp<QtEventFeeder> mQtEventFeeder;
    android::sp<android::MirInputDispatcherInterface> mDispatcher;
    android::sp<android::MirInputDispatcherThread> mDispatcherThread;
};

#endif // QPA_MIR_INPUT_MANAGER_H
