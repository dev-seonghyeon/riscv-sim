#include <inttypes.h>
#include <stdio.h>

struct mini {
	int32_t a : 3;
};

int main()
{
	struct mini m;
	m.a = -3;

	int64_t b = m.a;

	int64_t c = (int64_t)3;
	b = b + c;
	printf("0x%" PRIX64 "\n", b);

	return 0;


}
