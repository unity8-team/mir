/*
 * Copyright © 2013 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Thomas Voß <thomas.voss@canonicalcom>
 */

#include <ubuntu/hardware/alarm.h>

#include <cstdio>
#include <cstdlib>
#include <ctime>

int main(int argc, char** argv)
{
    UHardwareAlarm alarm = u_hardware_alarm_create();

    if (!alarm)
    {
        printf("Error creating handle to hardware alarms.\n");
        return 1;
    }

    timespec ts { 0, 0 };
    clock_gettime(CLOCK_REALTIME, &ts);

    int timeout_in_seconds = 5;

    // Let's see if a timeout has been specified on the command line
    if (argc > 1)
    {
        timeout_in_seconds = atoi(argv[1]);
    }

    // Alarm in two seconds.
    ts.tv_sec += timeout_in_seconds;

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

    // And now we do the same with the last boot as reference
    u_hardware_alarm_get_elapsed_real_time(alarm, &ts);

    ts.tv_sec += timeout_in_seconds;

    rc = u_hardware_alarm_set_relative_to_with_behavior(
        alarm,
        U_HARDWARE_ALARM_TIME_REFERENCE_BOOT,
        U_HARDWARE_ALARM_SLEEP_BEHAVIOR_WAKEUP_DEVICE,
        &ts);

    if (rc != U_STATUS_SUCCESS)
    {
        printf("Problem setting hardware alarm.\n");
        return 1;
    }

    rc = u_hardware_alarm_wait_for_next_alarm(alarm, &wait_result);

    if (rc != U_STATUS_SUCCESS)
    {
        printf("Problem waiting for hardware alarm to go off.\n");
        return 1;
    }

    printf("Successfully created and waited for a hw alarm.\n");

    return 0;
}
