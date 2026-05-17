#!/bin/bash

# reset-timestamps.sh - reset CSV and SpO2 timestamps (pulseoxdl)
# Copyright © 2025 Donatas Klimašauskas
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

# Take, as CLI arguments, the CSV file, produced by "pulseoxdl", to
# reset timestamps in its name and content, and new start date and
# time for the timestamps. Create corresponding SpO2. Remove old pair.
#
# Usage:
# ./reset-timestamps.sh <\d{14}\.csv> <Date %Y-%m-%d> <Time %H:%M:%S>

set -e

readonly CSV=${1:?CSV file to reset timestamps}
readonly DATE=${2:?start timestamp\'s date component}
readonly TIME=${3:?start timestamp\'s time component}

readonly NEWNAME=$(dirname "$CSV")/$(echo $DATE $TIME | sed 's/[- :]//g')
readonly NEWCSV=$NEWNAME.csv

exit_error ()
{
    echo "$(basename "$0"): error: $@"

    exit 1
}

is_format_valid ()
{
    test "$(echo "$1" | grep -P "^$2$")" || exit_error "format: $3"
}

# Validate user specified arguments.

test -e "$CSV" || exit_error 'CSV file does not exist'
is_format_valid "$(basename "$CSV")" '\d{14}\.csv' 'unexpected CSV filename'
is_format_valid $DATE '\d{4}-\d{2}-\d{2}' 'date'
is_format_valid $TIME '\d{2}:\d{2}:\d{2}' 'time'
test "$CSV" = "$NEWCSV" && exit_error 'CSV filename and timestamp match'

# Reset CSV timestamps.

head -n 1 "$CSV" >"$NEWCSV" # Copy the CSV header.

# Replace old dates and times with the new ones. All in UTC.
tail -n +2 "$CSV" | cut -d ' ' -f 3- |
    awk -v s=$(date -ud "$DATE $TIME" +%s) \
	'{ printf "%s, %s\n", strftime("%F, %T", s++, 1), $0 }' >>"$NEWCSV"

# Create SpO2 with reset timestamp.

"$(dirname "$(realpath "$0")")"/csv-to-spo2.pl <"$NEWCSV" >"$NEWNAME.SpO2"

# Remove the old files.

rm -f "$CSV" "${CSV/csv/SpO2}" # The old SpO2 file may not be present.

exit 0
