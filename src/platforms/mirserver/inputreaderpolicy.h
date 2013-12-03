#ifndef MIRSERVER_INPUT_READER_POLICY_H
#define MIRSERVER_INPUT_READER_POLICY_H

#include <InputReader.h>

#include <mir/input/input_region.h>

class InputReaderPolicy : public android::InputReaderPolicyInterface
{
public:
    InputReaderPolicy(std::shared_ptr<mir::input::InputRegion> inputRegion);

    // From InputReaderPolicyInterface
    void getReaderConfiguration(android::InputReaderConfiguration* outConfig) override;
    android::sp<android::PointerControllerInterface> obtainPointerController(int32_t deviceId) override;
    void notifyInputDevicesChanged(const android::Vector<android::InputDeviceInfo>& inputDevices) override;
    android::sp<android::KeyCharacterMap> getKeyboardLayoutOverlay(const android::String8& inputDeviceDescriptor) override;
    android::String8 getDeviceAlias(const android::InputDeviceIdentifier& identifier) override;

private:
    std::shared_ptr<mir::input::InputRegion> mInputRegion;
};

#endif // MIRSERVER_INPUT_READER_POLICY_H
