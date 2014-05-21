Testing applications with simulated sensor data
===============================================

Purpose
-------
platform-api's interface to sensor data is in the shared library
`libubuntu_platform_hardware_api.so`. That is only a stub which dynamically
(dlopen) loads a backend library which provides the actual implementation. By
default this is `/system/lib/libubuntu_application_api.so` which reads sensor
data from the Android side. For testing purposes this can be replaced with this
`libubuntu_application_api_test.so.1` which simulates sensors and their data based
on a simple text input file.

Using the test sensors
----------------------
Run your application under test with the environment variable

    UBUNTU_PLATFORM_API_BACKEND=libubuntu_application_api_test.so.1

and make sure that ld.so(8) can find it. If you don't have the library
installed in a standard system library path, it is recommended to set
`LD_LIBRARY_PATH` to the directory that contains the library (usually when using
the library right out of the build tree). Alternatively you can specify the
full path in `$UBUNTU_PLATFORM_API_BACKEND`.

The env variable `$UBUNTU_PLATFORM_API_SENSOR_TEST` needs to point to a file that
describes the desired sensor behaviour.

Data format
-----------
The test sensors use a simple line based file format. The first part
instantiates desired sensors with their parameters:

    create [accel|light] <min> <max> <resolution>
    # but no arguments for proximity sensor: 
    create proximity
  
After that, it defines events; <delay> specifies time after previous event
in ms:

    <delay> proximity [unknown|near|far]
    <delay> light <value>
    <delay> accel <x> <y> <z>

Empty lines and comment lines (starting with #) are allowed.

Example file:

    create light 0 10 1
    create accel 0 1000 0.1
    create proximity
     
    200 proximity near
    500 light 5
    # simulate crash on the ground
    500 accel 0.2 0.1 10
    100 accel 0.2 0.2 1000
    20 accel 0 0 0
    10 proximity far
    0 light 10


Complete example
----------------
 * Build platform-api:

        mkdir obj; (cd obj; cmake .. && make)

 * Put above example file into /tmp/test.sensors

 * Run the sensor test with it:
 
        LD_LIBRARY_PATH=obj/src/ubuntu/testbackend \
        UBUNTU_PLATFORM_API_BACKEND=libubuntu_application_api_test.so.1 \
        UBUNTU_PLATFORM_API_SENSOR_TEST=/tmp/test.sensors \
        obj/src/ubuntu/hybris/tests/test_android_sensors_api

