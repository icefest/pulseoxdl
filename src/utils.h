/* pulseoxdl - pulse oximetry downloader (Contec CMS50E, USB HID)
 * Copyright © 2021 Donatas Klimašauskas
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>. */

#ifndef _UTILS_H
#define _UTILS_H

#define CLI_ARG_MAX_LEN 50

/* Set program variable to executable basename. */
void set_program_name(char *pathname);

/* Print debugging message with program name prepended. */
void debug(const char *msg);

/* If system or library function failed, the error message is expected
 * to be the name of the function. If program failed -- description of
 * what went wrong. Formats and outputs an error string with a single
 * call, to avoid MT racing. */
void exit_error(const char *msg);

/* If reading fails from any device, exit the program. */
void exit_on_read_error(FILE *stream);

/* Make stdout stream not buffered. */
void unbuf_stdout(void);

/* Library file operation wrappers that exit the caller on error. */

void open_file(FILE **fp, const char *pathname, const char *mode);

void close_file(FILE *fp);

#endif /* _UTILS_H */
