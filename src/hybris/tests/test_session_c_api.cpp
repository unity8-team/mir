/*
 * Copyright © 2012 Canonical Ltd.
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
 * Authored by: Thomas Voß <thomas.voss@canonical.com>
 */

#include <ubuntu/ui/ubuntu_ui_session_service.h>

#include <getopt.h>

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

typedef union
{
    struct Components
    {
        uint8_t r;
        uint8_t g;
        uint8_t b;
        uint8_t a;
    } components;
    uint32_t value;
} Pixel;

struct Config
{
    Config() : take_screencast_flag(0),
               take_screenshot_flag(0)
    {
    }
    
    int take_screencast_flag;
    int take_screenshot_flag;
};

void on_snapshot_completed(const void* pixel_data, unsigned int width, unsigned int height, unsigned int x, unsigned int y, unsigned int source_width, unsigned int source_height, unsigned int stride, void* context)
{
    static unsigned int counter = 0;
    
    printf("%s: (%p, %d, %d, %d) \n",
           __PRETTY_FUNCTION__,
           pixel_data,
           width,
           height,
           stride);

    static const char snapshot_pattern[] = "./snapshot_%I_%M_%S.ppm";
    static const char frame_pattern[] = "./frame_%I_%M_%S.raw";
  
    char fn[256];

    int take_screenshot = 1;
    int take_screencast = 0;

    if (context != NULL)
    {
        Config* config = (Config*) context;

        take_screenshot = config->take_screenshot_flag;
        take_screencast = config->take_screencast_flag;
    }
    
    time_t curtime;
    struct tm *loctime;
    
    curtime = time (NULL);        
    loctime = localtime (&curtime);    
    
    static const char screenshot_file_mode[] = "w+";
    static const char screencast_file_mode[] = "wb+";
    
    FILE* f = NULL;
    if (take_screenshot)
    {
        strftime(fn, 256, snapshot_pattern, loctime);
        f = fopen(fn, screenshot_file_mode);
    } else if (take_screencast)
    {
        strftime(fn, 256, frame_pattern, loctime);
        f = fopen(fn, screencast_file_mode);
    }

    if (!f)
    {
        printf("Problem opening file: %s \n", fn);
        return;
    }
     
    if (take_screenshot)
    {
        const unsigned int* p = static_cast<const unsigned int*>(pixel_data);
        
        fprintf(f, "P3\n%d %d\n%d\n\n", width, height, 255);
        for(unsigned int i = 0; i < height; i++)
        {
            for(unsigned int j = 0; j < width; j++)
            {
                Pixel pixel; pixel.value = *p; ++p;
                fprintf(
                    f, "%d %d %d\t", 
                    pixel.components.r,
                    pixel.components.g,
                    pixel.components.b);
            }
        }
    }
    else if (take_screencast)
    {
        fwrite(pixel_data, sizeof(unsigned int), width*height, f);
        ubuntu_ui_session_snapshot_running_session_with_id(
            -1,
            on_snapshot_completed,
            context);
    }
}

void on_session_born(ubuntu_ui_session_properties props, void*)
{
    printf("%s:\n\t Id: %d \n\t Desktop file hint: %s \n",
           __PRETTY_FUNCTION__,
           ubuntu_ui_session_properties_get_application_instance_id(props),
           ubuntu_ui_session_properties_get_desktop_file_hint(props));

    ubuntu_ui_session_snapshot_running_session_with_id(
        ubuntu_ui_session_properties_get_application_instance_id(props),
        on_snapshot_completed,
        NULL);
}

void on_session_focused(ubuntu_ui_session_properties props, void*)
{
    printf("%s:\n\t Id: %d \n\t Desktop file hint: %s \n",
           __PRETTY_FUNCTION__,
           ubuntu_ui_session_properties_get_application_instance_id(props),
           ubuntu_ui_session_properties_get_desktop_file_hint(props));
}

void on_session_died(ubuntu_ui_session_properties props, void*)
{
    printf("%s:\n\t Id: %d \n\t Desktop file hint: %s \n",
           __PRETTY_FUNCTION__,
           ubuntu_ui_session_properties_get_application_instance_id(props),
           ubuntu_ui_session_properties_get_desktop_file_hint(props));
}



Config parse_cmd_line(int argc, char** argv)
{
    Config config;
    static struct option long_options[] = {
        {"take-screencast", no_argument, &config.take_screencast_flag, 1},
        {"take-screenshot", no_argument, &config.take_screenshot_flag, 1}
    };

    while (true)
    {
        int option_index = 0;
        
        int c = getopt_long(
            argc, 
            argv, 
            "",
            long_options, 
            &option_index);

        if (c == -1)
            break;

        switch (c)
        {
            case 0:
                // No need to do anything here: Flag is set automatically.
                break;
            default:
                break;
        }
    }

    return config;
}

int main(int argc, char** argv)
{
    static const int complete_session_id = -1;

    Config config = parse_cmd_line(argc, argv);

    if (config.take_screenshot_flag || config.take_screencast_flag)
    {
        ubuntu_ui_session_snapshot_running_session_with_id(
            complete_session_id,
            on_snapshot_completed,
            &config);

        return 0;
    }

    ubuntu_ui_session_lifecycle_observer observer;

    memset(&observer, 0, sizeof(observer));
    observer.on_session_born = on_session_born;
    observer.on_session_focused = on_session_focused;
    observer.on_session_died = on_session_died;

    ubuntu_ui_session_install_session_lifecycle_observer(&observer);

    while(true)
    {
    }

    return 0;
}

