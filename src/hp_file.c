#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#include "bf.h"
#include "hp_file.h"
#include "record.h"

char int_to_string[100];


#define CALL_BF(call)         \
  {                           \
    BF_ErrorCode code = call; \
    if (code != BF_OK)        \
    {                         \
      BF_PrintError(code);    \
      return HP_ERROR;        \
    }                         \
  }


int HP_CreateFile(char *fileName) {

    BF_Block *block;
    BF_Block_Init(&block);
    int file_desc;
    int status;
    switch (BF_CreateFile(fileName)) {
        case BF_OK:
            status = 0;
            break;
        default:
            BF_PrintError(BF_CreateFile(fileName));
            return -1;
    }
    switch (BF_OpenFile(fileName, &file_desc)) {
        case BF_OK:
            status = 0;
            break;
        default:
            BF_PrintError(BF_OpenFile(fileName, &file_desc));
            return -1;
    }
    switch (BF_AllocateBlock(file_desc, block)) {
        case BF_OK:
            status = 0;
            break;
        default:
            BF_PrintError(BF_AllocateBlock(file_desc, block));
            return -1;
    }

    char* blocks_data = BF_Block_GetData(block);
    sprintf(int_to_string, "%ld", (BF_BLOCK_SIZE / (sizeof(Record)))); /* αποθηκευω το μεγιστο αριθμο μπλοκ που χωρανε στο μπλοκ των μεταδεδομενων του αρχειου σωρου*/
    strcpy(blocks_data, int_to_string);
    BF_Block_SetDirty(block);
    BF_UnpinBlock(block);

    switch (BF_CloseFile(file_desc)) {
        case BF_OK:
            status = 0;
            break;
        default:
            BF_PrintError(BF_CloseFile(file_desc));
            return -1;
    }
    return status;
}

HP_info* HP_OpenFile(char *fileName, int *file_desc){
    
    HP_info *hp_info = malloc(sizeof(HP_info));
    HP_block_info *hp_block_info = malloc(sizeof(HP_block_info));

    if (BF_OpenFile(fileName, file_desc) != BF_OK){
        BF_PrintError(BF_OpenFile(fileName, file_desc));
        return NULL;
    }

    BF_Block* block;
    BF_Block_Init(&block);
    int number_of_blocks;
    BF_GetBlockCounter(*file_desc, &number_of_blocks); //βρισκουμε τον αριθμο των συνολικων μπλοκ
    BF_GetBlock(*file_desc, number_of_blocks - 1, block); //number_of_blocks - 1 επειδη η αριθιμιση των μπλοκ αρχιζει απο το 0

    if (number_of_blocks == 1){ //αν αριθμος των μπλοκ 1 δεν υπαρχουν μπλοκ με εγγραφες μονο το μπλοκ με τα μεταδεδομενα
        /* αρχικοποιούμε τις δομές και τοποθετούμε τα μεταδεδομένα στο πρώτο μπλοκ*/
        hp_block_info->num_of_records = 0;
        BF_GetBlock(*file_desc, 0, block); //φερνουμε το μπλοκ των μεταδεδομενων
        char* blocks_data = BF_Block_GetData(block);
        hp_info->num_of_max_records = (int)(blocks_data[0]) - 48;
        hp_info->file_desc = *file_desc;
        sprintf(int_to_string, "%d", hp_info->file_desc); //μετατρεπω το αναγνωριστικο του αρχειου σωρου σε string
        strcat(blocks_data, int_to_string);//βαζω το αναγνωριστικο του αρχειου σωρου στη δευτερη θεση του μπλοκ μεταδεδομενων
        hp_info->num_of_blocks = number_of_blocks;
        sprintf(int_to_string, "%d", hp_info->num_of_blocks);
        strcat(blocks_data, int_to_string); //βαζω τον αριθμο των μπλοκς στη δευτερη θεση του μπλοκ των μεταδεδομενων 
        strcat(blocks_data, int_to_string); 
        sprintf(int_to_string, "%d", hp_block_info->num_of_records);
        /*στο τελος του μπλοκ μεταδεδομενων βαζω των αριθμο των αρχειων που υπαρχουν στο αρχειο σωρου*/
        memcpy(blocks_data + hp_info->num_of_max_records * sizeof(Record), int_to_string, strlen(int_to_string) + 1);
        BF_Block_SetDirty(block);
        BF_UnpinBlock(block);
    }
    hp_info->last_blocks_id = number_of_blocks - 1; //το id του τελευταιου μπλοκ του αρχειου σωρου ειναι ο αριθμος των μπλοκ - 1
    free(hp_block_info);
    return hp_info;
}


int HP_CloseFile(int file_desc,HP_info* hp_info ){

    if (BF_CloseFile(file_desc) != BF_OK){
        BF_PrintError(BF_CloseFile(file_desc));
        return -1;
    }
    free(hp_info);
    return 0;
}

int HP_InsertEntry(int file_desc, HP_info* hp_info, Record record){

    BF_Block *block;
    BF_Block_Init(&block);

    int number_of_blocks; //μεταβλητη για να αποθηκευσω τον αριθμο των μπλοκς του σωρου
    BF_GetBlockCounter(file_desc, &number_of_blocks);
    BF_GetBlock(file_desc, number_of_blocks - 1, block);//αριθμος μπλοκ -1 γιατι στη σωρο τα μπλοκ ειναι αριθμημενα απο το 0
    if (number_of_blocks == 1){ //αν ο αριθμος των μπλοκ στη σωρο ειναι 1 δλδ μονο το μπλοκ με τα μεταδεδομενα της σωρου
        number_of_blocks++;
        BF_UnpinBlock(block);//Unpin λογο του προηγουμενου BF_GetBlock
        BF_AllocateBlock(file_desc, block); //δεσμευω νεο μπλοκ στη σωρο
    }

    char* blocks_data = BF_Block_GetData(block);
    int size_of_block = hp_info->num_of_max_records;
    int blocks_num_of_records = *(blocks_data + (size_of_block * sizeof(Record)));/*βρισκω των αριθμο των μπλοκς πολλαπλασιαζωντας των αριθμο των εγγραφων
                                                                                                με το μεγεθος του struct record*/
    if (blocks_num_of_records >= size_of_block - 1){//ελεγχω αν δεν χωρανε οι εγγραφες στο μπλοκ
        BF_UnpinBlock(block);//unpin λογο του προηγουμενου BF_AllocateBlock
        BF_AllocateBlock(file_desc, block);
        BF_Block_SetDirty(block);
        BF_UnpinBlock(block);
        BF_GetBlock(file_desc, number_of_blocks - 1, block);
        BF_UnpinBlock(block);//κανω unpin το μπλοκ απο το προηγουμενο BF_GetBlock
    }

    int blocks_capacity = (BF_BLOCK_SIZE - (sizeof(hp_info->num_of_max_records) + ((*blocks_data) * sizeof(Record))));
    if (blocks_capacity < sizeof(Record)){ //ελεγχω αν υπαρχει αρκετος χωρος για προσθηκη εγγραφης στο μπλοκ
        BF_AllocateBlock(file_desc, block);
        BF_Block_SetDirty(block);
        BF_UnpinBlock(block);
        BF_GetBlock(file_desc, number_of_blocks - 1, block);//αριθμος μπλοκ -1 γιατι στη σωρο τα μπλοκ ειναι αριθμημενα απο το 0 
    }

    sprintf(int_to_string, "%d", record.id); // εισαγουμε εγγραφη
    memcpy(((blocks_data) + (sizeof(Record) * blocks_num_of_records)), int_to_string, strlen(int_to_string));//αρχη του μπλοκ + των αριθμο των προηγουμενων εγγραφων
    memcpy(((blocks_data) + (sizeof(Record) * blocks_num_of_records)) + strlen(int_to_string) + 1, record.name, sizeof(record.name));/*αρχη του μπλοκ + των αριθμο των προηγουμενων εγγραφων
                                                                                                                                     + το μεγεθος του μελους της δομης που εγγραφηκε στο μπλοκ*/
    memcpy(((blocks_data) + (sizeof(Record) * blocks_num_of_records)) + strlen(int_to_string) + 1 + sizeof(record.name) + 1, 
    &record.surname, sizeof(record.surname));//ιδια λογικη οπως και προηγουμενος για το πως περναμε σημεια του μπλοκ για την επομενη εγγραφη μελους δομης
    memcpy(((blocks_data) + (sizeof(Record) * blocks_num_of_records)) + strlen(int_to_string) + 1 + sizeof(record.name) + 1 + sizeof(record.surname) + 1, 
    &record.city, sizeof(record.city));//ιδια λογικη οπως και προηγουμενος για το πως περναμε σημεια του μπλοκ για την επομενη εγγραφη μελους δομης

    *(blocks_data + size_of_block * sizeof(Record)) = (*(blocks_data + size_of_block * sizeof(Record))) + 1;
    int blocks_metadata = *(blocks_data + size_of_block * sizeof(Record));
    if (blocks_metadata){
        BF_Block_SetDirty(block);
        BF_UnpinBlock(block);//Unpin λογο του προηγουμενου BF_GetBlock
        BF_Block_Destroy(&block);
        return number_of_blocks;//επιστρεφω των αριθμο του μπλοκ που εγινε η εγγραφη δλδ το τελευταιο 
    }
    else return -1;
}

int HP_GetAllEntries(int file_desc, HP_info* hp_info, int id) {
    int len_counter, num_blocks, num_records, blocknum;
    BF_Block * block;
    char* blocks_data;
    char* blocks_data2;
    char* block_check;
    char buffer[50];
    Record record2;
    BF_Block_Init( & block);
    BF_GetBlockCounter(file_desc, & num_blocks);
    for (int i = 1; i < num_blocks; i++){ //ελεγχω ολα τα μπλοκ

        BF_GetBlock(hp_info -> file_desc, i, block);
        for (int j = 0; j < hp_info -> num_of_max_records; j++){
            BF_GetBlock(hp_info -> file_desc, i, block);
            blocks_data = BF_Block_GetData(block);
            /* βρισκω και συγκρινω τα ids των μπλοκ*/
            blocks_data = (blocks_data + j * sizeof(Record));
            blocks_data2 = blocks_data;
            block_check = blocks_data;
            sprintf(buffer, "%d", id);
            len_counter = strlen(block_check);
            while (1){
                block_check = strstr(block_check, buffer); //ελεγχω αν το check_block ειναι substring του Buffer
                if (block_check == NULL){
            
                    BF_UnpinBlock(block);
                    BF_GetBlockCounter(file_desc, &num_blocks);
                    break;
                } 
                else if ((block_check == blocks_data2) || !isalnum((unsigned char) block_check[-1])){
            
                    block_check += strlen(buffer);
                    if (!isalnum((unsigned char) * block_check)){
                        printf("\nRECORD FOUND AT BLOCK NUMBER: %d\n", i);
                        printf("ID: %s\n", blocks_data);
                        printf("NAME: %s\n", ((blocks_data) + sizeof(char) + (len_counter)));
                        printf("SURNAME: %s\n", ((blocks_data) + sizeof(char) + len_counter + sizeof(record2.name) + 1));
                        printf("CITY: %s\n", ((blocks_data) + sizeof(char) + len_counter + sizeof(record2.name) + 1 + sizeof(record2.surname) + 1));
                        BF_UnpinBlock(block);
                        BF_Block_Destroy( & block);

                        return i; //γυρναω των αριθμο των μπλοκ που εχουν ελεγχθει
                    }
                }
            block_check += 1;
            }
        }
    }
    BF_UnpinBlock(block);
    BF_Block_Destroy( & block);
    printf("\nID  %d NOT FOUND\n", id);
    return -1;
}