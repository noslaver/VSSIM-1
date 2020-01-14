#include "common.h"
#include "ssd.h"

#include <assert.h>
#include <stdlib.h>
#include <time.h>

#define RUN_TEST(SETUP, FUNC)        \
    if (SETUP)                       \
        popen("rm data/*.dat", "r"); \
    SSD_INIT();                      \
    FUNC;                            \
    if (SETUP)                       \
    SSD_TERM()

extern int32_t* mapping_table;
extern int page_writes;

/**
 * simple test that writes 70% of sectors in the device sequentially
 */
int test_access_seq(int req_size)
{
    int sectors_per_write = req_size * SECTORS_PER_PAGE;
	int i = 0;
    for (i = 0; i <= SECTOR_NB; i += sectors_per_write) {
        if ((i / SECTORS_PER_PAGE) % (1024 * 10) == 0) {
            LOG("wrote %.3lf of device", (double)i / (double)SECTOR_NB);
        }

        int lba = i % (int)(SECTOR_NB);
        SSD_WRITE(sectors_per_write, lba);
    }
    printf("%d page writes\n", page_writes);
    printf("%d i \n", i);
    printf("%lf WA\n", ((double)page_writes)/i * SECTORS_PER_PAGE);


    printf("wrote seq\n");

    return 0;
}

/**
 * simple test that writes 70% sectors in the device random
 */
int test_access_random(int req_size)
{
    int sectors_per_write = req_size * SECTORS_PER_PAGE;

    int i = 0;
    srand(time(NULL));
    while (i < 1.01 * SECTOR_NB) {
        if ((i / SECTORS_PER_PAGE) % (1024 * 10) == 0) {
            LOG("wrote %.3lf of device", (double)i / (double)SECTOR_NB);
        }

        // generating random lba, then aligning it to 4KB using SECTORS_PER_PAGE
        int rand_lba = rand() % (int)(SECTOR_NB);
        int lba = rand_lba - (rand_lba % SECTORS_PER_PAGE);
        SSD_WRITE(sectors_per_write, lba);
        i += sectors_per_write;
    }

    printf("wrote random\n");

    return 0;
}

int main(int argc, char* argv[])
{
    int setup = 1;
    if (argc < 3) {
        LOG("USAGE: %s <request_size> <workload>", argv[0]);
        return 1;
    }
    int req_size = atoi(argv[1]);
    char* workload = argv[2];

    if (req_size != 1 && req_size != 2 && req_size != 4 && req_size != 8) {
        LOG("<request_size> should be one of 1/2/4/8.");
        return 1;
    }

    if (strncmp(workload, "s", 1) == 0) {
        RUN_TEST(setup, test_access_seq(req_size));
    } else if (strncmp(workload, "r", 1) == 0) {
        RUN_TEST(setup, test_access_random(req_size));
    } else {
        LOG("bad workload argument. insert 'r' or 's'.");
        return 1;
    }

    return 0;
}
