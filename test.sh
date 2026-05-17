#!/bin/bash

# pulseoxdl - pulse oximetry downloader (Contec CMS50E, USB HID)
# Copyright © 2021-2022, 2024-2025 Donatas Klimašauskas
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

# Main.

# Connects two CLI programs' stdins to stdouts and vice versa.
readonly CRISSCROSSER=socat
readonly MEMORYCHECKER=valgrind
readonly DATE_TIME_SYNC_CMD=^83

if [ ! $(whereis -b $CRISSCROSSER | awk '{ print $2 }') ]; then
    echo "$0: program '$CRISSCROSSER' is required"

    exit 1
fi

readonly DIRROOT=data/test
readonly DIRTRAN=$DIRROOT/transmission
readonly DIRMANU=$DIRROOT/manufacturer
readonly EXPECTED=$DIRROOT/expected
readonly DIRTMP=$(mktemp -d --tmpdir pulseoxdl-test-XXXX)
readonly TMP=$DIRTMP/tmp.data
readonly SORTED=$TMP.sorted
readonly MOVE_NAME=move
readonly LIVE_NAME=live
readonly ECHO_NAME=echo
readonly MOVE_AUTO=auto
readonly MOVE_MANUAL=manual
readonly MOVE_SAVED_AUTO=20210515120642
readonly MOVE_SAVED_MANUAL=20250705165923
readonly LIVE_SAVED=20210430112839
readonly DIRMOVE=$DIRMANU/$MOVE_AUTO
readonly MOVECSV=$DIRMOVE/csv
readonly SIMULATE_MANUAL=1

readonly ANALYZER=analyze.awk
readonly ANALYZER_EXPECTED=$DIRROOT/$ANALYZER

# The simulator gets either a single transmission file of the live
# action, or two transmission files for the move action. Since the
# move action can be either for "Auto" or "Manual" mode record, the
# simulator utilizes an additional flag argument, in which presence it
# acts like a device has the "Manual" mode stored record. The testing
# code indicates which mode stored record it needs.

do_test ()
{
    echo $1${3:+: $3}

    local transmission manufacturer

    test $# -gt 2 && {
	transmission=$DIRTRAN/$3
	manufacturer=$DIRMANU/$3
	transmission="$transmission/spo2 $transmission/pr"
    } || {
	transmission=$DIRTRAN/$1
	manufacturer=$DIRMANU/$1
    }

    $CRISSCROSSER \
	EXEC:"$MEMORYCHECKER ./pulseoxdl /dev/hidrawN $1" \
	EXEC:"$MEMORYCHECKER ./simulator $transmission${4:+ $4}" \
	|& tee -a $TMP

    diff -q {${manufacturer}/,$2.}csv
    diff {${manufacturer}/,$2.}SpO2
}

do_test $MOVE_NAME $MOVE_SAVED_AUTO $MOVE_AUTO
do_test $MOVE_NAME $MOVE_SAVED_MANUAL $MOVE_MANUAL $SIMULATE_MANUAL
do_test $LIVE_NAME $LIVE_SAVED
do_test $ECHO_NAME $LIVE_SAVED

# Filter out the date and time synchronization.
grep -v $DATE_TIME_SYNC_CMD $TMP | sort -u >$SORTED

# Include expected echo action output and test for equality.
sort $EXPECTED $DIRMANU/$LIVE_NAME/csv >$TMP
diff -q $TMP $SORTED

NOPLOT=1 ./$ANALYZER $MOVECSV | tee $TMP
diff -q $ANALYZER_EXPECTED $TMP

# Utilities.

readonly DIRUTILS=utils
readonly CSVTOSPO2=$DIRUTILS/csv-to-spo2.pl
readonly SPO2TOCSV=$DIRUTILS/spo2-to-csv.pl
readonly PROGJAAM=$DIRUTILS/join-auto-as-manual.pl
readonly PREPARED=$DIRTMP/prepared.csv

print_utility ()
{
    test -n "$1" && echo -n "$1: " || echo 'OK'
}

test_utility ()
{
    print_utility $1

    $1 <$DIRMOVE/$2 >$TMP
    diff -q $DIRMOVE/$3 $TMP

    print_utility
}

test_utility $CSVTOSPO2 csv SpO2
test_utility $SPO2TOCSV SpO2 csv

# Test "Manual" mode record emulation: "Auto" mode records downloaded
# to a directory and the utility joining program ran to produce a
# single record with the gaps filled as by the "Manual" mode.

print_utility $PROGJAAM

# Prepare the CSV to compare the joined and converted SpO2 to. Set the
# datums to maximums of what the "Manual" mode would set at after an
# hour, for the duration of 5 minutes, and the same again, leaving
# real datums until the end. (First is header, then 1 line 1 s.)
awk '{ if (NR > 3601 && NR < 3902 || NR > 7501 && NR < 7802) { print $1, $2,
"127, 255\r"; next } print }' <$MOVECSV >$PREPARED

# Prepare 3 CSV files as downloaded from the "Auto" mode.
(
    set_finish ()
    {
	let finish=start+window
    }

    start=2 # The first measurement line of the expected CSV.
    window=3599 # The first measurement line has the datums.
    gap=301 # The 1 is due to inverse of the window length.
    set_finish # The last line of a CSV file.
    tmp=tmp.csv

    for i in 1 2 3
    do
	sed -n "1p;$start,${finish}p" $MOVECSV >$tmp
	mv -i $tmp $DIRTMP/$(sed -n '2s/[-T:, ]//gp' $tmp | cut -c -14).csv
	let start=finish+gap
	set_finish
    done
)

# Convert the 3 CSVs to SpO2s.
(
    for csv in $DIRTMP/*.csv
    do
	$CSVTOSPO2 <$csv >${csv/csv/SpO2}
    done
)

# Join the 3 SpO2s: first leave, then remove the used SpO2, CSV files.
(
    join=$(dirname "$(realpath "$0")")/$PROGJAAM
    cd $DIRTMP

    run_join ()
    {
	local count=$(test $1 && echo 0 || echo 6)

	eval ${1:+JAAM_REMOVE_USED=1} "$join"
	test $(ls *[0-9].{SpO2,csv} 2>/dev/null | wc -l) -eq $count
    }

    run_join
    rm *joined*
    run_join 'remove'
)

# Convert the joined SpO2 to the joined CSV.
(
    spo2=$DIRTMP/$MOVE_SAVED_AUTO.joined.SpO2
    $SPO2TOCSV <$spo2 >${spo2/SpO2/csv}
)

# The prepared CSV has to match the joined SpO2 converted to CSV.
diff -q $PREPARED $DIRTMP/*.joined.csv

print_utility

# Test splitting the "Manual" mode record into corresponding CSV files
# with valid measurements only, as if downloaded from "Auto" mode.
# Prepare a single file without artifacts from known "Manual" CSV with
# artifacts. Run the splitting utility on the known CSV. Check that
# the split CSV files were created. The prepared and split files'
# contents must match.

readonly PROGSMAA=$DIRUTILS/split-manual-as-auto.pl
readonly ARTIFACTS='127, 255'
readonly MANUAL=20250705165923.csv
readonly SPLIT_FIRST=20250705165950.csv
readonly SPLIT_SECOND=20250705173120.csv

print_utility $PROGSMAA

sed -n "/$ARTIFACTS/!p" $MANUAL >$PREPARED
SMAA_REMOVE_USED=1 ./$PROGSMAA $MANUAL
test -e $SPLIT_FIRST
test -e $SPLIT_SECOND
diff -q $PREPARED <(cat $SPLIT_FIRST && tail -n +2 $SPLIT_SECOND)

print_utility

# Reset timestamps. First, use the utility to reset timestamps in a
# CSV by setting the start of a record to be in the future. The total
# number of lines before and after the reset has to be the same. The
# current top measurements line gets gone; it has to be replaced with
# a line with the new timestamp. Second, convert the reset SpO2 into
# converted CSV. The reset CSV and the converted CSV have to be equal.

get_lines ()
{
    wc -l "$1" | cut -d ' ' -f 1
}

readonly RESETTIMESTAMPS=$DIRUTILS/reset-timestamps.sh
readonly MOVED_OLD=20210515120642.csv
readonly MOVED_NEW=20210515121642.csv # 10 minutes forward.
readonly TIME_OLD=12:06:42 # Time in the second line in a CSV file.
readonly TIME_NEW=12:16:42 # Ditto.
readonly DATE_OLD=2021-05-15
readonly MOVED_LINES=$(get_lines $MOVED_OLD)

print_utility $RESETTIMESTAMPS

# First.

$RESETTIMESTAMPS $MOVED_OLD $DATE_OLD $TIME_NEW
test $MOVED_LINES -eq $(get_lines $MOVED_NEW)
test -z "$(grep $TIME_OLD $MOVED_NEW)"
test -n "$(sed -n 2p $MOVED_NEW | grep $TIME_NEW)"

# Second.

readonly CONVERTED_NEW=$DIRTMP/$MOVED_NEW

$SPO2TOCSV <${MOVED_NEW/csv/SpO2} >$CONVERTED_NEW
diff -q $MOVED_NEW $CONVERTED_NEW

print_utility

# Cleanup.

rm $DIRTMP/*
rmdir $DIRTMP

echo 'pass'

exit 0
