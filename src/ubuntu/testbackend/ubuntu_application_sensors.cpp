/*
 * Copyright (C) 2013 Canonical Ltd
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
 * Authored by: Martin Pitt <martin.pitti@ubuntu.com>
 */

#include <ubuntu/application/sensors/ubuntu_application_sensors.h>
#include <ubuntu/application/sensors/accelerometer.h>
#include <ubuntu/application/sensors/proximity.h>
#include <ubuntu/application/sensors/light.h>

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <csignal>
#include <iostream>
#include <sstream>
#include <fstream>
#include <stdexcept>
#include <chrono>
#include <map>

using namespace std;

/***************************************
 *
 * test sensor implementation
 *
 ***************************************/

// this is only internal API, so we make everything public
struct TestSensor
{
    TestSensor(ubuntu_sensor_type _type, float _min_value, float _max_value, float _resolution) :
        type(_type),
        enabled(false),
        resolution(_resolution),
        min_delay(0),
        min_value(_min_value),
        max_value(_max_value),
        on_event_cb(NULL),
        event_cb_context(NULL),
        x(_min_value),
        y(_min_value),
        z(_min_value),
        distance((UASProximityDistance) 0),  // LP#1256969
        timestamp(0)
    {}

    ubuntu_sensor_type type;
    bool enabled;
    float resolution;
    uint32_t min_delay;
    float min_value, max_value;
    void (*on_event_cb)(void*, void*);
    void* event_cb_context;

    /* current value; note that we do not track separate Event objects/pointers
     * at all, and just always deliver the current value */
    float x, y, z;
    UASProximityDistance distance;
    uint64_t timestamp;
};

/* Singleton which reads the sensor data file and maintains the TestSensor
 * instances */
class SensorController
{
  public:
    // Ensure that controller is initialized, and return singleton
    static SensorController& instance()
    {
        static SensorController _inst;
        return _inst;
    }

    // Return TestSensor of given type, or NULL if it doesn't exist
    TestSensor* get(ubuntu_sensor_type type)
    {
        try {
            return sensors.at(type);
        } catch (const out_of_range&) {
            return NULL;
        }
    }

  private:
    SensorController();
    bool next_command();
    bool process_create_command();
    void process_event_command();
    void setup_timer(unsigned delay_ms);
    static void on_timer(union sigval sval);

    static ubuntu_sensor_type type_from_name(const string& type)
    {
        if (type == "light")
            return ubuntu_sensor_type_light;
        if (type == "proximity")
            return ubuntu_sensor_type_proximity;
        if (type == "accel")
            return ubuntu_sensor_type_accelerometer;

        cerr << "TestSensor ERROR: unknown sensor type " << type << endl;
        abort();
    }

    static map<ubuntu_sensor_type, TestSensor*> sensors;
    ifstream data;

    // current command/event
    string current_command;
    TestSensor* event_sensor;
    float event_x, event_y, event_z;
    UASProximityDistance event_distance;
};

map<ubuntu_sensor_type, TestSensor*> SensorController::sensors;

SensorController::SensorController()
{
    const char* path = getenv("UBUNTU_PLATFORM_API_SENSOR_TEST");

    if (path == NULL) {
        cerr << "TestSensor ERROR: Need $UBUNTU_PLATFORM_API_SENSOR_TEST to point to a data file\n";
        abort();
    }

    //cout << "SensorController ctor: opening " << path << endl;

    data.open(path);
    if (!data.is_open()) {
        cerr << "TestSensor ERROR: Failed to open data file " << path << ": " << strerror(errno) << endl;
        abort();
    }

    // process all "create" commands
    while (next_command()) {
        if (!process_create_command())
            break;
    }

    // start event processing
    if (!data.eof())
        process_event_command();
}

bool
SensorController::next_command()
{
    while (getline(data, current_command)) {
        // trim leading and trailing space
        current_command.erase(0, current_command.find_first_not_of(" \t"));
        current_command.erase(current_command.find_last_not_of(" \t") + 1);
        // ignore empty or comment lines
        if (current_command.size() == 0 || current_command[0] == '#')
            continue;
        return true;
    }
    return false;
}

bool
SensorController::process_create_command()
{
    stringstream ss(current_command, ios_base::in);
    string token;

    // we only process "create" commands here; if we have something else, stop
    ss >> token;
    if (token != "create")
        return false;

    ss >> token;
    ubuntu_sensor_type type = type_from_name(token);

    if (get(type) != NULL) {
        cerr << "TestSensor ERROR: duplicate creation of sensor type " << token << endl;
        abort();
    }

    float min = 0, max = 0, resolution = 0;

    if (type != ubuntu_sensor_type_proximity) {
        // read min, max, resolution
        ss >> min >> max >> resolution;

        if (max <= min) {
            cerr << "TestSensor ERROR: max_value must be >= min_value in  " << current_command << endl;
            abort();
        }
        if (resolution <= 0) {
            cerr << "TestSensor ERROR: resolution must be > 0 in " << current_command << endl;
            abort();
        }
    }

    //cout << "SensorController::process_create_command: type " << type << " min " << min << " max " << max << " res " << resolution << endl;

    sensors[type] = new TestSensor(type, min, max, resolution);
    return true;
}

void
SensorController::process_event_command()
{
    stringstream ss(current_command, ios_base::in);
    int delay;

    //cout << "TestSensor: processing event " << current_command << endl;

    // parse delay
    ss >> delay;
    if (delay <= 0) {
        cerr << "TestSensor ERROR: delay must be positive in command " << current_command << endl;
        abort();
    }

    // parse sensor type
    string token;
    ss >> token;
    ubuntu_sensor_type type = type_from_name(token);
    event_sensor = get(type);
    if (event_sensor == NULL) {
        cerr << "TestSensor ERROR: sensor does not exist, you need to create it: " << token << endl;
        abort();
    }

    switch (type) {
        case ubuntu_sensor_type_light:
            ss >> event_x;
            //cout << "got event: sensor type " << type << " (light), delay "
            //     << delay << " ms, value " << event_x << endl;
            break;

        case ubuntu_sensor_type_accelerometer:
            ss >> event_x >> event_y >> event_z;
            //cout << "got event: sensor type " << type << " (accel), delay "
            //     << delay << " ms, value " << event_x << "/" << event_y << "/" << event_z << endl;
            break;

        case ubuntu_sensor_type_proximity:
            ss >> token;
            if (token == "unknown")
                event_distance = (UASProximityDistance) 0;  // LP#1256969
            else if (token == "near")
                event_distance = U_PROXIMITY_NEAR;
            else if (token == "far")
                event_distance = U_PROXIMITY_FAR;
            else {
                cerr << "TestSensor ERROR: unknown proximity value " << token << endl;
                abort();
            }
            //cout << "got event: sensor type " << type << " (proximity), delay "
            //     << delay << " ms, value " << int(event_distance) << endl;
            break;

        default:
            cerr << "TestSensor ERROR: unhandled sensor type " << token << endl;
            abort();
    }

    // wake up after given delay for committing the change and processing the
    // next event
    setup_timer(unsigned(delay));
}

void
SensorController::setup_timer(unsigned delay_ms)
{
    static timer_t timerid; // we keep a pointer to that until on_timer
    struct sigevent sev;
    struct itimerspec its { {0, 0}, // interval
                            {(delay_ms / 1000), (delay_ms * 1000000L) % 1000000000L } };

    sev.sigev_notify = SIGEV_THREAD;
    sev.sigev_notify_function = SensorController::on_timer;
    sev.sigev_notify_attributes = NULL;
    sev.sigev_value.sival_ptr = &timerid;

    if (timer_create(CLOCK_MONOTONIC, &sev, &timerid) < 0) {
        perror("TestSensor ERROR: Failed to create timer");
        abort();
    }
    if (timer_settime(timerid, 0, &its, NULL) < 0) {
        perror("TestSensor ERROR: Failed to set up timer");
        abort();
    };
}

void
SensorController::on_timer(union sigval sval)
{
    timer_t timerid = *(static_cast<timer_t*>(sval.sival_ptr));
    //cout << "on_timer called\n";
    timer_delete(timerid);

    SensorController& sc = SensorController::instance();

    // update sensor values, call callback
    if (sc.event_sensor && sc.event_sensor->enabled) {
        sc.event_sensor->x = sc.event_x;
        sc.event_sensor->y = sc.event_y;
        sc.event_sensor->z = sc.event_z;
        sc.event_sensor->distance = sc.event_distance;
        sc.event_sensor->timestamp = chrono::duration_cast<chrono::nanoseconds>(
                chrono::system_clock::now().time_since_epoch()).count();
        if (sc.event_sensor->on_event_cb != NULL) {
            //cout << "TestSensor: calling sensor callback for type " << sc.event_sensor->type << endl;
            sc.event_sensor->on_event_cb(sc.event_sensor, sc.event_sensor->event_cb_context);
        } else {
            //cout << "TestSensor: sensor type " << sc.event_sensor->type << "has no callback\n";
        }
    } else {
        //cout << "TestSensor: sensor type " << sc.event_sensor->type << "disabled, not processing event\n";
    }

    // read/process next event
    if (sc.next_command())
        sc.process_event_command();
    else {
        //cout << "TestSensor: script ended, no further commands\n";
    }
}


/***************************************
 *
 * Acceleration API
 *
 ***************************************/

UASensorsAccelerometer* ua_sensors_accelerometer_new()
{
    return SensorController::instance().get(ubuntu_sensor_type_accelerometer);
}

UStatus ua_sensors_accelerometer_enable(UASensorsAccelerometer* s)
{
    static_cast<TestSensor*>(s)->enabled = true;
    return (UStatus) 0;
}

UStatus ua_sensors_accelerometer_disable(UASensorsAccelerometer* s)
{
    static_cast<TestSensor*>(s)->enabled = false;
    return (UStatus) 0;
}

uint32_t ua_sensors_accelerometer_get_min_delay(UASensorsAccelerometer* s)
{
    return static_cast<TestSensor*>(s)->min_delay;
}

float ua_sensors_accelerometer_get_min_value(UASensorsAccelerometer* s)
{
    return static_cast<TestSensor*>(s)->min_value;
}

float ua_sensors_accelerometer_get_max_value(UASensorsAccelerometer* s)
{
    return static_cast<TestSensor*>(s)->max_value;
}

float ua_sensors_accelerometer_get_resolution(UASensorsAccelerometer* s)
{
    return static_cast<TestSensor*>(s)->resolution;
}

void ua_sensors_accelerometer_set_reading_cb(UASensorsAccelerometer* s, on_accelerometer_event_cb cb, void* ctx)
{
    TestSensor* sensor = static_cast<TestSensor*>(s);
    sensor->on_event_cb = cb;
    sensor->event_cb_context = ctx;
}

uint64_t uas_accelerometer_event_get_timestamp(UASAccelerometerEvent* e)
{
    return static_cast<TestSensor*>(e)->timestamp;
}

float uas_accelerometer_event_get_acceleration_x(UASAccelerometerEvent* e)
{
    return static_cast<TestSensor*>(e)->x;
}

float uas_accelerometer_event_get_acceleration_y(UASAccelerometerEvent* e)
{
    return static_cast<TestSensor*>(e)->y;
}

float uas_accelerometer_event_get_acceleration_z(UASAccelerometerEvent* e)
{
    return static_cast<TestSensor*>(e)->z;
}

/***************************************
 *
 * Proximity API
 *
 ***************************************/

UASensorsProximity* ua_sensors_proximity_new()
{
    return SensorController::instance().get(ubuntu_sensor_type_proximity);
}

UStatus ua_sensors_proximity_enable(UASensorsProximity* s)
{
    static_cast<TestSensor*>(s)->enabled = true;
    return (UStatus) 0;
}

UStatus ua_sensors_proximity_disable(UASensorsProximity* s)
{
    static_cast<TestSensor*>(s)->enabled = false;
    return (UStatus) 0;
}

uint32_t ua_sensors_proximity_get_min_delay(UASensorsProximity* s)
{
    return static_cast<TestSensor*>(s)->min_delay;
}

// the next three function make no sense in the API, just return zero
float ua_sensors_proximity_get_min_value(UASensorsProximity*)
{
    return 0.0;
}

float ua_sensors_proximity_get_max_value(UASensorsProximity*)
{
    return 0.0;
}

float ua_sensors_proximity_get_resolution(UASensorsProximity*)
{
    return 0.0;
}

void ua_sensors_proximity_set_reading_cb(UASensorsProximity* s, on_proximity_event_cb cb, void* ctx)
{
    TestSensor* sensor = static_cast<TestSensor*>(s);
    sensor->on_event_cb = cb;
    sensor->event_cb_context = ctx;
}

uint64_t uas_proximity_event_get_timestamp(UASProximityEvent* e)
{
    return static_cast<TestSensor*>(e)->timestamp;
}

UASProximityDistance uas_proximity_event_get_distance(UASProximityEvent* e)
{
    return static_cast<TestSensor*>(e)->distance;
}


/***************************************
 *
 * Light API
 *
 ***************************************/

UASensorsLight* ua_sensors_light_new()
{
    return SensorController::instance().get(ubuntu_sensor_type_light);
}

UStatus ua_sensors_light_enable(UASensorsLight* s)
{
    static_cast<TestSensor*>(s)->enabled = true;
    return (UStatus) 0;
}

UStatus ua_sensors_light_disable(UASensorsLight* s)
{
    static_cast<TestSensor*>(s)->enabled = false;
    return (UStatus) 0;
}

uint32_t ua_sensors_light_get_min_delay(UASensorsLight* s)
{
    return static_cast<TestSensor*>(s)->min_delay;
}

float ua_sensors_light_get_min_value(UASensorsLight* s)
{
    return static_cast<TestSensor*>(s)->min_value;
}

float ua_sensors_light_get_max_value(UASensorsLight* s)
{
    return static_cast<TestSensor*>(s)->max_value;
}

float ua_sensors_light_get_resolution(UASensorsLight* s)
{
    return static_cast<TestSensor*>(s)->resolution;
}

void ua_sensors_light_set_reading_cb(UASensorsLight* s, on_light_event_cb cb, void* ctx)
{
    TestSensor* sensor = static_cast<TestSensor*>(s);
    sensor->on_event_cb = cb;
    sensor->event_cb_context = ctx;
}

uint64_t uas_light_event_get_timestamp(UASLightEvent* e)
{
    return static_cast<TestSensor*>(e)->timestamp;
}

float uas_light_event_get_light(UASLightEvent* e)
{
    return static_cast<TestSensor*>(e)->x;
}
