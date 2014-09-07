TEMPLATE = app

# CONFIG += testcase adds a  'make check' which is great. But by default it also
# adds a 'make install' that installs the test cases, which we do not want.
# Can configure it not to do that with 'no_testcase_installs'
CONFIG += testcase no_testcase_installs

QMAKE_CXXFLAGS = -std=c++11

QT += testlib

CONFIG += link_pkgconfig
PKGCONFIG += mirserver

include(google-mock.pri)
