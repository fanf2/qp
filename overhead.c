// calculate acbt memory overhead

#include <stdio.h>
#include <stdlib.h>

typedef unsigned char byte;

static byte data[256];

static int cmp(const void *va, const void *vb) {
	const byte *a = va, *b = vb;
	int ai = *a, bi = *b;
	return ai - bi;
}

static void shuffle(unsigned n) {
	for(unsigned i = 0; i < 256; i++)
		data[i] = i;
	for(unsigned i = 0; i < n; i++) {
		unsigned r = arc4random_uniform(255 - i);
		byte s = data[i + r];
		data[i + r] = data[i];
		data[i] = s;
	}
	qsort(data, n, 1, cmp);
}

// simples
static double cost_1(unsigned n) {
	if(n < 2)
		return 0;
	else
		return 3 * (n - 1);
}

static unsigned cost_2(unsigned n) {
  unsigned c = 0;
  unsigned i = 0;
  unsigned n01 = 0;
  for(unsigned b01 = 0x00; b01 <= 0xC0; b01 += 0x40) {
    unsigned n23 = 0;
    for(unsigned b23 = 0x00; b23 <= 0x30; b23 += 0x10) {
      unsigned n45 = 0;
      for(unsigned b45 = 0x00; b45 <= 0x0C; b45 += 0x04) {
	unsigned i67 = i;
	while(i < n && (data[i] & 0xFC) == (b01|b23|b45))
	  i++;
	unsigned n67 = i - i67;
	if(n67 >= 3) c += 2;
	if(n67 >= 2) c += 3;
	if(n67 >= 1) n45 += 1;
      }
      if(n45 >= 3) c += 2;
      if(n45 >= 2) c += 3;
      if(n45 >= 1) n23 += 1;
    }
    if(n23 >= 3) c += 2;
    if(n23 >= 2) c += 3;
    if(n23 >= 1) n01 += 1;
  }
  if(n01 >= 3) c += 2;
  if(n01 >= 2) c += 3;
  return c;
}

static unsigned cost_4(unsigned n) {
  unsigned c = 0;
  unsigned i = 0;
  unsigned n01 = 0;
  unsigned c23 = 0;
  for(unsigned b01 = 0x00; b01 <= 0xC0; b01 += 0x40) {
    unsigned n23 = 0;
    for(unsigned b23 = 0x00; b23 <= 0x30; b23 += 0x10) {
      unsigned n45 = 0;
      unsigned c67 = 0;
      for(unsigned b45 = 0x00; b45 <= 0x0C; b45 += 0x04) {
	unsigned i67 = i;
	while(i < n && (data[i] & 0xFC) == (b01|b23|b45))
	  i++;
	unsigned n67 = i - i67;
	if(n67 >= 3) c67 += 2;
	if(n67 >= 2) c67 += 3;
	if(n67 >= 1) n45 += 1;
      }
      if(n45 >= 3) {
	if(c67 > 13) c += 18;
	else c += c67 + 5;
      }
      if(n45 == 2) c += c67 + 3;
      if(n45 >= 1) n23 += 1;
    }
    if(n23 >= 3) c23 += 2;
    if(n23 >= 2) c23 += 3;
    if(n23 >= 1) n01 += 1;
  }
  if(n01 >= 3) {
    if(c23 > 13) c += 18;
    else c += c23 + 5;
  }
  if(n01 == 2) c += 3;
  return c;
}

static unsigned cost_a_32(unsigned n) {
	for(unsigned s = 8; s <= 1024; s *= 2) {
		unsigned m = (s - 7) / 5;
		if(n <= m) return s;
	}
	return 1024+7;
}

static unsigned cost_a_64(unsigned n) {
	for(unsigned s = 16; s <= 2048; s *= 2) {
		unsigned m = (s - 11) / 9;
		if(n <= m) return s;
	}
	return 2048+11;
}

static double mean(unsigned n, unsigned cost(unsigned n)) {
	double m = 0;
	for(unsigned i = 1; i <= 100000; i++) {
		shuffle(n);
		double c = cost(n);
		m += (c - m) / i;
	}
	return m;
}

int main(void) {
	setlinebuf(stdout);
	for(unsigned n = 1; n <= 256; n++) {
		double c1 = cost_1(n);
		double c2 = mean(n, cost_2);
		double c4 = mean(n, cost_4);
		double c1s = c1 * 4;
		double c1d = c1 * 8;
		double c2s = c2 * 4;
		double c2d = c2 * 8;
		double c4s = c4 * 4;
		double c4d = c4 * 8;
		double cas = cost_a_32(n);
		double cad = cost_a_64(n);
		printf("%d %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f\n", n,
		       c1s, c2s, c4s, cas, c1s/n, c2s/n, c4s/n, cas/n,
		       c1d, c2d, c4d, cad, c1d/n, c2d/n, c4s/n, cad/n);
	}
	return(0);
}
