#include <inttypes.h>
#include <stdio.h>

struct mini {
	uint32_t a : 12;
};

int main()
{
	struct mini m;
	m.a = -3;

	// 명시적인 수동 sign extension
	// 끝까지 갔다가 다시 돌리면
	// 젤 끝 비트로 채워짐
	int64_t b = ((int64_t)m.a << 52) >> 52;

	int64_t c = (int64_t)3;
	b = b + c;
	printf("0x%" PRIx64 "\n", b);

	return 0;
}
