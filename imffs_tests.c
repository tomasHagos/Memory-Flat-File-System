#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "a5_tests.h"
#include "a5_multimap.h"
#include "a5_imffs.h"
#include "a5_imffs_helpers.h" // to test the helper functions.




void testTypical()
{
    printf("\n........Testing typical cases.......\n");
    uint8_t chars[] = {'N','N','Y','N','Y'};

    //testing the free space function
    VERIFY_INT(2, find_free_space(chars,5));
    chars[2]  = 'N';
    VERIFY_INT(4, find_free_space(chars,5));
    chars[4] = 'N';
    VERIFY_INT(-1, find_free_space(chars,5));

    VERIFY_INT(1,get_block_number(0));
    VERIFY_INT(2,get_block_number(334));
    VERIFY_INT(1,get_block_number(256));
    VERIFY_INT(4,get_block_number(1000));
    VERIFY_INT(40,get_block_number(10000));
    VERIFY_INT(1,get_block_number(34));

    uint8_t free_space_arr[] = {'N','N','N'};
    free_space(free_space_arr,3,0);
    VERIFY_INT(1, free_space_arr[0] == 'Y');
    VERIFY_INT(1, free_space_arr[1] == 'Y');
    VERIFY_INT(1, free_space_arr[2] == 'Y');
    
    uint8_t free_space_arr2[] = {'N','N','N','N','N','N','N','N'};
    free_space(free_space_arr2,3,2);
    VERIFY_INT(1, free_space_arr2[0] == 'N');
    VERIFY_INT(1, free_space_arr2[1] == 'N');
    VERIFY_INT(1, free_space_arr2[2] == 'Y');
    VERIFY_INT(1, free_space_arr2[3] == 'Y');
    VERIFY_INT(1, free_space_arr2[4] == 'Y');
    VERIFY_INT(1, free_space_arr2[5] == 'N');
    VERIFY_INT(1, free_space_arr2[6] == 'N');
  
}

void test_edge_cases()
{
    printf("\n.......Testing Edge Cases........\n");
    uint8_t emptyarr[] = {};
    VERIFY_INT(-1,find_free_space(emptyarr,0));
    emptyarr[0] = 'Y';
    VERIFY_INT(0,find_free_space(emptyarr,1));

    VERIFY_INT(1,get_block_number(0));
    VERIFY_INT(3907,get_block_number(1000000));

    uint8_t free_space_arr2[] = {'Y','Y','Y'};
    free_space(free_space_arr2,0,3);
    VERIFY_INT(1, free_space_arr2[0] == 'Y');
    VERIFY_INT(1, free_space_arr2[1] == 'Y');
    VERIFY_INT(1, free_space_arr2[2] == 'Y');

    // Test free_space with an array containing 'N's
    uint8_t free_space_arr3[] = {'N', 'N', 'N'};
    free_space(free_space_arr3, 0, 3);
    VERIFY_INT(1, free_space_arr3[0] == 'N');
    VERIFY_INT(1, free_space_arr3[1] == 'N');
    VERIFY_INT(1, free_space_arr3[2] == 'N');
    
}

void test_invalid_cases()
{
    printf("\n.......Testing invalid Cases........\n");
    VERIFY_INT(-1,get_block_number(-1));
    VERIFY_INT(-1,find_free_space(NULL,13));
    uint8_t chars[] = {'N'};
    VERIFY_INT(-1,find_free_space(chars,-2));
    IMFFSPtr ptr;
    VERIFY_INT(1,imffs_create(-1,&ptr)== IMFFS_INVALID);
    VERIFY_INT(1,imffs_create(1,NULL) == IMFFS_INVALID);

    VERIFY_INT(1,imffs_save(ptr,NULL,"hello") == IMFFS_INVALID);
    VERIFY_INT(1,imffs_save(ptr,"hello",NULL) == IMFFS_INVALID);
    VERIFY_INT(1,imffs_save(NULL,"wow","hello") == IMFFS_INVALID);

    VERIFY_INT(1,imffs_load(ptr, "Hello", NULL) == IMFFS_INVALID);
    VERIFY_INT(1,imffs_load(ptr, NULL, "hello") == IMFFS_INVALID);
    VERIFY_INT(1,imffs_load(NULL, "Hello", "hello") == IMFFS_INVALID);

    VERIFY_INT(1,imffs_delete(ptr, NULL) == IMFFS_INVALID);
    VERIFY_INT(1,imffs_delete(NULL, "wo") == IMFFS_INVALID);

    VERIFY_INT(1,imffs_rename(ptr, NULL, "lol") == IMFFS_INVALID);
    VERIFY_INT(1,imffs_rename(ptr, "wow", NULL) == IMFFS_INVALID);
    VERIFY_INT(1,imffs_rename(NULL, "op", "lol") == IMFFS_INVALID);

    VERIFY_INT(1,imffs_dir(NULL) == IMFFS_INVALID);

    VERIFY_INT(1,imffs_fulldir(NULL) == IMFFS_INVALID);

    VERIFY_INT(1,imffs_defrag(NULL) == IMFFS_INVALID);

    VERIFY_INT(1,imffs_destroy(NULL) == IMFFS_INVALID);

}

void test_special_cases()
{
    printf("\n.......Testing special Cases........\n");
    uint8_t no_space[] = {'N','N','N'};
    VERIFY_INT(-1,find_free_space(no_space,3));

    free_space(no_space,0,3);
    VERIFY_INT(1,no_space[0] == 'N');
    VERIFY_INT(1,no_space[1] == 'N');
    VERIFY_INT(1,no_space[2] == 'N');

    uint8_t free_space_size_3[] = {'N', 'N', 'N'};
    free_space(free_space_size_3, 2, 0);
    VERIFY_INT(1, free_space_size_3[0] == 'Y');
    VERIFY_INT(1, free_space_size_3[1] == 'Y');
    VERIFY_INT(1, free_space_size_3[2] == 'N');

     // Test free_space with an array of size 1
    uint8_t free_space_size_1[] = {'N'};
    free_space(free_space_size_1, 0,0);
    VERIFY_INT(1, free_space_size_1[0] == 'N');
    free_space(free_space_size_1, 1,0);
    VERIFY_INT(1, free_space_size_1[0] == 'Y');

}


int main()
{
    testTypical();
    test_edge_cases();
    #ifdef NDEBUG
     test_invalid_cases();
    #endif
    test_special_cases();
    if (0 == Tests_Failed)
    {
        printf("\nAll %d tests passed.\n", Tests_Passed);
    } 
    else
    {
        printf("\nFAILED %d of %d tests.\n", Tests_Failed, Tests_Failed+Tests_Passed);
    }
  
    printf("\n*** Tests complete.\n");  
    return 0;
}