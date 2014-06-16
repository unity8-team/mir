/*
 * Copyright © 2013 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Thomas Voß <thomas.voss@canonical.com>
 */

#include <android/linux/android_alarm.h>

#include <ubuntu/hardware/alarm.h>

#include <cstdio>
#include <cstring>

#include <stdexcept>
#include <string>

#include <unistd.h>

#include <errno.h>
#include <fcntl.h>

#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>

namespace
{
struct HardwareAlarm
{
    HardwareAlarm() = default;
    virtual ~HardwareAlarm() = default;

    // Blocks the calling thread until an alarm fires or
    // an error occurs. Returns an int with bool semantics.
    virtual int wait_for() = 0;

    // Arms the alarm with the given properties.
    // Returns true if the alarm has been armed successfully, false otherwise.
    virtual bool set(
        UHardwareAlarmTimeReference time_reference,
        UHardwareAlarmSleepBehavior behavior,
        const struct timespec *ts) = 0;

    // Queries the time since last boot including deep sleep periods.
    virtual bool get_elapsed_realtime(struct timespec* ts) = 0;
};

// An implementation of HardwareAlarm based on Android's /dev/alarm.
struct DevAlarmHardwareAlarm : public HardwareAlarm
{
    int fd; // file descriptor referring to /dev/alarm

    DevAlarmHardwareAlarm() : fd(open("/dev/alarm", O_RDWR))
    {
        if (fd == -1)
        {
            auto error = errno;

            std::string what
            {
                "Could not open /dev/alarm: "
            };
            throw std::runtime_error
            {
                (what + strerror(error)).c_str()
            };
        }
    }

    ~DevAlarmHardwareAlarm()
    {
        // No need to check if fd is valid here.
        // Ctor would have thrown if fd was invalid.
        ::close(fd);
    }

    int wait_for()
    {
        int result{-1};

        do
        {
            result = ::ioctl(fd, ANDROID_ALARM_WAIT);
        } while (result < 0 && errno == EINTR);

        if (result < 0)
            fprintf(stderr, "Waiting for hw alarm failed with: %s\n", strerror(errno));

        return result;
    }

    bool set(UHardwareAlarmTimeReference time_reference,
             UHardwareAlarmSleepBehavior behavior,
             const struct timespec *ts)
    {
        if (not ts)
            return false;

        int type = 0;

        if (time_reference == U_HARDWARE_ALARM_TIME_REFERENCE_BOOT)
            switch(behavior)
            {
                case U_HARDWARE_ALARM_SLEEP_BEHAVIOR_WAKEUP_DEVICE:
                    type = ANDROID_ALARM_ELAPSED_REALTIME_WAKEUP_MASK;
                    break;
                case U_HARDWARE_ALARM_SLEEP_BEHAVIOR_KEEP_DEVICE_ASLEEP:
                    type = ANDROID_ALARM_ELAPSED_REALTIME_MASK;
                    break;
            }
        else if (time_reference == U_HARDWARE_ALARM_TIME_REFERENCE_RTC)
            switch(behavior)
            {
                case U_HARDWARE_ALARM_SLEEP_BEHAVIOR_WAKEUP_DEVICE:
                    type = ANDROID_ALARM_RTC_WAKEUP_MASK;
                    break;
                case U_HARDWARE_ALARM_SLEEP_BEHAVIOR_KEEP_DEVICE_ASLEEP:
                    type = ANDROID_ALARM_RTC_MASK;
                    break;
            }

        int result = ::ioctl(fd, ANDROID_ALARM_SET(type), ts);

        if (result < 0)
            fprintf(stderr, "Unable to set alarm: %s\n", strerror(errno));

        return not (result < 0);
    }

    bool get_elapsed_realtime(struct timespec* ts)
    {
        if (not ts)
            return false;

        int result = ::ioctl(
            fd,
            ANDROID_ALARM_GET_TIME(ANDROID_ALARM_ELAPSED_REALTIME),
            ts);

        return result == 0;
    }
};
}

class UbuntuHardwareAlarm
{
  public:
    static UbuntuHardwareAlarm& instance()
    {
        static UbuntuHardwareAlarm ha;
        return ha;
    }

    int wait_for()
    {
        return impl->wait_for();
    }

    bool set(UHardwareAlarmTimeReference time_reference,
             UHardwareAlarmSleepBehavior behavior,
             const struct timespec *ts)
    {
        return impl->set(time_reference, behavior, ts);
    }

    bool get_elapsed_realtime(struct timespec* ts)
    {
        return impl->get_elapsed_realtime(ts);
    }

    bool is_valid() const
    {
        return impl != nullptr;
    }

  private:
    UbuntuHardwareAlarm()
    {
        try
        {
            impl = new DevAlarmHardwareAlarm();
        } catch(const std::runtime_error& e)
        {
            fprintf(
                stderr, "%s: Error creating /dev/alarm-based implementation with: %s\n",
                __PRETTY_FUNCTION__,
                e.what());

            // TODO: Should we fallback to a timer-fd implementation here? I'm not
            // convinced that we should do so as a timer-fd wouldn't wakeup the device
            // from any sort of sleep mode.
        }
    }

    ~UbuntuHardwareAlarm()
    {
        delete impl;
    }

    HardwareAlarm* impl
    {
        nullptr
    };
};

UHardwareAlarm
u_hardware_alarm_create()
{
    auto result = &UbuntuHardwareAlarm::instance();

    if (result)
        if (result->is_valid())
            return result;

    return NULL;
}

void
u_hardware_alarm_ref(
    UHardwareAlarm alarm)
{
    // Considering a singleton pattern here, just voiding the argument.
    (void) alarm;
}

void
u_hardware_alarm_unref(
    UHardwareAlarm alarm)
{
    // Considering a singleton pattern here, just voiding the argument.
    (void) alarm;
}

UStatus
u_hardware_alarm_get_elapsed_real_time(
    UHardwareAlarm alarm,
    struct timespec* ts)
{
    return alarm->get_elapsed_realtime(ts) ? U_STATUS_SUCCESS : U_STATUS_ERROR;
}

UStatus
u_hardware_alarm_set_timezone(
    UHardwareAlarm alarm,
    const struct timezone *tz)
{
    int result = settimeofday(NULL, tz);

    if (result < 0)
        return U_STATUS_ERROR;

    return U_STATUS_SUCCESS;
}

UStatus
u_hardware_alarm_set_relative_to_with_behavior(
    UHardwareAlarm alarm,
    UHardwareAlarmTimeReference time_reference,
    UHardwareAlarmSleepBehavior behavior,
    const struct timespec *ts)
{
    return alarm->set(time_reference, behavior, ts) ?
            U_STATUS_SUCCESS :
            U_STATUS_ERROR;

}

UStatus
u_hardware_alarm_wait_for_next_alarm(
    UHardwareAlarm alarm,
    UHardwareAlarmWaitResult *result)
{
    int rc = alarm->wait_for();

    if (rc < 0)
        return U_STATUS_ERROR;

    if ((rc & ANDROID_ALARM_RTC_MASK) ||
        (rc & ANDROID_ALARM_RTC_WAKEUP_MASK))
        result->reference = U_HARDWARE_ALARM_TIME_REFERENCE_BOOT;
    else if ((rc & ANDROID_ALARM_ELAPSED_REALTIME_MASK) ||
             (rc & ANDROID_ALARM_ELAPSED_REALTIME_WAKEUP_MASK))
        result->reference = U_HARDWARE_ALARM_TIME_REFERENCE_RTC;

    if ((rc & ANDROID_ALARM_RTC_WAKEUP_MASK) ||
        (rc & ANDROID_ALARM_ELAPSED_REALTIME_WAKEUP_MASK))
        result->sleep_behavior = U_HARDWARE_ALARM_SLEEP_BEHAVIOR_WAKEUP_DEVICE;
    else
        result->sleep_behavior = U_HARDWARE_ALARM_SLEEP_BEHAVIOR_KEEP_DEVICE_ASLEEP;

    return U_STATUS_SUCCESS;
}
