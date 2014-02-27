/*
 * Copyright (C) 2012 Canonical Ltd
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
 * Authored by: Thomas Voss <thomas.voss@canonical.com>
 *              Ricardo Mendoza <ricardo.mendoza@canonical.com>
 */

// C APIs
#include <ubuntu/hardware/alarm.h>
#include <ubuntu/hardware/gps.h>

#include "bridge.h"

// Hardware - GPS
IMPLEMENT_FUNCTION1(
UHardwareGps, 
u_hardware_gps_new, 
UHardwareGpsParams*);

IMPLEMENT_VOID_FUNCTION1(
u_hardware_gps_delete, 
UHardwareGps);

IMPLEMENT_FUNCTION1(
bool, 
u_hardware_gps_start, 
UHardwareGps);

IMPLEMENT_FUNCTION1(
bool, 
u_hardware_gps_stop, 
UHardwareGps);

IMPLEMENT_VOID_FUNCTION4(
u_hardware_gps_inject_time, 
UHardwareGps, 
int64_t, 
int64_t, 
int);

IMPLEMENT_VOID_FUNCTION4(
u_hardware_gps_inject_location, 
UHardwareGps, 
double, 
double, 
float);

IMPLEMENT_VOID_FUNCTION2(
u_hardware_gps_delete_aiding_data, 
UHardwareGps, 
uint16_t);

IMPLEMENT_FUNCTION6(
bool, 
u_hardware_gps_set_position_mode, 
UHardwareGps, 
uint32_t, 
uint32_t,
uint32_t, 
uint32_t, 
uint32_t);

IMPLEMENT_VOID_FUNCTION3(
u_hardware_gps_inject_xtra_data, 
UHardwareGps, 
char*, 
int);

/****************** HW ALARMS API ******************/

IMPLEMENT_CTOR0(
        UHardwareAlarm,
        u_hardware_alarm_create);

IMPLEMENT_VOID_FUNCTION1(
        u_hardware_alarm_ref,
        UHardwareAlarm);

IMPLEMENT_VOID_FUNCTION1(
        u_hardware_alarm_unref,
        UHardwareAlarm);

IMPLEMENT_FUNCTION2(
        UStatus,
        u_hardware_alarm_set_timezone,
        UHardwareAlarm,
        const struct timezone*);

IMPLEMENT_VOID_FUNCTION4(
        u_hardware_alarm_set_relative_to_with_behavior,
        UHardwareAlarm,
        UHardwareAlarmTimeReference,
        UHardwareAlarmSleepBehavior,
        const struct timespec*);

IMPLEMENT_FUNCTION1(
        UHardwareAlarmWaitResult,
        u_hardware_alarm_wait_for_next_alarm,
        UHardwareAlarm);
