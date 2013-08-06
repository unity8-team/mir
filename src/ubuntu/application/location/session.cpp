#include "ubuntu/application/location/session.h"

void
ua_location_service_session_ref(
    UALocationServiceSession *session)
{
    (void) session;
}

void
ua_location_service_session_unref(
    UALocationServiceSession *session)
{
    (void) session;
}

void
ua_location_service_session_set_position_updates_handler(
    UALocationServiceSession *session,
    UALocationServiceSessionPositionUpdatesHandler *handler)
{
    (void) session;
    (void) handler;
}

void
ua_location_service_session_set_heading_updates_handler(
    UALocationServiceSession *session,
    UALocationServiceSessionHeadingUpdatesHandler *handler)
{
    (void) session;
    (void) handler;
}

void
ua_location_service_session_set_velocity_updates_handler(
    UALocationServiceSession *session,
    UALocationServiceSessionVelocityUpdatesHandler *handler)
{
    (void) session;
    (void) handler;
}

UStatus
ua_location_service_session_start_position_updates(
    UALocationServiceSession *session)
{
    (void) session;
    return U_STATUS_ERROR;
}

void
ua_location_service_session_stop_position_updates(
    UALocationServiceSession *session)
{
    (void) session;
}

UStatus
ua_location_service_session_start_heading_updates(
    UALocationServiceSession *session)
{
    (void) session;
    return U_STATUS_ERROR;
}

void
ua_location_service_session_stop_heading_updates(
    UALocationServiceSession *session)
{
    (void) session;
}

UStatus
ua_location_service_session_start_velocity_updates(
    UALocationServiceSession *session)
{
    (void) session;
    return U_STATUS_ERROR;
}

void
ua_location_service_session_stop_velocity_updates(
    UALocationServiceSession *session)
{
    (void) session;
}
