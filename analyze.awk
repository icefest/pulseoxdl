#!/usr/bin/env -S awk -We

# analyze.awk - pulseoxdl data analyzer
# Copyright © 2022 Donatas Klimašauskas
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

# Note. A term here is equivalent to a measurement (SpO2 or PR).

function make_term(term, name, field, drop, time, high, low, max, color)
{
	term["NAME"] = name
	term["FIELD"] = field
	term["LMT_DROP"] = drop
	term["LMT_TIME"] = time
	term["LMT_HIGH"] = high
	term["LMT_LOW"] = low

	term["previous"] = 0
	term["current"] = 0
	term["time"] = 0
	term["events"] = 0
	term["eventstime"] = 0
	term["totalterm"] = 0
	term["max"] = 0
	term["min"] = max
	term["higher"] = 0
	term["lower"] = 0
	term["top"] = 0
	term["bottom"] = GP_EVENT_PR # Not max: reset after any term's event.
	term["starting"] = 0
	term["ending"] = 0
	term["fall"] = 0
	term["rise"] = 0
	term["gptop"] = max
	term["gpsize"] = 0
	term["gpfile"] = GP_EVENTS""tolower(substr(name, 0,
						   index(name, ",") - 1))
	term["gpcolor"] = color
}

function get_timestamp(    datetime)
{
	datetime = $1"T"$2
	gsub(",", "", datetime)

	return datetime
}

function set_extreme_time(term, field)
{
	if (term["max"] < field)
		term["max"] = field
	else if (term["min"] > field)
		term["min"] = field
	if (term["LMT_HIGH"] < field)
		term["higher"]++
	else if (term["LMT_LOW"] > field)
		term["lower"]++
}

# The events per term files are kept, because gnuplot uses them to
# replot when user, e.g., zooms in. They have to be cleared before new
# data is gathered to cover the case when user changes sleep onset or
# awake from sleep time such, that a term may have no events written
# at the later execution, when the earlier had events written.
function clear_events_file(term)
{
	printf "" >term["gpfile"]
}

function push_record(term)
{
	term["time"]++
	term["ending"]++
	if (NOPLOT)
		return
	term[term["gpsize"]++] = $0
}

# Array is "0" indexed: 1st second of a record is at "0". It prints
# event records such, that visual marking starts just before the first
# fall and ends right before the first rise of an end of an event.
# Thus, the shortest in duration event will have -1 records.
function write_records(term,    i)
{
	term["starting"]--
	term["time"]--
	if (NOPLOT)
		return
	for (i = term["starting"]; i < term["time"]; i++) {
		$0 = term[i]
		print $1, $2, GP_EVENT_SPO2",", GP_EVENT_PR >term["gpfile"]
	}
}

function clear_records(term, all,    i)
{
	term["time"] = 0
	term["ending"] = 0
	if (all) {
		term["starting"] = 0
		term["top"] = 0
		term["bottom"] = GP_EVENT_PR
	}
	if (NOPLOT)
		return
	for (i = 0; i < term["gpsize"]; i++)
		delete term[i]
	term["gpsize"] = 0
}

function analyze_term(term, noevents,    field, delta)
{
	field = $term["FIELD"] + 0
	term["totalterm"] += field

	# First data line. Do initial set up.
	if (!term["top"]) {
		set_extreme_time(term, field)
		if (noevents)
			return
		push_record(term)
		term["top"] = field
		term["previous"] = field

		return
	}

	# Two consecutive falls start a potential event. Count two
	# consecutive rises. Any fall in between clears a rise and
	# extends a potential event. Track bottom and time on zeros.

	set_extreme_time(term, field)
	if (noevents)
		return
	term["current"] = field
	delta = term["current"] - term["previous"]
	term["previous"] = term["current"]

	push_record(term)
	if (delta >= 1) { # PR could go up by > 1.
		if (term["fall"] == 1) {
			term["fall"] = 0
			clear_records(term, CLEAR_ALL)

			return
		}
		if (term["top"] < term["current"]) {
			term["top"] = term["current"]
			clear_records(term)
		}
		term["rise"]++
		if (term["rise"] == 1) {
			term["ending"] = 0

			return
		}
	} else if (delta < 0) {
		term["fall"]++
		if (!term["starting"])
			term["starting"] = term["time"]
		if (term["bottom"] > term["current"])
			term["bottom"] = term["current"]
		term["ending"] = 0
		term["rise"] = 0

		return
	}
	if (term["rise"] > 1) {
		term["rise"] = 0
		term["fall"] = 0
		term["time"] -= term["ending"]
		if (term["time"] >= term["LMT_TIME"] &&
		    term["top"] - term["bottom"] >= term["LMT_DROP"]) {
			term["events"]++
			term["eventstime"] += term["time"]
			write_records(term)
		}
		clear_records(term, CLEAR_ALL)
	}
}

function print_term_thresholds(term)
{
	printf "%7s:%8d%8d%8d%8d\n",
		term["NAME"], term["LMT_DROP"], term["LMT_TIME"],
		term["LMT_HIGH"], term["LMT_LOW"]
}

function print_term_analysis(term)
{
	printf "%7s:%8.1f%8d%8.1f%8.1f%8.1f%8d%8d%8.1f%8.1f\n",
		term["NAME"],
		term["totalterm"] / RECORD_SECONDS,
		term["events"],
		term["eventstime"] / MIN_IN_SEC,
		term["events"] ? term["eventstime"] / term["events"] : 0,
		term["events"] / RECORD_HOURS,
		term["max"],
		term["min"],
		term["higher"] / MIN_IN_SEC,
		term["lower"] / MIN_IN_SEC
}

function distribute_spo2(set,    i)
{
	for (i = PERCENTILE_START; i > PERCENTILE_END; i -= PERCENTILE_STEP)
		if (set)
			if ($spo2["FIELD"] < i)
				distribution[i]++
			else
				return
		else
			printf "%8d%8.1f%8.1f\n",
				i, distribution[i] / MIN_IN_SEC,
				distribution[i] / RECORD_SECONDS * TO_PERCENT
}

function exit_error(msg)
{
	print PROGRAM": error: "msg >"/dev/stderr"
	error = 1

	exit error
}

function set_default(variable, value)
{
	return ENVIRON[variable] != "" ? ENVIRON[variable] : value
}

function plot_term(term,    paddedmax, endpoint)
{
	paddedmax = term["max"] + GP_PADDING
	endpoint = awakebottom - GP_HEADER_AND_NEXT_TRACKING
	system(sprintf(GP_CMD_LIST_FMT, terms, term["gpfile"], term["gpcolor"],
		       term["NAME"],
		       paddedmax > term["gptop"] ? term["gptop"] : paddedmax,
		       ONSET_SEC, endpoint, term["FIELD"],
		       GP_TREND_POINT_INCREMENT,
		       ONSET_SEC, endpoint, term["FIELD"],
		       term["FIELD"]))
}

BEGIN {
	PROGRAM = "analyze.awk"

	if (ARGC != 2)
		exit_error("CSV file with records has to be specified")

	HELP_REQUEST = "-h"
	HELP_RESPONSE = "\
Usage: [<option>...] ./"PROGRAM" <CSV>\n\
\n\
<option> is zero or more of (assigned value is the default):\n\
ONSETMIN=0\n\
	Falling a sleep time in minutes.\n\
AWAKEMIN=0\n\
	Waking up time in minutes.\n\
SPO2DROP=4\n\
	Minimum SpO2 drop in % for an event detection.\n\
SPO2TIME=10\n\
	Minimum SpO2 drop duration in seconds for an event detection.\n\
PRDROP=6\n\
	Minimum PR drop in BPM for an \"event\" detection.\n\
PRTIME=8\n\
	Minimum PR drop duration in seconds for an \"event\" detection.\n\
SPO2HIGH=96\n\
	Upper SpO2 bound in % to count duration in minutes above it.\n\
SPO2LOW=90\n\
	Lower SpO2 bound in % to count duration in minutes below it.\n\
PRHIGH=90\n\
	Upper PR bound in BPM to count duration in minutes above it.\n\
PRLOW=60\n\
	Lower PR bound in BPM to count duration in minutes below it.\n\
NOEVENTPR=1\n\
	Assign 0 to analyze PR \"events\" too (read README.md).\n\
NOPLOT=0\n\
	Assign 1 to do not plot. Otherwise, if gnuplot is installed,\n\
	interactive SpO2 and PR plot windows will open.\
"

	if (ARGV[1] == HELP_REQUEST) {
		print HELP_RESPONSE

		exit
	}

	terms = ARGV[1]

	ONSETMIN = set_default("ONSETMIN", 0)
	AWAKEMIN = set_default("AWAKEMIN", 0)
	SPO2DROP = set_default("SPO2DROP", 4)
	SPO2TIME = set_default("SPO2TIME", 10)
	PRDROP = set_default("PRDROP", 6)
	PRTIME = set_default("PRTIME", 8)
	SPO2HIGH = set_default("SPO2HIGH", 96)
	SPO2LOW = set_default("SPO2LOW", 90)
	PRHIGH = set_default("PRHIGH", 90)
	PRLOW = set_default("PRLOW", 60)
	NOEVENTPR = set_default("NOEVENTPR", 1)
	NOPLOT = set_default("NOPLOT", 0)

	if (!(SPO2HIGH > SPO2LOW && PRHIGH > PRLOW))
		exit_error("high limit has to be higher than the low")

	MIN_IN_SEC = 60
	ONSET_SEC = ONSETMIN * MIN_IN_SEC + 1 # 1 for header.
	AWAKE_SEC = AWAKEMIN * MIN_IN_SEC
	PERCENTILE_START = 95
	PERCENTILE_END = 45
	PERCENTILE_STEP = 5
	TO_PERCENT = 100
	SET_DISTRIBUTION = 1
	NAME_SPO2 = "SpO2, %"
	NAME_PR = "PR, BPM"
	FIELD_SPO2 = 3
	FIELD_PR = 4
	CLEAR_ALL = 1
	GP_TEST = "gnuplot --version >/dev/null"
	GP_EVENTS = "/dev/shm/gp-events-"
	GP_EVENT_SPO2 = 100
	GP_EVENT_PR = 255
	GP_COLOR_SPO2 = "0000ff" # Blue.
	GP_COLOR_PR = "ff0000" # Red.
	GP_PADDING = 10
	GP_HEADER_AND_NEXT_TRACKING = 2
	GP_TREND_POINT_INCREMENT = 150
	GP_CMD_LIST_FMT = "\
gnuplot -p -e \"\
terms = '%s';\
events = '%s';\
color = '%s';\
set terminal wxt size 1280,396 font 'Arial,12';\
set grid;\
set autoscale noextend;\
set timefmt '%%Y-%%m-%%d, %%H:%%M:%%S';\
set xtics format '%%H:%%M:%%S';\
set xdata time;\
set title terms.': %s / Time';\
set mouse mouseformat 3;\
set boxwidth 1;\
plot [][*:%d] \
terms every ::%d::%d using 1:%d linecolor rgb '0x'.color with steps notitle, \
terms every %d::%d::%d using 1:%d linecolor rgb '0xb0'.color linewidth 2 \
smooth bezier notitle, \
events using 1:%d linecolor rgb '0xd0'.color with boxes notitle\"\
 2>&1 | grep -v 'line 0: warning: Skipping data file with no valid points' >&2"
# Line immediately above suppresses the warning printed for the no events case.

	make_term(spo2,
		  NAME_SPO2, FIELD_SPO2,
		  SPO2DROP, SPO2TIME, SPO2HIGH, SPO2LOW,
		  GP_EVENT_SPO2, GP_COLOR_SPO2)
	make_term(pr,
		  NAME_PR, FIELD_PR,
		  PRDROP, PRTIME, PRHIGH, PRLOW,
		  GP_EVENT_PR, GP_COLOR_PR)
	if (!NOPLOT) {
		clear_events_file(spo2)
		clear_events_file(pr)
	}
}

{
	# Skip header.
	if (NR == 1)
		next

	# Skip falling asleep time.
	if (NR <= ONSET_SEC)
		next

	# Buffer the records for the size of the awaking time at the end.
	if (!awakebottom) {
		awakebottom = NR
		awaketop = NR
		awakesize = NR + AWAKE_SEC
	}
	if (awaketop < awakesize) {
		awake[awaketop++] = $0

		next
	}

	# Push the newest record and take the oldest record for
	# analysis. The awake window "moves" through the records.
	awake[awaketop++] = $0
	$0 = awake[awakebottom]
	delete awake[awakebottom++]

	if (!begin)
		begin = get_timestamp()
	end = get_timestamp()
	analyze_term(spo2)
	analyze_term(pr, NOEVENTPR)
	distribute_spo2(SET_DISTRIBUTION)
}

END {
	if (error)
		exit error

	if (NR == 0) {
		if (ARGV[1] == HELP_REQUEST)
			exit
		exit_error("no data")
	}
	# Have enough seconds to detect at least one, longest event.
	if (NR < ONSET_SEC + AWAKE_SEC +\
	    (SPO2TIME > PRTIME ? SPO2TIME : PRTIME) + EVENT_TRIGGER)
		exit_error("insufficient data")

	RECORD_SECONDS = NR - ONSET_SEC - AWAKE_SEC
	RECORD_HOURS = RECORD_SECONDS / MIN_IN_SEC^2

	printf "\
Thresholds\n\
	%8s%8s%8s%8s\n\
	%8s%8s\n\
",
		"Drop,", "Time,", "High", "Low",
		"1/#", "sec/#"
	print_term_thresholds(spo2)
	print_term_thresholds(pr)
	printf "\
Analysis of %s--%s, %.1f min of duration\n\
	%8s%8s%8s%8s%8s%8s%8s%8s%8s\n\
		%8s%8s%8s%8s		%8s%8s\n\
",
		begin, end, RECORD_SECONDS / MIN_IN_SEC,
		"Average", "Events,", "Spent,", "Mean,", "Index,",
		"Max", "Min", "Higher,", "Lower,",
		"#", "min", "sec/#", "#/h", "min", "min"
	print_term_analysis(spo2)
	print_term_analysis(pr)
	printf "\
%8s%8s%8s\n\
%8s%8s%8s\n\
",
		spo2["NAME"], "Time,", "Time,",
		"below", "min", "%"
	distribute_spo2()

	if (NOPLOT || system(GP_TEST))
		exit

	plot_term(spo2)
	plot_term(pr)
}
