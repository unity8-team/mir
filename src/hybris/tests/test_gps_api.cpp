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
 * Authored by: Daniel d'Andrada <daniel.dandrada@canonical.com>
 */
#include <ubuntu/application/ubuntu_application_gps.h>

#include <ctime>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

class GPSTest
{
  public:
    GPSTest();
    ~GPSTest();
    bool init_and_start();
    bool stop();
    void inject_time();

    UbuntuGps ubuntu_gps;
};

void gps_location_cb(UbuntuGpsLocation* location, void* context)
{
    printf("gps_location_cb() called.\n");
}

void gps_status_cb(uint16_t status, void* context)
{
    switch(status)
    {
    case UBUNTU_GPS_STATUS_NONE:
        printf("status: None\n");
        break;
    case UBUNTU_GPS_STATUS_SESSION_BEGIN:
        printf("status: Session Begin\n");
        break;
    case UBUNTU_GPS_STATUS_SESSION_END:
        printf("status: Session End\n");
        break;
    case UBUNTU_GPS_STATUS_ENGINE_ON:
        printf("status: Engine On\n");
        break;
    case UBUNTU_GPS_STATUS_ENGINE_OFF:
        printf("status: Engine Off\n");
    default:
        break;
    };
}

void gps_sb_status_cb(UbuntuGpsSvStatus* sv_info, void* context)
{
    printf("gps_sb_status_cb() called, listing %d space vehicles\n", sv_info->num_svs);
}

void gps_nmea_cb(int64_t timestamp, const char* nmea, int length, void* context)
{
    char str[length+1];
    memcpy(str, nmea, length);
    str[length] = 0;
    printf("gps_nmea_cb() - %s\n", str);
}

void gps_set_cabapilities_cb(uint32_t capabilities, void* context)
{
    printf("gps_set_cabapilities_cb() -");

    if (capabilities & UBUNTU_GPS_CAPABILITY_SCHEDULING)
        printf(" scheduling");
    if (capabilities & UBUNTU_GPS_CAPABILITY_MSB)
        printf(" MSB");
    if (capabilities & UBUNTU_GPS_CAPABILITY_MSA)
        printf(" MSA");
    if (capabilities & UBUNTU_GPS_CAPABILITY_SINGLE_SHOT)
        printf(" 'single shot'");
    if (capabilities & UBUNTU_GPS_CAPABILITY_ON_DEMAND_TIME)
        printf(" 'on demand time'");

    printf("\n");
}

void gps_request_utc_time_cb(void* context)
{
    printf("gps_request_utc_time_cb() called.\n");
    ((GPSTest*)context)->inject_time();
}

void gps_xtra_download_request_cb(void* context)
{
    printf("gps_xtra_download_request_cb() called.\n");
}

void agps_status_cb(UbuntuAgpsStatus* status, void* context)
{
    printf("agps status -");

    if (status->type == UBUNTU_AGPS_TYPE_SUPL)
        printf(" SUPL");
    else
        printf(" C2K");

    switch (status->status)
    {
    case UBUNTU_GPS_REQUEST_AGPS_DATA_CONN:
        printf(", request AGPS data connection");
        break;
    case UBUNTU_GPS_RELEASE_AGPS_DATA_CONN:
        printf(", release AGPS data connection");
        break;
    case UBUNTU_GPS_AGPS_DATA_CONNECTED:
        printf(", request AGPS data connected");
        break;
    case UBUNTU_GPS_AGPS_DATA_CONN_DONE:
        printf(", AGPS data connection done");
        break;
    default:
    case UBUNTU_GPS_AGPS_DATA_CONN_FAILED:
        printf(", AGPS data connection failed");
        break;
    }

    printf(" ipaddr=%u\n", status->ipaddr);
}

void gps_notify_cb(UbuntuGpsNiNotification *notification, void* context)
{
    printf("gps_notify_cb() called.\n");
}

void agps_ril_request_set_id_cb(uint32_t flags, void* context)
{
    printf("agps_ril_request_set_id_cb() called.\n");
}

void agps_ril_request_refloc_cb(uint32_t flags, void* context)
{
    printf("agps_ril_request_refloc_cb() called.\n");
}

GPSTest::GPSTest()
    : ubuntu_gps(NULL)
{
}

GPSTest::~GPSTest()
{
    if (ubuntu_gps)
        ubuntu_gps_delete(ubuntu_gps);
}

void GPSTest::inject_time()
{
    // A real implementation would inject time from some NTP server.
    time_t t = time(0);
    int64_t time_millis = (int64_t)t * (int64_t)1000;
    ubuntu_gps_inject_time(ubuntu_gps,
                           time_millis /*NTP time would go here*/,
                           time_millis /*internal time when that NTP time was taken*/,
                           10 /* possible deviation, in milliseconds*/);
}

bool GPSTest::init_and_start()
{
    UbuntuGpsParams gps_params;

    gps_params.location_cb = gps_location_cb;
    gps_params.status_cb = gps_status_cb;
    gps_params.sv_status_cb = gps_sb_status_cb;
    gps_params.nmea_cb = gps_nmea_cb;
    gps_params.set_capabilities_cb = gps_set_cabapilities_cb;
    gps_params.request_utc_time_cb = gps_request_utc_time_cb;
    gps_params.xtra_download_request_cb = gps_xtra_download_request_cb;
    gps_params.agps_status_cb = agps_status_cb;
    gps_params.gps_ni_notify_cb = gps_notify_cb;
    gps_params.request_setid_cb = agps_ril_request_set_id_cb;
    gps_params.request_refloc_cb = agps_ril_request_refloc_cb;
    gps_params.context = this;

    UbuntuGps ubuntu_gps = ubuntu_gps_new(&gps_params);
    if (!ubuntu_gps)
    {
        printf("GPS creation failed!\n");
        return false;
    }

    bool ok = ubuntu_gps_start(ubuntu_gps);
    if (!ok)
    {
        printf("GPS start up failed!\n");
        return false;
    }

    return true;
}

bool GPSTest::stop()
{
    bool ok = ubuntu_gps_stop(ubuntu_gps);
    if (!ok)
        printf("failed when stopping GPS!\n");

    return ok;
}

void wait_for_sigint()
{
    sigset_t signals;
    sigemptyset(&signals);
    sigaddset(&signals, SIGINT);

    int sig;
    int result = sigwait(&signals, &sig);
    if (result != 0)
        printf("sigwait failed!\n");
}

int main(int argc, char** argv)
{
    int return_value = 0;
    GPSTest test;

    if (!test.init_and_start())
        return 1;

    printf("GPS initialized and started. Now waiting for callbacks or SIGINT (to quit).\n");
    wait_for_sigint();
    printf("Exiting...\n");

    if (!test.stop())
        return 1;

    printf("GPS stopped.\n");
    return 0;
}
