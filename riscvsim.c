/*
 * title : riscvsim.c
 * author : Kimseonghyeon
 * description : simulate risc-v instructions
 *
 */

// TODO: 과거버전에서 첫 번째 문장을 실행하면 어떻게 되나 ??
// getline()함수를 쓰기 위한 매크로
// glibc버전마다 선언 매크로가 다름(glibc 2.10 기준)
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include <inttypes.h> // PRIx64, SCNx64
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 레지스터의 값(64비트)를 담는 배열
// 인덱스가 곧 레지스터 번호임
static uint64_t register_file[32] = {(uint64_t)0};

// 명령어 저장용 구조체
struct instructions {

	// TODO : 이거 signed, unsigned구분해서 써야함... 어쩌지...
	// R, I, S에 따라 구조체를 따로 작성하기? 단순함과 멀다....
	uint32_t bit_31_25 : 7; // f3 or imm
	uint32_t bit_24_20 : 5; // rs2 or imm
	uint32_t bit_19_15 : 5; // rs1
	uint32_t bit_14_12 : 3; // funct3
	uint32_t bit_11_7 : 5;	// rd or imm
	uint32_t bit_6_0 : 7;	// opcode

	struct instructions *next;
};

// 수평선 출력 유틸
static void print_horizon()
{
	printf("==========================================\n");
}

// 레지스터파일 디버깅용 출력 유틸
static void print_register_file()
{
	printf("\n");
	print_horizon();
	printf("===========register file value============\n");
	print_horizon();
	for (int i = 0; i < 32; i++) {
		printf("X%2d: 0x%016" PRIx64 "\n", i, register_file[i]);
	}
	print_horizon();
	printf("\n");
}

int main(int argc, char *argv[])
{
	// command line parameter validation
	if (argc < 3) {
		printf("usage ...\n");
		exit(EXIT_FAILURE);
	}

	print_register_file();

	FILE *regs = NULL;
	char *line = NULL;
	size_t size = 0;
	ssize_t nread;

	regs = fopen(argv[1], "r");
	if (!regs) {
		printf("regs.txt file open error\n");
		exit(EXIT_FAILURE);
	}

	while ((nread = getline(&line, &size, regs)) != -1) {

		// printf("%s", line);

		int sscanf_output_n = 0;
		int register_number;
		uint64_t register_value;

		sscanf_output_n = sscanf(line, "X%d %*s 0x%" SCNx64,
					 &register_number, &register_value);

		if (sscanf_output_n == -1) {
			perror("sscanf error");
		}
		// printf("reg num : %d\n", register_number);
		// printf("reg value : %" PRIx64 "\n", register_value);

		// x0는 0고정됨
		if (register_number != 0)
			register_file[register_number] |= register_value;
	}
	print_register_file();

	FILE *insts;
	char *inst_line = NULL;
	size_t size2 = 0;
	ssize_t nread2;

	insts = fopen(argv[2], "r");
	if (!insts) {
		perror("inst file open error\n");
		exit(EXIT_FAILURE);
	}

	while ((nread2 = getline(&inst_line, &size2, insts)) != -1) {
		int ssanf_output;

		// sscanf로 instruction 이름만 추출
		// 명령어에 따라 opcode, funct3, funct7값 명시적으로 할당
		// 명령어에 따라 파싱할 rs1, rs2, rd, imm값 지정
		// 숫자로 변환된 최종 inst를 linked list에 저장
		// struct inst가 있어야함
		// linked list 위한 구조체 필요함(struct inst 넣고, 포인터
		// 넣고?)
	}

	// =========자원해제==========
	free(line);
	fclose(regs);

	free(inst_line);
	fclose(insts);
	// free() 파싱한 명령어 문자열
	// free() instructions linked list 로직

	return 0;
}
