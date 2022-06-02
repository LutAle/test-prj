#include <stdio.h>   
 
 #include <string.h>
 #include "test/test-dataset/test-data.h"
 #include <stdbool.h>

//#define DBG

#define default_file_name "./test.txt"        // default data file with hex strings
#define CHR_CNT 0x1000                      // string bufer size 
#define B_CNT   0x1000                      // card data, in buferr

FILE * file_test_data;               
void * cdata = NULL;                // card data location
size_t cdata_sz = 0;                // card data size
char name[] = default_file_name;    // file name with test ANSI-HEX string
const char * user_data = NULL;            // argv command line string bind pointer

// optional vars 
long jobn = 0;                       // Num card job runs count 


void job (  )
{
    jobn++;
    /// Main processing
    

    /// reset to default state
    free(cdata);
    cdata = NULL;
    cdata_sz = 0;
}
/// push converted data to storege
void  push_cdata(char * source, size_t size)
    {
    void * cdataold = cdata ? cdata : NULL;
    // get memory
    cdata = malloc(size+cdata_sz);
    if (!cdata) // get null location 
        {
            perror("Missing memory");
            exit(1);
        }
    // copy old data if it present
    if (cdataold && cdata_sz>0 ) memmove(cdata,cdataold,cdata_sz);
    // store new data, assume cdata_sz index old data
    if (size>0) memmove((char *) cdata + cdata_sz ,source,size);  
    // copy done, go next
    cdata_sz +=size;    // rewind cdata memory size
    free(cdataold);
    #ifdef DBG
    printf ("%i\n",cdata_sz);
    #endif
    }

int main(int argc, char const *argv[])
{

user_data =  argc>1 ? argv[argc-1] : NULL;                 // get last cmd arg as file name

// read test data from file as last value string command
file_test_data = user_data ? fopen(user_data ,"r") : NULL;          // use user specifed test data file
file_test_data = file_test_data ? file_test_data : fopen(name,"r"); // use default test data file, if it exsist 
                                                                    // and user not allocate data file
if (file_test_data) cdata_sz = get_test_data();                        
else {                                                     // use cmd parametr as test data fallback                        
     file_test_data = tmpfile();
     fprintf(file_test_data,"%s",user_data);   
     rewind(file_test_data);
     cdata_sz = get_test_data();   
    }
if (!cdata_sz)  // get card data from terminal, if test data missing 
    {
        cdata = malloc(B_CNT); if (!cdata) exit(1);
        //  SendAPDU (cdata,B_CN,cdata,B_CN) ;
        //  ... etc..
    }

while (cdata_sz) 
    {
        job(cdata,cdata_sz);
        cdata_sz = get_test_data();
    }
fclose(file_test_data);     
printf("argv name: %s",user_data);
printf("\nJob runs %d time.",jobn);
exit(0); 

}

///Return 0 if can't fetch test data or length bind dataset 
int get_test_data()
{   
    unsigned char   strbuf[CHR_CNT];  // setup bufer
    unsigned short  index = 0;           // bufer index
    size_t data_cnt = 0;                 // hold all collected card data
    size_t str_sz = 0;                   // size for input string
    FILE *tfile = tmpfile();             // temp string bufer file   
    
    // load string from file in tmp file bufer, if string length present
    // until get EOF or new line char
    while (fgets(strbuf, CHR_CNT,file_test_data))
    { 
       str_sz = strlen(strbuf);
       fprintf(tfile,"%s", strbuf);    // pull out string to temp file
       if (feof(file_test_data) || strbuf[str_sz-1] == '\n') break; // single line parsing 
    }
    // start conversion processing  
    rewind(tfile); //start processing text file 
        while (fscanf(tfile,"%2hhx",&(strbuf[index++]))>0) // cactch next byte from text sream, if sucess
            {                                              // to source loading location              
            data_cnt++;
            // flush data bufer to storage location
            if (index==CHR_CNT+1) 
                {
                 push_cdata(strbuf,(size_t) index);
                 index = 0;
                }
            }
        if (data_cnt && index) // load result to cdata
            {
                push_cdata(strbuf,(size_t) index-1);
                index = 0;  // optional
            }
    fclose(tfile);
    return data_cnt; // byte loaded     
    }

// emulate read  card reader
int  SendAPDU (unsigned char* pTxBuf, unsigned short txBufSize, unsigned char* pRxBuf, unsigned short* rxBufLen)
{   
    return 0;
}

