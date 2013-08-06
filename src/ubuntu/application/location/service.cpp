#include "ubuntu/application/location/service.h"

UALocationServiceSession*
ua_location_service_create_session_for_low_accuracy(
    UALocationServiceRequirementsFlags /*flags*/)
{
    return 0;
}

UALocationServiceSession*
ua_location_service_create_session_for_high_accuracy(
    UALocationServiceRequirementsFlags /*flags*/)
{
    return 0;
}
