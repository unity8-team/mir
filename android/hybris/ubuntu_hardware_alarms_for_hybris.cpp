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

#include <ubuntu/hardware/alarm.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <errno.h>
#include <fcntl.h>

#include <linux/ioctl.h>
#include <linux/android_alarm.h>

#include <utils/Log.h>

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
        int result{-1};

        do
        {
            result = ::ioctl(fd, ANDROID_ALARM_WAIT);
        } while (result < 0 && errno == EINTR);

        if (result < 0)
            ALOGE("Waiting for hw alarm failed with: %s", strerror(errno));

        return result;
    }

    bool set(UHardwareAlarmTimeReference time_reference,
             UHardwareAlarmSleepBehavior behavior,
             const struct timespec *ts)
    {
        int type = 0;

        if (time_reference == U_HARDWARE_ALARM_TIME_REFERENCE_NOW)
            switch(behavior)
            {
                case U_HARDWARE_ALARM_SLEEP_BEHAVIOR_WAKEUP_DEVICE:
                    type = ANDROID_ALARM_ELAPSED_REALTIME_WAKEUP_MASK;
                    break;
                case U_HARDWARE_ALARM_SLEEP_BEHAVIOR_KEEP_DEVICE_ASLEEP:
                    type = ANDROID_ALARM_ELAPSED_REALTIME_MASK;
                    break;
            }
        else if (time_reference == U_HARDWARE_ALARM_TIME_REFERENCE_BOOT)
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
            ALOGE("Unable to set alarm.");

        return !(result < 0);
    }

    bool is_valid() const
    {
        return fd >= 0;
    }

  private:
    UbuntuHardwareAlarm() : fd(open("/dev/alarm", O_RDWR))
    {
    }

    ~UbuntuHardwareAlarm()
    {
        ::close(fd);
    }

    int fd;
};

UHardwareAlarm
u_hardware_alarm_create()
{
    if (UbuntuHardwareAlarm::instance().is_valid())
        return &UbuntuHardwareAlarm::instance();

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
        result->reference = U_HARDWARE_ALARM_TIME_REFERENCE_NOW;

    if ((rc & ANDROID_ALARM_RTC_WAKEUP_MASK) ||
        (rc & ANDROID_ALARM_ELAPSED_REALTIME_WAKEUP_MASK))
        result->sleep_behavior = U_HARDWARE_ALARM_SLEEP_BEHAVIOR_WAKEUP_DEVICE;
    else
        result->sleep_behavior = U_HARDWARE_ALARM_SLEEP_BEHAVIOR_KEEP_DEVICE_ASLEEP;

    return U_STATUS_SUCCESS;
}
