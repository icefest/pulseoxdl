/* pulseoxdl - pulse oximetry downloader (Contec CMS50E, USB HID)
 * Copyright © 2021-2022, 2024-2025 Donatas Klimašauskas
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
#include <time.h>
#include <string.h>
#include <endian.h>

#include "exchange.h"

#define CLI_ARG_COUNT_MIN 2
#define CLI_ARG_COUNT_MAX 3
#define CLI_ARG_DEVICE 1
#define CLI_ARG_ACTION 2
#define CLI_ARG_HELP "-h"
#define CLI_ARG_TIME "time"
#define CLI_ARG_MOVE "move"
#define CLI_ARG_COPY "copy"
#define CLI_ARG_LIVE "live"
#define CLI_ARG_ECHO "echo"
#define MULTIPLICAND 16 /* Adjustment's first B's low nibble -- multiplier. */
#define LEN_DATUMS_CMD 30 /* Datums command packet stored by any mode. */
#define SEQUENCE_MAX 16383 /* 0x7f + (0x7f << 7). */
#define LEN_MAX_RECORDS 99 /* Manufacturer specified maximum stored records. */
#define LEN_START_TIME 20 /* E.g., "2021-01-23T12:34:56". */
#define LEN_ORIG_TIME 21 /* E.g., "2021-01-23, 12:34:56". */
#define LEN_DURATION 9 /* E.g., "12:34:56". */
#define TIME_PARTS 6 /* E.g., "2021", "01", "23", "12", "34", "56". */
#define LEN_TIME_PART_BUF 5 /* E.g., "2021". */
#define LEN_TIME_PART_MIN 2 /* Month, day, hours, minutes, seconds. */
#define LEN_TIME_PART_MAX 4 /* Year. */
#define INT32_SIZE 4 /* In Bs. */
#define RECORD_INDEX 4 /* There Auto record's index -- its number, is set. */
#define LEN_BIN_MIDDLE 28 /* 7 * 4 LSB Bs: time parts and record length. */
#define LEN_TIME_FILENAME 20 /* E.g., "20210123123456.SpO2". */
#define TIME_DELIM "-T:" /* ISO 8601 format's delimiters, as used. */
#define HOUR_IN_SECONDS 3600
#define CSV_LINE_TERMINATOR "\r\n"
#define CSV_HEADER "DATE,TIME,SPO2,PULSE"
#define CSV_DATA_FMT "%s, %d, %d"CSV_LINE_TERMINATOR
#define LEN_LIVE_HEAP 2592000 /* 30 days in seconds. */
#define LEN_CMD_AMPLITUDES 6
#define LEN_CMD_MEASUREMENTS 8
#define MANUAL_DATUM_BYTES_CNT 14
#ifdef SIMULATOR
#define TEST_LIVE_START_TIME 1619782118 /* Seconds. */
#else
#define PING_INTERVAL 5
#endif

#define EPI_ENVVAR "PULSEOXDL_EPI" /* EPI -- Experimental PI, enable. */
#define EPI_ENVVAR_PIPI "PULSEOXDL_EPI_PIPI" /* Plethysmograph Inferred PI. */
#define EPI_ENVVAR_DELTA "PULSEOXDL_EPI_DELTA" /* PIPI minus true PI. */
#define EPI_ENVVAR_DETECT "PULSEOXDL_EPI_DETECT" /* Seconds to find true PI. */
#define EPI_CSV_DATA_FMT "%s, %d, %d, %.1f"CSV_LINE_TERMINATOR
#define EPI_DELTA_CSV_DATA_FMT "%s, %d, %d, %.1f, %.1f"CSV_LINE_TERMINATOR
#define EPI_MIN_PIPI 20 /* % */
#define EPI_MAX_PIPI 50 /* % */
#define EPI_MIN_DETECT 1 /* s */
#define EPI_MAX_DETECT 15 /* s */
#define EPI_DEFAULT_DETECT 2 /* s */
#define EPI_Y_MAX_ABS 128 /* [0..127] as CMS50E has it. */
#define EPI_Y_MAX_PERC 25 /* 25% as in manual_en.pdf of v3.1.0.1 EXE. */

enum stored_auto {
	AUTO_RECORD_START = 4,
	AUTO_RECORD_DELETED_2ND_CHECKSUM = 6,
	AUTO_RECORD_LEN = LEN_DATUMS_CMD,
	AUTO_MEASUREMENT_INDEX = 2,
	AUTO_MEASUREMENT_SPO2 = 0x01,
	AUTO_MEASUREMENT_PR = 0x02,
	AUTO_SEQUENCE_START = 3,
	AUTO_SEQUENCE_END = 4,
	AUTO_DATUMS_START = 8,
	AUTO_DATUMS_END = 29,
};
enum stored_manual {
	MANUAL_RECORD_START = 2,
	MANUAL_RECORD_DELETED_2ND_CHECKSUM = 3,
	MANUAL_RECORD_LEN = 20,
	MANUAL_MEASUREMENT_INDEX = 0,
	MANUAL_MEASUREMENT_SPO2 = 0xa3,
	MANUAL_MEASUREMENT_PR = REQUEST_MANUAL_PR,
	MANUAL_SEQUENCE_START = 1,
	MANUAL_SEQUENCE_END = 2,
	MANUAL_DATUMS_START = 5,
	MANUAL_DATUMS_END = 19,
};
enum artifacts {
	ARTIFACT_SPO2 = 127,
	ARTIFACT_PR = 255,
};

#define Is_artifact(value) ((flag) && (value) == 0x7f)

#define Set_flags()				\
	do {					\
		flag = flags & 0x1;		\
		flags >>= 1;			\
	} while (0)

#define Reset_pulsewave()			\
	do {					\
		pulsewavesum = 0;		\
		pulsewavecnt = 0;		\
		pulsewavemin = EPI_Y_MAX_ABS;	\
	} while (0)

#ifndef SIMULATOR
/* +1 B: checksum of the previous B.
 *
 * Sent to device every 5 s whenever no other exchange happens. This
 * includes the case when PC is only retrieving data from device. */
static const struct io_data ping = {
	{
		0x9a, 0x1a,
	},
	0,
};
static const char *pulsebars[] = { /* 16 lengths. */
	"-",
	"--",
	"---",
	"----",
	"-----",
	"------",
	"-------",
	"--------",
	"---------",
	"----------",
	"-----------",
	"------------",
	"-------------",
	"--------------",
	"---------------",
	"----------------",
};
#endif

/* Auto and Manual stored record modes have similar exchanges. Accrue
 * specifics of each in a following block of structures, with a next
 * structure composed from them, for use per a mode. */

struct record {
	const unsigned char start;
	const struct exchange * const meta;
	struct io_data * const request;
	const struct exchange * const delete;
	const unsigned char deleted2ndchecksum;
	const unsigned char dcmdlength;
};
struct measurement {
	const unsigned char index; /* B to set a requested measurement. */
	const unsigned char spo2;
	const unsigned char pr;
};
struct index {
	const unsigned char start;
	const unsigned char end;
};
struct process {
	unsigned int (* const flags)(void);
	void (* const nibbles)(const unsigned char nibbles,
			       const unsigned char flag);
};

static unsigned int process_flags_auto(void);
static unsigned int process_flags_manual(void);
static void process_nibbles_auto(const unsigned char nibbles,
				 const unsigned char flag);
static void process_nibbles_manual(const unsigned char nibbles,
				   const unsigned char flag);

static struct smode {
	const struct record record;
	const struct measurement measurement;
	const struct index sequence;
	const struct index datums;
	const struct process process;
} storedauto = {
	{
		AUTO_RECORD_START,
		&metaauto,
		&requestauto,
		&deleteauto,
		AUTO_RECORD_DELETED_2ND_CHECKSUM,
		AUTO_RECORD_LEN,
	},
	{
		AUTO_MEASUREMENT_INDEX,
		AUTO_MEASUREMENT_SPO2,
		AUTO_MEASUREMENT_PR,
	},
	{
		AUTO_SEQUENCE_START,
		AUTO_SEQUENCE_END,
	},
	{
		AUTO_DATUMS_START,
		AUTO_DATUMS_END,
	},
	{
		process_flags_auto,
		process_nibbles_auto,
	},
}, storedmanual = {
	{
		MANUAL_RECORD_START,
		&metamanual,
		&requestmanual,
		&deletemanual1,
		MANUAL_RECORD_DELETED_2ND_CHECKSUM,
		MANUAL_RECORD_LEN,
	},
	{
		MANUAL_MEASUREMENT_INDEX,
		MANUAL_MEASUREMENT_SPO2,
		MANUAL_MEASUREMENT_PR,
	},
	{
		MANUAL_SEQUENCE_START,
		MANUAL_SEQUENCE_END,
	},
	{
		MANUAL_DATUMS_START,
		MANUAL_DATUMS_END,
	},
	{
		process_flags_manual,
		process_nibbles_manual,
	},
};

static const struct fmt_time {
	const unsigned char size;
	const char *format; /* strftime(3) format string. */
} duration = {
	LEN_DURATION,
	"%T",
}, recstarttime = {
	LEN_START_TIME,
	"%FT%T",
}, origtime = {
	LEN_ORIG_TIME,
	"%F, %T",
};
struct timestamp {
	time_t sec; /* Seconds since the Epoch. */
	char str[LEN_START_TIME];
};
static struct metadata {
	unsigned char rindex; /* Record's index. */
	time_t length; /* Datums (and record's duration in s). */
	char duration[LEN_DURATION];
	struct timestamp starttime;
} records[LEN_MAX_RECORDS];

extern const unsigned char _binary_data_header_start;
extern const unsigned char _binary_data_header_end;

#ifndef SIMULATOR
static FILE *dev;
#endif

static char *cliaction;
static char isdstnow; /* Daylight saving time flag at start live. */
static char inauto; /* Getting the Auto mode stored record(s) flag. */
static unsigned char dcmdlength;
static unsigned char dcmdstart;
static unsigned char dcmdend;
static unsigned char seql; /* Sequence LSB. */
static unsigned char seqm; /* Sequence MSB. */
static unsigned char artifact;
static unsigned char echolive;
static unsigned char epi;
static unsigned char dodelta;
static unsigned char dcmdpkt[LEN_DATUMS_CMD]; /* Can contain Auto or Manual. */
static unsigned char binmiddle[LEN_BIN_MIDDLE];
static unsigned char *printstore; /* Points to memory for a measurement. */
static unsigned char *printstorespo2; /* Measurement's memory. */
static unsigned char *printstorepr; /* Measurement's memory. */
static unsigned char top;
static unsigned int datums;
static unsigned int mdatumbytesused;
static struct smode *smode; /* Points to Auto or Manual decoding data. */
static unsigned int (*process_flags)(void);
static void (*process_nibbles)(const unsigned char nibbles,
			       const unsigned char flag);

/* Flag Bs are in LSB. */

static unsigned int
process_flags_auto(void)
{
	return dcmdpkt[5] + (dcmdpkt[6] << 7) + (dcmdpkt[7] << 14);
}

static unsigned int
process_flags_manual(void)
{
	return dcmdpkt[3] + (dcmdpkt[4] << 7);
}

static void
store_datum_auto(const unsigned char nibble)
{
	printstore[--datums] = top - nibble;
}

static void
process_nibbles_auto(const unsigned char nibbles, const unsigned char flag)
{
	unsigned char high = nibbles >> 4;
	unsigned char low = nibbles & 0xf;
	static unsigned char first = 1;

	/* Adjust. */
	if (flag && high == 0x7) {
		if (first) {
			first = 0;
			top = MULTIPLICAND * low;
		} else {
			first = 1;
			top += low;
		}

		return;
	}

	/* Decode. */
	if (flag)
		store_datum_auto(high + 0x8);
	else
		store_datum_auto(high);
	if (low != 0xf)
		store_datum_auto(low);
}

static void
store_datum_manual(const unsigned char nibble)
{
	if (datums)
		printstore[--datums] = nibble;
}

static void
process_nibbles_manual(const unsigned char nibbles, const unsigned char flag)
{
	static unsigned char datum;
	unsigned char high, low;

	/* The first measurement B in datums command packet is always
	 * an absolute value. Later nibbles in Bs are deltas to be
	 * added or subtracted from the absolute value. */
	if (!(mdatumbytesused++ % MANUAL_DATUM_BYTES_CNT)) {
		datum = nibbles;
		if (Is_artifact(datum)) {
			store_datum_manual(artifact);
		} else {
			if (flag) /* 0x7f overflowed. Always false for SpO2. */
				datum += 128;
			store_datum_manual(datum);
		}

		return;
	}

	if (Is_artifact(nibbles)) {
		store_datum_manual(artifact);
		store_datum_manual(artifact);

		return;
	}

	high = nibbles >> 4;
	if (high) {
		if (flag)
			datum -= high;
		else
			datum += high;
	}
	store_datum_manual(datum);

	low = nibbles & 0xf;
	if (low) {
		if (low == 0xf) { /* Finger is out. */
			store_datum_manual(artifact);

			return;
		}
		if (low & 0x8)
			datum -= low & 0x7;
		else
			datum += low;
	}
	store_datum_manual(datum);
}

static void
set_current_time(unsigned char *buf)
{
	const time_t now = time(NULL);
	const struct tm *tm = localtime(&now);

	if (!tm)
		exit_error("localtime");
	buf[1] = (unsigned char) tm->tm_year - 100;
	buf[2] = (unsigned char) tm->tm_mon + 1;
	buf[3] = (unsigned char) tm->tm_mday;
	buf[4] = (unsigned char) tm->tm_hour;
	buf[5] = (unsigned char) tm->tm_min;
	buf[6] = (unsigned char) tm->tm_sec;
	isdstnow = tm->tm_isdst;
}

/* If DST information is not available, treat it as not in effect. */
static unsigned int
get_dst(const int dst)
{
	return dst > 0 ? HOUR_IN_SECONDS : 0;
}

static void
set_start_seconds(const unsigned char start, time_t *sec)
{
	struct tm tm;

	tm.tm_year	= in[start]	+ 100;
	tm.tm_mon	= in[start + 1] - 1;
	tm.tm_mday	= in[start + 2];
	tm.tm_hour	= in[start + 3];
	tm.tm_min	= in[start + 4];
	tm.tm_sec	= in[start + 5] - timezone;
	tm.tm_isdst	= -1;
	if ((*sec = mktime(&tm)) == -1)
		exit_error("mktime");
	*sec += get_dst(tm.tm_isdst);
}

static void
format_time(char *buf, const time_t len, const struct fmt_time *fmt)
{
	const time_t *length = &len;
	struct tm *tm;

	if (!(tm = gmtime(length)))
		exit_error("gmtime");
	if (!strftime(buf, fmt->size, fmt->format, tm))
		exit_error("could not format time");
}

static void
do_exchange(const struct exchange exchange)
{
#ifdef DEBUG
	debug_exchange(exchange);
#endif
#ifdef SIMULATOR
	write_data(stdout, exchange.write);
	read_data(stdin, exchange.read);
#else
	write_data(dev, exchange.write);
	read_data(dev, exchange.read);
#endif
}

/* Get and save how many records are stored on device, if any. */
static unsigned char
set_record_metadata(const unsigned int i)
{
	if (i == LEN_MAX_RECORDS)
		exit_error("more than maximum records");

	do_exchange(*smode->record.meta);

	/* Record's maximum length: 24 h. In s: 86400. On device, the
	 * length's int is encoded in 3 7-bit Bs. LSB order. B's MSb
	 * is not used and appears to always be 0. */

	records[i].rindex = inauto ? in[3] : 1; /* Ignored for the Manual. */
	records[i].length = in[10] + (in[11] << 7) + (in[12] << 14);
	format_time(records[i].duration, records[i].length, &duration);
	set_start_seconds(smode->record.start, &records[i].starttime.sec);
	format_time(records[i].starttime.str, records[i].starttime.sec,
		    &recstarttime);
#ifdef DEBUG
	fprintf(stderr,
		"Index: %d\nStart time: %s\nDuration: %s (%ld datums)\n",
		records[i].rindex, records[i].starttime.str,
		records[i].duration, records[i].length);
#endif

	return !in[1] && inauto;
}

static void
allocate_memory(unsigned char **printstore)
{
	if (!(*printstore = calloc(datums, 1)))
		exit_error("calloc");
}

/* The function requests, downloads and decodes datums. The approach
 * employs 2 buffers of 64 Bs and 30 Bs. First has the HID report,
 * second -- datums command of Auto/Manual record, which is parsed,
 * and driven by datums left. */
static void
extract_datums(const unsigned char rindex, const unsigned char measurement)
{
	if (inauto) {
		smode->record.request->data[RECORD_INDEX] = rindex;
	} else { /* In Manual. */
		if (measurement == storedmanual.measurement.spo2) {
			artifact = ARTIFACT_SPO2;
		} else {
			artifact = ARTIFACT_PR;
			/* On measurement type change, reset to sync
			 * to the absolute value, since extraction of
			 * datums can go out of sync, because a single
			 * B may be decoded as 1 or 2 datums. */
			mdatumbytesused = 0;
		}
	}
	smode->record.request->data[smode->measurement.index] = measurement;
#ifdef SIMULATOR
	write_data(stdout, *smode->record.request);
#else
	write_data(dev, *smode->record.request);
#endif

	allocate_memory(&printstore);

	/* Decode datums from the nibbles and store them. Ensure that
	 * datums command packets are in a sequence. Sequence is
	 * encoded in 2 Bs. LSB order. First packet is #0. As with the
	 * record length, the Bs count to 0x7f. */

	unsigned char i, flag;
	unsigned char next = 0, left = 0, less = 0;
	unsigned short sequence = 0;
	unsigned int reset = datums, flags;

	while (1) {
#ifdef SIMULATOR
		read_report(stdin);
#else
		read_report(dev);
#endif
		while (next + dcmdlength < LEN_HID_REPORT && datums) {
			/* Copy datums command packet from HID report. */
			if (left) {
				memcpy(dcmdpkt + left, in, less);
				next = less;
				left = 0;
			} else {
				memcpy(dcmdpkt, in + next, dcmdlength);
				next += dcmdlength;
			}

			checksum(CHECKSUM_CHECK, dcmdpkt, dcmdend);

			/* Validate in sequence. */
			if (sequence++ != dcmdpkt[seql] + (dcmdpkt[seqm] << 7))
				exit_error("out of sequence");
			if (sequence > SEQUENCE_MAX)
				exit_error("sequence would overflow");

			flags = process_flags();

			/* Decode datums. */
			for (i = dcmdstart; i < dcmdend && datums; i++) {
				Set_flags();
				process_nibbles(dcmdpkt[i], flag);
			}
		}
		if (!datums)
			break;
		if ((left = LEN_HID_REPORT - next)) { /* In old HID report. */
			memcpy(dcmdpkt, in + next, left);
			less = dcmdlength - left; /* In new HID report. */
		}
		next = 0;
	}
	datums = reset;

	/* Assign decoded datums to their print store. */
	if (measurement == smode->measurement.spo2)
		printstorespo2 = printstore;
	else
		printstorepr = printstore;
}

static void
set_binary_middle(const int data, const unsigned int offset)
{
	u_int32_t origendian = htole32(data);

	memcpy(binmiddle + offset * INT32_SIZE, &origendian, INT32_SIZE);
}

static void
write_binary(FILE *bin, const void *buf, const size_t size)
{
	if (fwrite(buf, 1, size, bin) != size)
		exit_error("writing SpO2 file failed");
}

/* Strip non-digits from ISO 8601 date and time. */
static void
strip_timestamp(char *timestamp)
{
	char stripped[LEN_TIME_FILENAME];
	char *part;
	unsigned int i;

	memset(stripped, 0, LEN_TIME_FILENAME);
	strcat(stripped, strtok(timestamp, TIME_DELIM));
	for (i = 1; (part = strtok(NULL, TIME_DELIM)); i++)
		strcat(stripped, part);
	memcpy(timestamp, stripped, LEN_TIME_FILENAME);
}

static void
set_filename(char *buf, const char *timestamp, const char *extension)
{
	memset(buf, 0, LEN_TIME_FILENAME);
	strcat(buf, timestamp);
	strcat(buf, extension);
}

static void
print_csv_header(FILE *csv)
{
	if (echolive && epi) {
		if (dodelta)
			fprintf(csv, CSV_HEADER",PI,PIPI-PI"
				CSV_LINE_TERMINATOR);
		else
			fprintf(csv, CSV_HEADER",PI"
				CSV_LINE_TERMINATOR);
	} else {
		fprintf(csv, CSV_HEADER""CSV_LINE_TERMINATOR);
	}
}

/* Write files of decoded datums in original CSV and SpO2 (binary)
 * formats. The binary is composed from header -- binary data, which
 * is, for CMS50E, repeated in every file, saved by the manufacturer's
 * software, middle -- 7 LSB, 4 Bs long integers of 6 parts of date
 * and time and record's length, and bottom -- the datums. */
static void
write_decoded_files(struct timestamp starttime)
{
	unsigned int i;

	/* Set the binary middle. */
	unsigned char length;
	char part[LEN_TIME_PART_BUF];
	char *full = starttime.str;

	strip_timestamp(full);
	for (i = 0; i < TIME_PARTS; i++) {
		length = i ? LEN_TIME_PART_MIN : LEN_TIME_PART_MAX;
		memcpy(part, full, length);
		part[length] = '\0';
		set_binary_middle(atoi(part), i);
		full += length;
	}
	set_binary_middle(datums, i);

	/* Write the files. */
	char timecsv[LEN_TIME_FILENAME], timebin[LEN_TIME_FILENAME];
	FILE *csv, *bin;
	char otm[LEN_ORIG_TIME];
	unsigned int binsize = &_binary_data_header_end -
		&_binary_data_header_start;

	set_filename(timecsv, starttime.str, ".csv");
	set_filename(timebin, starttime.str, ".SpO2");
	open_file(&csv, timecsv, "w");
	open_file(&bin, timebin, "wb");
	print_csv_header(csv);
	write_binary(bin, &_binary_data_header_start, binsize);
	write_binary(bin, binmiddle, LEN_BIN_MIDDLE);
	for (i = 0; datums--; i++) {
		format_time(otm, starttime.sec + i, &origtime);
		fprintf(csv, CSV_DATA_FMT, otm,
			printstorespo2[datums], printstorepr[datums]);
		putc(printstorespo2[datums], bin);
		putc(printstorepr[datums], bin);
	}
	close_file(csv);
	close_file(bin);

	free(printstorespo2);
	free(printstorepr);
}

static void
exit_help(void)
{
	printf("Version: %s\n"
	       "\n"
	       "Usage: ./%s /dev/hidraw<N> [%s|%s|%s|%s|%s]\n"
	       "\n"
	       "Action	Description\n"
	       "------	-----------\n"
	       "%s	Synchronize PC time to device.\n"
	       "%s	Move stored records from device to PC (default).\n"
	       "%s	Copy stored records on device to PC.\n"
	       "%s	Stream measurements from device to PC and save.\n"
	       "%s	Stream measurements from device to PC and echo.\n",
	       VERSION, program,
	       CLI_ARG_TIME,
	       CLI_ARG_MOVE, CLI_ARG_COPY, CLI_ARG_LIVE, CLI_ARG_ECHO,
	       CLI_ARG_TIME,
	       CLI_ARG_MOVE, CLI_ARG_COPY, CLI_ARG_LIVE, CLI_ARG_ECHO);

	exit(EXIT_SUCCESS);
}

static unsigned char
is_live_stop(unsigned char *buf, const unsigned char length,
	     const unsigned char nodata, unsigned char *fingerout)
{
	checksum_doable(CHECKSUM_CHECK, buf, length);
	if (buf[4] == nodata)
		*fingerout = 1;

	return *fingerout;
}

static void
compact_measurement_memory(unsigned char **old)
{
	memcpy(printstore, *old + LEN_LIVE_HEAP - datums, datums);
	if (!(*old = realloc(*old, datums)))
		exit_error("realloc");
	memcpy(*old, printstore, datums);
}

static void
request_end_live(void)
{
	do_exchange(endlive);
}

static void
exit_live_error(const char *msg)
{
	request_end_live();
	exit_error(msg);
}

static unsigned char
is_cli_action(const char *cmpaction)
{
	if (cliaction)
		return !strncmp(cliaction, cmpaction, CLI_ARG_MAX_LEN);
	else
		return 0;
}

static void
process_stored(void)
{
	unsigned int i;

	/* Download and save record(s) metadata. */
	for (i = 0; set_record_metadata(i); i++)
		;

	/* Read all records and their datums, decode, write files. */
	for (i = 0; records[i].rindex; i++) {
		datums = records[i].length;
		extract_datums(records[i].rindex, smode->measurement.spo2);
		extract_datums(records[i].rindex, smode->measurement.pr);
		write_decoded_files(records[i].starttime);
	}

	/* Remove record(s) from device. */
	if (!is_cli_action(CLI_ARG_COPY)) {
		if (smode == &storedmanual)
			do_exchange(deletemanual0);
		do_exchange(*smode->record.delete);
		checksum(CHECKSUM_CHECK, in +
			 smode->record.delete->read.checksum,
			 smode->record.deleted2ndchecksum);
	}
}

/* Set the variable for corresponding envvar to either the default
 * value, or to the verified value of the envvar. If set envvar is not
 * verified, exit the program. */
static void
set_verify_envvar(const char * const name, const char * const envvar,
		  const unsigned char min, const unsigned char max,
		  unsigned char *var, const unsigned char def)
{
	char *val;

	if ((val = getenv(envvar)) == NULL) {
		*var = def;

		return;
	}

	*var = (unsigned char) atoi(val);
	if (*var < min || *var > max) {
		fprintf(stderr, "%s: %.2s for %s is outside permitted range: "
			"[%d, %d]\n", program, val, name, min, max);

		exit(EXIT_FAILURE);
	}
}

/* Live data commands are 6 Bs -- pulse wave and bar amplitudes, and 8
 * Bs -- SpO2, PR, and PI. 3-4 such commands are present in every HID
 * report, but 8 Bs one is sent once a second. 20 HID reports/s. No
 * HID report boundary crossings. Usually, one second contains 59
 * amplitudes data commands and 1 measurements data command.
 *
 * Meanings:
 * 6 Bs
 * +1 B: type; amplitudes data 0x00.
 * +3 B: pulse wave amplitude 0x[0..7f]; finger out 0x40.
 * +4 B: pulse bar amplitude 0x[0..f]; finger out 0x60.
 * +5 B: checksum of the previous Bs.
 * 8 Bs
 * +1 B: type; measurements data 0x01.
 * +2 B: PR addition flag 0x[4|6]; on 0x06 add 0x80 to +3 B.
 * +3 B: PR 0x[0..7f].
 * +4 B: SpO2 0x[0..64]; finger out 0x7f.
 * +5 B: PI 0x[0..7f] LSB part or 0x7f when no PI.
 * +6 B: PI 0x[0..13] MSB part or 0x00 when no PI.
 * +7 B: checksum of the previous Bs.
 *
 * PI data of 0x7f00 could be the measurement value of 1.27%, when
 * decoded. Thus, true PI detection is based on the LSB variability.
 *
 * PIPI -- Plethysmograph Inferred Perfusion Index, or Plethysmograph
 * Index as PI, when true Perfusion Index is not available, envvar
 * enabled and adjustable, experimental feature, devised for the
 * project. It uses the plethysmograph data to compute the PIPI.
 *
 * Measurements data command usually arrives later than amplitudes
 * data command. Only when enough of the former is had it can be
 * determined whether the true PI is live streamed by a device.
 *
 * For live data processing, finger has to be in and for long enough
 * to start getting valid data from the device. A few seconds and
 * digits appearing on the device should be enough. */
void
process_live(void)
{
	struct timestamp starttime, endtime;
	unsigned char *buf = NULL;
	unsigned char pulsebar = 0, spo2 = 0, pr = 0, fresh = 0, fingerout = 0;
	unsigned char pipiperc = 0, detect = 0, dopipi = 0, truepi = 0,
		pistaticcnt = 0, pistatic = 0x7f,
		pulsewavecnt, pulsewavemin;
	unsigned short pulsewavesum;
	float pipi = 0, pi = 0;
#ifndef SIMULATOR
	unsigned long pingcounter = 0; /* Overflow is unlikely. */
#endif

	if (is_cli_action(CLI_ARG_ECHO)) {
		echolive = 1;
		if (getenv(EPI_ENVVAR) != NULL) {
			epi = 1;
			dopipi = 1; /* Yet unknown if true PI is present. */
			if (getenv(EPI_ENVVAR_DELTA) != NULL)
				dodelta = 1;

			set_verify_envvar("PIPI", EPI_ENVVAR_PIPI,
					  EPI_MIN_PIPI, EPI_MAX_PIPI,
					  &pipiperc, EPI_Y_MAX_PERC);
			set_verify_envvar("detection", EPI_ENVVAR_DETECT,
					  EPI_MIN_DETECT, EPI_MAX_DETECT,
					  &detect, EPI_DEFAULT_DETECT);
		}
	}

	if (echolive) {
#ifdef SIMULATOR
		print_csv_header(stderr);
#else
		print_csv_header(stdout);
#endif
	} else {
#ifndef SIMULATOR
		printf("Live measurements (remove finger to stop and save)\n");
#endif
		datums = LEN_LIVE_HEAP;
		allocate_memory(&printstorepr);
		allocate_memory(&printstorespo2);
	}

	/* First report read just to check for expected response. */
#ifdef SIMULATOR
	write_data(stdout, requestlivedata[0]);
	write_data(stdout, requestlivedata[1]);
	read_report(stdin);
#else
	write_data(dev, requestlivedata[0]);
	if (fflush(dev)) /* Avoid preceding write buffering. */
		exit_error("fflush");
	write_data(dev, requestlivedata[1]);
	read_report(dev);
#endif

	if (in[0] != 0xeb)
		exit_live_error("not live command retrieved");

#ifdef SIMULATOR
	starttime.sec = TEST_LIVE_START_TIME;
#else
	starttime.sec = time(NULL) - timezone + get_dst(isdstnow);
#endif

	Reset_pulsewave();

	while (1) {
#ifdef SIMULATOR
		read_report(stdin);
#else
		read_report(dev);
#endif

		/* Use up all live data commands in a HID report. */
		for (buf = in; buf[0] == 0xeb;) {
			switch (*(buf + 1)) {

			case 0x0:
				if (is_live_stop(buf, LEN_CMD_AMPLITUDES,
						 0x60, &fingerout))
					break;
				if (dopipi) {
					pulsewavesum += buf[3];
					pulsewavecnt++; /* May differ. */
					if (pulsewavemin > buf[3])
						pulsewavemin = buf[3];
				}
				if (!echolive)
					pulsebar = buf[4];
				buf += LEN_CMD_AMPLITUDES;
				break;

			case 0x1:
				if (is_live_stop(buf, LEN_CMD_MEASUREMENTS,
						 0x7f, &fingerout))
					break;
				pr = buf[2] & 0x2 ? buf[3] + 0x80 : buf[3];
				spo2 = buf[4];
				/* Heuristic true PI presence
				 * detection: if, in detection time,
				 * the most volatile -- LSB, has not
				 * changed, hold as not present. */
				if (!truepi && pistaticcnt < detect) {
					if (pistatic != buf[5])
						truepi = 1;
					else
						pistaticcnt++;
				}
				if (epi) {
					if (dopipi) {
						pipi = (((float) pulsewavesum /
							 pulsewavecnt -
							 pulsewavemin) /
							EPI_Y_MAX_ABS *
							pipiperc + pipi) / 2;
						Reset_pulsewave();
					}
					if (truepi) {
						if (!dodelta)
							dopipi = 0;
						pi = (float) (buf[5] +
							      (buf[6] << 7)) /
							100;
					}
				}
				fresh = 1;
#ifndef SIMULATOR
				if (!(++pingcounter % PING_INTERVAL))
					write_data(dev, ping);
#endif
				buf += LEN_CMD_MEASUREMENTS;
				break;

			default:
				fingerout = 1;
			}

			if (fingerout)
				break;
			if (echolive)
				continue;

#ifdef SIMULATOR
			/* Excludes -Werror=unused-but-set-variable. */
			pulsebar += 0;
			pi += 0;
#else
			/* Avoid overflow of stack index. Demo has
			 * 0x40 set too. */
			printf("\rSpO2: %3d%%, PR: %3d BPM, PB: |%-16s|",
			       spo2, pr, pulsebars[pulsebar & 0xf]);
#endif
		}

		if (fingerout) { /* Stop monitoring. */
			if (datums == LEN_LIVE_HEAP) { /* False for echo. */
				printf("\r%80s\r", " ");
				exit_live_error("finger out or too early");
			}
			break;
		}

		if (fresh) {
			if (echolive) {
				format_time(starttime.str, ++starttime.sec,
					    &origtime);
#ifdef SIMULATOR
				fprintf(stderr, CSV_DATA_FMT, starttime.str,
					spo2, pr);
#else
				if (epi) {
					if (dodelta)
						fprintf(stdout,
							EPI_DELTA_CSV_DATA_FMT,
							starttime.str, spo2,
							pr, pi, pipi - pi);
					else
						fprintf(stdout,
							EPI_CSV_DATA_FMT,
							starttime.str, spo2,
							pr, pi ? pi : pipi);
				} else {
					fprintf(stdout, CSV_DATA_FMT,
						starttime.str, spo2, pr);
				}
#endif
			} else {
				if (!datums--) /* Underflow -- live full. */
					break;
				printstorepr[datums] = pr;
				printstorespo2[datums] = spo2;
			}
			fresh = 0;
		}
	}

	request_end_live();

	if (echolive)
		return;

	/* Adjust for the datums to have only the count actually
	 * saved. Readjust memory accordingly. */
	datums = LEN_LIVE_HEAP - datums;
	allocate_memory(&printstore);
	compact_measurement_memory(&printstorespo2);
	compact_measurement_memory(&printstorepr);
	free(printstore);

	starttime.sec++; /* First measurements come after 1 s. */
	endtime.sec = starttime.sec + datums - 1; /* 1st s included. */

	format_time(starttime.str, starttime.sec, &recstarttime);
	write_decoded_files(starttime); /* Modifies variables. */

	format_time(starttime.str, starttime.sec, &recstarttime);
	format_time(endtime.str, endtime.sec, &recstarttime);
#ifdef SIMULATOR
	fprintf(stderr, "live: start: %s, end: %s\n", starttime.str,
		endtime.str);
#else
	printf("\r%80s\r%s--%s", " ", starttime.str, endtime.str);
#endif

	strip_timestamp(starttime.str);
#ifdef SIMULATOR
	fprintf(stderr, "live: saved: %s\n", starttime.str);
#else
	printf(" saved as %s\n", starttime.str);
#endif
}

int
main(int argc, char *argv[])
{
	set_program_name(argv[0]);

	if (argc < CLI_ARG_COUNT_MIN)
		exit_error("device file is required; try just "CLI_ARG_HELP);
	if (argc > CLI_ARG_COUNT_MAX)
		exit_error("too many arguments; try just "CLI_ARG_HELP);
	if (!strncmp(argv[CLI_ARG_DEVICE], CLI_ARG_HELP, CLI_ARG_MAX_LEN))
		exit_help();
	if (argc == CLI_ARG_COUNT_MAX)
		cliaction = argv[CLI_ARG_ACTION];
	if (!(!cliaction ||
	      is_cli_action(CLI_ARG_TIME) ||
	      is_cli_action(CLI_ARG_MOVE) ||
	      is_cli_action(CLI_ARG_COPY) ||
	      is_cli_action(CLI_ARG_LIVE) ||
	      is_cli_action(CLI_ARG_ECHO)))
		exit_error("unknown action specified");

	/* Initial exchange. */

	unsigned int i;

	unbuf_stdout(); /* Simulator and live need this. */
#ifndef SIMULATOR
	open_file(&dev, argv[CLI_ARG_DEVICE], "r+b");
#endif

	for (i = 0; i < LEN_EXCHANGES; i++) {
		if (exchanges[i].id == SYNCHRONIZE_DEVICE_DATE_AND_TIME)
			set_current_time(exchanges[i].write.data);
		do_exchange(exchanges[i]);
	}

	if (is_cli_action(CLI_ARG_TIME))
		exit(EXIT_SUCCESS);

	if (!cliaction ||
	    is_cli_action(CLI_ARG_MOVE) ||
	    is_cli_action(CLI_ARG_COPY)) {
		do_exchange(storedpresent);

		if (!in[2])
			exit_error("no record stored");
		if (in[3]) {
			smode = &storedauto;
			inauto = 1;
		} else {
			smode = &storedmanual;
		}

		dcmdlength = smode->record.dcmdlength;
		dcmdstart = smode->datums.start;
		dcmdend = smode->datums.end;
		seql = smode->sequence.start;
		seqm = smode->sequence.end;
		process_flags = smode->process.flags;
		process_nibbles = smode->process.nibbles;

		process_stored();
	} else if (is_cli_action(CLI_ARG_LIVE) ||
		   is_cli_action(CLI_ARG_ECHO)) {
		process_live();
	}

#ifndef SIMULATOR
	close_file(dev);
#endif

	exit(EXIT_SUCCESS);
}
