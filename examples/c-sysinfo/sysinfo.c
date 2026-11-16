/* sysinfo.c - muestra datos del sistema y prueba malloc (C, bFLT NOMMU). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>

int main(int argc, char **argv)
{
	struct utsname u;
	struct sysinfo si;
	size_t kb = argc > 1 ? (size_t)atoi(argv[1]) : 512;
	char *p;

	uname(&u);
	sysinfo(&si);
	printf("%s %s %s\n", u.nodename, u.release, u.machine);
	printf("encendida %ld s, RAM libre %lu KB de %lu KB\n", si.uptime,
	       si.freeram * si.mem_unit / 1024, si.totalram * si.mem_unit / 1024);
	p = malloc(kb * 1024);
	if (!p) {
		printf("malloc(%zu KB) falló: no hay un bloque contiguo así de grande\n", kb);
		return 1;
	}
	memset(p, 0x5a, kb * 1024);
	printf("malloc(%zu KB) OK\n", kb);
	free(p);
	return 0;
}
