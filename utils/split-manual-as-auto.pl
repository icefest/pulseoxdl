#!/usr/bin/perl

# split-manual-as-auto.pl - split "Manual" record as "Auto" (pulseoxdl)
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

# A downloaded record, stored by the "Manual" mode on device, may be
# split as if it were stored by "Auto" mode and downloaded.
#
# This program takes a CSV file name and looks into it for SpO2 and PR
# artifacts. If they are found, it inserts the ".tosplit" between the
# timestamp name and the ".csv" extension of the original file, and
# creates, at the working directory, new corresponding CSV file(s)
# with valid measurements only. The name of a CSV file and its
# extension do not have to be the timestamp and ".csv".
#
# Optionally, with the SMAA_REMOVE_USED=1 in the environment, it may
# remove the used for splitting CSV file.

use strict;
use warnings;
use autodie qw(:all);

use constant {
    ARTIFACT_SPO2 => 127,
    ARTIFACT_PR => 255,
    FIELD_DATE => 0,
    FIELD_TIME => 1,
    FIELD_SPO2 => 2,
    FIELD_PR => 3,
    SEPARATOR_FIELDS => ', ',
    SEPARATOR_LINES => "\r\n",
};
use constant ARTIFACTS => ARTIFACT_SPO2 . SEPARATOR_FIELDS . ARTIFACT_PR .
    SEPARATOR_LINES;

die "single SpO2 and PR measurements CSV file expected\n" if $#ARGV != 0;

my $csv = $ARGV[0];

die "the specified file does not exist\n" if ! -e $csv;
die "the specified file is empty\n" if -z $csv;
die "cannot read the specified file\n" if ! -r $csv;

my $tosplit = join '.tosplit.', split /\./, $csv;

die "the specified file has no extension\n" if $csv eq $tosplit;

open my $fh, '<', $csv;
my ($header, $measurements);
$header = <$fh>;
{
    local $/;
    $measurements = <$fh>;
}
close $fh;

die "no measurements found\n" if !$measurements;
die "no artifacts, nothing to do\n" if $measurements !~ ARTIFACTS;

rename $csv, $tosplit; # Splitting may create a file with the same name.

my ($opened, $split);
for (split /^/, $measurements) { # May have or be artifacts.
    my @f = split SEPARATOR_FIELDS;
    if ($f[FIELD_SPO2] != ARTIFACT_SPO2 && $f[FIELD_PR] != ARTIFACT_PR) {
	if (!$opened) {
	    open $split, '>',
		$f[FIELD_DATE] =~ s/-//gr . $f[FIELD_TIME] =~ s/://gr . '.csv';
	    $opened = 1;
	    print $split $header;
	}
	print $split $_;
    } elsif ($opened) {
	close $split;
	$opened = 0;
    }
}

unlink $tosplit if !!$ENV{'SMAA_REMOVE_USED'};
