// Dear Xilinx. I love you
#include <iostream>
#include <stdint.h>

constexpr auto SUCCESS				= 0;
constexpr auto FAILURE				= 1;

constexpr auto SZ_STR				= 64;
constexpr auto SZ_BLOCK				= 16384;

constexpr auto MAX_GEN				= 10.f;

#define ift_getexponent(A)			uint16_t(((A) >> 23) & 0xFF)													// checked
#define ift_getmantissa(A)			((A) & 0x007FFFFF)																// checked
#define ift_getsign(A)				(((A) >> 31) & 0x01)															// checked

#define ift_restoreleadingone(A)	(A | 0x800000)

#define ift_packfloat(S,E,M)		(((( ( (uint32_t) S )<<31 ) & 0x80000000) | (( ( (uint32_t) E )<<23 ) & 0x7F800000) | (M & 0x007FFFFF)))

#define SOFTFP_DISPLAY(A,B)			int2str_b(__float_as_uint(A), B); printf("Float %s = %e, %.8x, %s\n", #A, A, __float_as_uint(A), B);		// checked
#define SOFTAT_DISPLAY(A,B)			printf("Aligned %e to %.8x for %s\n", B, A, #A);

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

uint32_t GetMaxExponentCPU(const float* input, int n);
float __uint_as_float(uint32_t x);
uint32_t __float_as_uint(float x);

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
static int countLeadingZeros(int64_t In, int Start) {
	int len = sizeof(In) * 8 - 1;
	if (Start - 1 > len) { Start = len; }
	for (int i = Start, i2 = 0; i >= 0; i--) {
		if (In & uint64_t(1) << i) { return i2 - 1; }
		i2++;
	}
	return Start;
}
static void TryAlign(const float* Array, int N, int32_t *ArrayOut, uint32_t *SharedExp) {
	uint32_t max = GetMaxExponentCPU(Array, N);
	for (int i = 0; i < N; i++) {
		uint8_t sign = ift_getsign(__float_as_uint(Array[i]));
		uint32_t exp = ift_getexponent(__float_as_uint(Array[i]));
		uint32_t m = ift_getmantissa(__float_as_uint(Array[i]));
		int posShift = max - exp;
		m = ift_restoreleadingone(m);
		m >>= posShift;
		if (Array[i] == 0) { ArrayOut[i] = 0; } else { ArrayOut[i] = ((sign << 31) & 0x80000000) | m; }
	}
	*SharedExp = max;
}

static float Do_MulInt(int32_t A, int32_t B, uint32_t Exp) {
	char str[SZ_STR]{};
	uint8_t aSign = ift_getsign(A), bSign = ift_getsign(B), zSign = aSign ^ bSign;
	uint32_t aM = (A & 0x7FFFFFFF), bM = (B & 0x7FFFFFFF);
	uint32_t zExp = Exp + 2, zM0, zM1;
	uint64_t zM;
	uint32_t roundBits;

	if ((aM == 0 || bM == 0)) { return 0.f; }

	bM <<= 1;
	zM = uint64_t(aM) * uint64_t(bM);
	int shift = countLeadingZeros(zM, 48);
	zM0 = zM >> 18 - shift;
	zM1 = (zM << shift) & 0xFFFFFFFF;
	zM0 |= (zM1 != 0);
	zExp -= shift;
	if (0 > (int32_t)(zM0 << 1)) {
		zM0 >>= 1;
		zExp++;
	}

	roundBits = zM0 & 0x7F;
	zM0 = (zM0 + roundBits) >> 6;
	zM0 &= ~(((roundBits ^ 0x40) == 0) & 0);
	if (zM0 == 0) { zExp = 0; }
	return __uint_as_float(ift_packfloat(zSign, zExp, zM0));
}

static int Run_BlockOp(float BlockA[SZ_BLOCK], float BlockB[SZ_BLOCK], op_t Op, display_t Display) {
	float gen[SZ_BLOCK]{}, truth[SZ_BLOCK]{}, accGen{}, accTruth{};
	char str[SZ_STR]{};

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
	int32_t cvrtInt[SZ_BLOCK * 2]{};
	uint32_t sharedExp = 0;
	for (int i = 0; i < SZ_BLOCK; i++) {
		cvrtIn[i] = BlockA[i];
		cvrtIn[i+SZ_BLOCK] = BlockB[i];
	}
	//LaunchBFPCPUKernel(cvrtIn, cvrtOut, SZ_BLOCK * 2, 32, 1);
	//BFPPrimeCPUKernel(cvrtIn, cvrtOut, cvrtInt, &sharedExp, SZ_BLOCK * 2, 0, 1, 32, SZ_BLOCK * 2, SZ_BLOCK * 2, 24, 1);
	TryAlign(cvrtIn, SZ_BLOCK * 2, cvrtInt, &sharedExp);
	
	int count = 0;
	for (int i = 0; i < SZ_BLOCK; i++) {
		if (__float_as_uint(cvrtOut[i]) == __float_as_uint(BlockA[i])) { count++; } if (__float_as_uint(cvrtOut[i+SZ_BLOCK]) == __float_as_uint(BlockB[i])) { count++; }
	}

	// Generate block fp values.
	for (int i = 0; i < SZ_BLOCK; i++) {
		switch (Op) {
			case op_Add: gen[i] = cvrtOut[i] + cvrtOut[i + SZ_BLOCK]; break;
			case op_Mul: gen[i] = //Do_Mul(BlockA[i], BlockB[i]);
						 gen[i] = Do_MulInt(cvrtInt[i], cvrtInt[i+SZ_BLOCK], sharedExp);
				//gen[i] = cvrtOut[i] * cvrtOut[i+SZ_BLOCK];	
				//printf("MUL test: %e * %e = %e (should be %e)\n", BlockA[i].f, BlockB[i].f, ret.f, truth[i]);
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
	float bA[SZ_BLOCK]{}, bB[SZ_BLOCK]{};

	std::cout << "Block Floating Point Implementation Test!\n";
	srand((unsigned)time(0));

	// TODO: Best way to do this is with repeated tests and built up statistics over many, many blocks
	for (int i = 0; i < SZ_BLOCK; i++) {
		bA[i] = (rand() / float(RAND_MAX)) * MAX_GEN - (MAX_GEN / 2);
		bB[i] = (rand() / float(RAND_MAX)) * MAX_GEN - (MAX_GEN / 2);
	}

	//Run_BlockOp(bA, bB, op_Add, dt_results);
	Run_BlockOp(bA, bB, op_Mul, dt_results);
	//Run_BlockOp(bA, bB, op_Mac, dt_full);
}
