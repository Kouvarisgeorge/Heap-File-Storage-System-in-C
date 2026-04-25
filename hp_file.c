#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf.h"
#include "hp_file.h"
#include "record.h"
#define HP_ERROR -1

#define CALL_BF(call)       \
{                           \
  BF_ErrorCode code = call; \
  if (code != BF_OK) {      \
    BF_PrintError(code);    \
    return HP_ERROR;        \
  }                         \
}

int HP_CreateFile(char *fileName) {
    int fd1;                                        // File descriptor for the opened file
    BF_Block *block;
    void *data;                                     // Pointer to the data within the BF block
    HP_info hpinfo;
    BF_Block_Init(&block);

    CALL_BF(BF_CreateFile(fileName));
    CALL_BF(BF_OpenFile(fileName, &fd1));
    CALL_BF(BF_AllocateBlock(fd1, block));         // Allocate a block for storing metadata
    data = BF_Block_GetData(block);                // Get a pointer to the block's data
    hpinfo.last_block_id = 0;                      // Initialize the last block id
    hpinfo.records_per_block = BF_BLOCK_SIZE / sizeof(Record);  // Calculate records per block
    memcpy(data, &hpinfo, sizeof(HP_info));        // Copy the HP_info structure to the block
    BF_Block_SetDirty(block);
    CALL_BF(BF_UnpinBlock(block));
    BF_Block_Destroy(&block);
    CALL_BF(BF_CloseFile(fd1));

    return 0;                                      // Return 0 for successful file creation
}


HP_info* HP_OpenFile(char *fileName, int *file_desc) {
    // Allocate memory for the HP_info structure
    HP_info *hp_info = (HP_info *)malloc(sizeof(HP_info));
    if (hp_info == NULL) {
        // Handle memory allocation error
        return NULL;
    }

    BF_Block *block;
    void *data;
    BF_Block_Init(&block);

    BF_OpenFile(fileName, file_desc);

    // Get the metadata block (block 0) of the HP file
    if (BF_GetBlock(*file_desc, 0, block) != BF_OK) {
        // Handle error
        BF_Block_Destroy(&block);
        free(hp_info);                              // Free the allocated memory in case of an error
        return NULL;
    }

    data = BF_Block_GetData(block);
    memcpy(hp_info, data, sizeof(HP_info));         // Copy HP_info from the block to the allocated structure
    BF_Block_Destroy(&block);

    return hp_info;                                 // Return the HP_info
}



int HP_CloseFile(int file_desc, HP_info* hp_info) {
    free(hp_info);                     // Free the memory

    BF_Block *block;
    BF_Block_Init(&block);

    BF_GetBlock(file_desc, 0, block);  // Get the metadata block (block 0) for unpinning
    CALL_BF(BF_UnpinBlock(block));     // Unpin the metadata block

    BF_Block_Destroy(&block);

    CALL_BF(BF_CloseFile(file_desc));  // Close the HP file

    return 0;                          // Return 0 for success
}

int HP_InsertEntry(int file_desc, HP_info* hp_info, Record record) {
    BF_Block* block;
    void* data;
    int blockId;
    int block_count;

    if (BF_GetBlockCounter(file_desc, &block_count) != BF_OK) {
        printf("Failed to get block count.\n");
        return -1;
    }

    for (blockId = 1; blockId < block_count; blockId++) {
        //printf("Checking block %d\n", blockId);
        BF_Block_Init(&block);

        if (BF_GetBlock(file_desc, blockId, block) != BF_OK) {
            //printf("Failed to get block %d\n", blockId);
            BF_Block_Destroy(&block);
            return -1;
        }
        HP_block_info* block_info = (HP_block_info*)BF_Block_GetData(block);
        int rec_per_block = block_info->number_of_records;
        //printf("Records in block %d: %d\n", blockId, rec_per_block);
        int rblock = hp_info->records_per_block;                        // Check if there is space in the block for the new record
        if (rec_per_block < rblock) {
            //printf("Inserting into block %d\n", blockId);

            Record* records = (Record*)((char*)block_info + sizeof(HP_block_info)); // There is space in this block, insert the record
            records[rec_per_block] = record;
            block_info->number_of_records++;
            BF_Block_SetDirty(block);
            CALL_BF(BF_UnpinBlock(block));
            //printf("BLOCK_ID: %d\n", blockId);
            return blockId;
        }
        BF_Block_Destroy(&block);
    }

    // If all blocks are full, create a new block and insert the record
    BF_Block_Init(&block);
    //printf("Allocating a new block\n");
    if(BF_AllocateBlock(file_desc, block) != 0){
        BF_Block_Destroy(&block);
        //printf("BF memory is full. Unable to allocate a new block.\n");
        return -1;
    }
    data = BF_Block_GetData(block);

    HP_block_info* block_info = (HP_block_info*)data;
    block_info->next_block_pointer = NULL; // Assuming there is no next block initially
    block_info->number_of_records = 1;

    // Insert the record into the new block
    Record* records = (Record*)((char*)block_info + sizeof(HP_block_info));
    records[0] = record;

    // Mark the block as dirty and unpin it
    BF_Block_SetDirty(block);
    CALL_BF(BF_UnpinBlock(block));
    BF_Block_Destroy(&block);

    //printf("BLOCK_COUNT: %d\n", block_count);
    return block_count;

}


int HP_GetAllEntries(int file_desc, HP_info* hp_info, int value) {
    int blockId;
    int block_count;

    // Get the total number of blocks in the file
    if (BF_GetBlockCounter(file_desc, &block_count) != BF_OK) {
        return -1; // Error handling: Unable to get the block count
    }

    int matching_records = 0;

    // Iterate through all the blocks in the file
    for (blockId = 1; blockId < block_count; blockId++) {
        BF_Block* block;
        BF_Block_Init(&block);

        // Get the block with ID 'blockId'
        if (BF_GetBlock(file_desc, blockId, block) != BF_OK) {
            return -1; // Error handling: Unable to get the block
        }

        // Extract information from the block
        HP_block_info* block_info = (HP_block_info*)BF_Block_GetData(block);
        int records_per_block = block_info->number_of_records;

        // Access the records within the block
        Record* records = (Record*)((char*)block_info + sizeof(HP_block_info));

        // Iterate through all records in the block
        for (int i = 0; i < records_per_block; i++) {
            if (records[i].id == value) {
                // Print the matching record
                printRecord(records[i]);
                matching_records++;
            }
        }

        // Unpin the block when done
        BF_UnpinBlock(block);
        BF_Block_Destroy(&block);
    }

    // If no matching records were found, inform the user
    if (matching_records == 0) {
        printf("No records found with ID: %d\n", value);
    }

    return matching_records;
}
