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
 * Authored by: Daniel d'Andrada <daniel.dandrada@canonical.com>
 * Authored by: Thomas Voß <thomas.voss@canonical.com>
 */
#ifndef UBUNTU_HARDWARE_GPS_H_
#define UBUNTU_HARDWARE_GPS_H_

#include <ubuntu/visibility.h>

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** \defgroup gps_access Functions and types for accessing GPS H/W. */

/**
 * Maximum number of SVs for u_hardware_gps_sv_status_callback().
 * \ingroup gps_access
 */
#define U_HARDWARE_GPS_MAX_SVS 32

/** 
 * The status of the GPS hardware.
 * \ingroup gps_access
 */
enum
{
    /** GPS status unknown. */
    U_HARDWARE_GPS_STATUS_NONE = 0,
    /** GPS has begun navigating. */
    U_HARDWARE_GPS_STATUS_SESSION_BEGIN = 1,
    /** GPS has stopped navigating. */
    U_HARDWARE_GPS_STATUS_SESSION_END = 2,
    /** GPS has powered on but is not navigating. */
    U_HARDWARE_GPS_STATUS_ENGINE_ON = 3,
    /** GPS is powered off. */
    U_HARDWARE_GPS_STATUS_ENGINE_OFF = 4
};

/**
 * Flags for the gps_set_capabilities callback.
 * \ingroup gps_access
 * GPS HAL schedules fixes for U_HARDWARE_GPS_POSITION_RECURRENCE_PERIODIC mode.
 * If this is not set, then the framework will use 1000ms for min_interval
 * and will start and call start() and stop() to schedule the GPS.
 */
#define U_HARDWARE_GPS_CAPABILITY_SCHEDULING       0x0000001
/** GPS supports MS-Based AGPS mode */
#define U_HARDWARE_GPS_CAPABILITY_MSB              0x0000002
/** GPS supports MS-Assisted AGPS mode */
#define U_HARDWARE_GPS_CAPABILITY_MSA              0x0000004
/** GPS supports single-shot fixes */
#define U_HARDWARE_GPS_CAPABILITY_SINGLE_SHOT      0x0000008
/** GPS supports on demand time injection */
#define U_HARDWARE_GPS_CAPABILITY_ON_DEMAND_TIME   0x0000010

/**
 * UHardwareGpsNiNotifyFlags constants
 * \ingroup gps_access
 */
typedef uint32_t UHardwareGpsNiNotifyFlags;
/** NI requires notification */
#define U_HARDWARE_GPS_NI_NEED_NOTIFY          0x0001
/** NI requires verification */
#define U_HARDWARE_GPS_NI_NEED_VERIFY          0x0002
/** NI requires privacy override, no notification/minimal trace */
#define U_HARDWARE_GPS_NI_PRIVACY_OVERRIDE     0x0004

/**
 * GPS NI responses, used to define the response in
 * NI structures
 * \ingroup gps_access
 */
typedef int UHardwareGpsUserResponseType;

enum
{
    U_HARDWARE_GPS_NI_RESPONSE_ACCEPT = 1,
    U_HARDWARE_GPS_NI_RESPONSE_DENY = 2,
    U_HARDWARE_GPS_NI_RESPONSE_NORESP = 3
};

enum
{
    U_HARDWARE_GPS_NI_TYPE_VOICE = 1,
    U_HARDWARE_GPS_NI_TYPE_UMTS_SUPL = 2,
    U_HARDWARE_GPS_NI_TYPE_UMTS_CTRL_PLANE = 3
};

/**
 * String length constants
 * \ingroup gps_access
 */
#define U_HARDWARE_GPS_NI_SHORT_STRING_MAXLEN      256
#define U_HARDWARE_GPS_NI_LONG_STRING_MAXLEN       2048

/**
 * NI data encoding scheme
 * \ingroup gps_access
 */
typedef int UHardwareGpsNiEncodingType;

/**
 * Known encoding types for Ni responses
 * \ingroup gps_access
 */
enum
{
    U_HARDWARE_GPS_ENC_NONE = 0,
    U_HARDWARE_GPS_ENC_SUPL_GSM_DEFAULT = 1,
    U_HARDWARE_GPS_ENC_SUPL_UTF8 = 2,
    U_HARDWARE_GPS_ENC_SUPL_UCS2 = 3,
    U_HARDWARE_GPS_ENC_UNKNOWN   = -1
};

/**
 * Known AGPS types
 * \ingroup gps_access
 */
enum
{
    U_HARDWARE_GPS_AGPS_TYPE_SUPL = 1,
    U_HARDWARE_GPS_AGPS_TYPE_C2K = 2
};

/**
 * Known positioning modes
 * \ingroup gps_access
 */
enum
{
    /** Mode for running GPS standalone (no assistance). */
    U_HARDWARE_GPS_POSITION_MODE_STANDALONE = 0,
    /** AGPS MS-Based mode. */
    U_HARDWARE_GPS_POSITION_MODE_MS_BASED = 1,
    /** AGPS MS-Assisted mode. */
    U_HARDWARE_GPS_POSITION_MODE_MS_ASSISTED = 2
};

/**
 * Known positioning modes
 * \ingroup gps_access
 */
enum
{
    /** Receive GPS fixes on a recurring basis at a specified period. */
    U_HARDWARE_GPS_POSITION_RECURRENCE_PERIODIC = 0,
    /** Request a single shot GPS fix. */
    U_HARDWARE_GPS_POSITION_RECURRENCE_SINGLE = 1
};

enum
{
    /** GPS requests data connection for AGPS. */
    U_HARDWARE_GPS_REQUEST_AGPS_DATA_CONN = 1,
    /** GPS releases the AGPS data connection. */
    U_HARDWARE_GPS_RELEASE_AGPS_DATA_CONN = 2,
    /** AGPS data connection initiated */
    U_HARDWARE_GPS_AGPS_DATA_CONNECTED = 3,
    /** AGPS data connection completed */
    U_HARDWARE_GPS_AGPS_DATA_CONN_DONE = 4,
    /** AGPS data connection failed */
    U_HARDWARE_GPS_AGPS_DATA_CONN_FAILED = 5
};

/** UHardwareGpsLocation has valid latitude and longitude. */
#define U_HARDWARE_GPS_LOCATION_HAS_LAT_LONG   0x0001
/** UHardwareGpsLocation has valid altitude. */
#define U_HARDWARE_GPS_LOCATION_HAS_ALTITUDE   0x0002
/** UHardwareGpsLocation has valid speed. */
#define U_HARDWARE_GPS_LOCATION_HAS_SPEED      0x0004
/** UHardwareGpsLocation has valid bearing. */
#define U_HARDWARE_GPS_LOCATION_HAS_BEARING    0x0008
/** UHardwareGpsLocation has valid accuracy. */
#define U_HARDWARE_GPS_LOCATION_HAS_ACCURACY   0x0010

typedef struct UHardwareGps_* UHardwareGps;

/**
 * Models a location as reported by the GPS HAL.
 * \ingroup gps_access
 */
typedef struct
{
    /** set to sizeof(UHardwareGpsLocation) */
    size_t size;
    /** Contains U_HARDWARE_GPS_LOCATION_* flags bits. */
    uint16_t flags;
    /** Represents latitude in degrees. */
    double latitude;
    /** Represents longitude in degrees. */
    double longitude;
    /** Represents altitude in meters above the WGS 84 reference
     * ellipsoid. */
    double altitude;
    /** Represents speed in meters per second. */
    float speed;
    /** Represents heading in degrees. */
    float bearing;
    /** Represents expected accuracy in meters. */
    float accuracy;
    /** Timestamp for the location fix, in milliseconds since January 1, 1970 */
    int64_t timestamp;
} UHardwareGpsLocation;

/**
 * Represents space vehicle (satellite) information.
 * \ingroup gps_access
 */
typedef struct {
    /** set to sizeof(UHardwareGpsSvInfo) */
    size_t size;
    /** Pseudo-random number for the SV. */
    int prn;
    /** Signal to noise ratio. */
    float snr;
    /** Elevation of SV in degrees. */
    float elevation;
    /** Azimuth of SV in degrees. */
    float azimuth;
} UHardwareGpsSvInfo;

/**
 * Represents SV (Space Vehicle) status.
 * \ingroup gps_access
 */
typedef struct {
    /** set to sizeof(GpsSvStatus) */
    size_t size;

    /** Number of SVs currently visible. */
    int num_svs;

    /** Contains an array of SV information. */
    UHardwareGpsSvInfo sv_list[U_HARDWARE_GPS_MAX_SVS];

    /** Represents a bit mask indicating which SVs
     * have ephemeris data.
     */
    uint32_t ephemeris_mask;

    /** Represents a bit mask indicating which SVs
     * have almanac data.
     */
    uint32_t almanac_mask;

    /**
     * Represents a bit mask indicating which SVs
     * were used for computing the most recent position fix.
     */
    uint32_t used_in_fix_mask;
} UHardwareGpsSvStatus;

/**
 * Represents the status of AGPS.
 * \ingroup gps_access
 */
typedef struct {
    /** set to sizeof(UHardwareGpsAGpsStatus) */
    size_t size;

    uint16_t type;
    uint16_t status;
    uint32_t ipaddr;
} UHardwareGpsAGpsStatus;

/**
 * Represents an NI request
 * \ingroup gps_access
 */
typedef struct {
    /** set to sizeof(UHardwareGpsNiNotification) */
    size_t size;

    /**
     * An ID generated by HAL to associate NI notifications and UI
     * responses
     */
    int notification_id;

    /**
     * An NI type used to distinguish different categories of NI
     * events, such as U_HARDWARE_GPS_NI_TYPE_VOICE, U_HARDWARE_GPS_NI_TYPE_UMTS_SUPL, ...
     */
    uint32_t ni_type;

    /**
     * Notification/verification options, combinations of UHardwareGpsNiNotifyFlags constants
     */
    UHardwareGpsNiNotifyFlags notify_flags;

    /**
     * Timeout period to wait for user response.
     * Set to 0 for no time out limit.
     */
    int timeout;

    /**
     * Default response when time out.
     */
    UHardwareGpsUserResponseType default_response;

    /**
     * Requestor ID
     */
    char requestor_id[U_HARDWARE_GPS_NI_SHORT_STRING_MAXLEN];

    /**
     * Notification message. It can also be used to store client_id in some cases
     */
    char text[U_HARDWARE_GPS_NI_LONG_STRING_MAXLEN];

    /**
     * Client name decoding scheme
     */
    UHardwareGpsNiEncodingType requestor_id_encoding;

    /**
     * Client name decoding scheme
     */
    UHardwareGpsNiEncodingType text_encoding;

    /**
     * A pointer to extra data. Format:
     * key_1 = value_1
     * key_2 = value_2
     */
    char extras[U_HARDWARE_GPS_NI_LONG_STRING_MAXLEN];

} UHardwareGpsNiNotification;

typedef void (*UHardwareGpsLocationCallback)(UHardwareGpsLocation *location, void *context);
typedef void (*UHardwareGpsStatusCallback)(uint16_t status, void *context);
typedef void (*UHardwareGpsSvStatusCallback)(UHardwareGpsSvStatus *sv_info, void *context);
typedef void (*UHardwareGpsNmeaCallback)(int64_t timestamp, const char *nmea, int length, void *context);
typedef void (*UHardwareGpsSetCapabilities)(uint32_t capabilities, void *context);
typedef void (*UHardwareGpsRequestUtcTime)(void *context);

/** Callback to request the client to download XTRA data.
 *  The client should download XTRA data and inject it by calling inject_xtra_data().
 */
typedef void (*UHardwareGpsXtraDownloadRequest)(void *context);

/** Callback with AGPS status information.
 */
typedef void (*UHardwareGpsAGpsStatusCallback)(UHardwareGpsAGpsStatus *status, void *context);

/** Callback with NI notification.
 */
typedef void (*UHardwareGpsNiNotifyCallback)(UHardwareGpsNiNotification *notification, void *context);

/*
 Callback for AGPS RIL (Radio Interface Library) set id
*/
typedef void (*UHardwareGpsAGpsRilRequestSetId)(uint32_t flags, void *context);
typedef void (*UHardwareGpsAGpsRilRequestRefLoc)(uint32_t flags, void *context);

typedef struct
{

    UHardwareGpsLocationCallback location_cb;
    UHardwareGpsStatusCallback status_cb;
    UHardwareGpsSvStatusCallback sv_status_cb;
    UHardwareGpsNmeaCallback nmea_cb;
    UHardwareGpsSetCapabilities set_capabilities_cb;
    UHardwareGpsRequestUtcTime request_utc_time_cb;

    UHardwareGpsXtraDownloadRequest xtra_download_request_cb;

    UHardwareGpsAGpsStatusCallback agps_status_cb;

    UHardwareGpsNiNotifyCallback gps_ni_notify_cb;

    UHardwareGpsAGpsRilRequestSetId request_setid_cb;
    UHardwareGpsAGpsRilRequestRefLoc request_refloc_cb;

    void* context;
} UHardwareGpsParams;

/*
 You must create only one instance per process/application.
*/
UBUNTU_DLL_PUBLIC UHardwareGps
u_hardware_gps_new(UHardwareGpsParams *params);

UBUNTU_DLL_PUBLIC void
u_hardware_gps_delete(UHardwareGps handle);

UBUNTU_DLL_PUBLIC bool
u_hardware_gps_start(UHardwareGps self);

UBUNTU_DLL_PUBLIC bool
u_hardware_gps_stop(UHardwareGps self);

/*
    \param time NTP time, in milliseconds since Jan 1st 1970.
    \param time_reference time from the internal clock at the moment that NTP time was taken.
    \param uncertainty possible deviation in the time supplied (uncertainty) in milliseconds.
 */
UBUNTU_DLL_PUBLIC void
u_hardware_gps_inject_time(
    UHardwareGps self,
    int64_t time,
    int64_t time_reference,
    int uncertainty);

UBUNTU_DLL_PUBLIC void
u_hardware_gps_inject_location(
    UHardwareGps self,
    double latitude,
    double longitude,
    float accuracy);

UBUNTU_DLL_PUBLIC void
u_hardware_gps_delete_aiding_data(
    UHardwareGps self,
    uint16_t flags);

/*
    \param mode One of the U_HARDWARE_GPS_POSITION_MODE_* values
    \param recurrence One of the U_HARDWARE_GPS_POSITION_RECURRENCE_* values
    \param min_interval represents the time between fixes in milliseconds.
    \param preferred_accuracy The requested fix accuracy in meters. Can be zero.
    \param preferred_time The requested time to first fix in milliseconds. Can be zero.
 */
UBUNTU_DLL_PUBLIC bool
u_hardware_gps_set_position_mode(
    UHardwareGps self,
    uint32_t mode,
    uint32_t recurrence,
    uint32_t min_interval,
    uint32_t preferred_accuracy,
    uint32_t preferred_time);

UBUNTU_DLL_PUBLIC void
u_hardware_gps_inject_xtra_data(
    UHardwareGps self,
    char* data,
    int length);

#ifdef __cplusplus
}
#endif

#endif // UBUNTU_HARDWARE_GPS_H_
