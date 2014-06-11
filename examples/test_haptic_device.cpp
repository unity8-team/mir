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
 * Authored by: Ricardo Mendoza <ricardo.mendoza@canonical.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <ubuntu/application/sensors/haptic.h>

int main(int argc, char *argv[])
{
    UASensorsHaptic *sensor = ua_sensors_haptic_new();

    if (!sensor) {
        printf("Haptic device unavailable\n");
        return 1;
    }

    printf("Vibrating once for 1500ms\n");
    ua_sensors_haptic_vibrate_once(sensor, 1500);

    sleep(3);

    printf("Vibrating with pattern 6*1500, repeat twice.\n");
    uint32_t pattern[MAX_PATTERN_SIZE] = {1500, 1500, 1500, 1500, 1500, 1500};
    ua_sensors_haptic_vibrate_with_pattern(sensor, pattern, 2);

    return 0;
}
