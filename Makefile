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

SNAPVER:=1.1
VERSION="$(shell apt-cache policy mir-demos | grep '^ \*\*\* ' | cut -c1-7,10-17 --complement | cut -d' ' -f3)snap${SNAPVER}"

all: snap

snap: update-deb2snap
	cd deb2snap && ./deb2snap -d 15.04 -n mir -v ${VERSION} --overlay ../overlay -p mir-demos -p mir-graphics-drivers-desktop ../server
	mv deb2snap/*.snap .

update-deb2snap: deb2snap
	cd deb2snap && bzr pull && make

deb2snap:
	bzr branch lp:deb2snap
