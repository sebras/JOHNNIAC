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

#define nelem(arr) (sizeof(arr) / sizeof(arr[0]))

#define fatal(fmt, ...) do { printf("FATAL: " fmt, __VA_ARGS__); abort(); } while (0)
#define trace(fmt, ...) do { if (debug) printf(fmt, __VA_ARGS__); } while (0)

void reset_memory()
{
}

void reset_registers()
{
}

void reset_breakpoints()
{
}

void reset_card_punch()
{
}

void reset_all()
{
	reset_memory();
	reset_registers();
	reset_card_punch();
	reset_breakpoints();
}

void state()
{
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
