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
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum alu_operations { ADD, SUB, SLL, SRL, OR, AND, XOR };

// 사용하지 않는 필드는 MUX 구현에서 선택해야함 MUX는 그냥 if문으로 ??
struct instructions {
	int funct7_or_imm; // 의도적으로 imm은 그냥 이 필드에서 처리함... 단순함을 위해
	int rs2;
	int rs1;
	int funct3;
	int rd;
	int opcode;
	char name[5];
	struct instructions *next;
};

// =====================================

struct pipeline_registers {
	bool mem_to_reg;
	bool reg_write;
	bool mem_write;
	bool mem_read;
	bool alu_op;
	bool alu_src;

	int funct7_or_imm; // 의도적으로 imm은 그냥 이 필드에서 처리함... 단순함을 위해
	int rs2;
	int rs1;
	int funct3;
	int rd;
	int opcode;
	char name[5];

	uint64_t data_1;
	uint64_t data_2;
	uint64_t extended_imm;

	uint64_t data_m;
	uint64_t alu_result;
};

struct pipeline_registers nop{

};

// ==================================================
// 인덱스가 곧 레지스터 번호임
static uint64_t register_file[32] = {(uint64_t)0};

// 어차피 한 번씩만 지나가니까 전역으로 선언함 (beq도 없고)
struct instructions *inst_head = NULL;
struct instructions *inst_tail = NULL;
struct instructions *pc = NULL;
struct instructions *pc_next = NULL;

struct pipeline_registers if_id_reg;
struct pipeline_registers id_ex_reg;
struct pipeline_registers ex_mem_reg;
struct pipeline_registers mem_wb_reg;

struct pipeline_registers if_id_reg_next;
struct pipeline_registers id_ex_reg_next;
struct pipeline_registers ex_mem_reg_next;
struct pipeline_registers mem_wb_reg_next;

// ====================================================

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
		strcpy(inst_node->name, inst_name);

		if (!inst_head) {
			inst_head = inst_node;
			inst_tail = inst_node;
			pc = inst_node;
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

static void signal_control(int opcode)
{

	if (opcode == 0b0000011) { // ld
		id_ex_reg_next.alu_op = 0b00;
		id_ex_reg_next.alu_src = 1;
		id_ex_reg_next.mem_read = 1;
		id_ex_reg_next.mem_write = 0;
		id_ex_reg_next.mem_to_reg = 1;
		id_ex_reg_next.reg_write = 1;

	} else if (opcode == 0b0100011) { // sd
		id_ex_reg_next.alu_op = 0b00;
		id_ex_reg_next.alu_src = 1;
		id_ex_reg_next.mem_read = 0;
		id_ex_reg_next.mem_write = 1;
		id_ex_reg_next.mem_to_reg = 0;
		id_ex_reg_next.reg_write = 0;
	} else if (opcode == 0b0110011) { // R-type
		id_ex_reg_next.alu_op = 0b10;
		id_ex_reg_next.alu_src = 0;
		id_ex_reg_next.mem_read = 0;
		id_ex_reg_next.mem_write = 0;
		id_ex_reg_next.mem_to_reg = 0;
		id_ex_reg_next.reg_write = 1;

	} else { // I-type
		// 11로 일단 가정
		id_ex_reg_next.alu_op = 0b11;
		id_ex_reg_next.alu_src = 1;
		id_ex_reg_next.mem_read = 0;
		id_ex_reg_next.mem_write = 0;
		id_ex_reg_next.mem_to_reg = 0;
		id_ex_reg_next.reg_write = 1;
	}
}

static enum alu_operations alu_control(int alu_op, int funct7_or_imm, int funct3)
{

	if (alu_op == 0b00) {
		return ADD;
	} else if (alu_op == 0b10) {
		if (funct7_or_imm > 0)
			return SUB;
	}

	switch (funct3) {
	case 0b000:
		return ADD;
	case 0b001:
		return SLL;
	case 0b101:
		return SRL;
	case 0b110:
		return OR;
	case 0b111:
		return AND;
	case 0b100:
		return XOR;
	default:
		return -1;
	}
}

static void alu_calculate(uint64_t input_1, uint64_t input_2, enum alu_operations alu_operation)
{
	switch (alu_operation) {
	case ADD:
		ex_mem_reg_next.alu_result = input_1 + input_2;
	case SUB:
		ex_mem_reg_next.alu_result = input_1 - input_2;
	case SLL:
		ex_mem_reg_next.alu_result = input_1 << input_2;
	case SRL:
		ex_mem_reg_next.alu_result = input_1 >> input_2;
	case OR:
		ex_mem_reg_next.alu_result = input_1 | input_2;
	case AND:
		ex_mem_reg_next.alu_result = input_1 & input_2;
	case XOR:
		ex_mem_reg_next.alu_result = input_1 ^ input_2;
	}
}

static char *write_back_stage()
{
	if (mem_wb_reg.rd == 0)
		return mem_wb_reg.name;

	if (mem_wb_reg.mem_to_reg == 1)
		register_file[mem_wb_reg.rd] = mem_wb_reg.data_m;
	else
		register_file[mem_wb_reg.rd] = mem_wb_reg.alu_result;

	return mem_wb_reg.name;
}

static char *memory_stage()
{
	strcpy(mem_wb_reg_next.name, ex_mem_reg.name);
	mem_wb_reg_next.rd = ex_mem_reg.rd;

	if (ex_mem_reg.mem_write == 1)
		; // 메모리에쓴다()
	else if (ex_mem_reg.mem_read == 1)
		; // 메모리에서읽어서빼낸다()
	else
		mem_wb_reg_next.alu_result = ex_mem_reg.alu_result;

	return ex_mem_reg.name;
}

static char *excute_stage()
{
	strcpy(ex_mem_reg_next.name, id_ex_reg.name);
	uint64_t input_1 = id_ex_reg.data_1;
	uint64_t input_2;
	enum alu_operations alu_operation;

	if (id_ex_reg.alu_src == 1)
		input_2 = id_ex_reg.extended_imm;
	else
		input_2 = id_ex_reg.data_2;

	alu_operation = alu_control(id_ex_reg.alu_op, id_ex_reg.funct7_or_imm, id_ex_reg.funct3);

	alu_calculate(input_1, input_2, alu_operation);

	return id_ex_reg.name;
}

static char *instruction_decode_stage()
{
	signal_control(if_id_reg.opcode);

	// throw
	id_ex_reg_next = if_id_reg;

	// sign-extension
	id_ex_reg_next.extended_imm = (uint64_t)if_id_reg.funct7_or_imm;

	id_ex_reg_next.data_1 = register_file[if_id_reg.rs1];
	id_ex_reg_next.data_2 = register_file[if_id_reg.rs2];

	return if_id_reg.name;
}

static char *instruction_fetch_stage()
{

	if_id_reg_next.funct7_or_imm = pc->funct7_or_imm;
	if_id_reg_next.rs2 = pc->rs2;
	if_id_reg_next.rs1 = pc->rs1;
	if_id_reg_next.funct3 = pc->funct3;
	if_id_reg_next.rd = pc->rd;
	if_id_reg_next.opcode = pc->opcode;

	strcpy(if_id_reg_next.name, pc->name);

	pc_next = pc->next;

	return pc->name;
}

static void clock_first(int cycle)
{
	// hazard detect

	char *wb_name = NULL;
	char *mem_name = NULL;
	char *ex_name = NULL;
	char *id_name = NULL;
	char *if_name = NULL;

	wb_name = write_back_stage();
	mem_name = memory_stage();
	ex_name = excute_stage();
	id_name = instruction_decode_stage();
	if_name = instruction_fetch_stage();

	printf("%-8d%-8s%-8s%-8s%-8s%s\n", cycle, if_name, id_name, ex_name, mem_name, wb_name);
}

static void clock_next()
{
	if_id_reg = if_id_reg_next;
	id_ex_reg = id_ex_reg_next;
	ex_mem_reg = ex_mem_reg_next;
	mem_wb_reg = mem_wb_reg_next;
	pc = pc_next;
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
	printf("%-8s%-8s%-8s%-8s%-8s%s\n", "Cycle", "IF", "ID", "EX", "MEM", "WB");
	int cycle = 1;

	while (1) {

		clock_first(cycle);
		clock_next();

		cycle++;
		sleep(1);
	}

	// =========자원해제==========
	free_inst_linked_list();

	return 0;
}
