#include <iostream>

constexpr auto SUCCESS			= 0;
constexpr auto FAILURE			= 1;

constexpr auto SZ_STR			= 64;
constexpr auto SZ_BLOCK			= 16;

constexpr auto MAX_GEN			= 10.f;

#define DISPLAY(A,B)			int2str_b(A.i, B); printf("Float %s = %e, %.8x, %s\n", #A, A.f, A.i, str);

typedef union IFUnion {
	int32_t i;
	float f;
} int_float_t;
typedef enum OpTypes {
	op_Add,
	op_Mul,
	op_Mac,
} op_t;

static int int2str_b(int32_t In, char Str[SZ_STR]) {
	int i2 = 0;
	for (int i = (sizeof(In) * 8) - 1; i >= 0; i--, i2++) {
		Str[i2] = (In & (1 << i)) ? '1' : '0';
		if (i2 && (i % 8) == 0) {
			Str[++i2] = ' ';
		}
	}
	Str[i2] = '\0';

	return SUCCESS;
}
static int Run_BlockOp(int_float_t BlockA[SZ_BLOCK], int_float_t BlockB[SZ_BLOCK], op_t Op) {
	float gen[SZ_BLOCK]{}, truth[SZ_BLOCK]{}, accGen{}, accTruth{};

	// Generate truth values.
	for (int i = 0; i < SZ_BLOCK; i++) {
		switch (Op) {
			case op_Add: truth[i] = BlockA[i].f + BlockB[i].f; break;
			case op_Mul: truth[i] = BlockA[i].f * BlockB[i].f; break;
			case op_Mac: accTruth += (BlockA[i].f * BlockB[i].f); break;
			default: return FAILURE;
		}
	}

	// Generate block fp values.
	for (int i = 0; i < SZ_BLOCK; i++) {
		
		switch (Op) {
			case op_Add: break;
			case op_Mul: break;
			case op_Mac: break;
			default: return FAILURE;
		}

	}

	return SUCCESS;
}

int main() {
	int_float_t test{};
	char str[SZ_STR]{};
	int_float_t bA[SZ_BLOCK]{}, bB[SZ_BLOCK]{}, bC[SZ_BLOCK]{};

	std::cout << "Block Floating Point Implementation Test!\n";
	
	// TODO: Best way to do this is with repeated tests and built up statistics over many, many blocks
	for (int i = 0; i < SZ_BLOCK; i++) {
		bA[i].f = rand() / (MAX_GEN);
		bB[i].f = rand() / (MAX_GEN);
	}

	Run_BlockOp(bA, bB, op_Add);
	Run_BlockOp(bA, bB, op_Mul);
	Run_BlockOp(bA, bB, op_Mac);

	//DISPLAY(bA[i], str);
}
