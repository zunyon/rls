#include <stdio.h>

// ./a.out |less すると、表示できる色が変わる、/bin/ls 	- color |less と比べる
int
main(int argc, char **argv)
{
	printf(" 8 color:\n");
	printf(" standard colors/high-intensity colors\n");

	for (int i=30; i<38; i++) {
		printf("\033[%dm", i);
		printf("%5d", i);
		printf("\033[0m");

		printf("\033[%dm", i + 10);
		printf("%5d", i + 10);
		printf("\033[0m");
	}
	printf("\n");

	for (int i=90; i<98; i++) {
		printf("\033[%dm", i);
		printf("%5d", i);
		printf("\033[0m");

		printf("\033[%dm", i + 10);
		printf("%5d", i + 10);
		printf("\033[0m");
	}
	printf("\n\n");

	// --------------------------------------------------------------------------------
	int j;
	printf(" 256 color:\n");
	j=1;
	for (int i=0; i<16; i++) {
		printf("\033[38;5;%dm", i);
		printf("%5d", 3000 + i);
		printf("\033[0m");

		printf("\033[48;5;%dm", i);
		printf("%5d", 4000 + i);
		printf("\033[0m");

		if (j++ % 8 == 0) {
			printf("\n");
		}
	}
	printf("\n");

	// --------------------------------------------------------------------------------
	// ここの fg/bg は上と混ぜられない
	j=1;
	for (int i=16; i<232; i++) {
		printf("\033[38;5;%dm", i);
		printf("%5d", 3000 + i);
		printf("\033[0m");

		printf("\033[48;5;%dm", i);
		printf("%5d", 4000 + i);
		printf("\033[0m");

		if (j++ % 6 == 0) {
			printf("\n");
		}
	}
	printf("\n");

	// --------------------------------------------------------------------------------
	// fg/bg では、bg が優先される
	printf(" grayscale colors:\n");
	j=1;
	for (int i=232; i<256; i++) {
		printf("\033[38;5;%dm", i);
		printf("%5d", 3000 + i);
		printf("\033[0m");

		printf("\033[48;5;%dm", i);
		printf("%5d", 4000 + i);
		printf("\033[0m");

		if (j++ % 6 == 0) {
			printf("\n");
		}
	}

	return 0;
}
