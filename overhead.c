// calculate acbt memory overhead

#include <stdio.h>

static double cost1(double n) {
	return 3 * 0xFF * n / 256;
}

// just take the mean, I guess?
static double cost2(double n) {
	double raw_2 = 5 * 0x55 * n / 256;
	return (cost1(n) * (256 - n) +  raw_2 * n) / 256;
}

static double cost4(double n) {
	double raw_4 = 18 * 0x11 * n / 256;
	return (cost2(n) * (256 - n) +  raw_4 * n) / 256;
}

static double cost32(double n) {
	for(double s = 8; s < 1024; s *= 2) {
		double m = (s - 7) / 5;
		if(n <= m) return s;
	}
	return 1024+7;
}

static double cost64(double n) {
	for(double s = 16; s < 2048; s *= 2) {
		double m = (s - 11) / 9;
		if(n <= m) return s;
	}
	return 2048+11;
}

int main(void) {
	for(int n = 1; n <= 256; n++) {
		double c1 = cost1(n)/n;
		double c2 = cost2(n)/n;
		double c4 = cost4(n)/n;
		printf("%d %f %f %f %f %f %f %f %f\n", n,
		    c1*4, c2*4, c4*4, cost32(n)/n,
		    c1*8, c2*8, c4*8, cost64(n)/n);
	}
	return(0);
}
