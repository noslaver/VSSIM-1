// File: ftl.c
// Date: 2014. 12. 03.
// Author: Jinsoo Yoo (jedisty@hanyang.ac.kr)
// Copyright(c)2014
// Hanyang University, Seoul, Korea
// Embedded Software Systems Laboratory. All right reserved

#include "common.h"
#ifndef VSSIM_BENCH
#include "qemu-kvm.h"
#endif
#include <stdlib.h>

#ifdef FTL_GET_WRITE_WORKLOAD
FILE* fp_write_workload;
#endif
#ifdef FTL_IO_LATENCY
FILE* fp_ftl_w;
FILE* fp_ftl_r;
#endif

int g_init = 0;
extern double ssd_util;

void FTL_INIT(void) {
  if (g_init == 0) {
    printf("[%s] start\n", __FUNCTION__);

    INIT_SSD_CONFIG();

    INIT_MAPPING_TABLE();
    INIT_INVERSE_MAPPING_TABLE();
    INIT_BLOCK_STATE_TABLE();
    INIT_VALID_ARRAY();
    INIT_EMPTY_BLOCK_LIST();
    INIT_VICTIM_BLOCK_LIST();
    INIT_PERF_CHECKER();

#ifdef FTL_MAP_CACHE
    INIT_CACHE();
#endif
#ifdef FIRM_IO_BUFFER
    INIT_IO_BUFFER();
#endif
#ifdef MONITOR_ON
#ifndef LOCAL
    INIT_LOG_MANAGER();
#endif
#endif
    g_init = 1;
#ifdef FTL_GET_WRITE_WORKLOAD
    fp_write_workload = fopen("./data/p_write_workload.txt", "a");
#endif
#ifdef FTL_IO_LATENCY
    fp_ftl_w = fopen("./data/p_ftl_w.txt", "a");
    fp_ftl_r = fopen("./data/p_ftl_r.txt", "a");
#endif
    SSD_IO_INIT();

    printf("[%s] complete\n", __FUNCTION__);
  }
}

void FTL_TERM(void) {
  printf("[%s] start\n", __FUNCTION__);

#ifdef FIRM_IO_BUFFER
  TERM_IO_BUFFER();
#endif
  TERM_MAPPING_TABLE();
  TERM_INVERSE_MAPPING_TABLE();
  TERM_VALID_ARRAY();
  TERM_BLOCK_STATE_TABLE();
  TERM_EMPTY_BLOCK_LIST();
  TERM_VICTIM_BLOCK_LIST();
  TERM_PERF_CHECKER();

#ifdef MONITOR_ON
  TERM_LOG_MANAGER();
#endif

#ifdef FTL_IO_LATENCY
  fclose(fp_ftl_w);
  fclose(fp_ftl_r);
#endif
  printf("[%s] complete\n", __FUNCTION__);
}

void FTL_READ(int32_t sector_nb, unsigned int length) {
  int ret;

#ifdef GET_FTL_WORKLOAD
  FILE* fp_workload = fopen("./data/workload_ftl.txt", "a");
  struct timeval tv;
  struct tm* lt;
  double curr_time;
  gettimeofday(&tv, 0);
  lt = localtime(&(tv.tv_sec));
  curr_time = lt->tm_hour * 3600 + lt->tm_min * 60 + lt->tm_sec +
              (double)tv.tv_usec / (double)1000000;
  // fprintf(fp_workload,"%lf %d %ld %u %x\n",curr_time, 0, sector_nb, length,
  // 1);
  fprintf(fp_workload, "%lf %d %u %x\n", curr_time, sector_nb, length, 1);
  fclose(fp_workload);
#endif
#ifdef FTL_IO_LATENCY
  int64_t start_ftl_r, end_ftl_r;
  start_ftl_r = get_usec();
#endif
  ret = _FTL_READ(sector_nb, length);
#ifdef FTL_IO_LATENCY
  end_ftl_r = get_usec();
  if (length >= 128)
    fprintf(fp_ftl_r, "%ld\t%u\n", end_ftl_r - start_ftl_r, length);
#endif
}

void FTL_WRITE(int32_t sector_nb, unsigned int length) {
  int ret;

#ifdef GET_FTL_WORKLOAD
  FILE* fp_workload = fopen("./data/workload_ftl.txt", "a");
  struct timeval tv;
  struct tm* lt;
  double curr_time;
  gettimeofday(&tv, 0);
  lt = localtime(&(tv.tv_sec));
  curr_time = lt->tm_hour * 3600 + lt->tm_min * 60 + lt->tm_sec +
              (double)tv.tv_usec / (double)1000000;
  //	fprintf(fp_workload,"%lf %d %ld %u %x\n",curr_time, 0, sector_nb,
  // length, 0);
  fprintf(fp_workload, "%lf %d %u %x\n", curr_time, sector_nb, length, 0);
  fclose(fp_workload);
#endif
#ifdef FTL_IO_LATENCY
  int64_t start_ftl_w, end_ftl_w;
  start_ftl_w = get_usec();
#endif
  ret = _FTL_WRITE(sector_nb, length);
#ifdef FTL_IO_LATENCY
  end_ftl_w = get_usec();
  if (length >= 128)
    fprintf(fp_ftl_w, "%ld\t%u\n", end_ftl_w - start_ftl_w, length);
#endif
}

int _FTL_READ(int32_t sector_nb, unsigned int length) {
#ifdef FTL_DEBUG
  printf("[%s] Start\n", __FUNCTION__);
#endif

  if (sector_nb + length > SECTOR_NB) {
    printf("Error[%s] Exceed Sector number\n", __FUNCTION__);
    return FAIL;
  }

  double compression_alpha = (double)rand() / (double)RAND_MAX;

  printf("Compression alpha is %f\n", compression_alpha);
  int32_t lpn;
  int32_t ppn;
  int32_t lba = sector_nb;
  unsigned int remain = compression_alpha * length;
  unsigned int left_skip = sector_nb % SECTORS_PER_PAGE;
  unsigned int right_skip;
  unsigned int read_sects;

  unsigned int ret = FAIL;
  int read_page_nb = 0;
  int io_page_nb;

#ifdef FIRM_IO_BUFFER
  INCREASE_RB_FTL_POINTER(length);
#endif

  while (remain > 0) {
    if (remain > SECTORS_PER_PAGE - left_skip) {
      right_skip = 0;
    } else {
      right_skip = SECTORS_PER_PAGE - left_skip - remain;
    }
    read_sects = SECTORS_PER_PAGE - left_skip - right_skip;

    lpn = lba / (int32_t)SECTORS_PER_PAGE;
    ppn = GET_MAPPING_INFO(lpn);

    if (ppn == -1) {
#ifdef FIRM_IO_BUFFER
      INCREASE_RB_LIMIT_POINTER();
#endif
      return FAIL;
    }

    lba += read_sects;
    remain -= read_sects;
    left_skip = 0;
  }

  io_alloc_overhead = ALLOC_IO_REQUEST(sector_nb, length, READ, &io_page_nb);

  remain = length;
  lba = sector_nb;
  left_skip = sector_nb % SECTORS_PER_PAGE;

  while (remain > 0) {
    if (remain > SECTORS_PER_PAGE - left_skip) {
      right_skip = 0;
    } else {
      right_skip = SECTORS_PER_PAGE - left_skip - remain;
    }
    read_sects = SECTORS_PER_PAGE - left_skip - right_skip;

    lpn = lba / (int32_t)SECTORS_PER_PAGE;

#ifdef FTL_MAP_CACHE
    ppn = CACHE_GET_PPN(lpn);
#else
    ppn = GET_MAPPING_INFO(lpn);
#endif

    if (ppn == -1) {
#ifdef FTL_DEBUG
      printf("ERROR[%s] No Mapping info\n", __FUNCTION__);
#endif
    }

    ret = SSD_PAGE_READ(CALC_FLASH(ppn), CALC_BLOCK(ppn), CALC_PAGE(ppn),
                        read_page_nb, READ, io_page_nb);

    if (compression_alpha > 0.5)
    {
        /* if alpha is greater than 0.5, it is more likely that the page is spread over
         * two pages, so we read them both. */
        ret = SSD_PAGE_READ(CALC_FLASH(ppn + 1), CALC_BLOCK(ppn + 1),
                CALC_PAGE(ppn + 1), read_page_nb, READ, io_page_nb);
    }

#ifdef FTL_DEBUG
    if (ret == SUCCESS) {
      printf("\t read complete [%u]\n", ppn);
    } else if (ret == FAIL) {
      printf("ERROR[%s] %u page read fail \n", __FUNCTION__, ppn);
    }
#endif
    read_page_nb++;

    lba += read_sects;
    remain -= read_sects;
    left_skip = 0;
  }

  INCREASE_IO_REQUEST_SEQ_NB();

#ifdef FIRM_IO_BUFFER
  INCREASE_RB_LIMIT_POINTER();
#endif

#ifdef MONITOR_ON
  char szTemp[1024];
  sprintf(szTemp, "READ PAGE %d ", length);
  WRITE_LOG(szTemp);
#endif

#ifdef FTL_DEBUG
  printf("[%s] Complete\n", __FUNCTION__);
#endif

  return ret;
}

int _FTL_WRITE(int32_t sector_nb, unsigned int length) {
#ifdef FTL_DEBUG
  printf("[%s] Start\n", __FUNCTION__);
#endif

#ifdef FTL_GET_WRITE_WORKLOAD
  fprintf(fp_write_workload, "%d\t%u\n", sector_nb, length);
#endif

  int io_page_nb;


  double compression_alpha = (double)rand() / (double)RAND_MAX;
  printf("Compression alpha is %f\n", compression_alpha);

  static int32_t write_buffer = 0;
  write_buffer += compression_alpha * length * SECTOR_SIZE;
  printf("Write buffer is %d, but was %f\n", write_buffer, write_buffer - compression_alpha * length * SECTOR_SIZE + length * SECTOR_SIZE);
  if (write_buffer < 4096) {
      return SUCCESS;
  }
  length = write_buffer / SECTOR_SIZE;

  int32_t lba = sector_nb;
  int32_t lpn;
  int32_t new_ppn;
  int32_t old_ppn;

  unsigned int remain = length;
  unsigned int left_skip = sector_nb % SECTORS_PER_PAGE;
  unsigned int right_skip;
  unsigned int write_sects;

  unsigned int ret = FAIL;
  int write_page_nb = 0;

  while (remain > 0) {
    if (remain > SECTORS_PER_PAGE - left_skip) {
      right_skip = 0;
    } else {
      right_skip = SECTORS_PER_PAGE - left_skip - remain;
    }

    write_sects = SECTORS_PER_PAGE - left_skip - right_skip;

#ifdef FIRM_IO_BUFFER
    INCREASE_WB_FTL_POINTER(write_sects);
#endif

#ifdef WRITE_NOPARAL
    ret = GET_NEW_PAGE(VICTIM_NOPARAL, empty_block_table_index, &new_ppn);
#else
    ret = GET_NEW_PAGE(VICTIM_OVERALL, EMPTY_TABLE_ENTRY_NB, &new_ppn);
#endif
    if (ret == FAIL) {
      printf("ERROR[%s] Get new page fail \n", __FUNCTION__);
      return FAIL;
    }

    lpn = lba / (int32_t)SECTORS_PER_PAGE;
    old_ppn = GET_MAPPING_INFO(lpn);

    /* NOTE: the mapping table needs to record the offset of the logical page inside
     * the physical page and how much space is left inside each page (when searching for
     * a page to write to). */
    if (old_ppn != -1) {
        int32_t read_page_nb = 0;
        int io_page_read_nb;
        io_alloc_overhead = ALLOC_IO_REQUEST(sector_nb, length, READ, &io_page_read_nb);
        /* Since there are multiple pages mapped to the same physical page, we must
         * read the old page before writing to it.
         * If alpha is greater than 0.5, it is more likely that the page is spread over
         * two pages, so we read them both. */
	printf("Before ssd read\n");
        ret = SSD_PAGE_READ(CALC_FLASH(old_ppn), CALC_BLOCK(old_ppn), CALC_PAGE(old_ppn),
                        read_page_nb, READ, io_page_read_nb);
	printf("After ssd read\n");

        if (compression_alpha > 0.5)
        {
	    printf("Before ssd read comp > 0.5\n");
            ret = SSD_PAGE_READ(CALC_FLASH(old_ppn), CALC_BLOCK(old_ppn),
                    CALC_PAGE(old_ppn), read_page_nb, READ, io_page_read_nb);
	    printf("After ssd read comp > 0.5\n");
        }
    }

  if (sector_nb + length > SECTOR_NB) {
    printf("ERROR[%s] Exceed Sector number\n", __FUNCTION__);
    return FAIL;
  } else {
    io_alloc_overhead = ALLOC_IO_REQUEST(sector_nb, length, WRITE, &io_page_nb);
  }
    if ((left_skip || right_skip) && (old_ppn != -1)) {
      ret = SSD_PAGE_PARTIAL_WRITE(CALC_FLASH(old_ppn), CALC_BLOCK(old_ppn),
                                   CALC_PAGE(old_ppn), CALC_FLASH(new_ppn),
                                   CALC_BLOCK(new_ppn), CALC_PAGE(new_ppn),
                                   write_page_nb, WRITE, io_page_nb);
    } else {
      ret =
          SSD_PAGE_WRITE(CALC_FLASH(new_ppn), CALC_BLOCK(new_ppn),
                         CALC_PAGE(new_ppn), write_page_nb, WRITE, io_page_nb);
    }

    write_page_nb++;

  printf("1\n");
    UPDATE_OLD_PAGE_MAPPING(lpn);
  printf("2\n");
    UPDATE_NEW_PAGE_MAPPING(lpn, new_ppn);

#ifdef FTL_DEBUG
    if (ret == SUCCESS) {
      printf("\twrite complete [%d, %d, %d]\n", CALC_FLASH(new_ppn),
             CALC_BLOCK(new_ppn), CALC_PAGE(new_ppn));
    } else if (ret == FAIL) {
      printf("ERROR[%s] %d page write fail \n", __FUNCTION__, new_ppn);
    }
#endif
    lba += write_sects;
    remain -= write_sects;
    left_skip = 0;
  }

  INCREASE_IO_REQUEST_SEQ_NB();
#ifdef GC_ON
  GC_CHECK(CALC_FLASH(new_ppn), CALC_BLOCK(new_ppn));
#endif

#ifdef FIRM_IO_BUFFER
  INCREASE_WB_LIMIT_POINTER();
#endif

#ifdef MONITOR_ON
  char szTemp[1024];
  sprintf(szTemp, "WRITE PAGE %d ", length);
  WRITE_LOG(szTemp);
  sprintf(szTemp, "WB CORRECT %d", write_page_nb);
  WRITE_LOG(szTemp);
#endif

#ifdef FTL_DEBUG
  printf("[%s] End\n", __FUNCTION__);
#endif
  return ret;
}
