#!/usr/bin/perl

# csv-to-spo2.pl - pulseoxdl CSV file to SpO2 file converter
# Copyright © 2024 Donatas Klimašauskas
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

# Take the CSV content, produced by "pulseoxdl" when saving the
# downloaded record from the device, on stdin and print it converted
# to the equivalent SpO2 content on stdout.
#
# The CSV expected single measurement line format is:
# <Date %Y-%m-%d>, <Time %H:%M:%S>, <SpO2 \d{2}>, <PR \d{2,3}>\r\n

use strict;
use warnings;

use Cwd 'realpath';
use File::Copy 'copy';
use File::Basename 'dirname';
use File::Spec::Functions qw(catfile updir);

# The first measurement line has the record's start timestamp. The
# number of the measurement lines (2 datums per line) is the length of
# the datums of the binary bottom, doubled. All 7 fields of the middle
# are 4 Bs long LSBs.

# CSV input on stdin.

die "unexpected CSV header\n" if <> ne "DATE,TIME,SPO2,PULSE\r\n";

my @measurements = <>; # Get all the measurement lines.

# SpO2 output on stdout.

# Fixed header data that every SpO2 has.
chdir dirname realpath $0;
copy((catfile updir, 'data', 'header'), \*STDOUT) or die "header: $!\n";

# Middle of the metadata. In the external parentheses: extract the
# record's start timestamp of 6 fields. The formatting as ephemeral
# ISO 8601 is for simpler splitting to the time elements. The 7th
# field is the record's length: the number of measurements.
print pack 'V7', (split /[-T:]/, (substr $measurements[0], 0, 20) =~ s/, /T/r),
    scalar @measurements;

# Measurements bottom.
print pack 'C2', (split /, /)[2, 3] for @measurements;
