#include "inputreaderpolicy.h"

#include <mir/geometry/rectangle.h>

using namespace android;

InputReaderPolicy::InputReaderPolicy(std::shared_ptr<mir::input::InputRegion> inputRegion)
    : mInputRegion(inputRegion)
{
}

void InputReaderPolicy::getReaderConfiguration(InputReaderConfiguration* outConfig)
{
    static int32_t const default_display_id = 0;
    static bool const is_external = false;
    static int32_t const default_display_orientation = DISPLAY_ORIENTATION_0;

    auto bounds = mInputRegion->bounding_rectangle();
    auto width = bounds.size.width.as_float();
    auto height = bounds.size.height.as_float();

    outConfig->setDisplayInfo(
        default_display_id,
        is_external,
        width,
        height,
        default_display_orientation);

    outConfig->pointerVelocityControlParameters.acceleration = 1.0;
}

sp<PointerControllerInterface> InputReaderPolicy::obtainPointerController(int32_t deviceId)
{
    (void)deviceId;
    return nullptr;
}

void InputReaderPolicy::notifyInputDevicesChanged(const Vector<InputDeviceInfo>& inputDevices)
{
    (void)inputDevices;
}

sp<KeyCharacterMap> InputReaderPolicy::getKeyboardLayoutOverlay(const String8& inputDeviceDescriptor)
{
    (void)inputDeviceDescriptor;
    return KeyCharacterMap::empty();
}

String8 InputReaderPolicy::getDeviceAlias(const InputDeviceIdentifier& identifier)
{
    (void)identifier;
    return String8();
}
