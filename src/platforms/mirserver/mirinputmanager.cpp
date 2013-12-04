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
    mDispatcher = new MirInputDispatcher(inputReport);
    mDispatcherThread = new MirInputDispatcherThread(mDispatcher);
}

void MirInputManager::start()
{
    status_t result;

    mDispatcher->setInputDispatchMode(true /*enabled*/, false /*frozen*/);
    result = mDispatcherThread->run("InputDispatcher", PRIORITY_URGENT_DISPLAY);
    if (result) {
        qCritical() << "Could not start InputDispatcher thread due to error" << result;
        return;
    }

    result = mReaderThread->run("InputReader", PRIORITY_URGENT_DISPLAY);
    if (result) {
        qCritical() << "Could not start InputReader thread due to error" << result;
    }
}

void MirInputManager::stop()
{
    mReaderThread->requestExit();
    mEventHub->wake();
    mReaderThread->join();

    mDispatcherThread->requestExit();
    mDispatcher->setInputDispatchMode(false /*enabled*/, true /*frozen*/);
    mDispatcherThread->join();
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
    (void)info;
    (void)input_mode;

    sp<android::InputChannel> androidChannel =
        static_cast<MirInputChannel*>(opened_channel.get())->serverSideChannel;

    mDispatcher->registerInputChannel(androidChannel, false /*monitor*/);
}

void MirInputManager::input_channel_closed(
        std::shared_ptr<input::InputChannel> const& closed_channel)
{
    sp<android::InputChannel> androidChannel =
        static_cast<MirInputChannel*>(closed_channel.get())->serverSideChannel;

    mDispatcher->unregisterInputChannel(androidChannel);
}

void MirInputManager::focus_changed(std::shared_ptr<input::InputChannel const> const& focus_channel)
{
    (void)focus_channel;
}

void MirInputManager::focus_cleared()
{
}
