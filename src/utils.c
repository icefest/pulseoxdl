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

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <libgen.h>

#include "utils.h"

char *program;

void
set_program_name(char *pathname)
{
	program = basename(pathname);
	if (strlen(program) > CLI_ARG_MAX_LEN)
		exit_error("program name is unexpected");
}

void
debug(const char *msg)
{
	fprintf(stderr, "%s: %s\n", program, msg);
}

void
exit_error(const char *msg)
{
	if (errno)
		fprintf(stderr, "%s: %s(): %s\n", program, msg,
			strerror(errno));
	else
		debug(msg);

	exit(EXIT_FAILURE);
}

void
exit_on_read_error(FILE *stream)
{
	if (ferror(stream))
		exit_error("reading from device failed");
}

void
unbuf_stdout(void)
{
	if (setvbuf(stdout, NULL, _IONBF, 0))
		exit_error("setvbuf");
}

void
open_file(FILE **fp, const char *pathname, const char *mode)
{
	if (!(*fp = fopen(pathname, mode)))
		exit_error("fopen");
}

void
close_file(FILE *fp)
{
	if (fclose(fp))
		exit_error("fclose");
}
