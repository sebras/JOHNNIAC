#include <string.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <readline/readline.h>
#include <readline/history.h>

#define VERSION "0.0.1"

static bool showstate = true;
static bool debug = true;

uint16_t breakpoint[8];

static bool ignore_overflow = true;
static bool t1 = false;
static bool t2 = false;
static bool t3 = false;
static bool h1 = false;
static bool h2 = false;
static bool h3 = false;

static uint64_t memory[4096];

static uint64_t card[2 * 12];
static uint8_t card_row;
static unsigned int card_nr;

static uint16_t nia;
static uint8_t lr;
static uint64_t ir;
static uint64_t a;
static uint64_t mq;
static uint64_t n;
static uint8_t dev;
static bool halt;
static bool overflow;

static const char *mnemonic[] = {
	"blank", "tnl", "tpl", "tfl", "lm", "tnr", "tpr", "tfr",
	"trl", "t1l", "t2l", "t3l", "trr", "t1r", "t2r", "t3r",
	"ra", "rs", "rav", "rsv", "a", "s", "av", "sv",
	"mr", "mnr", "m", "mn", "mb", "mnb", "ma", "mna",
	"ds", "dns", "ill", "ill", "d", "dn", "ill", "ill",
	"st", "sol", "sal", "shl", "sab", "sor", "sar", "shr",
	"stq", "snq", "svq", "snv", "aqs", "sqs", "avs", "svs",
	"src", "clc", "lrc", "llc", "srh", "clh", "lrh", "llh",
	"sel", "c", "ill", "ill", "dis", "hut", "ej", "clk",
	"rd", "wd", "ill", "ill", "ill", "ill", "ill", "ill",
	"zta", "zta", "zta", "zta", "pi", "ni", "pmi", "nmi",
	"htl", "h1l", "h2l", "h3l", "htr", "h1r", "h2r", "h3r",
	"ill", "ill", "ill", "ill", "ill", "ill", "ill", "ill",
	"ill", "ill", "ill", "ill", "ill", "ill", "ill", "ill",
	"ill", "ill", "ill", "ill", "ill", "ill", "ill", "ill",
	"ill", "ill", "ill", "ill", "ill", "ill", "ill", "ill",
};

#define nelem(arr) (sizeof(arr) / sizeof(arr[0]))

#define fatal(fmt, ...) do { printf("FATAL: " fmt, __VA_ARGS__); abort(); } while (0)
#define trace(fmt, ...) do { if (debug) printf(fmt, __VA_ARGS__); } while (0)

void reset_memory()
{
	memset(memory, 0, sizeof(memory));
}

void reset_registers()
{
	a = mq = n = ir = nia = lr = 0;
	overflow = false;
	dev = 0;
}

void reset_breakpoints()
{
	int i;
	for (i = 0; i < nelem(breakpoint); i++)
		breakpoint[i] = UINT16_MAX;
}

void reset_card_punch()
{
	card_row = 0;
	memset(card, 0, nelem(card));
}

void reset_all()
{
	reset_memory();
	reset_registers();
	reset_card_punch();
	reset_breakpoints();
}

void fetch()
{
	ir = memory[nia];
}

void advance()
{
	lr++;
	if (lr >= 02)
	{
		lr = 00;
		nia++;
		if (nia >= 07777)
		{
			nia = 00000;
		}
	}
};

void punch_card()
{
	int x, y, k;
	uint64_t val;

	printf(" /-------------------------------------------------------------------------------+\n");
	printf("/  CARD %10u                                                               |\n", card_nr++);
	for (y = 0; y < 12; y++)
	{
		printf("|");

		val = card[y * 2 + 0];
		for (x = 39; x >= 0; x--)
			printf("%c", ((val >> x) & 1) ? 'o' : '.');
		val = card[y * 2 + 1];
		for (x = 39; x >= 0; x--)
			printf("%c", ((val >> x) & 1) ? 'o' : '.');

		printf("|\n");
	}
	printf("+--------------------------------------------------------------------------------+\n");

	card_row = 0;
	memset(card, 0, nelem(card));
}

void decode()
{
	// |       |   111111111|12|2222222|223333333333|
	// |0123456|789012345678|90|1234567|890123456789|
	// | order | address    |xx| order | address    |

	uint8_t ord;
	uint16_t adr;
	uint8_t cls;
	uint8_t var;
	int tmp;

	if (lr)
	{
		ord = (ir >> 12) & 0177;
		adr = (ir >> 0) & 07777;
	}
	else
	{
		ord = (ir >> 33) & 0177;
		adr = (ir >> 21) & 07777;
	}

	cls = (ord >> 3) & 017;
	var = (ord >> 0) & 07;

#define instr(i) do { trace("nia %04o lr %01o cls %02o var %01o order %03o adr %04o %s\n", nia, lr, cls, var, ord, adr, i); } while (0)
#define unknown_class() do { printf("ERROR: unknown order class %02o for order %03o, abort\n", cls, ord); halt = true; } while (0)
#define unknown_variation() do { printf("ERROR: unknown order variation %01o for order %03o, abort\n", var, ord); halt = true; } while (0)
#define unknown_device() do { printf("ERROR: unknown device selected %01o.\n", dev); halt = true; } while (0)

	switch (cls)
	{
	case 000: // conditional transfer and other operations
		switch (var)
		{
		case 00: instr("blank"); // no operation
			advance(); break;

		case 04: instr("lm"); // load multiplier
			n = memory[adr]; mq = n; advance(); break;

		case 01: instr("tnl"); // transfer negative left
			if ((a >> 39) & 1) { lr = 0; nia = adr; } else advance(); break;

		case 05: instr("tnr"); // transfer negative right
			if ((a >> 39) & 1) { nia = adr; lr = 1; } else advance(); break;

		case 02: instr("tpl"); // transfer positive left
			if (!((a >> 39) & 1)) { nia = adr; lr = 0; } else advance(); break;

		case 06: instr("tpr"); // transfer positive right
			if (!((a >> 39) & 1)) { nia = adr; lr = 1; } else advance(); break;

		case 03: instr("tfl"); // transfer overflow left
			if (overflow) { nia = adr; lr = 0; overflow = false; } else advance(); break;

		case 07: instr("tfr"); // transfer overflow right
			if (overflow) { nia = adr; lr = 1; overflow = false; } else advance(); break;

		default: unknown_variation();
		}
		break;

	case 001: // transfer operations
		switch (var)
		{
		case 00: instr("trl"); // transfer to the left
			nia = adr; lr = 0; break;

		case 04: instr("trr"); // transfer to the right
			nia = adr; lr = 1; break;

		case 01: instr("t1l"); // transfer to left on t1
			if (t1) { nia = adr; lr = 0; } else advance(); break;

		case 05: instr("t1r"); // transfer to right on t1
			if (t1) { nia = adr; lr = 1; } else advance(); break;

		case 02: instr("t2l"); // transfer to left on t2
			if (t2) { nia = adr; lr = 0; } else advance(); break;

		case 06: instr("t2r"); // transfer to right on t2
			if (t2) { nia = adr; lr = 1; } else advance(); break;

		case 03: instr("t3l"); // transfer to left on t3
			if (t3) { nia = adr; lr = 0; } else advance(); break;

		case 07: instr("t3r"); // transfer to right on t3
			if (t3) { nia = adr; lr = 1; } else advance(); break;

		default: unknown_variation();
		}
		break;

	case 002: // add operations
		switch (var)
		{
		case 00: instr("ra"); // reset add
			a = 0; n = memory[adr]; a = n; break;

		case 04: instr("a"); // add
			n = memory[adr]; a += n; break;

		case 05: instr("s"); // subtract
			n = -memory[adr]; a += n; break;
			break;

		case 03: instr("rsv"); // reset subtract absolute value
			a = 0; n = memory[adr]; if ((n >> 39) & 1) a += n; else a += -n; break;
			break;

		case 01: instr("rs"); // reset subtract
			a = 0; n = memory[adr]; a += -n; break;
			break;

		default: unknown_variation();
		}
		overflow = (((a >> 39) & 1) != ((n >> 39) & 1));
		advance();
		break;

	case 005: // store and substitute operations
		switch (var)
		{
		case 00: instr("st"); // store full word
			memory[adr] = (a & 017777777777777) | (memory[adr] & 000000000000000); break;

		case 01: instr("sol"); // store left operation
			memory[adr] = (a & 017700000000000) | (memory[adr] & 000077777777777); break;

		case 02: instr("sal"); // store left address
			memory[adr] = (a & 000077774000000) | (memory[adr] & 017700003777777); break;

		case 03: instr("shl"); // store left half word
			memory[adr] = (a & 017777774000000) | (memory[adr] & 000000003777777); break;

		case 04: instr("sab"); // store both addresses
			memory[adr] = (a & 000077774007777) | (memory[adr] & 017700003770000); break;

		case 05: instr("sor"); // store right operation
			memory[adr] = (a & 000000003770000) | (memory[adr] & 017777774007777); break;

		case 06: instr("sar"); // store right address
			memory[adr] = (a & 000000000007777) | (memory[adr] & 017777777770000); break;

		case 07: instr("shr"); // store right half word
			memory[adr] = (a & 000000003777777) | (memory[adr] & 017777774000000); break;

		default: unknown_variation();
		}
		advance();
		break;

	case 007: // shift operations
		switch (var)
		{
		case 00: instr("src"); // shift accumulator short to the right
			mq = 0; a >>= 1; break;

		case 05: instr("clh"); // circular long shift left hold
			int tmp = (a >> 39) & 1;
			a <<= 1;
			a |= (mq >> 39) & 1;
			mq <<= 1;
			mq |= tmp;
			break;


		default: unknown_variation();
		}
		advance();
		break;

	case 010: // input output operations
		switch (var)
		{
		case 00: instr("sel"); // select input or output device
			dev = adr & 07; break;

		case 01: instr("c"); // copy operation
			switch (dev)
			{
			case 02: // card punch feed
				n = memory[adr];
				printf("A = %014" PRIo64 " N = %014" PRIo64 "\n", a, n);
				card[card_row * 2 + 0] = n & 017777777777777;
				card[card_row++ * 2 + 1] = a & 017777777777777;
				if (card_row == 12)
					punch_card();
				break;

			default: unknown_device();
			}
			break;

		case 06: instr("ej"); // eject page from printer
			break;

		default: unknown_variation();
		}
		advance();
		break;

	case 012: // intersection or logical product operations
		switch (var)
		{
		case 00:
		case 01:
		case 02:
		case 03: instr("zta"); // zero to accumulator
			a = 0; break;

		default: unknown_variation();
		}
		advance();
		break;

	case 013: // halt and transfer control operations
		switch (var)
		{
		case 00: instr("htl"); // unconditional halt and transfer left
			nia = adr; lr = 0; halt = true; break;

		case 04: instr("htr"); // unconditional halt and transfer right
			nia = adr; lr = 1; halt = true; break;

		default: unknown_variation();
		}
		break;

	default: unknown_class();
	}

	if (overflow && !ignore_overflow)
		halt = true;
}

void step()
{
	int i;

	fetch();
	decode();

	for (i = 0; i < nelem(breakpoint); i++)
	{
		if (breakpoint[i] == UINT16_MAX)
			continue;

		if ((breakpoint[i] & 010000) == 0)
			continue;

		if (nia != (breakpoint[i] & 07777))
			continue;

		printf("hit breakpoint %01o at %04o.\n", i, nia);
		halt = true;
		break;
	}
}

void steps(int argc, char **argv)
{
	uint64_t steps = 1;
	uint64_t i;

	halt = false;

	steps = argc >= 1 ? strtoull(argv[0], NULL, 10) : 1;
	for (i = 0; !halt && i < steps; i++)
		step();
}

void go(int argc, char **argv)
{
	halt = false;

	while (!halt)
		step();
}

void toggle(int argc, char **argv)
{
	if (argc <= 0)
		return;

	if (!strcmp(argv[0], "t1"))
		t1 = t1 ? false : true;
	else if (!strcmp(argv[0], "t2"))
		t2 = t2 ? false : true;
	else if (!strcmp(argv[0], "t3"))
		t3 = t3 ? false : true;
	else if (!strcmp(argv[0], "h1"))
		h1 = h1 ? false : true;
	else if (!strcmp(argv[0], "h2"))
		h2 = h2 ? false : true;
	else if (!strcmp(argv[0], "h3"))
		h3 = h3 ? false : true;
	else
		printf("ERROR: unknown toggle switch\n");
}

void load(int argc, char **argv)
{
	uint16_t adr;
	uint64_t word;
	FILE *f;
	char line[21];
	char *end;

	if (argc != 1)
	{
		printf("ERROR: incorrect number of argument to load command.\n");
		return;
	}

	f = fopen(argv[0], "r");
	if (f == NULL)
	{
		printf("ERROR: can not open input file.\n");
		return;
	}

	while (true)
	{
		memset(line, 0, nelem(line));
		if (!fgets(line, nelem(line), f))
			break;

		if (line[4] != ' ')
			{ printf("ERROR: expected space between address and word.\n"); return; }
		if (line[19] != '\n')
			{ printf("ERROR: expected newline after word\n"); return; }

		line[4] = '\0';
		line[19] = '\0';

		end = NULL;
		adr = strtoull(&line[0], &end, 8);
		if (strlen(end) > 0)
		{
			printf("ERROR: invalid octal digit found in address.\n");
			return;
		}
		word = strtoull(&line[5], &end, 8);
		if (strlen(end) > 0)
		{
			printf("ERROR: invalid octal digit found in word.\n");
			return;
		}

		if (adr > 07777)
		{
			printf("ERROR: address out of range.\n");
			return;
		}
		if (word > 017777777777777)
		{
			printf("ERROR: word out of range.\n");
			return;
		}

		memory[adr] = word;
	}

	if (fclose(f))
	{
		printf("ERROR: can not close output file.\n");
		return;
	}
}

void save(int argc, char **argv)
{
	uint16_t start, end, adr;
	FILE *f = NULL;

	if (argc < 1)
	{
		printf("ERROR: filename missing.\n");
		return;
	}

	if (argc > 1 && argc < 3)
	{
		printf("ERROR: address range missing.\n");
		return;
	}

	if (argc == 3)
	{
		char *ptr = NULL;

		start = strtoull(argv[1], &ptr, 8);
		if (strlen(ptr) > 0)
		{
			printf("ERROR: non-octal digit in start address.\n");
			return;
		}

		end = strtoull(argv[2], &ptr, 8);
		if (strlen(ptr) > 0)
		{
			printf("ERROR: non-octal digit in end address.\n");
			return;
		}
	}
	else
	{
		start = 00000;
		end = 07777;
	}

	if (start > 07777)
	{
		printf("ERROR: start address out of range.\n");
		return;
	}
	if (end > 07777)
	{
		printf("ERROR: end address out of range.\n");
		return;
	}
	if (start > end)
	{
		printf("ERROR: invalid address range.\n");
		return;
	}

	printf("Saving memory %04o-%04o into %s.\n", start, end, argv[0]);

	f = fopen(argv[0], "w");
	if (f == NULL)
	{
		printf("ERROR: can not open output file.\n");
		return;
	}

	for (adr = start; adr <= end; adr++)
		fprintf(f, "%04o %014" PRIo64 "\n", adr, memory[adr]);

	if (fclose(f))
	{
		printf("ERROR: can not close output file.\n");
		return;
	}
}

void reset(int argc, char **argv)
{
	int i;

	if (argc == 0)
		reset_all();
	else
		for (i = 0; i < argc; i++)
		{
			if (!strcmp(argv[i], "mem"))
				reset_memory();
			else if (!strcmp(argv[i], "reg"))
				reset_registers();
		}
}

void reg(int argc, char **argv)
{
	uint64_t val, maxval;
	char *end = NULL;

	if (argc == 0)
	{
		trace(
				"%4s %2s %14s %14s %14s %2s %2s %2s %2s %2s %2s %2s %2s\n"
				"%04o %2o %014" PRIo64 " %014" PRIo64 " %014" PRIo64 " %2o %2o %2o %2o %2o %2o %2o %2o\n",
				"nia","lr", "a", "mq", "n", "of", "io", "t1", "t2", "t3", "h1", "h2", "h3",
				nia, lr, a, mq, n, overflow, ignore_overflow, t1, t2, t3, h1, h2, h3);
	}
	else
		while (argc > 0)
		{
			if (!strcmp(argv[0], "nia"))
				printf("nia %04o\n", nia);
			else if (!strcmp(argv[0], "lr"))
				printf("lr %01o\n", lr);
			else if (!strcmp(argv[0], "ir"))
				printf("ir %014" PRIo64 "\n", ir);
			else if (!strcmp(argv[0], "a"))
				printf("a %014" PRIo64 "\n", a);
			else if (!strcmp(argv[0], "mq"))
				printf("mq %014" PRIo64 "\n", mq);
			else if (!strcmp(argv[0], "n"))
				printf("n %014" PRIo64 "\n", n);
			else if (!strcmp(argv[0], "of"))
				printf("of %01o\n", overflow);
			else
			{
				printf("ERROR: unknown register.\n");
				return;
			}

			argc--;
			argv++;
		}
}

void setreg(int argc, char **argv)
{
	uint64_t val, maxval;
	char *end = NULL;

	if (argc < 1)
	{
		printf("ERROR: register name missing.\n");
		return;
	}

	if (argc < 2)
	{
		printf("ERROR: register value missing.\n");
		return;
	}

	val = strtoull(argv[1], &end, 8);
	if (strlen(end) > 0)
	{
		printf("ERROR: non-octal digit in register value\n");
		return;
	}

	if (!strcmp(argv[0], "lr"))
		maxval = 01;
	else if (!strcmp(argv[0], "nia"))
		maxval = 07777;
	else if (!strcmp(argv[0], "ir") || !strcmp(argv[0], "a") || !strcmp(argv[0], "mq") || !strcmp(argv[0], "n"))
		maxval = 07777;

	if (val >= maxval)
	{
		printf("ERROR: %s value out of range.\n", argv[0]);
		return;
	}

	if (!strcmp(argv[0], "lr"))
		lr = val;
	else if (!strcmp(argv[0], "nia"))
		nia = val;
	else if (!strcmp(argv[0], "ir"))
		ir = val;
	else if (!strcmp(argv[0], "a"))
		a = val;
	else if (!strcmp(argv[0], "mq"))
		mq = val;
	else if (!strcmp(argv[0], "n"))
		n = val;
	else
	{
		printf("ERROR: unknown register.\n");
		return;
	}
}

void mem(int argc, char **argv)
{
	uint16_t start, end, adr;
	char *ptr;

	if (argc < 0)
		return;

	while (argc > 0)
	{
		ptr = NULL;
		start = end = strtoull(argv[0], &ptr, 8);
		if (strlen(ptr) > 0)
		{
			if (*ptr != '-')
			{
				printf("ERROR: neither octal digit, nor range dash in start address.\n");
				return;
			}

			ptr++;

			end = strtoull(ptr, &ptr, 8);
			if (strlen(ptr) > 0)
			{
				printf("ERROR: non-octal digit in end address.\n");
				return;
			}
		}

		if (start >= 07777)
		{
			printf("ERROR: start address out of range.\n");
			return;
		}
		if (end >= 07777)
		{
			printf("ERROR: start address out of range.\n");
			return;
		}
		if (start > end)
		{
			printf("ERROR: invalid address range.\n");
			return;
		}

		for (adr = start; adr <= end; adr++)
		{
			printf("memory[%04o]: %014" PRIo64 "\n",
					adr, memory[adr]);
		}

		argc--;
		argv++;
	}
}

void setmem(int argc, char **argv)
{
	uint16_t start, end, adr;
	uint64_t val;
	char *ptr;
	int i;

	if (argc < 0)
		return;

	while (argc > 0)
	{
		ptr = NULL;
		start = end = strtoull(argv[0], &ptr, 8);
		if (strlen(ptr) > 0)
		{
			if (*ptr != '-')
			{
				printf("ERROR: neither octal digit, nor range dash in start address: %s.\n", argv[0]);
				return;
			}

			ptr++;

			end = strtoull(ptr, &ptr, 8);
			if (strlen(ptr) > 0)
			{
				printf("ERROR: non-octal digit in end address: %s\n", argv[0]);
				return;
			}
		}

		if (start >= 07777)
		{
			printf("ERROR: start address out of range: %s\n", argv[0]);
			return;
		}
		if (end >= 07777)
		{
			printf("ERROR: start address out of range: %s\n", argv[0]);
			return;
		}
		if (start > end)
		{
			printf("ERROR: invalid address range: %s\n", argv[0]);
			return;
		}

		argc--;
		argv++;

		if (argc < end - start + 1)
		{
			printf("ERROR: too few words to set at memory address: %s\n", argv[-1]);
			return;
		}

		for (adr = start; adr <= end; adr++)
		{
			char *p = argv[0];

			while (*p)
			{
				if (*p == '.')
					memmove(p, p + 1, strlen(p + 1) + 1);
				else
					p++;
			}

			val = strtoull(argv[0], &ptr, 8);
			if (strlen(ptr) > 0)
			{
				printf("ERROR: non-octal digit in word: %s\n", argv[0]);
				return;
			}
			if (val > 017777777777777)
			{
				printf("ERROR: word value out of range: %s\n", argv[0]);
				return;
			}

			memory[adr] = val;

			argc--;
			argv++;
		}
	}
}

void disasm(int argc, char **argv)
{
	uint16_t start, end, adr;
	uint16_t lord, ladr, rord, radr;
	char *ptr;

	if (argc < 0)
		return;

	while (argc > 0)
	{
		ptr = NULL;
		start = end = strtoull(argv[0], &ptr, 8);
		if (strlen(ptr) > 0)
		{
			if (*ptr != '-')
			{
				printf("ERROR: neither octal digit, nor range dash in start address.\n");
				return;
			}

			ptr++;

			end = strtoull(ptr, &ptr, 8);
			if (strlen(ptr) > 0)
			{
				printf("ERROR: non-octal digit in end address.\n");
				return;
			}
		}

		for (adr = start; adr <= end; adr++)
		{
			lord = (memory[adr] >> 33) & 0177;
			ladr = (memory[adr] >> 21) & 07777;
			rord = (memory[adr] >> 12) & 0177;
			radr = (memory[adr] >> 0) & 07777;

			printf("memory[%04o] = %5s %04o %5s %04o, %03o %04o %03o %04o, %014" PRIo64 "\n",
				adr,
				mnemonic[lord], ladr, mnemonic[rord], radr,
				lord, ladr, rord, radr,
				memory[adr]);
		}

		argc--;
		argv++;
	}
}

void setbreak(int argc, char **argv)
{
	uint16_t adr;
	char *end = NULL;
	int i;

	if (argc == 0)
	{
		for (i = 0; i < nelem(breakpoint); i++)
			if (breakpoint[i] != UINT16_MAX)
				printf("breakpoint %01o @ %04o %sabled\n",
						i, breakpoint[i] & 07777,
						(breakpoint[i] & 010000) ? "en" : "dis");
	}
	else if (argc == 1 && !strcmp(argv[0], "rm"))
	{
		for (i = 0; i < nelem(breakpoint); i++)
			breakpoint[i] = UINT16_MAX;
	}
	else if (argc == 1)
	{
		adr = strtoull(argv[0], &end, 8);
		if (strlen(end) > 0)
		{
			printf("ERROR: non-octal digit in breakpoint address.\n");
			return;
		}

		if (adr >= 07777)
		{
			printf("ERROR: breakpoint address out of range.\n");
			return;
		}

		i = 0;
		while (i < nelem(breakpoint) && breakpoint[i] != UINT16_MAX)
			i++;

		if (i == nelem(breakpoint))
		{
			printf("ERROR: too many breakpoints.\n");
			return;
		}

		breakpoint[i] = adr | 010000;

		printf("breakpoint %01o @ %04o %sabled\n",
				i, breakpoint[i] & 07777,
				(breakpoint[i] & 010000) ? "en" : "dis");
	}
	else if (argc >= 2)
	{
		i = strtoull(argv[0], &end, 8);
		if (strlen(end) > 0)
		{
			printf("ERROR: non-octal digit in breakpoint index.\n");
			return;
		}

		if (i >= nelem(breakpoint))
		{
			printf("ERROR: breakpoint index out of range.\n");
			return;
		}

		if (breakpoint[i] == UINT16_MAX)
		{
			printf("ERROR: breakpoint %01o unused.\n", i);
			return;
		}

		if (!strcmp(argv[1], "dis"))
			breakpoint[i] &= 07777;
		else if (!strcmp(argv[1], "en"))
			breakpoint[i] |= 010000;
		else if (!strcmp(argv[1], "toggle"))
			breakpoint[i] ^= 010000;
		else if (!strcmp(argv[1], "rm"))
			breakpoint[i] = UINT16_MAX;
		else
		{
			printf("ERROR: unknown breakpoint command.\n");
			return;
		}

		if (breakpoint[i] != UINT16_MAX)
			printf("breakpoint %01o @ %04o %sabled\n",
					i, breakpoint[i] & 07777,
					(breakpoint[i] & 010000) ? "en" : "dis");
	}
}

void state()
{
	uint16_t lord, ladr, rord, radr;

	trace(
		"%4s %2s %14s %14s %14s %2s %2s %2s %2s %2s %2s %2s %2s %3s\n"
		"%04o %2o %014" PRIo64 " %014" PRIo64 " %014" PRIo64 " %2o %2o %2o %2o %2o %2o %2o %2o %3o\n",
		"nia","lr", "a", "mq", "n", "of", "io", "t1", "t2", "t3", "h1", "h2", "h3", "dev",
		nia, lr, a, mq, n, overflow, ignore_overflow, t1, t2, t3, h1, h2, h3, dev);

	lord = (memory[nia] >> 33) & 0177;
	ladr = (memory[nia] >> 21) & 07777;
	rord = (memory[nia] >> 12) & 0177;
	radr = (memory[nia] >> 0) & 07777;

	trace(
		"memory[nia] = %03o %04o %03o %04o (%014" PRIo64 ")\n",
		lord, ladr, rord, radr);
	trace(
		"             %-5s    %-5s\n",
		mnemonic[lord], mnemonic[rord]);
}

void run()
{
	char *oldline = NULL;
	char *line = NULL;
	int argc = 0;
	char **argv = NULL;
	char *p;
	char *cmd;

	printf("JOHNNIAC emulator %s, published under the ISC license.\n", VERSION);

	while (true)
	{
		if (showstate)
			state();

		if (!(line = readline("> ")))
		{
			printf("\n");
			break;
		}

		if (strlen(line) == 0)
		{
			free(line);
			line = NULL;
			if (oldline)
				line = strdup(oldline);
		}
		else
		{
			if (oldline)
				free(oldline);
			oldline = strdup(line);
			add_history(line);
		}

		if (line == NULL)
			continue;

		argc = 0;
		argv = NULL;

		p = &line[0];

		while (p && *p)
		{
			while (*p && *p == ' ')
				*p++ = '\0';

			if (*p)
			{
				argv = realloc(argv, (argc + 1) * sizeof(char *));
				argv[argc++] = p;

				while (*p && *p != ' ')
					p++;
			}
		}

		if (argc == 0)
			continue;

		cmd = argv[0];
		argc--;
		argv++;

		if (!strcmp(cmd, "g") || !strcmp(cmd, "go"))
			go(argc, argv);
		else if (!strcmp(cmd, "s") || !strcmp(cmd, "step"))
			step(argc, argv);
		else if (!strcmp(cmd, "reg"))
			reg(argc, argv);
		else if (!strcmp(cmd, "setreg"))
			setreg(argc, argv);
		else if (!strcmp(cmd, "load"))
			load(argc, argv);
		else if (!strcmp(cmd, "save"))
			save(argc, argv);
		else if (!strcmp(cmd, "mem"))
			mem(argc, argv);
		else if (!strcmp(cmd, "setmem"))
			setmem(argc, argv);
		else if (!strcmp(cmd, "disasm") || !strcmp(cmd, "dis"))
			disasm(argc, argv);
		else if (!strcmp(cmd, "toggle"))
			toggle(argc, argv);
		else if (!strcmp(cmd, "reset"))
			reset(argc, argv);
		else if (!strcmp(cmd, "state"))
			showstate = !showstate;
		else if (!strcmp(cmd, "br"))
			setbreak(argc, argv);
		else if (!strcmp(cmd, "quit") || !strcmp(cmd, "exit"))
			{ printf("\n"); break; }
		else
			printf("ERROR: unknown command\n");

		free(argv - 1);
		free(line);
	}

	free(oldline);
}

int main(int argc, char **argv)
{
	reset_all();
	run();
}
