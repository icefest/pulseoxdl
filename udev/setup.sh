#!/bin/bash

# pulseoxdl - pulse oximetry downloader (Contec CMS50E, USB HID)
# Copyright © 2023, 2025 Donatas Klimašauskas
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

# Debian (v11) has plugdev system group. Normal users are its members.
# Fedora (v37) does not have that group by default. This program will
# create it and add all normal users, which have directories under
# /home/, to the group. If the group exists on Fedora, normal user, if
# not a member already, will have to be added to it manually. It is
# held that user's OS is one of those two or a derivative.

set -e

test $EUID -eq 0 || {
    echo 'error: user must be root'

    exit 1
}

readonly HOME_USERS=$(cd /home/ && l=$(ls -dx **/) && l=${l//\//} && echo $l)
readonly UDEV_ADMIN="${1:-/etc/udev/rules.d}"
readonly UDEV_GROUP=plugdev
readonly UDEV_RULES=pulseoxdl.rules

# The command fails if group exists or not on Fedora like OS -- OK.
groupadd --system -U ${HOME_USERS// /,} $UDEV_GROUP 2>/dev/null &&
    echo "$0:
Created group: $UDEV_GROUP
Added users to it: $HOME_USERS
Log out/in required for the changes to take effect"

cd "$(dirname "$0")"
cp "$UDEV_RULES" "$UDEV_ADMIN"
chmod 0644 "$UDEV_ADMIN/$UDEV_RULES" # If user's umask created other mode.
touch "$UDEV_ADMIN" # Make sure udevd detects, if the old file was present.

exit 0
