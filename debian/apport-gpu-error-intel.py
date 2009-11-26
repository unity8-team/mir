#!/usr/bin/python

import os
import os.path
import sys

from apport.hookutils import *

from apport import unicode_gettext as _

def main(argv=None):

    if argv is None:
        argv = sys.argv

    from apport.packaging_impl import impl as packaging
    if not packaging.enabled():
        return -1

    import apport.report
    pr = apport.report.Report(type='Crash')

    pr.add_os_info()
    pr.add_proc_info()
    pr.add_user_info()
    pr['Package'] = 'xserver-xorg-video-intel'

    pr['IntelGpuDump'] = command_output(['intel_gpu_dump'])

    if pr.check_ignored():
        return 0

    nowtime = datetime.datetime.now()
    pr_filename = '/var/crash/%s.%s.crash' % (pr['Package'], str(nowtime).replace(' ', '_'))
    report_file = os.fdopen(os.open(pr_filename, os.O_WRONLY|os.O_CREAT|os.O_EXCL), 'w')
    os.chmod(pr_filename, 0600)

    try:
        pr.write(report_file)
    finally:
        report_file.close()
    return 0

if __name__ == '__main__':
    sys.exit(main())
