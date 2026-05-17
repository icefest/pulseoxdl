#!/usr/bin/perl

# spo2-to-csv.pl - pulseoxdl SpO2 file to CSV file converter
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

# Do the reverse of CSV to SpO2 conversion: take SpO2 content on stdin
# and print equivalent CSV content on stdout.

use strict;
use warnings;

use POSIX 'strftime';
use Time::Local 'timegm_modern';

# SpO2 input on stdin.

my $data;
{
    local $/;
    $data = <>;
}
my ($year, $month, $day, $hour, $min, $sec, $length, @datums) =
    unpack 'x1056V7C*', $data;

$length *= 2; # Correspond to datums count (2 per measurement line).

die "meta length disjoint to datums present\n" if $length != @datums;

my $time = timegm_modern $sec, $min, $hour, $day, $month - 1, $year;

# CSV output on stdout.

print "DATE,TIME,SPO2,PULSE\r\n";
for (my $i = 0; $i < $length; $i += 2) {
    print +(strftime '%Y-%m-%d, %H:%M:%S, ', gmtime $time++),
	$datums[$i], ', ', $datums[$i + 1], "\r\n";
}
