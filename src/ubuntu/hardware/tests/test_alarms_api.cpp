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

#include <ubuntu/hardware/alarm.h>

#include <cstdio>
#include <ctime>

int main(int argc, char** argv)
{
    UHardwareAlarm alarm = u_hardware_alarm_create();

    if (!alarm)
    {
        printf("Error creating handle to hardware alarms.\n");
        return 1;
    }

    // Alarm in two seconds.
    timespec ts { 2, 0 };


    UStatus rc = u_hardware_alarm_set_relative_to_with_behavior(
        alarm,
        U_HARDWARE_ALARM_TIME_REFERENCE_RTC,
        U_HARDWARE_ALARM_SLEEP_BEHAVIOR_WAKEUP_DEVICE,
        &ts);

    if (rc != U_STATUS_SUCCESS)
    {
        printf("Problem setting hardware alarm.\n");
        return 1;
    }

    UHardwareAlarmWaitResult wait_result;
    rc = u_hardware_alarm_wait_for_next_alarm(alarm, &wait_result);

    if (rc != U_STATUS_SUCCESS)
    {
        printf("Problem waiting for hardware alarm to go off.\n");
        return 1;
    }

    printf("Successfully created and waited for a hw alarm.\n");

    return 0;
}
