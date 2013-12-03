#include "mirinputmanager.h"
#include "mirinputchannel.h"
#include "inputreaderpolicy.h"

#include <QDebug>

using mir::input::InputReport;
using namespace mir;
using namespace android;

MirInputManager::MirInputManager(std::shared_ptr<input::InputReport> inputReport,
                                 std::shared_ptr<input::InputRegion> inputRegion)
{
    mEventHub = new EventHub(inputReport);
    mInputReaderPolicy = new InputReaderPolicy(inputRegion);
    mQtEventFeeder = new QtEventFeeder;
    mInputReader = new InputReader(mEventHub, mInputReaderPolicy, mQtEventFeeder);
    mReaderThread = new InputReaderThread(mInputReader);
}

void MirInputManager::start()
{
    status_t result;

    result = mReaderThread->run("InputReader", PRIORITY_URGENT_DISPLAY);
    if (result) {
        qCritical() << "Could not start InputReader thread due to error" << result;
    }
}

void MirInputManager::stop()
{
}

std::shared_ptr<input::InputChannel> MirInputManager::make_input_channel()
{
    return std::shared_ptr<input::InputChannel>(new MirInputChannel);
}

void MirInputManager::input_channel_opened(
        std::shared_ptr<input::InputChannel> const& opened_channel,
        std::shared_ptr<input::Surface> const& info,
        input::InputReceptionMode input_mode)
{
    (void)opened_channel;
    (void)info;
    (void)input_mode;
}

void MirInputManager::input_channel_closed(
        std::shared_ptr<input::InputChannel> const& closed_channel)
{
    (void)closed_channel;
}

void MirInputManager::focus_changed(std::shared_ptr<input::InputChannel const> const& focus_channel)
{
    (void)focus_channel;
}

void MirInputManager::focus_cleared()
{
}
