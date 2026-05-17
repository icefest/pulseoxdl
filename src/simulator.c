/* pulseoxdl - pulse oximetry downloader (Contec CMS50E, USB HID)
 * Copyright © 2021-2022, 2025 Donatas Klimašauskas
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

#include "exchange.h"

enum tests {
	TEST_MOVE,
	TEST_LIVE,
};
enum argc_modes {
	ARGC_LIVE = 2,
	ARGC_AUTO,
	ARGC_MANUAL,
};

static struct io_data request;

static void
respond_data(const char * const measurement, const unsigned char test)
{
	if (test == TEST_MOVE)
		read_data(stdin, request);

	FILE *fp;

	open_file(&fp, measurement, "rb");
	while (1) {
		if (fread(in, 1, LEN_HID_REPORT, fp) != LEN_HID_REPORT)
			if (feof(fp))
				break;
		exit_on_read_error(fp);
		if (fwrite(in, 1, LEN_HID_REPORT, stdout) != LEN_HID_REPORT)
			exit_error("writing to stdout failed");
	}
	close_file(fp);
}

static void
do_exchange(const struct exchange exchange)
{
#ifdef DEBUG
	debug_exchange(exchange);
#endif
	read_data(stdin, exchange.write);
	write_data(stdout, exchange.read);
}

int
main(int argc, char *argv[])
{
	set_program_name(argv[0]);

	if (argc < ARGC_LIVE)
		exit_error("<SpO2> <PR> [any for manual] or <live> "
			   "transmission file(s) required");

	struct exchange meta, delete;
	unsigned char manual = 0;

	switch (argc) {

	case ARGC_LIVE:
		break;

	case ARGC_AUTO:
		request = requestauto;
		meta = metaauto;
		delete = deleteauto;
		break;

	case ARGC_MANUAL:
		request = requestmanual;
		meta = metamanual;
		delete = deletemanual1;
		storedpresent.read.data[3] = 0;
		manual = 1;
		break;

	default:
		exit_error("unexpected simulation mode indicated");
	}

	unsigned int i;

	unbuf_stdout();
	for (i = 0; i < LEN_EXCHANGES; i++)
		do_exchange(exchanges[i]);

	if (argc > ARGC_LIVE) { /* Move; Auto or Manual. */
		do_exchange(storedpresent);
		do_exchange(meta);

		/* SpO2 expected first, then PR. */
		respond_data(argv[1], TEST_MOVE);
		if (manual)
			request.data[0] = REQUEST_MANUAL_PR;
		respond_data(argv[2], TEST_MOVE);

		if (manual)
			do_exchange(deletemanual0);
		do_exchange(delete);
	} else { /* Live. */
		read_data(stdin, requestlivedata[0]);
		read_data(stdin, requestlivedata[1]);

		respond_data(argv[1], TEST_LIVE);

		do_exchange(endlive);
	}

	exit(EXIT_SUCCESS);
}
