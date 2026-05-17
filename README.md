# About

**pulseoxdl - pulse oximetry downloader**

Pulse oximeter target device with USB HID (from hereon -- device):

* Contec CMS50E.
  * Hardware: 2.0.0.
  * Firmware: 2.0.2.

Pulse oximeters known to work with `pulseoxdl`:

* Beurer PO 80 (CMS50E).
  * Hardware: 2.0.0.
  * Firmware: 2.0.5.
* Pulox PO-400 (CMS50F).
  * Hardware: 2.0.0.
  * Firmware: 2.0.9.

Apparently, an older device reports version numbers for hardware
and/or firmware formatted as "X.Y", and its USB looking cable for
communication with PC has an in-built USB to UART bridge controller,
while a newer device reports the versions formatted as "X.Y.Z", and a
regular USB cable is used.

Device can have a record stored by "Auto" or "Manual" mode, set at its
"Record Menu". Downloading records stored by either of the modes is
supported by the software of the project.

Live data can as well be monitored and stored directly in PC or echoed
(the mode of the recording, or its absence, does not matter).

Specify device file, using the HIDRAW driver, as an argument to the
CLI program and it will communicate with the device to synchronize
date and time of the PC to the device and download from it all the
stored records to the working directory in manufacturer's CSV and SpO2
(binary) formats or monitor live data and save it in those formats, or
echo the live data to the console (user decides where it goes and how
it is saved). Filenames will have the date and time of the start of a
record and the extensions of the formats.

Records stored by the "Auto" mode may be saved as if stored by the
"Manual" mode.

Perfusion Index (PI) echoing is available (experimental). For a
device, which does not show it on its screen, an approximate PI can be
computed. Thus, the PI in the output may stand for *true* PI, if a
compatible device supplies it (shown on screen), or *Plethysmograph
Index*, an abbreviation of *Plethysmograph Inferred Perfusion Index*
(PIPI), devised for the project -- computed from the plethysmograph
during a second of live measurements.

The documentation is based on the observation of the device
communication with its manufacturer's software. The code is based on
the insights, obtained from the observation, and written to liberate
the user of the device from the need to use the proprietary software
for downloading records.

The documentation is technical; supplements the code and vice versa.

The code is written in C99 with few GNU extensions (time and
endianness). It may be used standalone, but would be much more useful
in sleep tracking and analysis free software, like [OSCAR].

[OSCAR]: https://www.sleepfiles.com/OSCAR/

# How Manufacturer's and this PC Software Differ

## Manufacturer's Software

Source code is not available (proprietary).

Host OS: MS-Windows.

Device manufacturer's PC software:

`Smart Device Assistant V3.1.0.1 Setup.exe`

Its SHA1:

`ee579cbb93bf42a8f8e3891e988aed35ab18643e`

Saves files in dedicated, top directory. File names have the format:

`_<user name>_<user index>_<record index>_<file's save timestamp>.{extension}`

`{extension}` may be one of `{csv|{SpO2|part}|txt}`. The number of
files saved per a record variate.

Always deletes "Auto" mode records from a device after they are
downloaded, but keeps the "Manual" mode record.

Saves live data in different CSV and SpO2 (binary) formats than of the
stored records: includes PI, even when a device has no such feature.

Allows to save up to 3 days (72 h) of live data.

## This Software

This is free software (GPLv3+).

Host OS: any, which can build the binaries and run them.

Saves files in the working directory. File names have the format:

`<record's start timestamp>.{csv|SpO2}`

Decoded data points are saved in manufacturer's CSV and SpO2 formats;
2 files per record.

Can leave the downloaded files on the device (they can be downloaded
again), but by default deletes them.

Saves live data in the same CSV and SpO2 formats as if they were
stored in the device, i.e. as if they were not monitored live.

Allows to save up to 30 days (arbitrary choice) of live data.

The live data may be echoed to console on its standard out. The format
is the same as of the stored CSV. No data saving. No duration limit.

PI echoing may be enabled, even for a device, which does not have it
shown on its screen.

# Communication Protocol Parts and Data Interpretation

## Overview

The device is HID, with which the communication happens employing USB
(Bluetooth could work too), through HIDRAW driver, with 64 B HID
reports, both ways. The reports are not numbered.

Bs (from hereon, and in code comments, "Bs" is the abbreviation of
"bytes", for brevity) in the HID reports have manufacturer's commands
with their data. The commands variate in length, but always end with a
single checksum B, which is a sum of all the previous Bs of a command
to a 7 bits B.

Manufacturer's commands with measurements data have the first B using
8 bits, but all the rest Bs using 7 bits. If measurement values are
encoded in nibbles, high nibble is 3 bits and low nibble is 4 bits.

### Stored

Measurements data can be stored in "Auto" or "Manual" mode.

The device provides the metadata about record(s) stored on it, which
includes the date and time when a record was started, the number of
measurement data points (from hereon -- datums) of the record -- the
length of the record.

Maximum length of a record is 24 h.

The datums of a record are sent in batches per a measurement. SpO2 and
PR are requested and downloaded separately, and differently for each
stored mode.

Datums containing command (from hereon -- datums command) may cross
the HID report boundary.

#### "Auto" Mode

Maximum length of a record, however, does not include the number of
adjustment Bs, which depends on the dynamics of a measurement. The
adjustment Bs may be anywhere inside datums and their number cannot be
known in advance. 2 datums of a measurement are encoded in a single B.

#### "Manual" Mode

"Manual" stored record encoding is rather different to "Auto". Albeit
single B is used to represent 2 datums, but there are no adjustments
employed -- the running absolute datum is used.

### Live

Live data retrieval has to be requested from the device. The device
provides two kinds of live data commands:

1. Pulse amplitudes.
1. Measurements.

Both have to be requested separately. Both are stopped with a single
command.

When PR is under 128 BPM, the HID reports with the live data commands
have the absolute, not encoded, amplitudes and measurements.
Otherwise, PR becomes relative and needs an addition.

Every 5 s the device gets a ping command sent to keep it streaming
live data for as long as PC requests to stop. It does not matter
whether finger is inserted into device or not.

## Datums and Interpretation

### Stored in "Auto"

#### Datums Command

Datums command is 30 Bs in size. Datums in it start from the 9th B and
end at the 29th B. In the datum Bs, there may be 2 consecutive
adjustment Bs; they may cross the packet boundary. The first Bs of
datums command start with those adjustment Bs.

The 2 adjustment Bs are used to set a current, top, decoded datum of a
measurement, from which encoded datum nibble values are to be
subtracted to decode the next datum. Datums are encoded in high and
low nibbles of a datums B.

0x7f B in datums is padding, which can be ignored, since datum left
equaling to 0 indicates to not read another report. It is assumed that
if the last datum is in the last B's low nibble (before checksum B) in
the report, new datums command to indicate that there are no more
datum is not sent to PC (a few cases are seen where the single padding
B of 0x7f is right before checksum and no new datums command of 30 Bs
is written; all the rest Bs are 0).

#### Datums B Interpretation

Datums command has 3 flag Bs. 7 least significant bits of each are the
flags for respective datums Bs. A datums B packs 2 encoded datums. For
the datums B, the flag set for it indicates whether singleton, duple
or adjustment interpretation should be done:

1. *Singleton*. Range 0x[0..7]. Bs with them have flag unset. All
other Bs have flags set.

2. *Duple*. Range 0x[0..6] and 0x[8..e]. High nibble is always lower
than the low nibble. To decode high nibble datum, subtract 0x8 more.

3. *Adjustment*. Always 2 consecutive Bs and have high nibbles 0x7
(duple has no 0x7 as a high nibble value). First adjustment B's low
nibble is the multiplier to 16. Second adjustment B's low nibble is an
addition to the result of the previous multiplication.

For singleton and duple, low nibble may be 0xf, which indicates the
value to drop and is followed by 2 adjustment Bs (if not already in
adjustment or no more datum).

### Stored in "Manual"

#### Datums Command

Datums command is 20 Bs in size. Datums in it start from the 6th B and
end at the 19th B.

Each datums command has the first B of datums always as an absolute
value. Then, the rest of the datums Bs encode 2 datums per B, in high
and low nibbles. A single datums command may have up to 27 datums.

When a finger is not inserted into device, the "Manual" mode keeps on
recording, saving datums as *artifacts*: SpO2 as 127 and PR as 255.

It seems that at the beginning of a recording, the measurements of the
first 27 s are always stored as artifacts; a finger may be in, and
device screen may show measurements, and a recording may be started
while the finger is in and measurements are shown.

Since the first B of a datums command has to be an absolute value, the
maximum delay of the start of storing of the valid measurements may be
up to 26 s: when the first artifact is stored as an absolute value and
valid measurements start to get in just after it, they are replaced
with the artifacts, and the valid measurements are stored from the
next absolute value. The principle holds for the interspersed span(s)
of time when a finger is pulled out and inserted back in.

The recording may be stopped, while a finger is in, without artifacts
stored at the end of a record.

#### Datums B Interpretation

The low nibble's MSb may be set. If so, it indicates a subtraction of
the value of the rest of the bits from a running absolute datum to get
the new current datum. If it is unset -- the bits are to be added to
the datum.

The high nibble does not have the addition/subtraction indicating
bit. To compensate for this, there are 2 flag Bs. These cover the 14
Bs of datums in the corresponding 7 bits in each of the flag Bs.

It is similar to the "Auto" mode stored record, but it is used
differently. If a corresponding flag of a B is set, then the value in
the high nibble has to be subtracted from a running absolute datum for
the new current datum. Otherwise, added. The flag state has no meaning
for the low nibble.

It is assumed that the maximum 3-bit value in a nibble -- 7, covers
all possible physiological cases of change per a second. The SpO2 is
not likely to change by that much, but it is, though, observed that,
while exercising, the PR can get close to the maximum. Whether it can
cross it, how device would encode it if it would cross it, would it
encode it, are unknowns.

There may be more artifact looking Bs at an end of a transmission, and
one artifact B, other than the first, is decoded as 2 artifacts for a
measurement. Hence, the 0 count of datums left is used to end the
decoding; the same as for "Auto".

### Live

Live measurements data starts with data commands about pulse wave and
pulse bar amplitudes. Then, data commands with SpO2 and PR are
sent. This is repeated until stopped.

There are 3-4 commands in every live data HID report. No HID report
boundary crossings. The majority HID reports contain 3 commands of 6
Bs in length -- the pulse amplitudes. At every second, live data HID
report contains 1, out of the 3, 8 Bs long command -- measurements.
The order of the commands in such HID report could be one of (Bs):
6-6-8, 6-8-6, 8-6-6. The order is not guaranteed. At random times,
rarely, there are 4 commands in one HID report: 1 8 Bs, and 3 6 Bs.

Measurements are sent every 20th HID report. Usually, one second
contains 59 amplitude data commands and 1 measurements data command.

The type, of amplitudes or measurements, are encoded in the second B
of the 6 Bs and 8 Bs data commands; 0x00 and 0x01 accordingly.

When PR is at 128 BPM or above, preceding B changes from 0x4 to 0x6
and the PR B's value becomes relative. To get the absolute value of
the PR, the 128 has to be added to its relative value. This gives the
range 0x[0..ff] of possible PR values.

The last two Bs, before checksum B, are for PI. When device does not
supply it, they are static and set to 0x7f00. When device supplies it,
the Bs encode, in 7 bits per B, a fraction, but two orders of
magnitude higher, as an integer (the same as described in 5th page of
"Communication protocol of pulse oximeter V7.0", for an UART device).

## Individual Records

Even though manufacturer software's PC GUI allows to select individual
records, but the individually selected records (if not all) cannot be
downloaded, nor they can be deleted. By the GUI software, all "Auto"
records are downloaded at the same time and deleted, but the single
"Manual" record is kept. It can be deleted manually through the GUI.

# Testing

## Who should Perform

Testing is meant for development. If just everyday usage is cared
about, please read the **Usage**.

## Macros Used

To test the software executables, these compiler preprocessor
arguments to the `-D` option are used:

* `SIMULATOR` -- use test device, i.e. all software ran locally.

* `DEBUG` -- print general debugging output.

* `DEBUG_WRITE` -- monitor HID reports of exchanges of the software
with the real device (except the download of datums commands, that
could be voluminous; other tool is to be used for those). When testing
with the simulator, printing written reports duplicates read reports.

## Automation

### Fully Automated

Fully automated test is with the simulator, i.e. no real device is
needed. The test is performed with real measurements, with over 10000
datums for the stored record downloading and over 12000 data commands
for a live stream. To build executables and run the test on them:

`$ make testlocal`

If the last printed line is the "pass", the executables building and
foreseen tests ran successfully.

This will test the conversion and joining utilities too.

### Partially Automated

Testing with the real device involves manual work, which has to be
done for records or live data download from the device and reviewing
detailed program output to console. To build the executable:

`$ make testdevice`

# Usage

## Installation

### As Normal User

To build and install the executable for everyday usage of downloading
measurement records, stored on device, or monitoring the measurements
live, in the installation directory (below has an example directory,
which must exist, within the normal user's home directory), run:

`$ make && make DESTDIR=~/pulse-oximetry/ install`

If the last printed line is the "done", the `pulseoxdl` executable is
ready for everyday usage.

### As Root User (Optional)

Running the following program will setup udev rules file
`pulseoxdl.rules` in local administration directory (i.e.,
`/etc/udev/rules.d/`; other directory may be specified as argument):

`# udev/setup.sh`

The rules file, on every device turn on, will create constant filename
for the device and make it usable for normal user. If one device is
used at a time, the device file (technically, it is a symbolic link,
through which the device file is accessed) will always be named:

`/dev/cms50e1`

More than one device may be used at the same time. The devices will be
enumerated in a sequence of their turn on. If a second device of the
same model is plugged in and turned on, it will be named:

`/dev/cms50e2`

The rules file utilizes a system group "plugdev" to allow
communication with the device for the normal user. If the group is not
present on the OS, the udev setup program will create it and add
normal users of the OS to it for the above to work.

After running the setup program, normal user may communicate with the
device through the device file immediately. Just replace, in the usage
examples below, the `/dev/hidraw<N>` with the `/dev/cms50e1`. E.g.:

`$ ./pulseoxdl /dev/cms50e1`

## Everyday

Get a build version number and a brief usage help of the program:

`$ ./pulseoxdl -h`

Plug in the device to PC and turn it on.

### Finding Device File and Granting Access

If the udev rules file is not setup as described, this subsection is
to be followed every time the device is plugged in and turned on, in
order to communicate with it as normal user.

Find out device ID:

`$ lsusb`

Find out its HIDRAW device file:

`# echo /dev/$(dmesg | grep -i <device ID> | grep -oP 'hidraw\d+')`

Make normal user to be able to communicate with the device:

`# setfacl -m u:<normal user>:rw /dev/hidraw<N>`

### Examples

Synchronize PC time to device and *move* its records to the PC:

`$ ./pulseoxdl /dev/hidraw<N>`

Or do the same with:

`$ ./pulseoxdl /dev/hidraw<N> move`

Synchronize PC time to device and *copy* its records to the PC:

`$ ./pulseoxdl /dev/hidraw<N> copy`

Or just synchronize PC *time* to device:

`$ ./pulseoxdl /dev/hidraw<N> time`

The downloaded records will be in the directory from where the
executable was ran. If the same records are downloaded again, they
will overwrite the previously downloaded records with the same
filenames and their same contents.

For monitoring, insert your finger in the device, wait for SpO2 and PR
measurement digits to appear on the device (should happen within a few
seconds), run the following command to synchronize PC time to device
and *live* stream the measurements to the PC:

`$ ./pulseoxdl /dev/hidraw<N> live`

This live information will be shown:

```
Live measurements (remove finger to stop and save)
SpO2:  98%, PR:  68 BPM, PB: |---------       |
```

*PB* is acronym of Pulse Bar.

Pull out your finger to stop the live monitoring and the recorded
measurements will be saved in the directory, where the executable was
ran from, and the line with live data on PC will be replaced with
something like this:

```
2021-04-30T11:28:39--2021-04-30T11:32:10 saved as 20210430112839
```

The first date and time (before the "`--`") is of the start of the
live streaming and the second duo is of the its end. The saved file
name is formatted the same as described in **This Software**.

Instead of the live data being presented as a gauge and saved in PC at
the end of the monitoring, the live stream may be printed to console
with the *echo*:

`$ ./pulseoxdl /dev/hidraw<N> echo`

The datums are printed on the standard out in the same CSV format as
when it is saved in PC (it will print the header as the first line).
This action does not save files at the end of the monitoring, thus has
no duration limit. The program will stop if the finger is pulled out.

The echo action makes it possible to use various CLI utilities to
handle the live data freely. A few examples follow.

1. Use an unlimited duration recording of the live monitoring with the
same file naming format as with the other data saving actions:

   `$ ./pulseoxdl /dev/hidraw<N> echo >$(date +%Y%m%d%H%M%S).csv`

1. Save elsewhere than working directory with seconds from the Epoch:

   `$ ./pulseoxdl /dev/hidraw<N> echo >/home/<user>/$(date +%s).csv`

1. Monitor live data locally and send it over a network:

   `$ ./pulseoxdl /dev/hidraw<N> echo | tee >(nc -q 0 <host> <port>)`

   The other end of the connection (started first) could retrieve the
   data, monitor and save it (or pipe to graphing utility, etc.):

   `$ nc -l -p <port> | tee /home/<user>/<file>`

### Notes

If a long live streaming of measurements is planned, it is probably
prudent to record it on the device too: if live stream is interrupted
for any reason, the record on PC may not be saved, but will be there
on the device. (Use "Auto" or "Manual" recording mode to be able to
download it with the `pulseoxdl`.)

A similar situation may occur:

* Device clock is ahead of PC time.
* The finger is put in.
* Device starts recording.
* User runs the program with the live action.
* Device gets its time synchronized to the PC time.
* The finger is pulled out and the recording stops.
* Device stores the record with the prior-synchronization start time.
* The program saves the record with the PC time of the program's
  start, with the corresponding datum timestamps.
* User downloads the record from the device with the move or copy
  action and has the prior-synchronization time as the file base-name
  and datum timestamps, which *cannot* be compared to what device has
  stored with what was saved by the program with the live action.

To avoid that kind of situation:

* User runs the program with the time action.
* Device time is synchronous with the PC time.
* The finger is put in.
* Device starts recording.
* User runs the program with the live action.
* The finger is pulled out and the recording stops.
* Device stores the record with synchronous-to-PC start time.
* The program saves the record with the PC time of the program's
  start, with the corresponding datum timestamps.
* User downloads the record from the device with the move or copy
  action and has the synchronous-to-PC time as the file base-name and
  datum timestamps, which *can* be compared to what device has stored
  with what was saved by the program with the live action.

The device updates what it shows on the screen of the time not by the
second, i.e. internally it keeps the time accurately to a second, but
updates its screen every 60 s from the moment when it gets the new
time data sent and set.

When device records and PC records too, there may be small datum
discrepancies due to what device records and what it shows on its
screen and sends to PC (the latter two appear to match). Thus, there
may be differences when comparing the same time span of the whole
record, downloaded from the device, and a subset of that live
recording, which is saved by the PC. The datums recorded by the PC
should be more accurate. Additionally, PC may save a datum more, and
live starts gathering datums 1-2 s later, because the device delays
their sending by that long.

The time printed for the echo action line of measurements is the time
of the PC. For long running live monitoring, the time of the device
could diverge from time of the PC, but PC time may be more accurate
(or accurate with, e.g., NTP).

If the live streaming is interrupted (e.g., by typing `Ctrl`+`C`) and
the `pulseoxdl` is rerun soon after, it may exit with the "checksum
failed" error. If you get this error, just run `pulseoxdl` again.

If `Xorg` (e.g., v1.20.11) is used, turning on the device multiple
times per session while physically connected to PC may result in GUI
usability problems (may even have to reboot the OS). This is because
the device is being taken to be a keyboard-joystick, which confuses
the `Xorg`. With each such turn on during the same session the
confusion may increase. This is not caused by the `pulseoxdl`. To
avoid the confusion, add to, e.g., `/etc/X11/xorg.conf`:

```
Section "InputClass"
	Identifier "Pulse Oximeter: Contec CMS50E with USB HID"
	MatchUSBID "28e9:028a"
	Option "Ignore" "1"
EndSection
```

Restart the `Xorg` (or reboot the OS) for the change to take effect.

# Utilities

The following programs will allow to do back and forth conversions of
textual and binary contents of the CSV and SpO2 files, and provide a
possibility to emulate the download of "Auto" mode records as a
"Manual" mode record, saved as a single SpO2 file, as well as to reset
timestamps in related CSV and SpO2 files.

Albeit the conversion utility programs will work with the files
produced by manufacturer's software and `pulseoxdl`, and the emulation
utility program will work with the files produced by `pulseoxdl` or by
any software if file names and content are formatted accordingly, they
are written to work with trusted source of files, i.e. `pulseoxdl`.

Perl 5 language interpreter is required.

## CSV to SpO2

Convert CSV file's content to SpO2 file's content by specifying the
corresponding files on stdin and on stdout, e.g.:

`$ ./csv-to-spo2.pl <<CSV> ><SpO2>`

The program file is *dependent*: has to be where it is -- at the
`utils` directory of the project (it uses a relative path-name to get
the static SpO2 header binary).

## SpO2 to CSV

Convert SpO2 file's content to CSV file's content by specifying the
corresponding files on stdin and on stdout, e.g.:

`$ ./spo2-to-csv.pl <<SpO2> ><CSV>`

The program file is independent: can be in any directory.

## Join "Auto" as "Manual"

Running the program at the working directory will join all not-joined
SpO2 files present in it to a *joined* SpO2: all the used SpO2 files'
datums in it and gaps between them filled with the values as would be
filled by the "Manual" mode, and the name having the same timestamp as
the first -- the oldest, SpO2, with the "joined" added.

`$ ./join-auto-as-manual.pl`

The program file is independent: can be in any directory.

If you prefer to be informed what the program has done successfully:

`$ JAAM_VERBOSE=1 ./join-auto-as-manual.pl`

Do the same and remove the used SpO2, and corresponding CSV, files
from the working directory:

`$ JAAM_VERBOSE=1 JAAM_REMOVE_USED=1 ./join-auto-as-manual.pl`

Do the same, but quietly:

`$ JAAM_REMOVE_USED=1 ./join-auto-as-manual.pl`

To *emulate* the download of the "Manual" mode SpO2 record to the
working directory, while datums are actually stored in the device by
the "Auto" mode, and provided that the working directory has no other
not-joined SpO2 files, you could run:

`$ ./pulseoxdl /dev/hidraw<N> && ./join-auto-as-manual.pl`

## Download "Auto" and Join as "Manual"

If you would like to have the "Auto" records downloaded and
corresponding SpO2 files joined, and you will be using the joined
files only, thus would like the used for joining and related files
removed automatically, run the utility as `pulseoxdl` at the working
directory, e.g.:

`$ ./pulseoxdl-as-manual.sh /dev/hidraw<N>`

The result will be the download and the new joined SpO2 file. There
can be other joined SpO2 files in the working directory, they will not
be touched. There should not be other not-joined SpO2 files, because
all such files will be used by the `join-auto-as-manual.pl`, but it
will not overwrite an existing joined SpO2 file.

It is a few lines of code wrapper script around the `pulseoxdl` and
`join-auto-as-manual.pl`. All the three programs must be at the same
directory. E.g., have them where the `pulseoxdl` is installed.

The working directory does not have to be the same with the program
files. E.g., you may have a dedicated directory for storing only the
joined SpO2 files; have the directory as a working directory and run
the `pulseoxdl-as-manual.sh` as a relative or absolute path-name.

It is fast to modify the utility to different preference: turn on (1)
or off (0) the environment flags for the `join-auto-as-manual.pl` in
the `pulseoxdl-as-manual.sh`.

## Split "Manual" as "Auto"

The "Manual" mode record may as well be *split* as if it were
downloaded from the "Auto" mode. The artifacts would be removed and
only valid measurements be saved to separate files. Useful if you
would like to analyze spans of valid measurements with `analyze.awk`
(please read the **Analysis** below), since it should not be used with
a CSV file containing artifacts.

The splitting utility program has to be given only a CSV file with
measurements. Running the program will rename the original CSV file by
adding "tosplit" after the timestamp and create, at the working
directory, corresponding CSV files with timestamp names of when the
valid measurements were started to be stored.

`$ ./split-manual-as-auto.pl 20250705165923.csv`

The program file is independent: can be in any directory.

The to-split CSV file may be removed automatically:

`$ SMAA_REMOVE_USED=1 ./split-manual-as-auto.pl 20250705165923.csv`

The SpO2 file, if present, related to a CSV file of the "Manual" mode,
will not be touched in either of the invocations. Corresponding SpO2
files of the created CSV files will not be created. (They can be
created with the `csv-to-spo2.pl` utility of the project.)

## Reset Timestamps

If a device has been not used for a longer period of time, its clock
may drift off significantly. If it is, then, used to make a record of
measurements without first synchronizing the clock with the real time
(by hand or by connecting to PC and using the `time` or any other
`pulseoxdl` action, provided PC time is synchronized), later, when
`pulseoxdl` downloaded to PC, the record will have the time drift.

E.g., if a record, made on a device, which had its time off by plus
ten minutes, got downloaded to PC, record's time can be corrected:

`$ ./reset-timestamps.sh 20210515120642.csv 2021-05-15 11:56:42`

Corresponding CSV and SpO2 files will be created in the CSV source
directory, and the old pair will be removed from it.

Like `csv-to-spo2.pl`, which it uses, the program file has to be at
the `utils` directory, but it can be run from any working directory.

# Analysis

## Overview

The analysis program `analyze.awk` will take the CSV file, produced by
`pulseoxdl` (or manufacturer's software, which has a datum per second,
or any software produced CSV file, which is formatted such), analyze
it and print a report for SpO2 desaturation events and PR parameters.

According to AASM manual's (2007) VIII.2.B: O2 resaturation could be
considered to terminate an event when it is by at least 2%. This is
the approach implemented.

Manufacturer's software, albeit arbitrarily, treats PR data to produce
events in the same way as SpO2. AASM gives no indication for this and
such *PR "events"* probably have no physiologic significance
(gathering data from the PR channel, as opposed to ECG, would allow to
infer only unclassified tachycardia or bradycardia). By default, they
are not analyzed, reported nor plotted by this program (i.e., report
has zeros for them), but may be if wanted (please read below). AASM
II.1.E described average and highest heart rates cardiac events (as
singulars) are reported (with a few others).

This program detects more SpO2 events (and PR "events" too, if
chosen), than the software which is intended to be used by the
manufacturer of the device. Why manufacturer's software detects less
is not clear.

Default values are the same as used by the manufacturer's software.
(Since 2012, AASM revised desaturation reporting to be by at least
3%.) They, and others, may be changed. The start and end of a
recording may be adjusted too.

**Warning**. The results of the program are not equivalent to the PSG
results. It may only be used to gain a clue of whether there may be a
need for the PSG or as a lax way to monitor when some reliable
knowledge is held about and may be applied comparatively.

## Requirements

1. AWK interpreter: `mawk` v1.3.3.
1. Plotter (not required for a report): `gnuplot` v5.2 patchlevel 6.

## Examples

### Invocation

Get help about the use of the analysis program:

`$ ./analyze.awk -h`

Analyze whole record:

`$ ./analyze.awk data/test/manufacturer/csv`

Consider a desaturation by at least 3%:

`$ SPO2DROP=3 ./analyze.awk data/test/manufacturer/csv`

Sleep onset (the time at the beginning of a recording for, e.g.,
putting device on, getting to bed, typical falling asleep time) or
awake from sleep time (the time at the end of a recording, e.g.,
typical until fully awake time, getting out of bed, removing device)
may be specified to obtain more accurate analysis of desaturation
events during sleep, if not other than sleep record is analyzed.

Remove from the analysis the individual time, in minutes, for how long
it takes to fall asleep and the time it takes until fully awake, or
either:

```
$ ONSETMIN=10 AWAKEMIN=5 ./analyze.awk data/test/manufacturer/csv
$ ONSETMIN=10 ./analyze.awk data/test/manufacturer/csv
$ AWAKEMIN=5 ./analyze.awk data/test/manufacturer/csv
```

Include PR "events":

`$ NOEVENTPR=0 ./analyze.awk data/test/manufacturer/csv`

### Report

The following reports are generated for the test record's data.

#### Default

```
Thresholds
	   Drop,   Time,    High     Low
	     1/#   sec/#
SpO2, %:       4      10      96      90
PR, BPM:       6       8      90      60
Analysis of 2021-05-15T12:06:42--2021-05-15T15:00:26, 173.8 min of duration
	 Average Events,  Spent,   Mean,  Index,     Max     Min Higher,  Lower,
		       #     min   sec/#     #/h		     min     min
SpO2, %:    93.5       5    23.5   281.8     1.7      98      90     3.2     0.0
PR, BPM:    67.1       0     0.0     0.0     0.0     112      55     3.5     5.6
 SpO2, %   Time,   Time,
   below     min       %
      95   141.3    81.3
      90     0.0     0.0
      85     0.0     0.0
      80     0.0     0.0
      75     0.0     0.0
      70     0.0     0.0
      65     0.0     0.0
      60     0.0     0.0
      55     0.0     0.0
      50     0.0     0.0
```

#### With PR "Events"

The **Default** and **With PR "Events"** reports differ by the second
"PR, BPM" line. Both lines accordingly:

```
PR, BPM:    67.1       0     0.0     0.0     0.0     112      55     3.5     5.6
PR, BPM:    67.1      64    37.9    35.5    22.1     112      55     3.5     5.6
```

### Plot

The SpO2 and PR plots are produced by `gnuplot` and are interactive.
Zooming in/out, fitting the graph into the zoom-changed area with `e`
key, measuring with `r` key, reploting with `a` key are very useful,
as are many other possibilities.

Both plots have measurement changes per time graphs and their trend
lines. SpO2 plot has the desaturation events, if any, marked with
square areas. The same marking may be obtained for the PR "events".

#### Default

![SpO2 plot]

[SpO2 plot]: data/img/spo2.png

![PR plot]

[PR plot]: data/img/pr.png

#### With PR "Events"

The SpO2 plot is the same, but the PR plot now has the "events".

![PR with "events" plot]

[PR with "events" plot]: data/img/pr-with-events.png

# Experimental Perfusion Index (EPI)

## Description

Some devices (e.g., CMS50E, CMS50FW) do not display, store, stream the
PI measurement, but many do display and stream the plethysmograph,
which can be held as a representation of the PI.

PIPI is an approximation of the true PI, when a device does not supply
it. In this case, the PI *is* PIPI. The steps to compute the PIPI are:

1. Sum pulse wave values in a second.
1. Find the average value of the values.
1. Subtract minimum value to normalize the baseline.
1. Find the vertical percentage of the absolute range.
1. Map to vertical percentage range of a plethysmograph window.
1. Use the value, of the above steps, of the previous second to reduce
value variability of the current second (moving average of 2
smoothing; as a side effect, first PI will be a half lower).

The EPI feature has to be explicitly enabled, and it can only be used
with the echo action (it has no effect on other actions).

If EPI is enabled without an other option, there will be no indication
in the output of the `pulseoxdl` whether the PI is PIPI or true PI. If
a held device has PI on its screen, the PI of the program is going to
be the PI of the device. Otherwise, it is the PIPI.

PI and a difference of PIPI and PI may be echoed at the same time.
This could be used by someone, who has a device with true PI, to
assess or adjust the PIPI computation.

## Examples

The following examples depict the echoing of the live stream testing
data. (The CMS50E does not have the true PI.)

Enable EPI and use the defaults of 25% maximum vertical mapping for
PIPI and 2 seconds for true PI detection:

`$ PULSEOXDL_EPI= ./pulseoxdl /dev/hidraw<N> echo`

```
DATE,TIME,SPO2,PULSE,PI
2021-04-30, 11:28:39, 95, 81, 1.4
2021-04-30, 11:28:40, 95, 81, 2.1
2021-04-30, 11:28:41, 95, 80, 2.8
2021-04-30, 11:28:42, 95, 80, 2.8
2021-04-30, 11:28:43, 95, 79, 2.8
2021-04-30, 11:28:44, 95, 79, 3.2
2021-04-30, 11:28:45, 95, 77, 3.3
2021-04-30, 11:28:46, 95, 77, 3.1
2021-04-30, 11:28:47, 95, 77, 3.2
2021-04-30, 11:28:48, 95, 78, 3.2
```

Set the PIPI mapping to 20%, instead of the default 25%:

`$ PULSEOXDL_EPI= PULSEOXDL_EPI_PIPI=20 ./pulseoxdl /dev/hidraw<N> echo`

```
DATE,TIME,SPO2,PULSE,PI
2021-04-30, 11:28:39, 95, 81, 1.1
2021-04-30, 11:28:40, 95, 81, 1.7
2021-04-30, 11:28:41, 95, 80, 2.3
2021-04-30, 11:28:42, 95, 80, 2.3
2021-04-30, 11:28:43, 95, 79, 2.3
2021-04-30, 11:28:44, 95, 79, 2.6
2021-04-30, 11:28:45, 95, 77, 2.6
2021-04-30, 11:28:46, 95, 77, 2.5
2021-04-30, 11:28:47, 95, 77, 2.5
2021-04-30, 11:28:48, 95, 78, 2.6
```

Add PIPI minus PI column:

`$ PULSEOXDL_EPI= PULSEOXDL_EPI_DELTA= ./pulseoxdl /dev/hidraw<N> echo`

```
DATE,TIME,SPO2,PULSE,PI,PIPI-PI
2021-04-30, 11:28:39, 95, 81, 0.0, 1.4
2021-04-30, 11:28:40, 95, 81, 0.0, 2.1
2021-04-30, 11:28:41, 95, 80, 0.0, 2.8
2021-04-30, 11:28:42, 95, 80, 0.0, 2.8
2021-04-30, 11:28:43, 95, 79, 0.0, 2.8
2021-04-30, 11:28:44, 95, 79, 0.0, 3.2
2021-04-30, 11:28:45, 95, 77, 0.0, 3.3
2021-04-30, 11:28:46, 95, 77, 0.0, 3.1
2021-04-30, 11:28:47, 95, 77, 0.0, 3.2
2021-04-30, 11:28:48, 95, 78, 0.0, 3.2
```

Try to detect true PI for 5 seconds, instead of the default 2 seconds:

`$ PULSEOXDL_EPI= PULSEOXDL_EPI_DETECT=5 ./pulseoxdl /dev/hidraw<N> echo`

Any combination of the PIPI, DELTA, DETECT can be used.

# Copyright

Copyright © 2021-2025 Donatas Klimašauskas

# License

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or (at
your option) any later version.

Full license is at the COPYING file.
