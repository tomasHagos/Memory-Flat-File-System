/*
 *
 * PURPOSE: To make a simulation of a storage device.
 */

#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>


#include "Boolean.h"
#include "a5_imffs.h"
#include "a5_multimap.h"

const int BLOCK_BYTE_SIZE = 256;

typedef struct IMFFS {
    uint8_t *device;
    uint8_t *free_blocks;
    int block_count;
    Multimap *index;
} Imffs;

typedef struct KEYHOLDER
{
    char *file_name;
    long file_byte_size;
}KeyHolder;


//comparison functions.
static int compare_keys(void *a, void *b);
static int compare_values_always_greater(void *a, void *b);

//helper methods that are testable.
int find_free_space(uint8_t *free_blocks, int block_count);
int get_block_number(int file_size); 
void free_space(uint8_t *free_blocks, int blocks, int starting_block);  

//helper methods not testable.
void rename_key_and_add_again(Multimap *mm, void *key, char *renamed_name);
Boolean file_name_exists(Multimap *index , char *file); 
int get_key__with_name(Multimap *mm, char *name, void **key); 
void initialize_free_blocks(uint8_t *free_blocks, int block_count);
void print_chunks_info(Multimap *mm, void *key, int size);
IMFFSResult add_contents_to_device(FILE *source, IMFFSPtr fs, char *name);
void remove_values_and_key(Imffs *fs, void *key);
void load_data_to_file(IMFFSPtr fs, void *key, FILE *out);

//helper functions for defrag
IMFFSResult reconstruct_index(IMFFSPtr fs,KeyHolder **chunks_arr);
int defrag_operation(IMFFSPtr fs, KeyHolder **chunks_arr, int size, int pos);
int find_same_type_key(KeyHolder **chunks_arr, int start, int size, KeyHolder *key);
int find_empty_space(KeyHolder **chunks_arr, int end);
int find_key_to_be_moved(KeyHolder **chunks_arr, int starting, int size);
void shift_chunks_array(IMFFSPtr fs, int from, int to, KeyHolder **chunks_arr);
void move_file_within_device(IMFFSPtr fs, int index_to, int index_from,KeyHolder **chunks_arr);
void fill_chunks_array(IMFFSPtr fs, KeyHolder **chunks_arr);
void add_keys_to_chunks_array(KeyHolder **chunks_arr, void *key, int starting_block, int blocks);
void initialize_defrag_pointers_to_null(KeyHolder **chunks_arr, int size);



static int compare_keys(void *a, void *b)
{
  assert(NULL != a && NULL != b);
  return strcasecmp(((KeyHolder*)a)->file_name, ((KeyHolder*)b)->file_name);
}

//make it greater so when you insert it adds, it at the end of the linked list.
static int compare_values_always_greater(void *a, void *b)
{
  assert(NULL != a && NULL != b);
  
  return 1;
}


// this function will create the filesystem with the given number of blocks;
// it will modify the fs parameter to point to the new file system or set it
// to NULL if something went wrong (fs is a pointer to a pointer)
IMFFSResult imffs_create(uint32_t block_count, IMFFSPtr *fs)
{
    assert((int) (block_count) > 0);
    assert(fs != NULL);

    IMFFSResult returned = IMFFS_OK;

    if(NULL != fs && (int)(block_count) > 0)
    {
        *fs = malloc(sizeof(Imffs));

        if(NULL != *fs && (int) block_count > 0)
        {
            (*fs)->device = malloc(BLOCK_BYTE_SIZE * (int)(block_count));
            (*fs)->block_count = (int)block_count;

            if(NULL != (*fs)->device)
            {
                (*fs)->free_blocks = malloc((int)block_count * sizeof(uint8_t));

                if(NULL != (*fs)->free_blocks)
                {
                        initialize_free_blocks((*fs)->free_blocks,(int)block_count);
                        (*fs)->index = mm_create((int)block_count, compare_keys, compare_values_always_greater);

                        if(NULL == (*fs)->index)
                        {
                            free((*fs)->device);
                            free((*fs)->free_blocks);
                            free(*fs);
                            *fs = NULL;
                            returned = IMFFS_FATAL;
                        }
                }
                else
                {
                        free((*fs)->device);
                        free(*fs);
                        *fs = NULL;
                        returned = IMFFS_FATAL;
                }
            }
            else
            {
                free(*fs);
                *fs = NULL;
                returned = IMFFS_FATAL;
            }
        }
        else
        {
            returned = IMFFS_FATAL;
        }
    }
    else
    {
        returned = IMFFS_INVALID;
    }

    assert(returned == IMFFS_FATAL || returned == IMFFS_OK || returned == IMFFS_INVALID);

    return returned;
}



// save diskfile imffsfile copy from your system to IMFFS
IMFFSResult imffs_save(IMFFSPtr fs, char *diskfile, char *imffsfile)
{
    assert(NULL != fs);
    assert(NULL !=diskfile);
    assert(NULL != imffsfile);

    IMFFSResult returned = IMFFS_OK;
    
    if(NULL != fs && NULL != diskfile && NULL != imffsfile)
    {
        //if the file name doesn't exist in imffs.
        if(!file_name_exists(fs->index,imffsfile))
        {
            FILE *source_file = fopen(diskfile,"r");

            if(source_file != NULL)
            {
                //add the contents.
                returned = add_contents_to_device(source_file,fs,imffsfile);  
                fclose(source_file);
            }
            else
            {
                fprintf(stderr,"Error,File could not be opened.\n");
                returned = IMFFS_ERROR;
            }
        }
        else
        {
            returned = IMFFS_ERROR;
            fprintf(stderr,"Error! File with the name \"%s\" already exists in IMFFS.\n",imffsfile);
        }
    }
    else
    {
        returned = IMFFS_INVALID;
    }
    
    assert(returned == IMFFS_ERROR || returned == IMFFS_OK || returned == IMFFS_INVALID);

    return returned;

}


IMFFSResult imffs_rename(IMFFSPtr fs, char *imffsold, char *imffsnew)
{
    assert(NULL != fs);
    assert(NULL != imffsold);
    assert(NULL != imffsnew);

    IMFFSResult returned = IMFFS_OK;

    if(NULL != fs && NULL != imffsold && NULL != imffsnew)
    {
        void *key;

        //get the key.
        if(get_key__with_name(fs->index,imffsold,&key) == 0)
        {
            //if the new file name doesn't exist in the file already.
            if(!file_name_exists(fs->index,imffsnew))
            {
                rename_key_and_add_again(fs->index,key,imffsnew);
            }
            else
            {
                fprintf(stderr, "Error! file with the name \"%s\" already exists.\n",imffsnew);
                returned = IMFFS_ERROR;
            }
        }
        else
        {
            //if key is not found.
            fprintf(stderr,"Error! file with the name \"%s\" is not found in imffs.\n",imffsold);
            returned  = IMFFS_ERROR;
        }
    }
    else
    {
        returned = IMFFS_INVALID;
    }

    assert(returned == IMFFS_OK || returned == IMFFS_INVALID || returned == IMFFS_ERROR);

    return returned;
}


// dir will list all of the files and the number of bytes they occupy
IMFFSResult imffs_dir(IMFFSPtr fs)
{
    IMFFSResult returned = IMFFS_OK;
    assert(NULL != fs);

    if(NULL != fs)
    {
        void *key;
        int total_bytes = 0;
        if (mm_get_first_key(fs->index, &key) > 0) 
        {
            printf("-----------------------------------------\n");
            do
            {
                printf("File Name: %s\n",(((KeyHolder*)key)->file_name));
                total_bytes += ((KeyHolder*)key)->file_byte_size;

                printf("File Size: %lu bytes\n",((KeyHolder*)key)->file_byte_size);
                //calculate the number of blocks the easy way.
                printf("Blocks: %d\n",get_block_number(((KeyHolder*)key)->file_byte_size));

                printf("Chunks: %d\n",mm_count_values(fs->index,key));
                printf("-----------------------------------------\n");
            } while (mm_get_next_key(fs->index, &key) > 0);

            printf("\n");
        }
        else
        {
            printf("No files saved in IMFFS\n");
        }

        printf("Total bytes: %d\n",total_bytes);
    }
    else
    {
        returned = IMFFS_INVALID;
    }

    assert(returned == IMFFS_OK || returned == IMFFS_INVALID);
    return returned;
}

IMFFSResult imffs_load(IMFFSPtr fs, char *imffsfile, char *diskfile)
{
    assert(NULL!= fs);
    assert(NULL != imffsfile);
    assert(NULL != diskfile);

    IMFFSResult returned = IMFFS_OK;

    if(NULL != fs && NULL != imffsfile && NULL != diskfile)
    {
        void *key;

        //if the key is found.
        if(get_key__with_name(fs->index,imffsfile,&key) == 0)
        {
            FILE *out;
            out = fopen(diskfile,"w");

            if(NULL != out)
            {
                load_data_to_file(fs,key,out);
                fclose(out);
            }
            else
            {
                printf("Error trying to open the file: \"%s\" ",diskfile);
                returned = IMFFS_ERROR;
            }
        }
        else
        {
            printf("File with the name \"%s\" does not exist in IMFFS.\n",imffsfile);
            returned = IMFFS_ERROR;
        }
    }
    else
    {
        returned = IMFFS_INVALID;
    }

    assert(returned == IMFFS_ERROR || returned == IMFFS_OK || returned== IMFFS_INVALID);
    return returned;
}

// delete imffsfile remove the IMFFS file from the system, allowing the blocks to be used for other files
IMFFSResult imffs_delete(IMFFSPtr fs, char *imffsfile)
{
    assert(NULL != fs);
    assert(NULL != imffsfile);

    IMFFSResult returned = IMFFS_OK;

    if(NULL != fs && NULL != imffsfile)
    {
        void *key;

        //if the key is found.
        if(get_key__with_name(fs->index,imffsfile,&key) == 0)
        {
            //removes the values and key from the multimap.
            remove_values_and_key(fs,key);
        }
        else
        {
            fprintf(stderr,"Error! File with the name: \"%s\" does not exist.\n",imffsfile);
            returned = IMFFS_ERROR;
        }
    }
    else
    {
        returned = IMFFS_INVALID;
    }

    assert(returned == IMFFS_ERROR || returned == IMFFS_OK || returned== IMFFS_INVALID);

     return returned;
}

// fulldir is like "dir" except it shows a the files and details about all of the chunks they are stored in (where, and how big)
IMFFSResult imffs_fulldir(IMFFSPtr fs)
{
    assert(NULL != fs);

    IMFFSResult returned = IMFFS_OK;

    if(NULL != fs)
    {
        void *key;
        int num_chunks;
        int total_bytes = 0;

        if (mm_get_first_key(fs->index, &key) > 0) 
        {
            printf("-------------------------------------\n");
            do
            {
                printf("File Name:%s\n",(((KeyHolder*)key)->file_name));
                total_bytes += ((KeyHolder*)key)->file_byte_size;

                printf("File Size: %lu bytes\n",((KeyHolder*)key)->file_byte_size);
            
                num_chunks = mm_count_values(fs->index, key);

                printf("Total Blocks: %d\n",get_block_number(((KeyHolder*)key)->file_byte_size));
                printf("Total Chunks: %d\n",num_chunks);

                print_chunks_info(fs->index,key,num_chunks);
                printf("-----------------------------------------\n");

            } while (mm_get_next_key(fs->index, &key) > 0);

            printf("\n");
        }
        else
        {
            printf("No files in imffs\n");
        }

        printf("Total bytes: %d\n",total_bytes);
    }
    else
    {
        returned = IMFFS_INVALID;
    }

    assert(returned == IMFFS_OK || returned == IMFFS_INVALID);

    return returned;
}

// quit will quit the program: clean up the data structures
IMFFSResult imffs_destroy(IMFFSPtr fs)
{
    assert(NULL != fs);
    IMFFSResult returned = IMFFS_OK;

    if(NULL != fs)
    {
        void *key;
        if (mm_get_first_key(fs->index, &key) > 0) 
        {
            do 
            {
                //remove the key
                mm_remove_key(fs->index,key);
                //free the name
                free(((KeyHolder*)key)->file_name);
                //free the key struct as a whole
                free((KeyHolder*)key); 
            } while (mm_get_next_key(fs->index, &key) > 0);
        }

        //free the multimap
        mm_destroy(fs->index);
        //free the device
        free(fs->device);
        //free the free blocks.
        free(fs->free_blocks);
    }
    else
    {
        returned = IMFFS_INVALID;
    }

    assert(returned == IMFFS_OK || returned == IMFFS_INVALID);
    
    return returned;
}

// defrag will defragment the filesystem: if you haven't implemented it, have it print "feature not implemented" and return IMFFS_NOT_IMPLEMENTED
IMFFSResult imffs_defrag(IMFFSPtr fs)
{
    assert(NULL != fs);
    IMFFSResult returned = IMFFS_OK;

    if(NULL !=fs)
    {
        //this array keeps track of where each chunk is for a key.(in order)
        //allocate the array we use to defrag
        KeyHolder **chunks_arr = malloc(fs->block_count * sizeof(KeyHolder *));
        //initialize each pointers to null at first.
        initialize_defrag_pointers_to_null(chunks_arr,fs->block_count);
        //fill in the defrag array with the keys(in order).
        fill_chunks_array(fs,chunks_arr);

        //get the number of keys in the multimap.
        int keys = mm_count_keys(fs->index);
        int num = 0;
        int i=0;

        //while loop to initiate defrag operation
        while(i < keys)
        {
            //num gets updated each time, it signfies the position in chunks_arr when the defragement file ends.
            //for each call one whole file gets defragemented, hence why  i < keys
            num =  defrag_operation(fs,chunks_arr,fs->block_count,num);
            i++;
        }

        //reconstruct the index, destroy the earlier one.
        returned = reconstruct_index(fs,chunks_arr); // can return fatal if we run out of malloc memory.

        free(chunks_arr);
    }
    else
    {
        returned = IMFFS_INVALID;
    }

    assert(returned == IMFFS_INVALID || returned == IMFFS_OK || returned == IMFFS_FATAL);

    return returned;
}

/**
 * PURPOSE: this defrags one file at a time.
 * INPUT PARAMETERS:
 *    chunkz_arr: the key array
 *    size: size of chunks_arr
 *    pos: the starting position of the fragmented datas in the file.
 */

int defrag_operation(IMFFSPtr fs, KeyHolder **chunks_arr, int size, int pos)
{
    //find the position of they key to be moved. using pos as starting position. 
    //for example at the start pos = 0, the the moved key will be at pos 0. 
    // but as we edit the chunks_arr for each loop call, position will be where the prior file ends(index).
    int moving_key_pos = find_key_to_be_moved(chunks_arr,pos,size); 

    KeyHolder *key = chunks_arr[moving_key_pos];
    //find an empty space before the moving_key_position.
    int empty_space = find_empty_space(chunks_arr, moving_key_pos);
    //if its not the position we are in currently.
    if(empty_space != moving_key_pos)
    {
        //move the file from the moving_key_pos to empty space. (it edits the device)
        move_file_within_device(fs,empty_space,moving_key_pos,chunks_arr);
    }

    //now we find the key with the same type as the earlier move key.
    //we are looking for its position
    //this function gives us the position where the first key with the same type appears.
    //we do this to preserve order.
    int new_pos = find_same_type_key(chunks_arr, empty_space+1,size,key);

    //if there is new same type key left, we loop.
    while(new_pos > 0)
    {
        //if the space after the prior key is NULL(empty), move the file to that space.
        if(chunks_arr[empty_space+1] == NULL)
        {
            //this swaps a single block at a time.
            move_file_within_device(fs,empty_space+1,new_pos,chunks_arr);
        }
        else
        {
            //we need to do shifiting
            shift_chunks_array(fs,empty_space+1,new_pos,chunks_arr);
        }
        //after shifting the new key, will in empty space+1 so we increment empty space.
        empty_space++;
        //repeat the process until we have no more keys left.
        new_pos =  find_same_type_key(chunks_arr,empty_space+1,size,key);
    }
    
    //returns the space that comes after where the last key value was stored.
    //so we can begin at that exact place when defrag is called again.
    return empty_space+1;
}

/**
 * PURPOSE: moves file from one block to another block within the device.
 * INPUT PARAMETERS:
 *    index_to: the position where the file is moved
 *    index_from: where the file being move is located
 *     chunkz_arr: the array of the keys
 */
void move_file_within_device(IMFFSPtr fs, int index_to, int index_from, KeyHolder **chunks_arr)
{
   //copy the data
   memcpy(fs->device +(index_to * BLOCK_BYTE_SIZE), fs->device+(index_from*BLOCK_BYTE_SIZE), BLOCK_BYTE_SIZE);
   //update the position in the chunk array.
   chunks_arr[index_to] = chunks_arr[index_from];
   chunks_arr[index_from] = NULL;
}

/**
 * PURPOSE:  to shift the chunkz array to the right
 * INPUT PARAMETERS:
 *    from: where the shift begins
 *    to: where the shift ends
 */
void shift_chunks_array(IMFFSPtr fs, int from, int to, KeyHolder **chunks_arr)
{
    //store one block in a temporary pointer.
    uint8_t *temp_file = malloc(BLOCK_BYTE_SIZE);
    //read in the temp file.
    memcpy(temp_file, fs->device+(to*BLOCK_BYTE_SIZE), BLOCK_BYTE_SIZE);

    KeyHolder *tempKey = chunks_arr[to];
    //shift the defrag array with the files.
    int move_to = to;
    //do the shifting operation
    for(int i=to-1; i >= from; i--)
    {
        if(chunks_arr[i] != NULL)
        {
            move_file_within_device(fs, move_to,i,chunks_arr);
            move_to = i;
        }
    }

    //finally copy back the data to the device.
    memcpy(fs->device+(from*BLOCK_BYTE_SIZE),temp_file,BLOCK_BYTE_SIZE);
    //and also update the defrag array position.
    chunks_arr[from] = tempKey;
    free(temp_file);
}

void initialize_defrag_pointers_to_null(KeyHolder **chunks_arr, int size)
{
    for(int i=0; i < size; i++)
    {
        chunks_arr[i] = NULL;
    }
}


//this fills in the defrag array with the memory address of the key pointers.
void fill_chunks_array(IMFFSPtr fs, KeyHolder **chunks_arr)
{
    void *key;
    Value *values;
    int num_values;
    int starting_block;

    if (mm_get_first_key(fs->index, &key) > 0) 
    {
         do
        {
            num_values = mm_count_values(fs->index,key);
            values = malloc(num_values * sizeof(Value));
            mm_get_values(fs->index,key,values,num_values);

            for(int i=0; i < num_values; i++)
            {
                //get the block where chunk starts at.
                starting_block = (uint8_t*)(values[i].data) - (fs->device);
                starting_block = starting_block/BLOCK_BYTE_SIZE; 
                //and add the key to the defrag array using the starting_block and total number of blocks in that chunk.
                add_keys_to_chunks_array(chunks_arr, key, starting_block, values[i].num);
            }

        } while (mm_get_next_key(fs->index, &key) > 0);
    }
}

//this adds each chunk to chunk array.
void add_keys_to_chunks_array(KeyHolder **chunks_arr, void *key, int starting_block, int blocks)
{
    for(int i=starting_block; i < starting_block+blocks; i++)
    {
        chunks_arr[i] = key;
    }
}


/**
 * PURPOSE: Deletes the prior index, and reconstructs a new one for the degramented datas.
 */
IMFFSResult reconstruct_index(IMFFSPtr fs,KeyHolder **chunks_arr)
{
    IMFFSResult returned = IMFFS_OK;
    //we don't free the keys as they will be used again.
    mm_destroy(fs->index);
    //create new index.
    fs->index = mm_create(fs->block_count,compare_keys, compare_values_always_greater); 

    if(fs->index != NULL)
    {
        int i=0;
        KeyHolder *key;
        int blocks = 0;
        int starting;
        int total_blocks = 0; //used to occupy the free blocks array.

        while(chunks_arr[i] != NULL)
        {
            key = chunks_arr[i];
            starting = i;

            //since the data now packed in contiguous blocks, we can just count how many blocks there are
            while(chunks_arr[i] == key)
            {
                blocks++;
                i++;
            }
            //insert the key
            mm_insert_value(fs->index,key,blocks,fs->device+(starting*BLOCK_BYTE_SIZE));
            //increment total blocks.
            total_blocks += blocks;
            //make it 0, to count set of blocks for another key.
            blocks = 0;
        }

        //make the free blocks all free.
        initialize_free_blocks(fs->free_blocks, fs->block_count);

        //now since we know that all blocks are contiguous blocks. We can just occupy 0-blocks-1 index in free blocks tracker array.
        for(int i=0; i < total_blocks; i++)
        {
            fs->free_blocks[i] = 'N';
        }
    }
    else
    {
        returned = IMFFS_FATAL;
    }

    return returned;
}


int find_same_type_key(KeyHolder **chunks_arr, int start, int size, KeyHolder *key)
{
    Boolean found = FALSE;
    int i = start;
    //find a key with the same type of parameter key starting from position start.
    while(!found && i < size)
    {
        if(chunks_arr[i] != NULL && chunks_arr[i] == key)
        {
            found = TRUE;
        }
        else
        {
            i++;
        }
    }
    if(!found)
    {
        i= -1;
    }
    return i;
}

//find empty space in chunks_array
int find_empty_space(KeyHolder **chunks_arr, int end)
{
    Boolean found = FALSE;
    int i = 0;

    while(!found && i < end)
    {
        if(chunks_arr[i] == NULL)
        {
            found = TRUE;
        }
        else
        {
            i++;
        }
    }

    return i;
}

/**
 * PURPOSE: this finds a key to be defragged using the starting position.
 * INPUT PARAMETERS:
 *    starting: where the search should beging
 *    size: size of array
 */
int find_key_to_be_moved(KeyHolder **chunks_arr, int starting, int size)
{
    Boolean found =FALSE;
    int i=starting;

    while(!found && i < size)
    {
        if(chunks_arr[i] != NULL)
        {
            found = TRUE;
        }
        else
        {
            i++;
        }
    }

    return i;

}


void initialize_free_blocks(uint8_t *free_blocks, int block_count)
{
    //Y represents free
    //N represents occupied
    for(int i=0; i < block_count; i++)
    {
        free_blocks[i] = 'Y';
    }
}

//this returns the position of the first free space found.
//returns -1 on error
int find_free_space(uint8_t *free_blocks, int block_count)
{
    assert(NULL != free_blocks);

    if(NULL != free_blocks)
    {
        for(int i=0; i < block_count; i++)
        {
            if(free_blocks[i] == 'Y')
            {
                return i;
            }
        }
    }
    return -1;
}


//this finds a file with a name
Boolean file_name_exists(Multimap *index , char *file)
{
    Boolean found = FALSE;
    void *key;

     if (mm_get_first_key(index, &key) > 0) 
    {
        do
        {
            if(strcasecmp(((KeyHolder *)key)->file_name,file) == 0)
            {
                found = TRUE;
            }

        } while (mm_get_next_key(index, &key) > 0 && !found);
    }

    return found;
}

void print_chunks_info(Multimap *mm, void *key, int size)
{
    Value values[size];

    mm_get_values(mm,key,values,size);
    
    for(int i=0; i < size; i++)
    {
        printf("Chunk: %d  ",i+1);
        printf("Place: %p  ", values[i].data);
        printf("Blocks used: %d\n",values[i].num);
    }
}

//this uses an efficient algorithm to calculate the block_numbers
int get_block_number(int file_size)
{
    assert(file_size >= 0);

    if(file_size >=0)
    {
        double blocks = file_size/((double)(BLOCK_BYTE_SIZE));
        int block_rounded_up = (int)blocks;

        //if block number is 0, that means it 0 bytes, but that zero bytes still occupy a block
        if(block_rounded_up < blocks || block_rounded_up == 0)
        {
            block_rounded_up++; 
        }
        
        return block_rounded_up;
    }
    return -1;
}


int get_key__with_name(Multimap *mm, char *name, void **key)
{
    int returned = 0;
    Boolean found = FALSE;

    if (mm_get_first_key(mm, &(*key)) > 0) 
       {
        do
        {
            if(strcasecmp(((KeyHolder*)(*key))->file_name,name) == 0)
            {
                found = TRUE;
            }
        } while (!found && mm_get_next_key(mm, &(*key)) > 0);
    }

    if(!found)
    {
        returned = -1;
    }

    return returned;
}


//this adds contents to the file, used in the IMFFS_SAVE function.
IMFFSResult add_contents_to_device(FILE *source, IMFFSPtr fs, char *name)
{
    assert(NULL != source);
    assert(NULL != fs);
    assert(NULL!=name);
    IMFFSResult returned = IMFFS_OK;

    if(NULL != source && NULL != fs && NULL != name)
    {
        int space = find_free_space(fs->free_blocks, fs->block_count);

        if(space == -1)
        {
            fprintf(stderr,"Error! No more space to store the file: \"%s\" in imffs\n",name);
        }
        else
        {
            //malloc key and allocate the file name.
            KeyHolder *key = malloc(sizeof(KeyHolder));
            key->file_name = strdup(name);


            int starting_point = BLOCK_BYTE_SIZE * space;
            int chunks_start = starting_point;
            int block_number = 0;
            long total_byte_size = 0;
            Boolean done = FALSE;


            while(!done)
            {
                //if there is enough space.
                total_byte_size += fread((fs->device)+starting_point,1,BLOCK_BYTE_SIZE,source);

                //increment the byte size.
                block_number++;
                fs->free_blocks[space] = 'N';

                //if we are done.
                if(feof(source))
                {
                    done = TRUE;
                    mm_insert_value(fs->index,key,block_number,(fs->device)+chunks_start);
                }

                else
                {
                    //if the space next is free
                    if(fs->free_blocks[space+1] == 'Y')
                    {
                        space++;
                        starting_point = BLOCK_BYTE_SIZE * space;
                    }
                    else
                    {
                        //if we get here it means its fragmeneted
                        //insert the current chunk.
                        mm_insert_value(fs->index,key,block_number,(fs->device)+chunks_start);
                        //find a free space
                        space = find_free_space(fs->free_blocks,fs->block_count);
                        //if no space left we are done.
                        if(space == -1)
                        {
                            done = TRUE;
                        }
                        else
                        {
                            starting_point = BLOCK_BYTE_SIZE *space;
                            block_number = 0;
                            chunks_start = starting_point;
                        }
                    }
                }
            }
            key->file_byte_size = total_byte_size;

            //if we get here and the file is not fully read, we have to remove it.
            if(!feof(source))
            {
                fprintf(stderr,"Error! Not enough space to store the file: \"%s\"\n",name);
                imffs_delete(fs,key->file_name);
                returned = IMFFS_ERROR;
            }
        }
    }
    else
    {
        returned = IMFFS_INVALID;
    }
    
    assert(returned == IMFFS_OK || returned == IMFFS_ERROR || returned == IMFFS_INVALID);

    return returned;
}

/**
 * PURPOSE: this removes a key, renames it, and adds it again.
 * INPUT PARAMETERS:
 * renamed_name:the new name
 * key: the key to be edited
 */

void rename_key_and_add_again(Multimap *mm, void *key, char *renamed_name)
{
     int values_num = mm_count_values(mm,key);
     Value value[values_num];
     mm_get_values(mm,key,value,values_num);
     mm_remove_key(mm,key);

    //free the earlier name first.
    free(((KeyHolder*)key)->file_name);

    ((KeyHolder*)key)->file_name = strdup(renamed_name);
    //adds the keys and values again.
    for(int i=0; i < values_num; i++)
    {
        mm_insert_value(mm,key,value[i].num,value[i].data);
    }
}

/**
 * PURPOSE: this frees up space in the free_blocks array.
 * INPUT PARAMETERS:
  * starting_block: where the freeing should start
  * blocks: the number of spaces to be freed.
 */
void free_space(uint8_t *free_blocks, int blocks, int starting_block)
{
 
    for(int i=starting_block; i < starting_block+blocks; i++)
    {
        free_blocks[i] = 'Y';
    }
}

/**
 * PURPOSE: this removes values and keys from the multimap while also freeing space. used in the delete operation.
 */

void remove_values_and_key(Imffs *fs, void *key)
{

     int num_values = mm_count_values(fs->index,key);

     Value values[num_values];
     mm_get_values(fs->index,key,values,num_values);

     int starting_block;
     for(int i=0; i < num_values; i++)
     {
        //calculate the starting block
         starting_block = (uint8_t*)(values[i].data) - (fs->device);
         starting_block = starting_block/BLOCK_BYTE_SIZE;

         //free the spaces in the free space list, so we can save new file there.
         free_space(fs->free_blocks,values[i].num,starting_block);
     } 

     mm_remove_key(fs->index,key);

     //free the key name and key variable.
     free(((KeyHolder*)key)->file_name);
     free(key);
}

//this loads data, used in the load function.
void load_data_to_file(IMFFSPtr fs, void *key, FILE *out)
{
    //get the number of chunks and initialize the value array
    int num_values = mm_count_values(fs->index,key);
    Value *values = malloc(num_values * sizeof(Value));

    //get the chunks from the multimap.
    mm_get_values(fs->index,key,values,num_values);


    int total_byte_read = 0;
    int num_elements = 0;

    KeyHolder *read_key = key;

    for(int i=0; i < num_values; i++)
    {
        //if the bytes left to read is more than the chunk's total byte. read the entire chunk.
        if((read_key->file_byte_size - total_byte_read) > (values[i].num * BLOCK_BYTE_SIZE))
        {
            num_elements = values[i].num * BLOCK_BYTE_SIZE;
        }
        else
        {
            //this means we have used a block but part of it only.
            num_elements = read_key->file_byte_size - total_byte_read;
        }
        
        total_byte_read += fwrite(values[i].data,1,num_elements,out);
    }

    //free the values.
    free(values);
}
