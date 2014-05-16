TEMPLATE = subdirs

SUBDIRS += platforms/mirserver platforms/ubuntuclient modules/Mir/Application
modules_Mir_Application.depends = platforms_mirserver
