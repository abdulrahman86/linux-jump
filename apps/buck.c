/**
 * This example implements the Bucket Sort (BK) on top of the Jump DSM system.
 */

#include <stdio.h>
#include <stdlib.h>
#include "jia.h"
#include "time.h"

#define BUCKETS       256         /* number of buckets for each proc */
#define SEED            1

unsigned int *a, *b, *flag;

unsigned int do_pow(int x)
{
	int i = 0;
	unsigned int j = 1;
	for (i = 0; i < x; i++)
		j *= 2;
	return j;
}

unsigned int do_log2(int x)
{
	unsigned int i = 0;
	int j = 1;
	while (x >= j * 2) {
		i++;
		j = j * 2;
	}
	return i;
}

int main(int argc, char ** argv)
{
	unsigned int count[BUCKETS*16];
	int j, k, x, y, counter, z;
	unsigned int i;
	int temp1, temp2, temp3, temp, start, locked;
	struct timeval time1, time2, time3, time4, time5, time6;
	unsigned int KEY, RANGE, MAGIC, BSIZE, PAGES;

	if (argc >= 2) {
		if (argv[1][0] == 'a')
			KEY = 262144;
		else if (argv[1][0] == 'b')
			KEY = 524288;
		else if (argv[1][0] == 'c')
			KEY = 1048576;
		else if (argv[1][0] == 'd')
			KEY = 2097152;
		else if (argv[1][0] == 'e')
			KEY = 4194304;
		else if (argv[1][0] == 'f')
			KEY = 8388608;
		else
			KEY = 4194304;
	} else {
		KEY = 4194304;
	}

	jia_init(argc, argv);

	jia_barrier();
	gettime(&time1);

	BSIZE = KEY / jiahosts / BUCKETS;
	PAGES = BSIZE / 1024 + 1;
	BSIZE = PAGES * 1024;

	a = (unsigned int *) jia_alloc(BUCKETS * BSIZE *
			jiahosts * sizeof(unsigned int));
	b = (unsigned int *) jia_alloc(KEY * sizeof(unsigned int));
	flag = (unsigned int *) jia_alloc(4096);

	jia_barrier();
	printf("Stage 1: Memory allocation done!\n");
	gettime(&time2);

	srand(jiapid+SEED);

	MAGIC = (KEY / jiahosts) * jiapid;

	if (jiapid == 0) {
		flag[0] = 0; 
		flag[1] = 0;
	}

	for (i = 0; i < KEY / jiahosts; i++) {
		temp1 = rand() % 1024;
		temp2 = rand() % 2048;
		temp3 = rand() % 2048;
		j = MAGIC + i;
		b[j] = (temp1 << 22) + (temp2 << 11) + temp3;
		if(b[j] == 0) b[j] = 1;
	}

	jia_barrier();
	gettime(&time3);
	printf("Stage 2: %d Integers initialized!\n", KEY / jiahosts);

	for (i = 0; i < BUCKETS * 16; i++)
		count[i] = 0;

	RANGE = do_pow(32 - do_log2(BUCKETS));
	printf("range = %d, jiahosts = %d\n", RANGE, jiahosts);

	for (i = 0; i < KEY / jiahosts; i++) {
		j = MAGIC + i;
		k = (b[j] / RANGE) * jiahosts + jiapid;
		a[k * BSIZE + count[k]] = b[j];

		count[k]++;
	}

	jia_barrier();
	gettime(&time4);
	printf("Stage 3: Distribution done! (%u)\n", KEY);

	RANGE = RANGE / jiahosts;

	for (i = 0; i < BUCKETS * 16; i++)
		count[i] = 0;

	for (i = 0; i < ((unsigned int) (BUCKETS/jiahosts)); i++) {
		counter = KEY / jiahosts * jiapid;
		for (j = 0; j < jiahosts; j++) {
			k = (jiapid * BUCKETS + i * jiahosts + j) * BSIZE;
			if (k > 16 * 1048576) printf("ERROR KEY!\n");
			while (a[k] > 0) {
				b[counter] = a[k];
				counter++;
				k++;
			}
		}

		/* printf("RANGE calculated!\n"); */

		for (j = KEY / jiahosts * jiapid; j < counter; j++) {
			k = b[j] / RANGE;
			a[k * BSIZE + count[k]] = b[j];
			count[k]++;
		}

		for (j = 0; j < jiahosts; j++) {
			k = jiapid * BUCKETS + i * jiahosts + j;

			MAGIC = k * BSIZE;
			printf("MAGIC = %d\n", MAGIC);

			for (x = count[k]; x >= 1; x--)
				for (y = 2; y <= x; y++) {
					z = MAGIC + y - 1;
					if (a[z-1] > a[z]) {
						temp = a[z-1];
						a[z-1] = a[z];
						a[z] = temp;
					}
				}
		}
	}

	jia_barrier();
	gettime(&time5);
	printf("Stage 4: Local Sorting done! (%u)\n", KEY);

	counter = 0;
	for (i = 0; i < BUCKETS*16; i++)
		counter += count[i];

	jia_lock(0);
	while (((signed) flag[0]) != jiapid) {  
		jia_unlock(0);
		for (i = 0; ((signed) i) < rand() * 100; i++);
		jia_lock(0);
	}

	start = flag[1];
	flag[1] += counter;
	printf("My start is %d, end at %d\n", start, start + counter);
	flag[0]++;
	jia_unlock(0);

	x = start;
	if (x % BSIZE > 0) {
		jia_lock(jiapid);
		locked = 1;
	} else {
		locked = 0; 
	}

	for (i = BUCKETS * jiapid; ((signed) i) < BUCKETS * (jiapid + 1); i++) {
		y = i * BSIZE;
		for (j = 0; j < ((signed) (count[i])); j++) {
			if (locked == 1 && x % BSIZE == 0) {
				jia_unlock(jiapid);
				locked = 0;
			} else if ((locked == 0) && (start + counter - x < ((signed) BSIZE)) && (x%BSIZE == 0)) {
				jia_lock(jiapid+1);
				locked = 1;
			}
			b[x] = a[y + j];
			x++;
		}
	}

	if (locked == 1)
		jia_unlock(jiapid+1);   

	jia_barrier();
	gettime(&time6);
	printf("Stage 5: Write back to array done!\n");

	if (jiapid == 0)
		for (i = 0; i < KEY-1; i++)
			if (b[i] > b[i+1])
				printf("Error in keys %d (%d) and %d (%d)\n", i, b[i], i+1,
						b[i+1]);

	printf("Partial time 1:\t\t %ld sec\n", time_diff_sec(&time2, &time1));
	printf("Partial time 2:\t\t %ld sec\n", time_diff_sec(&time3, &time2));
	printf("Partial time 3:\t\t %ld sec\n", time_diff_sec(&time4, &time3));
	printf("Partial time 4:\t\t %ld sec\n", time_diff_sec(&time5, &time4));
	printf("Total time:\t\t %ld sec\n", time_diff_sec(&time5, &time1));
	jia_exit();

	return 0;
}
