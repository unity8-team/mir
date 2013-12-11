TEMPLATE = subdirs

SUBDIRS += platforms/mirserver platforms/ubuntuclient modules/Mir/Application
platforms_mirserver.depends = modules_Mir_Application
