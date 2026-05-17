#!/bin/bash

# pulseoxdl-as-manual.sh - download "Auto" and join as "Manual"
# Copyright © 2024-2025 Donatas Klimašauskas
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

set -e

readonly WD=$(dirname "$(realpath "$0")")

"$WD/pulseoxdl" $@
JAAM_VERBOSE=0 JAAM_REMOVE_USED=1 "$WD/join-auto-as-manual.pl"

exit 0
