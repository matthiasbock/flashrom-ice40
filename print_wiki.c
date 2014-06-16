/*
 * This file is part of the flashrom project.
 *
 * Copyright (C) 2009 Uwe Hermann <uwe@hermann-uwe.de>
 * Copyright (C) 2009 Carl-Daniel Hailfinger
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "flash.h"
#include "flashchips.h"
#include "programmer.h"

static const char wiki_header[] = "= Supported devices =\n\n\
<div style=\"margin-top:0.5em; padding:0.5em 0.5em 0.5em 0.5em; \
background-color:#eeeeee; text-align:left; border:1px solid #aabbcc;\">\
<small>\n\
'''Last update:''' %s (generated by flashrom %s)<br />\n\
The tables below are generated from flashrom's source by copying the output of '''flashrom -z'''.<br /><br />\n\
A short explanation of the cells representing the support state follows:<br />\n\
{| border=\"0\" valign=\"top\"\n\
! style=\"text-align:left;\" |\n\
! style=\"text-align:left;\" |\n\
|-\n\
|{{OK}}\n\
| The feature was '''tested and should work''' in general unless there is a bug in flashrom or another component in \
the system prohibits some functionality.\n\
|-\n\
|{{Dep}}\n\
| '''Configuration-dependent'''. The feature was tested and should work in general but there are common \
configurations that drastically limit flashrom's capabilities or make it completely stop working.\n\
|-\n\
|{{?3}}\n\
| The feature is '''untested''' but believed to be working.\n\
|-\n\
|{{NA}}\n\
| The feature is '''not applicable''' in this configuration (e.g. write operations on ROM chips).\n\
|-\n\
|{{No}}\n\
| The feature is '''known to not work'''. Don't bother testing (nor reporting. Patches welcome! ;).\n\
|}\n\
</small></div>\n";

static const char th_start[] = "| valign=\"top\"|\n\n\
{| border=\"0\" style=\"font-size: smaller\" valign=\"top\"\n\
|- bgcolor=\"#6699dd\"\n";

#if CONFIG_INTERNAL == 1
static const char chipset_th[] = "\
! align=\"left\" | Vendor\n\
! align=\"left\" | Southbridge\n\
! align=\"center\" | PCI IDs\n\
! align=\"center\" | Status\n\n";

static const char board_th[] = "\
! align=\"left\" | Vendor\n\
! align=\"left\" | Mainboard\n\
! align=\"left\" | Required option\n\
! align=\"center\" | Status\n\n";

static const char board_intro[] = "\
\n== Supported mainboards ==\n\n\
In general, it is very likely that flashrom works out of the box even if your \
mainboard is not listed below.\n\nThis is a list of mainboards where we have \
verified that they either do or do not need any special initialization to \
make flashrom work (given flashrom supports the respective chipset and flash \
chip), or that they do not yet work at all. If they do not work, support may \
or may not be added later.\n\n\
Mainboards (or individual revisions) which don't appear in the list may or may \
not work (we don't know, someone has to give it a try). Please report any \
further verified mainboards on the [[Mailinglist|mailing list]].\n";
#endif

static const char chip_th[] = "\
! align=\"left\" | Vendor\n\
! align=\"left\" | Device\n\
! align=\"center\" | Size [kB]\n\
! align=\"center\" | Type\n\
! align=\"center\" colspan=\"4\" | Status\n\
! align=\"center\" colspan=\"2\" | Voltage [V]\n\n\
|- bgcolor=\"#6699ff\"\n| colspan=\"4\" | &nbsp;\n\
| Probe\n| Read\n| Erase\n| Write\n\
| align=\"center\" | Min \n| align=\"center\" | Max\n\n";

static const char chip_intro[] = "\
\n== Supported flash chips ==\n\n\
The list below contains all chips that have some kind of explicit support added to flashrom and their last \
known test status. Newer SPI flash chips might work even without explicit support if they implement SFDP ([\
http://www.jedec.org/standards-documents/docs/jesd216 Serial Flash Discoverable Parameters - JESD216]). \
Flashrom will detect this automatically and inform you about it.\n\n\
The names used below are designed to be as concise as possible and hence contain only the characters \
describing properties that are relevant to flashrom. Irrelevant characters specify attributes flashrom can not \
use or even detect by itself (e.g. the physical package) and have no effect on flashrom's operation. They are \
replaced by dots ('.') functioning as wildcards (like in Regular Expressions) or are completely omitted at the \
end of a name.\n";

static const char programmer_th[] = "\
! align=\"left\" | Programmer\n\
! align=\"left\" | Vendor\n\
! align=\"left\" | Device\n\
! align=\"center\" | IDs\n\
! align=\"center\" | Status\n\n";

/* The output of this module relies on MediaWiki templates to select special formatting styles for table cells
 * reflecting the test status of the respective hardware. This functions returns the correct template name for
 * the supplied enum test_state. */
static const char *test_state_to_template(enum test_state test_state)
{
	switch (test_state) {
	case OK: return "OK";
	case BAD: return "No";
	case NA: return "NA";
	case DEP: return "Dep";
	case NT:
	default: return "?3";
	}
}

#if CONFIG_INTERNAL == 1
static const char laptop_intro[] = "\n== Supported laptops/notebooks ==\n\n\
In general, flashing laptops is more difficult because laptops\n\n\
* often use the flash chip for stuff besides the BIOS,\n\
* often have special protection stuff which has to be handled by flashrom,\n\
* often use flash translation circuits which need drivers in flashrom.\n\n\
<div style=\"margin-top:0.5em; padding:0.5em 0.5em 0.5em 0.5em; \
background-color:#ff6666; align:right; border:1px solid #000000;\">\n\
'''IMPORTANT:''' At this point we recommend to '''not''' use flashrom on \
untested laptops unless you have a means to recover from a flashing that goes \
wrong (a working backup flash chip and/or good soldering skills).\n</div>\n";

static void print_supported_chipsets_wiki(int cols)
{
	int i;
	unsigned int lines_per_col;
	const struct penable *e;
	int enablescount = 0, color = 1;

	for (e = chipset_enables; e->vendor_name != NULL; e++)
		enablescount++;

	/* +1 to force the resulting number of columns to be < cols */
	lines_per_col = enablescount / cols + ((enablescount%cols) > 0 ? 1 : 0);

	printf("\n== Supported chipsets ==\n\nTotal amount of supported chipsets: '''%d'''\n\n"
	       "{| border=\"0\" valign=\"top\"\n", enablescount);

	e = chipset_enables;
	for (i = 0; e[i].vendor_name != NULL; i++) {
		if ((i % lines_per_col) == 0)
			printf("%s%s", th_start, chipset_th);

		/* Alternate colors if the vendor changes. */
		if (i > 0 && strcmp(e[i].vendor_name, e[i - 1].vendor_name))
			color = !color;

		printf("|- bgcolor=\"#%s\"\n| %s || %s "
		       "|| %04x:%04x || {{%s}}\n", (color) ? "eeeeee" : "dddddd",
		       e[i].vendor_name, e[i].device_name,
		       e[i].vendor_id, e[i].device_id,
		       test_state_to_template(e[i].status));

		if (((i % lines_per_col) + 1) == lines_per_col)
			printf("\n|}\n\n");
	}

	/* end inner table if it did not fill the last column fully */
	if (((i % lines_per_col)) > 0)
		printf("\n|}\n\n");
	printf("\n\n|}\n");
}

static void print_supported_boards_wiki_helper(const char *devicetype, int cols, const struct board_info boards[])
{
	int i, k;
	unsigned int boardcount, lines_per_col;
	unsigned int boardcount_good = 0, boardcount_bad = 0, boardcount_nt = 0;
	int num_notes = 0, color = 1;
	char *notes = calloc(1, 1);
	char tmp[900 + 1];
	const struct board_match *b = board_matches;

	for (i = 0; boards[i].vendor != NULL; i++) {
		if (boards[i].working == OK)
			boardcount_good++;
		else if (boards[i].working == NT)
			boardcount_nt++;
		else
			boardcount_bad++;
	}
	boardcount = boardcount_good + boardcount_nt + boardcount_bad;

	/* +1 to force the resulting number of columns to be < cols */
	lines_per_col = boardcount / cols + ((boardcount%cols) > 0 ? 1 : 0);

	printf("\n\nTotal amount of known good %s: '''%d'''; "
	       "Untested (e.g. user vanished before testing new code): '''%d'''; "
	       "Not yet supported (i.e. known-bad): '''%d'''.\n\n"
	       "{| border=\"0\" valign=\"top\"\n", devicetype, boardcount_good, boardcount_nt, boardcount_bad);

	for (i = 0; boards[i].vendor != NULL; i++) {
		if ((i % lines_per_col) == 0)
			printf("%s%s", th_start, board_th);

		/* Alternate colors if the vendor changes. */
		if (i > 0 && strcmp(boards[i].vendor, boards[i - 1].vendor))
			color = !color;

		k = 0;
		while ((b[k].vendor_name != NULL) &&
			(strcmp(b[k].vendor_name, boards[i].vendor) ||
			 strcmp(b[k].board_name, boards[i].name))) {
			k++;
		}

		printf("|- bgcolor=\"#%s\"\n| %s || %s%s %s%s || %s%s%s%s "
		       "|| {{%s}}", (color) ? "eeeeee" : "dddddd",
		       boards[i].vendor,
		       boards[i].url ? "[" : "",
		       boards[i].url ? boards[i].url : "",
		       boards[i].name,
		       boards[i].url ? "]" : "",
		       b[k].lb_vendor ? "-p internal:mainboard=" : "&mdash;",
		       b[k].lb_vendor ? b[k].lb_vendor : "",
		       b[k].lb_vendor ? ":" : "",
		       b[k].lb_vendor ? b[k].lb_part : "",
		       test_state_to_template(boards[i].working));

		if (boards[i].note) {
			num_notes++;
			printf(" <span id=\"%s_ref%d\"><sup>[[#%s_note%d|%d]]</sup></span>\n",
			       devicetype, num_notes, devicetype, num_notes, num_notes);
			int ret = snprintf(tmp, sizeof(tmp),
					   "<span id=\"%s_note%d\">%d. [[#%s_ref%d|&#x2191;]]</span>"
					   " <nowiki>%s</nowiki><br />\n", devicetype, num_notes, num_notes,
					   devicetype, num_notes, boards[i].note);
			if (ret < 0 || ret >= sizeof(tmp)) {
				fprintf(stderr, "Footnote text #%d of %s truncated (ret=%d, sizeof(tmp)=%zu)\n",
					num_notes, devicetype, ret, sizeof(tmp));
			}
			notes = strcat_realloc(notes, tmp);
		} else {
			printf("\n");
		}

		if (((i % lines_per_col) + 1) == lines_per_col)
			printf("\n|}\n\n");
	}

	/* end inner table if it did not fill the last column fully */
	if (((i % lines_per_col)) > 0)
		printf("\n|}\n\n");
	printf("|}\n");

	if (num_notes > 0)
		printf("\n<small>\n%s</small>\n", notes);
	free(notes);
}

static void print_supported_boards_wiki(void)
{
	printf("%s", board_intro);
	print_supported_boards_wiki_helper("boards", 2, boards_known);

	printf("%s", laptop_intro);
	print_supported_boards_wiki_helper("laptops", 1, laptops_known);
}
#endif

static void print_supported_chips_wiki(int cols)
{
	unsigned int lines_per_col;
	char *s;
	char vmax[6];
	char vmin[6];
	const struct flashchip *f, *old = NULL;
	int i = 0, c = 1, chipcount = 0;

	for (f = flashchips; f->name != NULL; f++) {
		chipcount++;
	}

	/* +1 to force the resulting number of columns to be < cols */
	lines_per_col = chipcount / cols + ((chipcount%cols) > 0 ? 1 : 0);

	printf("%s", chip_intro);
	printf("\nTotal amount of supported chips: '''%d'''\n\n"
	       "{| border=\"0\" valign=\"top\"\n", chipcount);

	for (f = flashchips; f->name != NULL; f++) {
		if ((i % lines_per_col) == 0)
			printf("%s%s", th_start, chip_th);

		/* Alternate colors if the vendor changes. */
		if (old != NULL && strcmp(old->vendor, f->vendor))
			c = !c;

		old = f;
		s = flashbuses_to_text(f->bustype);
		sprintf(vmin, "%0.03f", f->voltage.min / (double)1000);
		sprintf(vmax, "%0.03f", f->voltage.max / (double)1000);
		printf("|- bgcolor=\"#%s\"\n| %s || %s || align=\"right\" | %d "
		       "|| %s || {{%s}} || {{%s}} || {{%s}} || {{%s}}"
		       "|| %s || %s \n",
		       (c == 1) ? "eeeeee" : "dddddd", f->vendor, f->name,
		       f->total_size, s,
		       test_state_to_template(f->tested.probe),
		       test_state_to_template(f->tested.read),
		       test_state_to_template(f->tested.erase),
		       test_state_to_template(f->tested.write),
		       f->voltage.min ? vmin : "?",
		       f->voltage.max ? vmax : "?");
		free(s);

		if (((i % lines_per_col) + 1) == lines_per_col)
			printf("\n|}\n\n");
		i++;
	}
	/* end inner table if it did not fill the last column fully */
	if (((i % lines_per_col)) > 0)
		printf("\n|}\n\n");
	printf("|}\n\n");
}

/* Following functions are not needed when no PCI/USB programmers are compiled in,
 * but since print_wiki code has no size constraints we include it unconditionally. */
static int count_supported_devs_wiki(const struct dev_entry *devs)
{
	unsigned int count = 0;
	unsigned int i = 0;
	for (i = 0; devs[i].vendor_id != 0; i++)
		count++;
	return count;
}

static void print_supported_devs_wiki_helper(const struct programmer_entry prog)
{
	int i = 0;
	static int c = 0;
	const struct dev_entry *devs = prog.devs.dev;
	const unsigned int count = count_supported_devs_wiki(devs);

	/* Alternate colors if the vendor changes. */
	c = !c;

	for (i = 0; devs[i].vendor_id != 0; i++) {
		printf("|- bgcolor=\"#%s\"\n", (c) ? "eeeeee" : "dddddd");
		if (i == 0)
			printf("| rowspan=\"%u\" | %s |", count, prog.name);
		printf("| %s || %s || %04x:%04x || {{%s}}\n", devs[i].vendor_name, devs[i].device_name,
		       devs[i].vendor_id, devs[i].device_id, test_state_to_template(devs[i].status));
	}
}

static void print_supported_devs_wiki()
{
	unsigned int pci_count = 0;
	unsigned int usb_count = 0;
	unsigned int i;

	for (i = 0; i < PROGRAMMER_INVALID; i++) {
		const struct programmer_entry prog = programmer_table[i];
		switch (prog.type) {
		case USB:
			usb_count += count_supported_devs_wiki(prog.devs.dev);
			break;
		case PCI:
			pci_count += count_supported_devs_wiki(prog.devs.dev);
			break;
		case OTHER:
		default:
			break;
		}
	}

	printf("\n== PCI Devices ==\n\n"
	       "Total amount of supported PCI devices flashrom can use as a programmer: '''%d'''\n\n"
	       "{%s%s", pci_count, th_start, programmer_th);

	for (i = 0; i < PROGRAMMER_INVALID; i++) {
		const struct programmer_entry prog = programmer_table[i];
		if (prog.type == PCI) {
			print_supported_devs_wiki_helper(prog);
		}
	}
	printf("\n|}\n\n|}\n");

	printf("\n== USB Devices ==\n\n"
	       "Total amount of supported USB devices flashrom can use as a programmer: '''%d'''\n\n"
	       "{%s%s", usb_count, th_start, programmer_th);

	for (i = 0; i < PROGRAMMER_INVALID; i++) {
		const struct programmer_entry prog = programmer_table[i];
		if (prog.type == USB) {
			print_supported_devs_wiki_helper(prog);
		}
	}
	printf("\n|}\n\n|}\n");

	printf("\n== Other programmers ==\n\n"
	       "{%s", th_start);
	printf("! align=\"left\" | Programmer\n"
	       "! align=\"left\" | Note\n\n");

	for (i = 0; i < PROGRAMMER_INVALID; i++) {
		static int c = 0;
		const struct programmer_entry prog = programmer_table[i];
		if (prog.type == OTHER && prog.devs.note != NULL) {
			c = !c;
			printf("|- bgcolor=\"#%s\"\n", (c) ? "eeeeee" : "dddddd");
			printf("| %s || %s", prog.name, prog.devs.note);
		}
	}
	printf("\n|}\n\n|}\n");
}

void print_supported_wiki(void)
{
	time_t t = time(NULL);
	char buf[sizeof("1986-02-28T12:37:42Z")];
	strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", gmtime(&t));

	printf(wiki_header, buf, flashrom_version);
	print_supported_chips_wiki(2);
#if CONFIG_INTERNAL == 1
	print_supported_chipsets_wiki(3);
	print_supported_boards_wiki();
#endif
	print_supported_devs_wiki();
}

