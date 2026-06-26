/*
 * title : riscvsim.c
 * author : Kimseonghyeon
 * description : simulate risc-v instructions
 */

// TODO: 과거버전에서 첫 번째 문장을 실행하면 어떻게 되나 ??
// getline() -glibc버전마다 선언 매크로가 다름(glibc 2.10 기준)-
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include <inttypes.h> // PRIx64, SCNx64
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 인덱스가 곧 레지스터 번호임
static uint64_t register_file[32] = {(uint64_t)0};

// 사용하지 않는 필드는 MUX 구현에서 선택해야함
struct instructions {
	int funct7_or_imm;
	int rs2;
	int rs1;
	int funct3;
	int rd;
	int opcode;
	struct instructions *next;
};

// 어차피 한 번씩만 지나가니까 전역으로 선언함 (beq도 없고)
struct instructions *inst_head = NULL;
struct instructions *inst_tail = NULL;

static void print_horizon()
{
	printf("==========================================\n");
}

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

static void read_regs_txt(char *file_name)
{
	FILE *regs_file = NULL;
	char *line = NULL;
	size_t size = 0;
	ssize_t nread;
	int n;
	int register_number;
	uint64_t register_value;

	regs_file = fopen(file_name, "r");
	if (!regs_file) {
		printf("regs.txt file open error\n");
		exit(EXIT_FAILURE);
	}

	while ((nread = getline(&line, &size, regs_file)) != -1) {
		// printf("%s", line);
		n = sscanf(line, "X%d %*s 0x%" SCNx64, &register_number, &register_value);
		if (n == -1) {
			perror("sscanf error");
			exit(EXIT_FAILURE);
		}
		// printf("reg num : %d\n", register_number);
		// printf("reg value : %" PRIx64 "\n", register_value);

		// x0는 0고정됨
		if (register_number != 0)
			// 초기 값이 모두 0일 때 성립
			// 그냥 대입해도 되겠는데? ,,,
			register_file[register_number] |= register_value;
	}

	free(line);
	fclose(regs_file);
}

static void read_inst_txt(char *file_name)
{
	FILE *insts_file = NULL;
	char *inst_line = NULL;
	size_t size = 0;
	ssize_t nread;
	int n;
	char inst_name[5];

	insts_file = fopen(file_name, "r");
	if (!insts_file) {
		perror("inst file open error\n");
		exit(EXIT_FAILURE);
	}

	while ((nread = getline(&inst_line, &size, insts_file)) != -1) {

		n = sscanf(inst_line, "%s", inst_name);
		if (n == -1) {
			perror("sscanf error");
			exit(EXIT_FAILURE);
		}
		// printf("%s\n", inst_name);

		struct instructions *inst_node =
		    (struct instructions *)malloc(sizeof(struct instructions));

		inst_node->next = NULL;

		if (!inst_head) {
			inst_head = inst_node;
			inst_tail = inst_node;
		} else {
			inst_tail->next = inst_node;
			inst_tail = inst_node;
		}

		if (strcmp(inst_name, "ld") == 0) {
			inst_node->funct3 = 0b011;
			inst_node->opcode = 0b0000011;

			sscanf(inst_line, "%*s x%d %*s %d (x%d %*s", &inst_node->rd,
			       &inst_node->funct7_or_imm, &inst_node->rs1);

			continue;
		}
		if (strcmp(inst_name, "sd") == 0) {
			inst_node->funct3 = 0b011;
			inst_node->opcode = 0b0100011;

			sscanf(inst_line, "%*s x%d %*s %d (x%d %*s", &inst_node->rs2,
			       &inst_node->funct7_or_imm, &inst_node->rs1);

			continue;
		}

		if ('i' == inst_name[strlen(inst_name) - 1]) {
			inst_node->opcode = 0b0010011; // 나머지 I-type

			sscanf(inst_line, "%*s x%d %*s x%d %*s %d", &inst_node->rd, &inst_node->rs1,
			       &inst_node->funct7_or_imm);
		} else {
			inst_node->opcode = 0b0110011; // R-type

			sscanf(inst_line, "%*s x%d %*s x%d %*s x%d", &inst_node->rd,
			       &inst_node->rs1, &inst_node->rs2);

			if (strcmp(inst_name, "sub") == 0)
				inst_node->funct7_or_imm = 0b0100000; // funct7
			else
				inst_node->funct7_or_imm = 0b0000000; // funct7
		}

		if ((strstr(inst_name, "add") != NULL) || (strstr(inst_name, "sub") != NULL)) {
			inst_node->funct3 = 0b000;
			continue;
		}
		if (strstr(inst_name, "sll") != NULL) {
			inst_node->funct3 = 0b001;
			continue;
		}
		if (strstr(inst_name, "srl") != NULL) {
			inst_node->funct3 = 0b101;
			continue;
		}
		if (strstr(inst_name, "xor") != NULL) {
			// or보다 먼저 나와야함!!
			inst_node->funct3 = 0b100;
			continue;
		}
		if (strstr(inst_name, "and") != NULL) {
			inst_node->funct3 = 0b111;
			continue;
		}
		if (strstr(inst_name, "or") != NULL) {
			inst_node->funct3 = 0b110;
			continue;
		}
	}

	free(inst_line);
	fclose(insts_file);
}

static void print_inst_linked_list()
{
	struct instructions *ptr = inst_head;

	printf("f7|imm  rs2   rs1   f3   rd    opcode\n");
	while (ptr != NULL) {
		printf("%07b ", ptr->funct7_or_imm);
		printf("%05b ", ptr->rs2);
		printf("%05b ", ptr->rs1);
		printf("%03b ", ptr->funct3);
		printf("%05b ", ptr->rd);
		printf("%07b ", ptr->opcode);
		printf("\n");
		ptr = ptr->next;
	}
}

static void free_inst_linked_list()
{
	struct instructions *ptr = inst_head;
	while (ptr != NULL) {
		struct instructions *temp = ptr->next;
		free(ptr);
		ptr = temp;
	}
	inst_head = NULL;
	inst_tail = NULL;
}

int main(int argc, char *argv[])
{
	// ======================
	if (argc < 3) {
		printf("usage ... 2 files\n");
		exit(EXIT_FAILURE);
	}

	// =======================
	// print_register_file();
	read_regs_txt(argv[1]);
	// print_register_file();

	// =======================
	read_inst_txt(argv[2]);
	// print_inst_linked_list();

	// ===========main loop========
	while (1) {
		// up
		// IF -> ID -> EX -> MEM -> WB
		// pipeline register write
		// WB write
		//

		// down
		// IF -> ID -> EX -> MEM -> WB
		// pipeline register read
		// ID read

		// 언제 종료함 ???
		break;
	}

	// =========자원해제==========
	free_inst_linked_list();

	return 0;
}
