# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog], and this project adheres to
[Semantic Versioning].

## [1.5.0] - 2025-12-30

### Added

- This file.
- Apt debugging and production compiler options.
- Utility: split "Manual" record as "Auto". Only CSV with valid datums
  should be specified for the measurements analysis program of the
  project. "Manual" record will have some datums as artifacts. The
  utility will create "Auto" like CSV file(s), which can be analyzed.
- "Manual" mode stored record downloading. The `pulseoxdl` will detect
  which storage mode a device is set to and will use appropriate
  communication to handle the stored data (like "Auto").
- In README.md, a note about program version and brief usage
  information availability in the output of the `pulseoxdl` when
  invoked with the `-h` option.

### Changed

- Removed surplus ping HID reports from the `simulator` transmission
  of the live testing. They had no impact on the simulation, but a
  device does not send any pings to a PC.
- When `pulseoxdl` is requested to download stored data, but there is
  no, it prints a different error message: "no record stored" instead
  of "no Auto mode data stored".
- Refactored datums adjustment and decoding flags handling, code for
  uniform downloading of "Auto" mode and "Manual" mode records, and
  their testing.

### Fixed

- Added a symlink for the analysis examples to work again, since, with
  live monitoring addition, measurements file argument of the analysis
  example commands in README.md has been moved to another directory.

## [1.4.0] - 2025-05-05

### Added

- Experimental feature to echo Perfusion Index (PI), if it is
  available, or Plethysmograph Inferred Perfusion Index (PIPI)
  instead, with the `echo` action. The PIPI is computed from the
  plethysmograph data, and is adjustable. PI and a difference of PIPI
  and PI may be echoed at the same time, to assess the computation or
  to adjust it.
- Semantic Versioning extension labels to testing builds. Testing case
  name as a pre-release and its build timestamp as a build metadata,
  appended to testing version core, to distinguish testing from
  production in the help output of the `pulseoxdl` executable.
- Utility: reset CSV and SpO2 timestamps. Useful for adjusting
  timestamps of filenames and of measurements in them when device's
  clock has drifted too much before a downloaded record was started.
- Memory usage checking with `valgrind` (v3.19.0) during local testing
  with the `simulator`.
- Static analysis option of `gcc` (v12.2.0) to detect more possible
  bugs during compilation.
- In README.md, listed a new compatible device: Pulox PO-400. Thanks
  to [@tholin] for informing about this.
- In README.md, the original device names for the re-branded devices,
  and a note about differences in hardware and firmware versions
  reporting by older and newer devices, and that Semantic Versioning
  is used by the project.

### Changed

- Pathnames with whitespaces can be specified for shell scripts.
- In README.md, sub-listed compatible devices' hardware and firmware
  lines for better readability.

## [1.3.0] - 2024-05-28

### Added

- Utility: download "Auto" and join as "Manual". A wrapper program,
  that allows to download many "Auto" SpO2 files and join them to a
  single "Manual" SpO2 file in one invocation.
- Utility: join "Auto" records as "Manual". "Manual" mode record's CSV
  and SpO2 files can be constructed from corresponding files of "Auto"
  mode records.
- Utility: SpO2 file to CSV file converter. Takes input on stdin and
  produces output on stdout.
- Utility: CSV file to SpO2 file converter. Takes input on stdin and
  produces output on stdout.
- In README.md, listed a new compatible device: Beurer PO 80. Thanks
  to [@HelWeis] for informing about this.

### Changed

- Refactored testing.

## [1.2.0] - 2024-04-18

### Added

- The `echo` action. It allows a user to have an unlimited live
  streaming of measurements from a device to a PC (if no recording on
  a device is done at the same time, since it may, or may not, stop
  the streaming at reaching its storage limit, set by manufacturer),
  in the same CSV file format as saved by `move` and `copy` actions.
- The `time` action. It only synchronizes the PC time to a device,
  without any other effect. Convenient if the device was not used for
  a longer period of time and its clock has drifted noticeably, or on
  DST change.
- The `pulseoxdl` version number to its help output.

### Changed

- Testing, to use expected unique strings instead of all.
- README.md, for notes to be under their own heading.

## [1.1.1] - 2024-03-05

### Fixed

- Daylight saving time (DST) handling. Time zones with DST information
  on a particular system could have had an hour added to local time
  when DST was not in effect when saving files. Thanks to [@n47h4n]
  for bringing this issue up.

### Changed

- Refactored DST handling.

## [1.1.0] - 2023-02-03

### Added

- The udev rules setup for automatic device file creation for a
  connected device for non-privileged user usage. More than one device
  may be connected at the same time.
- Xorg section description for it to ignore the connected device to
  avoid possible negative impact to a GUI session arising due to
  default recognition by Xorg of it as a keyboard-joystick.

### Changed

- Makefile: hidden too verbose echo and expected error output, stopped
  ignoring possible build and test files removal errors.
- README.md, for better structure, more clarity, usage examples to be
  closer to corresponding descriptions.

## [1.0.0] - 2022-12-20

### Added

- .gitignore for build and test result files.
- Live monitoring feature to get the live streaming of measurements
  from a device, display, and save them.
- An optional CLI argument "move" to give an explicit indication to
  delete data after its download from a device.
- Analysis program for analyzing downloaded measurements of "Auto"
  mode, with optional plotting through `gnuplot`.

### Changed

- Timestamps of saved files are in local time. Previously, in UTC.
- CLI arguments are handled more strictly.
- Moved stored and live processing code to corresponding functions.
- Refactored exchanges, checksum handling, stored data detection,
  patterned memory allocation and freeing, and timestamp stripping.
- Modified and moved apt parts of source documentation to README.md.

## [Initial] - 2021-11-14

### Added

- Initial publication of the project.

[Keep a Changelog]: https://keepachangelog.com/en/1.1.0/
[Semantic Versioning]: https://semver.org/spec/v2.0.0.html
[1.5.0]: https://codeberg.org/klimd/pulseoxdl/compare/v1.4.0..v1.5.0
[1.4.0]: https://codeberg.org/klimd/pulseoxdl/compare/v1.3.0..v1.4.0
[1.3.0]: https://codeberg.org/klimd/pulseoxdl/compare/v1.2.0..v1.3.0
[1.2.0]: https://codeberg.org/klimd/pulseoxdl/compare/v1.1.1..v1.2.0
[1.1.1]: https://codeberg.org/klimd/pulseoxdl/compare/v1.1.0..v1.1.1
[1.1.0]: https://codeberg.org/klimd/pulseoxdl/compare/v1.0.0..v1.1.0
[1.0.0]: https://codeberg.org/klimd/pulseoxdl/compare/4121c5e37b..v1.0.0
[Initial]: https://codeberg.org/klimd/pulseoxdl/commit/4121c5e37b
[@tholin]: https://codeberg.org/tholin
[@HelWeis]: https://codeberg.org/HelWeis
[@n47h4n]: https://codeberg.org/n47h4n
