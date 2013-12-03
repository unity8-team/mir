#ifndef MIR_QT_EVENT_FEEDER_H
#define MIR_QT_EVENT_FEEDER_H

// android-input
#include <InputReader.h>

class QTouchDevice;

/*
  Fills Qt's event loop with events from android-input layer.
 */
class QtEventFeeder : public android::InputListenerInterface
{
public:
    QtEventFeeder();

    // From InputListenerInterface
    void notifyConfigurationChanged(const android::NotifyConfigurationChangedArgs* args) override;
    void notifyKey(const android::NotifyKeyArgs* args) override;
    void notifyMotion(const android::NotifyMotionArgs* args) override;
    void notifySwitch(const android::NotifySwitchArgs* args) override;
    void notifyDeviceReset(const android::NotifyDeviceResetArgs* args) override;

private:
    QTouchDevice *mTouchDevice;
};

#endif // MIR_QT_EVENT_FEEDER_H
