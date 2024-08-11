// Dear Xilinx. I love you
#include <iostream>
#include <stdint.h>

constexpr auto SUCCESS				= 0;
constexpr auto FAILURE				= 1;

constexpr auto SZ_STR				= 64;
constexpr auto SZ_BLOCK				= 16;

constexpr auto MAX_GEN				= 10.f;

#define ift_getexponent(A)			uint16_t(((A) >> 23) & 0xFF)													// checked
#define ift_getmantissa(A)			((A) & 0x007FFFFF)																// checked
#define ift_getsign(A)				(((A) >> 31) & 0x01)															// checked

#define ift_restoreleadingone(A)	(A | 0x800000)

#define ift_packfloat(S,E,M)		packFloat32(S,E,M);//(( ( (uint32_t) S )<<31 ) & 0x80000000 | ( ( (uint32_t) E )<<23 ) & 0x7F800000 | M & 0x007FFFFF)

#define SOFTFP_DISPLAY(A,B)			int2str_b(__float_as_uint(A), B); printf("Float %s = %e, %.8x, %s\n", #A, A, __float_as_uint(A), B);		// checked

typedef enum OpTypes {
	op_Add,
	op_Mul,
	op_Mac,
} op_t;
typedef enum DisplayType {
	dt_none = 0x0,
	dt_elements = 0x01,
	dt_results = 0x02,
	dt_full = dt_elements | dt_results,
} display_t;

uint32_t __float_as_uint(float x);
void LaunchBFPPrimeCPUKernel(const float* input, float* output, const int n, const int bit_width, const int block_size, const int sub_block_size,
	const int sub_block_shift_bits, const int rounding_mode);

static int int2str_b(int32_t In, char Str[SZ_STR]) {
	int i2 = 0;
	for (int i = (sizeof(In) * 8) - 1; i >= 0; i--, i2++) {
		Str[i2] = (In & (1 << i)) ? '1' : '0';
		if (i == 31 || i == 23) {
			Str[++i2] = ' ';
		}
	}
	Str[i2] = '\0';

	return SUCCESS;
}
static int Run_BlockOp(float BlockA[SZ_BLOCK], float BlockB[SZ_BLOCK], op_t Op, display_t Display) {
	float gen[SZ_BLOCK]{}, truth[SZ_BLOCK]{}, accGen{}, accTruth{};
	char str[SZ_STR]{};
	float tst;
	/*uint16_t zExp, zSign;
	uint32_t aM, bM, zM;
	uint64_t zM64;*/

	if (Display != dt_none) {
		switch (Op) {
			case op_Add: printf("\nOp ADD:\n"); break;
			case op_Mul: printf("\nOp MUL:\n"); break;
			case op_Mac: printf("\nOp MAC:\n"); break;
			default: return FAILURE;
		}
	}

	// Generate truth values.
	for (int i = 0; i < SZ_BLOCK; i++) {
		switch (Op) {
			case op_Add: truth[i] = BlockA[i] + BlockB[i]; break;
			case op_Mul: truth[i] = BlockA[i] * BlockB[i]; break;
			case op_Mac: accTruth += (BlockA[i] * BlockB[i]); break;
		}
	}

	float cvrtIn[SZ_BLOCK*2]{}, cvrtOut[SZ_BLOCK*2]{};
	for (int i = 0; i < SZ_BLOCK; i++) {
		cvrtIn[i] = BlockA[i];
		cvrtIn[i+SZ_BLOCK] = BlockB[i];
	}
	LaunchBFPPrimeCPUKernel(cvrtIn, cvrtOut, SZ_BLOCK * 2, 32, SZ_BLOCK * 2, SZ_BLOCK * 2, 23, 1);

	// Generate block fp values.
	for (int i = 0; i < SZ_BLOCK; i++) {
		switch (Op) {
			case op_Add: gen[i] = cvrtOut[i] + cvrtOut[i + SZ_BLOCK]; break;
			case op_Mul: 
				gen[i] = cvrtOut[i] * cvrtOut[i+SZ_BLOCK];
				/*zSign = ift_getsign(BlockA[i].i) ^ ift_getsign(BlockB[i].i);
				zExp = (ift_getexponent(BlockA[i].i) + ift_getexponent(BlockB[i].i)) - uint16_t(127);
				aM = ift_getmantissa(BlockA[i].i); bM = ift_getmantissa(BlockB[i].i);
				aM = ift_restoreleadingone(aM) << 7; bM = ift_restoreleadingone(bM) << 8;
				shift64RightJamming(uint64_t(aM) * bM, 32, &zM64); zM = zM64;
				if (0 <= (int32_t)(zM << 1)) {
					zM <<= 1;
					--zExp;
				}
				zM >>= 7;
				ret.f = truth[i];
				IFT_DISPLAY(ret, str);
				ret.f = 0.f;
				tst = ift_packfloat(zSign, zExp, zM);
				SOFTFP_DISPLAY(tst, str);
				printf("MUL test: %e * %e = %e (should be %e)\n", BlockA[i].f, BlockB[i].f, ret.f, truth[i]);*/
				
				break;
			case op_Mac: accGen += (cvrtOut[i] * cvrtOut[i+SZ_BLOCK]); break;
		}
	}

	float maxdif = -500.f;
	if (Op == op_Mac) {
		if (Display & dt_elements) {
			SOFTFP_DISPLAY(accTruth, str);
			SOFTFP_DISPLAY(accGen, str);
		}
		maxdif = fabsf(accTruth - accGen);
	} else {
		for (int i = 0; i < SZ_BLOCK; i++) {
			float diff = fabsf(truth[i] - gen[i]);
			if (maxdif < diff) { maxdif = diff; }
			if (Display & dt_elements) {
				SOFTFP_DISPLAY(truth[i], str);
				SOFTFP_DISPLAY(gen[i], str);
				printf("\n");
			}
		}
	}

	if (Display & dt_results) {
		printf("Max absolute difference for op is %e\n", maxdif);
	}

	return SUCCESS;
}

int main() {
	char str[SZ_STR]{};
	float bA[SZ_BLOCK]{}, bB[SZ_BLOCK]{}, bC[SZ_BLOCK]{};

	std::cout << "Block Floating Point Implementation Test!\n";
	
	// TODO: Best way to do this is with repeated tests and built up statistics over many, many blocks
	for (int i = 0; i < SZ_BLOCK; i++) {
		bA[i] = (rand() / float(RAND_MAX)) * MAX_GEN;
		bB[i] = (rand() / float(RAND_MAX)) * MAX_GEN;
	}

	Run_BlockOp(bA, bB, op_Add, dt_results);
	Run_BlockOp(bA, bB, op_Mul, dt_results);
	Run_BlockOp(bA, bB, op_Mac, dt_full);
}
