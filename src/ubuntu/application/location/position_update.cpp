#include "ubuntu/application/location/position_update.h"

void
ua_location_position_update_ref(
    UALocationPositionUpdate *update)
{
    (void) update;
}

void
ua_location_position_update_unref(
    UALocationPositionUpdate *update)
{
    (void) update;
}

uint64_t
ua_location_position_update_get_timestamp(
    UALocationPositionUpdate *update)
{
    (void) update;

    return 0;
}

double
ua_location_position_update_get_latitude_in_degree(
    UALocationPositionUpdate *update)
{
    (void) update;

    return 0;
}

double
ua_location_position_update_get_longitude_in_degree(
    UALocationPositionUpdate *update)
{
    (void) update;

    return 0;
}

bool
ua_location_position_update_has_altitude(
    UALocationPositionUpdate *update)
{
    (void) update;

    return false;
}

double
ua_location_position_update_get_altitude_in_meter(
    UALocationPositionUpdate *update)
{
    (void) update;

    return 0;
}
