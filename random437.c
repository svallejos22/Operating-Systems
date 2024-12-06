#include "random437.h"
#include <stdlib.h>
#include <math.h>

int poissonRandom(int meanArrival) {
	double l = exp(-meanArrival);
	int k = 0;
	double p = 1.0;

	do {
		k++;
		p *= (double)rand() /RAND_MAX;
	} while (p > l);

	return k  - 1;
}
