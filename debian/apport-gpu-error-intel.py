#!/usr/bin/python

import os
import os.path
import sys
import re
import hashlib

from apport.hookutils import *

from apport import unicode_gettext as _

pci_devices = [
    { 'name':'i810',        're':'(8086:7121)' },
    { 'name':'i810dc',      're':'(8086:7123)' },
    { 'name':'i810e',       're':'(8086:7125)' },
    { 'name':'i815',        're':'(8086:1132|82815)' },
    { 'name':'i830',        're':'(8086:3577|82830)' },
    { 'name':'i845',        're':'(8086:2562|82845G)' },
    { 'name':'i855',        're':'(8086:3582|855GM)' },
    { 'name':'i865',        're':'(8086:2572|82865G)' },
    { 'name':'i915g',       're':'(8086:2582)' },
    { 'name':'i915gm',      're':'(8086:2592|915GM)' },
    { 'name':'e7221',       're':'(8086:258a)' },
    { 'name':'i945g',       're':'(8086:2772|945G[ \/]|82945G[ \/])' },
    { 'name':'i945gm',      're':'(8086:27a2|945GM[ \/]|82945GM[ \/])' },
    { 'name':'i945gme',     're':'(8086:27ae|945GME|82945GME)' },
    { 'name':'IGDg',        're':'(8086:a001)' },
    { 'name':'IGDgm',       're':'(8086:a011)' },
    { 'name':'pineviewg',   're':'(8086:a001)' },
    { 'name':'pineviewgm',  're':'(8086:a011)' },
    { 'name':'i946gz',      're':'(8086:2972|82946GZ)' },
    { 'name':'g35',         're':'(8086:2982|82G35)' },
    { 'name':'i965q',       're':'(8086:2992|Q965)' },
    { 'name':'i965g',       're':'(8086:29a2|G965)' },
    { 'name':'g33',         're':'(8086:29c2|82G33)' },
    { 'name':'q35',         're':'(8086:29b2)' },
    { 'name':'q33',         're':'(8086:29d2)' },
    { 'name':'i965gm',      're':'(8086:2a02|GM965)' },
    { 'name':'i965gme',     're':'(8086:2a12)' },
    { 'name':'gm45',        're':'(8086:2a42)' },
    { 'name':'IGDeg',       're':'(8086:2e02)' },
    { 'name':'q45',         're':'(8086:2e12)' },
    { 'name':'g45',         're':'(8086:2e22)' },
    { 'name':'g41',         're':'(8086:2e32)' },
    { 'name':'b43',         're':'(8086:2e42)' },
    { 'name':'clarkdale',   're':'(8086:0042)' },
    { 'name':'arrandale',   're':'(8086:0046)' },
    { 'name':'sandybridge-hb',    're':'(8086:0100)' },
    { 'name':'sandybridge-gt1',   're':'(8086:0102)' },
    { 'name':'sandybridge-m-hb',  're':'(8086:0104)' },
    { 'name':'sandybridge-m-gt1', 're':'(8086:0106)' },
    { 'name':'sandybridge-s-hb',  're':'(8086:0108)' },
    { 'name':'sandybridge-s',     're':'(8086:010a)' },
    { 'name':'sandybridge-gt2',   're':'(8086:0112)' },
    { 'name':'sandybridge-m-gt2', 're':'(8086:0116)' },
    { 'name':'sandybridge-gt2+',  're':'(8086:0122)' },
    { 'name':'sandybridge-m-gt2+','re':'(8086:0126)' },
    ]
for device in pci_devices:
    device['rc'] = re.compile(device['re'], re.IGNORECASE)

def get_pci_device(text):
    regex_vga = re.compile('VGA compatible controller (.*)', re.IGNORECASE)

    lines = regex_vga.findall(text)
    if len(lines) > 0:
        for l in lines:
            if len(l.strip())>0:
                for device in pci_devices:
                    if device['rc'].search(l.strip()):
                        return device['name']
    return None

def get_dump_signature(text):
    if not text:
        return None
    m = hashlib.md5()
    m.update(text)
    codes = []
    for k in ["EIR", "ESR", "PGTBL_ER", "IPEHR"]:
        regex = re.compile(k+": 0x([0-9a-fA-F]+)")
        match = regex.search(text)
        if match and match.group(1) != "00000000":
            codes.append("%s: 0x%s" %(k, match.group(1)))
    if len(codes) > 1:
        return "(%s)" %( string.join(codes, " ") )
    elif len(codes) == 1:
        return "%s (%s)" %( m.hexdigest()[:8], string.join(codes, " ") )
    else:
        return "%s" %( m.hexdigest()[:8] )

def main(argv=None):
    if argv is None:
        argv = sys.argv

    from apport.packaging_impl import impl as packaging
    if not packaging.enabled():
        return -1

    import apport.report
    report = apport.report.Report(type='Crash')
    report.setdefault('Tags', '')
    report.setdefault('Title', 'GPU lockup')

    report.add_os_info()
    report.add_proc_info()
    report.add_user_info()

    report['Package'] = 'xserver-xorg-video-intel'
    report['Tags'] += ' freeze'
    report['Lspci'] = command_output(['lspci', '-vvnn'])
    report['IntelGpuDump'] = root_command_output(['intel_gpu_dump'])
    chipset = get_pci_device(report['Lspci'])
    if chipset:
        report['Chipset'] = chipset
        report['Title'] = "[%s] GPU lockup" %(report['Chipset'])
    dump_signature = get_dump_signature(report['IntelGpuDump'])
    if dump_signature:
        report['DumpSignature'] = dump_signature
        report['Title'] += " " + report['DumpSignature']

    attach_hardware(report)
    attach_related_packages(report, ["xserver-xorg", "libdrm2", "xserver-xorg-video-intel"])
    attach_file_if_exists(report, '/etc/X11/xorg.conf', 'XorgConf')
    attach_file(report, '/var/log/Xorg.0.log', 'XorgLog')
    attach_file_if_exists(report, '/var/log/Xorg.0.log.old', 'XorgLogOld')
    attach_file_if_exists(report, '/sys/kernel/debug/dri/0/i915_error_state', 'i915_error_state')

    nowtime = datetime.datetime.now()
    report_filename = '/var/crash/%s.%s.crash' % (report['Package'], str(nowtime).replace(' ', '_'))
    report_file = os.fdopen(os.open(report_filename, os.O_WRONLY|os.O_CREAT|os.O_EXCL), 'w')
    os.chmod(report_filename, 0600)

    try:
        report.write(report_file)
    finally:
        report_file.close()
    return 0

if __name__ == '__main__':
    sys.exit(main())
