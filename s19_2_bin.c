#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_EXTENSION_SIZE 16
#define MAX_LINE_SIZE 1024
#define MAX_FILE_NAME_SIZE 260
typedef char filetype[MAX_FILE_NAME_SIZE];

filetype    Filename;  
FILE *fin,*fout;
int filemode;

unsigned char Ascii2Int(unsigned char tmp0, unsigned char tmp1)
{
    unsigned char s[2];
    unsigned char value;
    s[0] = 0;
    s[1] = 0;

    if (tmp0 >= '0' && tmp0 <= '9')
        s[0] = tmp0 - '0';
    else if (tmp0 >= 'a' && tmp0 <='f')
        s[0] = tmp0 - 'a' + 0xA;
    else if (tmp0 >= 'A' && tmp0 <= 'F') 
        s[0] = tmp0 - 0x61 + 0XA;

    if (tmp1 >= '0' && tmp1 <= '9')
        s[1] = tmp1 - '0';
    else if (tmp1 >= 'a' && tmp1 <= 'f')
        s[1] = tmp1 - 'a' + 0xA;
    else if (tmp1 >= 'A' && tmp1 <= 'F')
        s[1] = tmp1 - 'A' + 0xA;

    value = s[0]<<4;
    value = value + s[1];

    return value;
}

void PutExtension(char *Flnm, char *Extension)
{
    char *Period;        /* location of period in file name */
    char Samename = -1;

    /* This assumes DOS like file names */
    /* Don't use strchr(): consider the following filename:
     ../my.dir/file.hex
    */
    if ((Period = strrchr(Flnm,'.')) != NULL)
        *(Period) = '\0';

    if (strcmp(Extension, Period + 1) == 0)
        Samename = 0;

    strcat(Flnm,".");
    strcat(Flnm, Extension);
    if (0 == Samename) {
        printf ("Input and output filenames (%s) are the same.", Flnm);
    }
}


int main(int argc, char *argv[])
{
    unsigned char tmp[2];
    char buf[20];
    unsigned char value = 0;
    unsigned char chksum = 0;
    unsigned char num = 0;
    unsigned char func = 0xff;
    int addr = 0;
    int size = 0;

    /* line inputted from file */
    char Line[MAX_LINE_SIZE];
    char Extension[MAX_EXTENSION_SIZE]; 
    strcpy(Extension, "bin");

    /*读取文件名*/
    strncpy(Filename, argv[argc -1], sizeof(Filename));

    /*打开或创建两个文件*/
    fin = fopen(Filename, "r");
    PutExtension(Filename, Extension);
    fout = fopen(Filename,"wb");

    while(!feof(fin))
    {
        /*读取一行数据，以's'开始*/
        while (1) {
            fread(tmp, sizeof(char), 1, fin);
            /*寻找行首*/
            if (tmp[0]=='S'||tmp[0]=='s')
                break;

            if (feof(fin)) {
                fclose(fin);
                fclose(fout);
                printf("file convert ok!\n");
                return;
            }
        }

        chksum = 0;
        addr = 0;

        /*读取's'后面的字符: type*/
        fread(tmp, sizeof(char), 1, fin);
        func = tmp[0];

        /*获取数据数量，两个字符: count*/
        fread(tmp, sizeof(char), 2, fin);
        num = Ascii2Int(tmp[0], tmp[1]);
        chksum += num;

        /*类型type, 处理每一行的地址address数据*/
        switch (func) {
        case '0':
                fread(tmp, sizeof(char), 2, fin);
                value = Ascii2Int(tmp[0], tmp[1]);
                chksum += value;
                addr += value;
                addr = addr<<8;

                fread(tmp, sizeof(char), 2, fin);
                value = Ascii2Int(tmp[0], tmp[1]);
                chksum += value;
                addr += value;
                num -= 2;
                break;

         case '1':
                fread(tmp, sizeof(char), 2, fin);
                value = Ascii2Int(tmp[0], tmp[1]);
                chksum += value;
                addr += value;
                addr=addr<<8;

                fread(tmp, sizeof(char), 2, fin);
                value = Ascii2Int(tmp[0], tmp[1]);
                chksum += value;
                addr += value;
                num -= 2;
                break;

         case '2':
                fread(tmp, sizeof(char), 2, fin);
                value = Ascii2Int(tmp[0], tmp[1]);
                chksum += value;
                addr += value;
                addr = addr<<8;

                fread(tmp, sizeof(char), 2, fin);
                value = Ascii2Int(tmp[0], tmp[1]);
                chksum += value;
                addr += value;
                addr = addr<<8;

                fread(tmp,sizeof(char), 2, fin);
                value = Ascii2Int(tmp[0], tmp[1]);
                chksum += value;
                addr += value;
                num -= 3;
                break;

        case '3':
                fread(tmp, sizeof(char), 2, fin);
                value=Ascii2Int(tmp[0], tmp[1]);
                chksum += value;
                addr += value;
                addr = addr<<8;

                fread(tmp, sizeof(char), 2, fin);
                value = Ascii2Int(tmp[0], tmp[1]);
                chksum += value;
                addr += value;
                addr = addr<<8;

                fread(tmp, sizeof(char), 2, fin);
                value = Ascii2Int(tmp[0], tmp[1]);
                chksum += value;
                addr += value;
                addr = addr<<8;

                fread(tmp, sizeof(char), 2, fin);
                value = Ascii2Int(tmp[0], tmp[1]);
                chksum += value;
                addr += value;
                num -= 4;
                break;

        case '5':
                break;

        case '7':
                break;

        case '8':
                break;

        case '9':
                break;

        default :
                break;
        }

#if 0
        switch (func) {
        case '0':
           printf("\n module version:\n");
           break;

        case '1':
           //Memo1->Lines->Add(" \n地址 ");
           //sprintf(buf," 0x%x ",addr);
           //Memo1->Text=Memo1->Text+buf;
           //Memo1->Text=Memo1->Text+"-数据信息:";
           break;

        case '2':
           //Memo1->Lines->Add(" \n地址 ");
           //sprintf(buf," 0x%x ",addr);
           //Memo1->Text=Memo1->Text+buf;
           //Memo1->Text=Memo1->Text+"-数据信息:";
           break;

        case '3':
           //Memo1->Lines->Add(" \n地址 ");
           //sprintf(buf," 0x%x ",addr);
           //Memo1->Text=Memo1->Text+buf;
           //Memo1->Text=Memo1->Text+"-数据信息:";
           break;

        case '5':
           printf("\n data line count:\n");
           break;

        case '7':
           printf(" \n program start address:\n");
           break;

        case '8':
           printf(" \n program start address:\n");
           break;

        case '9':
           printf(" \n program start address:\n");
           break;
        }
#endif

        /*循环读取一行数据， 写入.bin文件中*/
        while (num > 1) {
            fread(tmp, sizeof(char), 2, fin);
            num--;
            value = Ascii2Int(tmp[0], tmp[1]);

            switch (func) {
                case '0':
//                  sprintf(buf," 0x%2x ",value);
//                  printf("s0 text: %s\n", buf);
                    break;

                case '1':
//                  sprintf(buf," 0x%2x ",value);
//                  printf("s0 text: %s\n", buf);
                    fwrite(&value, sizeof(char), 1, fout);
                    size++;
                    break;

                case '2':
//                  sprintf(buf," 0x%2x ",value);
//                  printf("s0 text: %s\n", buf);
                    fwrite(&value, sizeof(char), 1, fout);
                    size++;
                    break;

                case '3':
//                  sprintf(buf," 0x%2x ",value);
//                  printf("s0 text: %s\n", buf);
                    fwrite(&value, sizeof(char), 1, fout);
                    size++;
                    break;

                case '5':
//                  sprintf(buf," 0x%2x ",value);
//                  printf("s5 text: %s\n", buf);
                    break;

                case '7':
//                  sprintf(buf," 0x%2x ",value);
//                  printf("s7 text: %s\n", buf);
                    break;

                case '8':
//                  sprintf(buf," 0x%2x ",value);
//                  printf("s8 text: %s\n", buf);
                    break;

                case '9':
//                  sprintf(buf," 0x%2x ",value);
//                  printf("s9 text: %s\n", buf);
                    break;
            }

            chksum += value;
        }
        /*获取check sum*/
        fread(tmp, sizeof(char), 2, fin);
        value = Ascii2Int(tmp[0], tmp[1]);
        if(value != 0xff - chksum)
            printf("crc sum error! text_value:0x%x, clc_chksum:0x%x\n", value, (0xff - chksum));
        else
            printf("crc sum ok! text_value:0x%x, clc_chksum:0x%x\n", value, (0xff - chksum));
   }
}