#include "system.h"

int main()
{
	struct system_t system = system_init();

	system_delete(system);
	return 0;
}
