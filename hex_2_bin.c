#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>


typedef struct {
    unsigned int file_size;
    unsigned int start_addr;
    unsigned int end_addr;
    FILE *file_in, *file_out;
} update_file_info, * p_update_file_info;
update_file_info g_update_ecu_file_info;

#define MAX_LINE_SIZE 1024
#define MAX_EXTENSION_SIZE 16
#define MAX_FILE_NAME_SIZE 260
typedef char filetype[MAX_FILE_NAME_SIZE];

#define NO_ADDRESS_TYPE_SELECTED    0
#define LINEAR_ADDRESS              1
#define SEGMENTED_ADDRESS           2

void get_line(char* str, FILE *in)
{
    char *result;
    result = fgets(str, MAX_LINE_SIZE, in);
    if ((NULL == result) && !feof (in)) 
        printf("Error occurred while reading from file\n");
}

void put_extension(char *file_name, char *extension)
{
    /* location of period in file name */
    char *Period;       
    int ret = -1;

    /*find '.': the last postion*/
    if (NULL != (Period = strrchr(file_name,'.')))
        *(Period) = '\0';

    if (strcmp(extension, Period + 1) == 0)
        ret = 0;

    strcat(file_name, ".");
    strcat(file_name, extension);
}

void hex_to_bin(char *path)
{
    unsigned int i;
    int ret;
    /*记录行数*/
    unsigned int record_line = 0;   

    unsigned char *memory_block = NULL;
    unsigned int max_data_length;

    /*line content*/
    unsigned int data_length;
    unsigned int address;
    unsigned int type;
    char *p_data = NULL;
    unsigned int check_sum;

    unsigned int phys_addr;
    unsigned int seg_lin_select = NO_ADDRESS_TYPE_SELECTED;
    /*线性基地址*/
    unsigned int upper_address;
    unsigned int segment;
    unsigned int temp2;

    /* cmd-line parameter # */
    char *p;

    /*line inputted from file*/
    char *p_line = NULL;

    char extension[MAX_EXTENSION_SIZE] = {0};

    filetype filename;

    g_update_ecu_file_info.start_addr = (unsigned int)(-1);
    g_update_ecu_file_info.end_addr = 0;

    p_line = (char *)malloc(MAX_LINE_SIZE * sizeof(char));
    if (NULL == p_line) 
        printf("malloc line buffer error. \n");

    p_data = (char *)malloc(MAX_LINE_SIZE * sizeof(char));
    if (NULL == p_data) 
        printf("malloc data buffer error. \n");


    /* default is for binary file extension */
    strncpy(extension, "bin", sizeof(extension));

     /* get filename */
    if (strlen(path) < MAX_FILE_NAME_SIZE) {
        strcpy(filename, path);
    } else {
        printf("filename length exceeds %d characters.\n", MAX_FILE_NAME_SIZE);
    }

    /* Just a normal file name */
    g_update_ecu_file_info.file_in = fopen(filename, "r");

    put_extension(filename, extension);
    g_update_ecu_file_info.file_out = fopen(filename,"wb");


    /**************process 1: deal with addess infomation*/
    /*get highest and lowest addresses so that we can allocate the right size*/
    do {
        /* Read a line from input file. */
        get_line(p_line, g_update_ecu_file_info.file_in);
        record_line++;

        /* Remove carriage return/line feed at the end of line. */
        i = strlen(p_line); 
        if (--i != 0) {
            if (p_line[i] == '\n') 
                p_line[i] = '\0';

            ret = sscanf(p_line, ":%2x%4x%2x%s",&data_length, &address, &type, p_data);
            if (4 != ret) 
                printf("Error in line %d of hex file\n", record_line);

            /*行内容：data段的起始位置*/
            p = (char *)p_data;

            /* If we're reading the last record, ignore it. */
            switch (type) {
                /* Data record */
                case 0:
                    if (0 == data_length)
                        break;

                    if (SEGMENTED_ADDRESS == seg_lin_select) {
                        phys_addr = (segment << 4) + address;
                    } else {
                    /* LINEAR_ADDRESS or NO_ADDRESS_TYPE_SELECTED
                        Upper_Address = 0 as specified in the Intel spec. until an extended address
                        record is read. */
                        phys_addr = ((upper_address << 16) + address);
                    }

                    if (g_update_ecu_file_info.start_addr > phys_addr)
                        g_update_ecu_file_info.start_addr = phys_addr;

                    phys_addr = phys_addr + data_length - 1;
                    if (g_update_ecu_file_info.end_addr < phys_addr) 
                        g_update_ecu_file_info.end_addr = phys_addr;

                    break;

                case 1:
//                  printf("End of File record\n");
//                  printf("extended linear address record %d\n", record_line);
                    break;

                case 2:
                    /* First extended segment address record ? */
                    if (NO_ADDRESS_TYPE_SELECTED == seg_lin_select) {
                        seg_lin_select = SEGMENTED_ADDRESS;
                    }

                    if (SEGMENTED_ADDRESS == seg_lin_select) {
                        ret = sscanf(p, "%4x%2x",&segment, &check_sum);
                        if (2 != ret) 
                            printf("Error in line %d of hex file\n", record_line);

//                      printf("Extended Segment Address record: %04X\n", segment);

                        /* Update the current address. */
                        phys_addr = (segment << 4);
                    } else {
                        printf("Ignored extended linear address record %d\n", record_line);
                    }
//                  printf("extended linear address record %d\n", record_line);
                    break;

                case 3:
//                  printf("Start Segment Address record: ignored\n");
                    break;

                case 4:
                    /* First extended linear address record ? */
                    if (NO_ADDRESS_TYPE_SELECTED == seg_lin_select)
                        seg_lin_select = LINEAR_ADDRESS;

                    if (LINEAR_ADDRESS == seg_lin_select) {
                        ret = sscanf(p, "%4x%2x", &upper_address, &check_sum);
                        if (2 != ret) 
                            printf("Error in line %d of hex file\n", record_line);

//                      printf("Extended Linear Address record: %04X\n", address);

                        /* Update the current address. */
                        phys_addr = (upper_address << 16);

//                      printf("Physical Address: %08X\n",phys_addr);
                    } else {
                        printf("Ignored extended segment address record %d\n", record_line);
                    }
//                  printf("extended segment address record %d\n", record_line);
                    break;

                case 5:
//                  printf("Start Linear Address record: ignored\n");
                    break;

                default:
                    break;
            }
        }
    } while (!feof(g_update_ecu_file_info.file_in));

    /*calc the data size*/
    max_data_length = g_update_ecu_file_info.end_addr - g_update_ecu_file_info.start_addr + 1;
    g_update_ecu_file_info.file_size = max_data_length;

    printf("start addr:0x%08x\n", g_update_ecu_file_info.start_addr);
    printf("end addr:0x%08x\n", g_update_ecu_file_info.end_addr);
    printf("data size: %d Bytes\n", g_update_ecu_file_info.file_size);

    memory_block = (unsigned char *)malloc(max_data_length * sizeof(unsigned char));
    if (NULL == memory_block) 
        printf("malloc memory_block buffer error. \n");

    memset (memory_block, 0xFF, max_data_length);



    /*********process 2: deal with data infomation*/
    segment = 0;
    upper_address = 0;
    record_line = 0;
    /*init file inner pointer*/
    rewind(g_update_ecu_file_info.file_in);

    /* Read the file & process the lines. */
    /* repeat until EOF(Filin) */
    do {
         /* Read a line from input file. */
        get_line(p_line, g_update_ecu_file_info.file_in);
        record_line++;

        i = strlen(p_line);
        if (--i != 0) {
            if (p_line[i] == '\n') 
                p_line[i] = '\0';

            ret = sscanf(p_line, ":%2x%4x%2x%s",&data_length, &address, &type, p_data);
            if (4 != ret) 
                printf("Error in line %d of hex file\n", record_line);

            check_sum = data_length + (address >> 8) + (address & 0xFF) + type;

            /*行内容：data段的起始位置*/
            p = (char *)p_data;

            /* If we're reading the last record, ignore it. */
            switch (type) {
             /* Data record */
            case 0:
                if (0 == data_length) {
                    printf("0 byte length Data record ignored\n");
                    break;
                }

                if (SEGMENTED_ADDRESS == seg_lin_select) {
                    phys_addr = (segment << 4) + address;
                } else {
                    /* LINEAR_ADDRESS or NO_ADDRESS_TYPE_SELECTED
                        Upper_Address = 0 as specified in the Intel spec. until an extended address
                        record is read. */
                    phys_addr = ((upper_address << 16) + address);
                }

                /* Check that the physical address stays in the buffer's range. */
                if ((phys_addr >= g_update_ecu_file_info.start_addr) && 
                    (phys_addr <= g_update_ecu_file_info.end_addr)) {

                    /* The memory block begins at Lowest_Address */
                    phys_addr -= g_update_ecu_file_info.start_addr;

                    /* Check that the physical address stays in the buffer's range. */
                    if (phys_addr < g_update_ecu_file_info.file_size)
                        /*save data*/
                        do {
                            ret = sscanf(p, "%2x", &temp2);
                            if (1 != ret) 
                                printf("ReadDataBytes: error in line %d of hex file\n", record_line);
                            p += 2;

                            memory_block[phys_addr++] = temp2;

                            check_sum = (check_sum + temp2) & 0xFF;
                            } while (--data_length != 0);
                    else
                        printf("Overlapped record detected\n");

                    /* Read the Checksum value. */
                    ret = sscanf(p, "%2x",&temp2);
                    if (1 != ret) 
                        printf("Error in line %d of hex file\n", record_line);

                    /* Verify Checksum value. */
                    check_sum = (check_sum + temp2) & 0xFF;
                    if (0 != check_sum)
                        printf("Checksum error in record %d: should be %02X\n", record_line, (256 - check_sum) & 0xFF);
                } else {
                    if (SEGMENTED_ADDRESS == seg_lin_select)
                        printf("Data record skipped at %4X:%4X\n", segment, address);
                    else
                        printf("Data record skipped at %8X\n", phys_addr);
                }
                break;

            /* End of file record */
            case 1:
                /* Simply ignore checksum errors in this line. */
                break;

            /* Extended segment address record */
            case 2:
                /* First_Word contains the offset. It's supposed to be 0000 so
                   we ignore it. */

               /* First extended segment address record ? */
                if (NO_ADDRESS_TYPE_SELECTED == seg_lin_select) {
                    seg_lin_select = SEGMENTED_ADDRESS;
                }

                /* Then ignore subsequent extended linear address records */
                if (SEGMENTED_ADDRESS == seg_lin_select) {
                    ret = sscanf(p, "%4x%2x",&segment, &temp2);
                    if (2 != ret) 
                        printf("Error in line %d of hex file\n", record_line);

//                      printf("Extended Segment Address record: %04X\n", segment);

                    /* Update the current address. */
                    phys_addr = (segment << 4);

                    /* Verify Checksum value. */
                    check_sum = (check_sum + (segment >> 8) + 
                                (segment & 0xFF) + temp2) & 0xFF;
                    if (0 != check_sum)
                        printf("Checksum error in record %d: should be %02X\n", record_line, 
                        (256 - check_sum) & 0xFF);
                }
                break;


            /* Start segment address record */
            case 3:
                /* Nothing to be done since it's for specifying the starting address for
                   execution of the binary code */
                break;

            /* Extended linear address record */
            case 4:
                /* First extended linear address record ? */
                if (NO_ADDRESS_TYPE_SELECTED == seg_lin_select)
                    seg_lin_select = LINEAR_ADDRESS;

                if (LINEAR_ADDRESS == seg_lin_select) {
                    ret = sscanf(p, "%4x%2x", &upper_address, &temp2);
                    if (2 != ret) 
                        printf("Error in line %d of hex file\n", record_line);

//                      printf("Extended Linear Address record: %04X\n", address);

                    /* Update the current address. */
                    phys_addr = (upper_address << 16);

                    /* Verify Checksum value. */
                    check_sum = (check_sum + (upper_address >> 8) + 
                                (upper_address & 0xFF) + temp2) & 0xFF;
                    if (0 != check_sum)
                        printf("Checksum error in record %d: should be %02X\n", record_line, 
                        (256 - check_sum) & 0xFF);
                }
                break;

            /* Start linear address record */
            case 5:
                /* Nothing to be done since it's for specifying the starting address for
                   execution of the binary code */
                break;

            default:
                printf("Unknown record type\n");
                break;
            }
        }


    }while (!feof(g_update_ecu_file_info.file_in));

    /*write binary file*/
    fwrite (memory_block, g_update_ecu_file_info.file_size, 1, g_update_ecu_file_info.file_out);
    fflush(g_update_ecu_file_info.file_out);

    fclose(g_update_ecu_file_info.file_in);
    fclose(g_update_ecu_file_info.file_out);

    free(memory_block);
    memory_block = NULL;

    free(p_line);
    p_line = NULL;

    free(p_data);
    p_data = NULL;
}

int main(int argc, char *argv[])
{
    hex_to_bin(argv[argc-1]);
    return 0;
}



