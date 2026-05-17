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

#include "utils.h"

#define LEN_HID_REPORT 64 /* Device sent HID report. */
#define LEN_EXCHANGES (sizeof(exchanges) / sizeof(exchanges[0]))
#define DELETED_FIRST_OK_CMD 0xef, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x6f
#define REQUEST_MANUAL_PR 0xa2

enum commands {
	STOP_SENDING_DATA,
	UNKNOWN_0,
	UNKNOWN_1,
	SYNCHRONIZE_DEVICE_DATE_AND_TIME,
	USER_NAME,
	MODEL_NAME,
	STORED_PRESENT,
	GET_RECORD_METADATA_AUTO,
	GET_RECORD_METADATA_MANUAL,
	DELETE_RECORDS_AUTO,
	DELETE_RECORD_MANUAL_0,
	DELETE_RECORD_MANUAL_1,
	END_LIVE_DATA,
};
enum booleans {
	CHECKSUM_SET,
	CHECKSUM_CHECK,
};
#ifdef DEBUG
enum debug_lengths {
	DEBUG_IO_COLUMNS = 16,
	/* 3 chars per 1 B and '\n' per row. */
	DEBUG_IO_BUF = LEN_HID_REPORT * 3 + LEN_HID_REPORT / DEBUG_IO_COLUMNS,
};
#endif

extern char *program;

static unsigned char in[LEN_HID_REPORT];

/* Except for the first write, all the rest writes and reads have
 * checksum of their last data B. The checksum includes everything
 * before it. If there are multiple device sent commands per HID
 * report, then it includes everything before the preceding
 * checksum. Checksum is a sum into single 7 bits B. Checksum member
 * of a struct:
 * - > 0 means that checksum must be set or checked and the
 * value is the data length to be written or read (includes the
 * checksum);
 * - 0 means no checksum.
 *
 * The below is the sequence of exchanges that manufacturer's software
 * does with the device every time on initial communication. But it is
 * probably not strictly necessary, because it is possible to, e.g.,
 * request device model name without any unwanted effects, or when
 * some of the later exchanges that happened in sequence initially,
 * then happen in a different sequence.
 *
 * Thus, the approach is hybrid. Initial communication is emulated --
 * done the same as what manufacturer's software does, but retrieving
 * metadata of records and them, and their deletion, are not preceded
 * with exchanges that the manufacturer's software does. They appear
 * not to be required.
 *
 * Similarly, between user name and model name exchanges, the stored
 * data exchange is excluded from the initial communication. It is
 * used when user specifies downloading the stored records.
 */
struct io_data {
	unsigned char data[LEN_HID_REPORT];
	unsigned char checksum;
};
static struct exchange {
	unsigned char id;
#ifdef DEBUG
	char *name;
#endif
	struct io_data write;
	struct io_data read;
} exchanges[] = {
	{
		STOP_SENDING_DATA,
#ifdef DEBUG
		"stop sending data",
#endif
		{
			{
				0x7d, 0x81, 0xa7, 0x80, 0x80, 0x80, 0x80, 0x80,
				0x80, 0x7d, 0x81, 0xa2, 0x80, 0x80, 0x80, 0x80,
				0x80, 0x80,
			},
			0,
		},
		{
			{
				0xf0, 0x70,
			},
			2,
		},
	},
	{
		UNKNOWN_0,
#ifdef DEBUG
		"unknown 0",
#endif
		{
			/* Could be whether PI-capable, etc. */
			{
				0x82, 0x02,
			},
			0,
		},
		{
			{
				0xf2, 0x00, 0x00, 0x02, 0x02, 0x00, 0x0b, 0x01,
			},
			8,
		},
	},
	{
		UNKNOWN_1,
#ifdef DEBUG
		"unknown 1",
#endif
		{
			{
				0x80, 0x00,
			},
			0,
		},
		{
			{
				0xf0, 0x70,
			},
			2,
		},
	},
	{
		SYNCHRONIZE_DEVICE_DATE_AND_TIME,
#ifdef DEBUG
		"synchronize device date and time",
#endif
		{
			/* Next 6 Bs: %y%m%d%H%M%S. Then 2 Bs unclear
			 * and a checksum B. */
			{
				0x83,
			},
			10,
		},
		{
			{
				0xf3, 0x00, 0x73,
			},
			3,
		},
	},
	{
		USER_NAME,
#ifdef DEBUG
		"user name",
#endif
		{
			{
				0x8e, 0x03, 0x11,
			},
			0,
		},
		{
			/* Left-space padded "user"; format "%7s".
			 * (The last B is a checksum, that here
			 * matches the space B's value.) */
			{
				0xfe, 0x03, 0x20, 0x20, 0x20, 0x75, 0x73, 0x65,
				0x72, 0x20,

			},
			10,
		},
	},
	{
		MODEL_NAME,
#ifdef DEBUG
		"model name",
#endif
		{
			{
				0x81, 0x01,
			},
			0,
		},
		{
			/* Right-space padded "50E"; format "%-8s". */
			{
				0xf1, 0x35, 0x30, 0x45, 0x20, 0x20, 0x20, 0x20,
				0x20, 0x3b,
			},
			10,
		},
	},
};
static struct exchange storedpresent = {
	STORED_PRESENT,
#ifdef DEBUG
	"stored present",
#endif
	{
		{
			0x9f, 0x1f,
		},
		0,
	},
	{
		/* +2 B: stored record(s) present if not 0.
		 * +3 B: Auto mode record(s) if not 0, Manual otherwise.
		 * +7 B: checksum of the previous Bs.
		 *
		 * Auto mode response command. */
		{
			0xef, 0x00, 0x01, 0x40, 0x00, 0x00, 0x00, 0x30,
		},
		8,
	},
};
static const struct exchange metaauto = {
	GET_RECORD_METADATA_AUTO,
#ifdef DEBUG
	"get record metadata: auto",
#endif
	{
		{
			0x9c, 0x01, 0x1d,
		},
		0,
	},
	{
		/* +1 B: is last record? 0x00 no; 0x40 yes.
		 * +2 B: user index (always 0x01 for 50E).
		 * +3 B: record's index; always > 0.
		 * +4..9 Bs: record's start: %y%m%d%H%M%S.
		 * +10..12 Bs: record's datums #, 1/s; LSB.
		 * +13..19 Bs: user name.
		 * +20 B: checksum of the previous Bs.
		 *
		 * User name field's width is 7. Maximum datums within
		 * 24 h are 86400; 3 Bs are required, with 7 b/B. */
		{
			0xec, 0x40, 0x01, 0x01, 0x15, 0x05, 0x0f, 0x0c,
			0x06, 0x2a, 0x39, 0x51, 0x00, 0x00, 0x75, 0x73,
			0x65, 0x72, 0x00, 0x00, 0x51,

		},
		21,
	},
}, metamanual = {
	GET_RECORD_METADATA_MANUAL,
#ifdef DEBUG
	"get record metadata: manual",
#endif
	{
		{
			0xa0, 0x00, 0x20,
		},
		0,
	},
	{
		/* +1 B: unknown; seems to be unused and always 0.
		 * +2..+7 Bs: record's start: %y%m%d%H%M%S.
		 * +8..+9 Bs: unknown; seem to be unused and always 0.
		 * +10..+12 Bs: record's datums #, 1/s; LSB, as Auto.
		 * +13 B: checksum of the previous Bs. */
		{
			0xd0, 0x00, 0x19, 0x07, 0x05, 0x10, 0x3b, 0x17,
			0x00, 0x00, 0x02, 0x10, 0x00, 0x69,
		},
		14,
	},
}, deleteauto = {
	DELETE_RECORDS_AUTO,
#ifdef DEBUG
	"delete records: auto",
#endif
	{
		{
			0x9d, 0x7f, 0x7f, 0x7f, 0x7f, 0x00, 0x00, 0x19,
		},
		0,
	},
	{
		/* The response, unlike other non-datums commands,
		 * that have 1 command per HID report, contains 2
		 * commands and their checksums. First checksum will
		 * be checked by the reading function. Second -- has
		 * to be checked separately, after the first. */
		{
			DELETED_FIRST_OK_CMD,
			0xed, 0x7f, 0x7f, 0x7f, 0x7f, 0x00, 0x69,
		},
		8,
	},
}, deletemanual0 = {
	DELETE_RECORD_MANUAL_0,
#ifdef DEBUG
	"delete record: manual: 0",
#endif
	{
		{
			0xa1, 0x00, 0x21,
		},
		0,
	},
	{
		/* Manual record deletion has 2 exchanges. The first
		 * is an exchange of single commands, while the second
		 * exchange is similar to the Auto. */
		{
			0xd1, 0x00, 0x00, 0x51,
		},
		4,
	},
}, deletemanual1 = {
	DELETE_RECORD_MANUAL_1,
#ifdef DEBUG
	"delete record: manual: 1",
#endif
	{
		{
			0xa1, 0x01, 0x22,
		},
		0,
	},
	{
		/* Similar to the Auto. */
		{
			DELETED_FIRST_OK_CMD,
			0xd1, 0x01, 0x00, 0x52,
		},
		8,
	},
};
/* Auto +4 B is record's index; always > 0.
 *
 * The same data structures will be used to request SpO2 and PR,
 * because they, per a record, apart from the checksum, differ only by
 * 2 Bs for Auto and 1 B for Manual.
 *
 * Unlike Auto, Manual, between downloading SpO2 and PR, does another
 * request/response of {0xa7, 0x01, 0x28}/{0xd8, 0x01, 0x00, 0x59},
 * but this seems not to be necessary. */
static struct io_data requestauto = {
	{
		0x9d, 0x04, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00,
		0x24,
	},
	9,
}, requestmanual = {
	{
		0xa3, 0x00, 0x00, 0x00, 0x23,
	},
	5,
};
static const struct io_data requestlivedata[] = {
	{
		/* Amplitudes. */
		{
			0x9b, 0x00, 0x1b,
		},
		0,
	},
	{
		/* Measurements. */
		{
			0x9b, 0x01, 0x1c,
		},
		0,
	},
};
static const struct exchange endlive = {
	END_LIVE_DATA,
#ifdef DEBUG
	"end live data",
#endif
	{
		{
			0x9b, 0x7f, 0x1a,
		},
		0,
	},
	{
		{
			0xeb, 0x7f, 0x6a,
		},
		3,
	},
};

#ifdef DEBUG
static void
debug_exchange(const struct exchange ex)
{
	fprintf(stderr, "%s: exchange: %s\n", program, ex.name);
}
#endif

static void
checksum(const unsigned char set, unsigned char *buf, const unsigned int end)
{
	unsigned int i;
	unsigned char sum = 0;

	for (i = 0; i < end; i++)
		sum += buf[i];
	sum &= 0x7f;
	if (set == CHECKSUM_SET)
		buf[end] = sum;
	else if (sum != buf[end])
		exit_error("checksum failed");
}

static void
checksum_doable(const unsigned char set, unsigned char *buf,
		const unsigned char length)
{
	if (length)
		checksum(set, buf, length - 1);
}

#ifdef DEBUG
static void
debug_io(const char *msg, const unsigned char *buf)
{
	unsigned int i;
	char str[DEBUG_IO_BUF];
	char *end = str;

	debug(msg);
	for (i = 0; i < LEN_HID_REPORT; i++) {
		if (i != 0 && !(i % DEBUG_IO_COLUMNS))
			end += sprintf(end, "\n");
		end += sprintf(end, "%02x ", buf[i]);
	}
	*end = '\0';
	fprintf(stderr, "%s\n", str);
}
#endif

static void
write_data(FILE *stream, struct io_data io)
{
	checksum_doable(CHECKSUM_SET, io.data, io.checksum);
#ifdef DEBUG_WRITE
	debug_io("write", io.data);
#endif
#ifndef SIMULATOR
	putc(0, stream); /* For HIDRAW driver, the numbered report B. */
#endif
	if (fwrite(io.data, 1, LEN_HID_REPORT, stream) != LEN_HID_REPORT)
		exit_error("writing HID report failed");
}

static void
read_report(FILE *stream)
{
	if (fread(in, 1, LEN_HID_REPORT, stream) != LEN_HID_REPORT)
		exit_error("HID report read is too short");
	exit_on_read_error(stream);
}

static void
read_data(FILE *stream, const struct io_data io)
{
	read_report(stream);
#ifdef DEBUG
	debug_io("read", in);
#endif
	checksum_doable(CHECKSUM_CHECK, in, io.checksum);
	if (in[0] != io.data[0])
		exit_error("read unexpected command");
}
