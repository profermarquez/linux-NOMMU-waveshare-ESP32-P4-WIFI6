#include <stdio.h>
#include <sys/utsname.h>

int main(void)
{
	struct utsname u;
	uname(&u);
	printf("Hola desde %s %s (%s)\n", u.sysname, u.release, u.machine);
	return 0;
}
