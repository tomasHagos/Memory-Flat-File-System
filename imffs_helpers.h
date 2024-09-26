#ifndef _A5_IMMFS_HELPERS
#define _A5_IMMFS_HELPERS

#include "a5_multimap.h"
int find_free_space(uint8_t *free_blocks, int block_count);
int get_block_number(int file_size); 
void free_space(uint8_t *free_blocks, int blocks, int starting_block);  
#endif
