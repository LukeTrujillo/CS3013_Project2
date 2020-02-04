#include <stdio.h>

int dec2bin(int);

int main(int argc, char** argv) {
	printf("%i\n", dec2bin(27))	;
}

int dec2bin(int i){
	int place = 1;
	int ret = 0;
	while (i > 0){
		ret += (i&1)*place;
		i = i >> 1;
		place = place*10;
	}
	return ret;
}
