#!/usr/bin/perl

# join-auto-as-manual.pl - join "Auto" records as "Manual" (pulseoxdl)
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

# The downloaded records, which were stored on the device by the
# "Auto" mode, may be joined as if they were a single record of the
# "Manual" mode. This "Manual" mode record emulation may be done by
# joining all the "Auto" mode records (which should be of the last
# night) together and filling the time gaps between them with the SpO2
# and PR values which would be stored by the "Manual" mode. The result
# would be a single measurements file with the timestamp name of the
# first record and which would end with the last datum of the last
# measurements file.
#
# This program does that joined file creation and inserts the
# ".joined" between the timestamp name and the ".SpO2" extension of
# the new file.
#
# Optionally, with the JAAM_REMOVE_USED=1 in the environment, it may
# remove the used SpO2, and corresponding CSV, files at the working
# directory, to possibly alleviate the user from having to do this
# manually every day.
#
# The program is silent if everything goes well. It can produce status
# output with the JAAM_VERBOSE=1 in the environment: the created
# file's name is printed and, if removal of the used files is
# requested, the names of the removed files are printed too.

use strict;
use warnings;
use autodie qw(:all);

use File::Copy 'copy';
use File::Spec::Functions 'curdir';
use Time::Local 'timegm_modern';

opendir my $dh, curdir;
my @spo2s = sort grep { /^\d{14}\.SpO2$/ } readdir $dh;
closedir $dh;

die "no expected SpO2 files\n" if !@spo2s;
die "single expected SpO2 file, nothing to do\n" if @spo2s == 1;

my $joined = join '.joined.', split /\./, $spo2s[0];

die "the file already exists: $joined\n" if -e $joined;

copy $spo2s[0], $joined or die "copying the oldest SpO2: $!\n";

sub set_data
{
    open my $fh, '<', $_[0];
    my $data;
    {
	local $/;
	$data = <$fh>;
    }
    my ($year, $month, $day, $hour, $min, $sec, $length, $datums) =
	unpack 'x1056V7a*', $data;
    $_[1] = timegm_modern $sec, $min, $hour, $day, $month - 1, $year;
    $_[2] = $length;
    $_[3] = $datums;
    close $fh;
}

my ($jtime, $jlength, $jdatums);
set_data $joined, $jtime, $jlength, $jdatums;
undef $jdatums; # Already used.

open my $joinedfh, '+<', $joined;
seek $joinedfh, 0, 2; # To EOF.
for my $spo2 (@spo2s[1 .. $#spo2s]) {
    my ($time, $length, $datums);
    set_data $spo2, $time, $length, $datums;
    my $gap = $time - ($jtime + $jlength);
    $jlength += $gap + $length;
    print $joinedfh "\x{7f}\x{ff}" x $gap, $datums;
}
seek $joinedfh, 1080, 0; # 1080 = 1056 + 6 * 4
print $joinedfh pack 'V', $jlength;
close $joinedfh;

sub get_preference
{
    return !!$ENV{"JAAM_$_[0]"};
}

my $verbose = get_preference 'VERBOSE';

print "created: $joined\n" if $verbose;

exit unless get_preference 'REMOVE_USED';

unlink @spo2s, map { s/SpO2/csv/r } @spo2s;
if ($verbose) {
    local $\ = $, = "\n";
    print 'removed:', map { s/(SpO2)/{csv,$1}/r } @spo2s;
}
