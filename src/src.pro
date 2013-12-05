TEMPLATE = subdirs

SUBDIRS += platforms/mirserver modules/Mir/Application
platforms_mirserver.depends = modules_Mir_Application
