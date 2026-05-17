# pulseoxdl - pulse oximetry downloader (Contec CMS50E, USB HID)
# Copyright © 2021, 2023-2025 Donatas Klimašauskas
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

VERSION := 1.5.0

.SUFFIXES :
.SUFFIXES : .h .c .o
.PHONY : production test testlocal testdevice clean cleanall install

SHELL := /bin/bash
CC := gcc
LD := ld
INSTALL := install

# If user specified goal is a test case, prepare SemVer extensions.
ifneq ($(findstring test,$(MAKECMDGOALS)),)
	EXTENSIONS := -$(MAKECMDGOALS)+$(shell date +%Y%m%d%H%M%S)
endif

CFLAGS := -std=gnu99 -pipe -pedantic-errors -Werror -Wall -Wextra -fanalyzer \
	-DVERSION=\"$(VERSION)$(EXTENSIONS)\"
DEBUG := -DDEBUG -Og -g3
TESTLOCAL := -DSIMULATOR
TESTDEVICE := -DDEBUG_WRITE
PRODUCTION := -O2 -s

OBJECTS := utils.o
PODOBJS := pulseoxdl.o
SIMOBJS := simulator.o

VPATH := src data

ifeq ($(strip $(DESTDIR)),)
	DESTDIR := ~
endif

# Default target and others, to be used as arguments to make command.
production : CFLAGS += $(PRODUCTION)
production : clean pulseoxdl
testlocal : CFLAGS += $(DEBUG) $(TESTLOCAL)
testlocal : clean pulseoxdl simulator test
testdevice : CFLAGS += $(DEBUG) $(TESTDEVICE)
testdevice : clean pulseoxdl
# Remove all object and downloaded by testlocal records files.
cleanall :
	@rm -fv *.o pulseoxdl simulator *.{csv,SpO2}
# Copy executable to user specified directory.
install :
	$(INSTALL) pulseoxdl $(DESTDIR)
	@test -x $(DESTDIR)/pulseoxdl
	@echo 'done'

# These targets are not meant to be used as arguments to make command.
pulseoxdl : $(OBJECTS) $(PODOBJS) header.o
	$(CC) -o $@ $(CFLAGS) $^
simulator : $(OBJECTS) $(SIMOBJS)
	$(CC) -o $@ $(CFLAGS) $^
pulseoxdl.o : exchange.h
simulator.o : exchange.h
utils.o : utils.h
% : %.o
header.o : header
	$(LD) -o $@ -z noexecstack -r -b binary $<
test :
	./test.sh
clean :
	@rm -fv $(OBJECTS) $(PODOBJS) $(SIMOBJS)
