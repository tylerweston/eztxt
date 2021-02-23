// util.c

// based on bisqwit cpu usage from *that* editor
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "headers/util.h"


// show info from /proc/version maybe?

double cpu_info()
{
	// calculate avg mhz used by cpus currently
	char buf[1024];
	FILE* fptr = fopen("/proc/cpuinfo", "r");
	if (!fptr)
		return 0;
	unsigned int total = 0;
	double avg_mhz = 0.0;
	while (fgets(buf, sizeof(buf), fptr))
	{
		if (strncmp(buf, "cpu MHz", 7) == 0)
		{
			const char* p = buf;
			while(*p && *p != ':') ++p;
			while(*p && *p == ':') ++p;
			avg_mhz += strtod(p, NULL) * 1e-3;
			++total;
		}
	}
	fclose(fptr);
	return total ? avg_mhz / total : 0;
}
