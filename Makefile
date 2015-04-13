#!/bin/bash
# -*- Mode: Makefile; indent-tabs-mode: t; tab-width: 4 -*-
#
# Copyright (C) 2015 Canonical, Ltd.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 3.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

all: snap

snap: update-snappy-preload
	cd snappy-preload/tools && ./make-snap -d 15.04/beta-2 -n mir -v 0 --overlay ../../overlay -p mir-demos -p mir-graphics-drivers-desktop ../../server
	mv snappy-preload/tools/*.snap .

update-snappy-preload: snappy-preload
	cd snappy-preload && bzr pull && make

snappy-preload:
	bzr branch lp:~mterry/+junk/snappy-preload
