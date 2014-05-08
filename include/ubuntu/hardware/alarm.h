/*
 * Copyright © 2014 Canonical Ltd.
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
#ifndef UBUNTU_HARDWARE_ALARM_H_
#define UBUNTU_HARDWARE_ALARM_H_

#include <ubuntu/status.h>
#include <ubuntu/visibility.h>

#ifdef __cplusplus
extern "C" {
#endif

/** The time reference that alarms are setup to. */
typedef enum
{
    U_HARDWARE_ALARM_TIME_REFERENCE_BOOT, /**< Relative to the device's boot time, including sleep. */
    U_HARDWARE_ALARM_TIME_REFERENCE_RTC /**< Wall clock time in UTC. */
} UbuntuHardwareAlarmTimeReference;

typedef UbuntuHardwareAlarmTimeReference UHardwareAlarmTimeReference;

/** Describes if an alarm is able to wakup the device from sleep. */
typedef enum
{
    /** Alarm will wakeup the device from sleep. */
    U_HARDWARE_ALARM_SLEEP_BEHAVIOR_WAKEUP_DEVICE,
    /** Alarm will not wakeup the device and will be delivered on the next wakeup of the device */
    U_HARDWARE_ALARM_SLEEP_BEHAVIOR_KEEP_DEVICE_ASLEEP
} UbuntuHardwareAlarmSleepBehavior;

typedef UbuntuHardwareAlarmSleepBehavior UHardwareAlarmSleepBehavior;

/** Bundles the time reference and sleep/wakeup behavior when waiting for an alarm to happen. */
typedef struct
{
  UHardwareAlarmTimeReference reference;
  UHardwareAlarmSleepBehavior sleep_behavior;
} UbuntuHardwareAlarmWaitResult;

typedef UbuntuHardwareAlarmWaitResult UHardwareAlarmWaitResult;

/** Opaque type modelling access to the kernel/hw-level alarm capabilities. */
typedef struct UbuntuHardwareAlarm* UHardwareAlarm;

/** Creates an instance and/or increments its refcount. */
UBUNTU_DLL_PUBLIC UHardwareAlarm
u_hardware_alarm_create();

/** Increments the instance's ref count. */
UBUNTU_DLL_PUBLIC void
u_hardware_alarm_ref(
    UHardwareAlarm alarm);

/** Decrements the instance's ref count. */
UBUNTU_DLL_PUBLIC void
u_hardware_alarm_unref(
    UHardwareAlarm alarm);

/** Query the time that elapsed since boot, including deep sleeps. */
UBUNTU_DLL_PUBLIC UStatus
u_hardware_alarm_get_elapsed_real_time(
    UHardwareAlarm alarm,
    struct timespec *tz);

/** Reports a timezone change to kernel and HW. */
UBUNTU_DLL_PUBLIC UStatus
u_hardware_alarm_set_timezone(
    UHardwareAlarm alarm,
    const struct timezone *tz);

/** Sets and arms a timer. */
UBUNTU_DLL_PUBLIC UStatus
u_hardware_alarm_set_relative_to_with_behavior(
    UHardwareAlarm alarm,
    UHardwareAlarmTimeReference time_reference,
    UHardwareAlarmSleepBehavior behavior,
    const struct timespec *ts);

/** Blocks until the next alarm occurs. */
UBUNTU_DLL_PUBLIC UStatus
u_hardware_alarm_wait_for_next_alarm(
    UHardwareAlarm alarm,
    UHardwareAlarmWaitResult *result);

#ifdef __cplusplus
}
#endif

#endif
