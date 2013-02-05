#include <ubuntu/ui/ubuntu_ui_session_service.h>

#include <cstdio>
#include <cstring>

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        printf("Usage: %s {show|hide}", argv[0]);
        return 1;
    }

    if (strcmp("show", argv[1]) == 0)
        ubuntu_ui_report_osk_visible(0, 0, 0, 0);
    else if (strcmp("hide", argv[1]) == 0)
        ubuntu_ui_report_osk_invisible();
    
    return 0;
}
