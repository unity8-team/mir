TEMPLATE = subdirs
SUBDIRS = src

!no_tests {
    SUBDIRS += tests
    tests.depends = src
}
