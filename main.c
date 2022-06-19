 #include <stdio.h>   
 #include <string.h>
 #include <limits.h>
 #include <stdlib.h>
 #include "EMV 4.3 Book 3 Application Specification.h"

//#define DBG

/// Two byte tag value 
# define DB_TG(bytes)  ((bytes[0]<<8)+bytes[1]) 

#define default_file_name "./test.txt"      // default data file with hex strings
#define CHR_CNT 0x1000                      // string bufer size 
#define B_CNT   0x1000                      // card data, in buferr

FILE * file_test_data;               
void * cdata = NULL;                // card data location
size_t cdata_sz = 0;                // card data size
char name[] = default_file_name;    // file name with test ANSI-HEX string
const char * user_data = NULL;      // argv command line string bind pointer

const unsigned char rs_RID[] = {0xA0,0x00,0x00,0x06,0x58};    //reader supporID

    typedef unsigned char byte;
    typedef unsigned short udbyte;
    typedef struct _TLV TLV;
    typedef struct _TLVNODE TLVNODE;
    

    struct _TLV {
        TLV *parrent;        //parent tag structure
        TLV *child;
        TLV *first;
        TLV *last;
        TLV *next;
        TLV *prev;
        byte *self_data;    // contain 
        int self_size; 
        byte isDataTLV; 
        udbyte tag;
        byte len;
        byte *data;
    };

// optional vars 
long jobn = 0;                       // Num card job runs count 


///
/// Return 1 if value standart tag or return 0, else.
/// @param tag Short (double bute) tag value.
/// @return Value 1, if is standart tag, else return 0 - unknown tag or arbitrary data
///
int isTag(udbyte tag)
    {
    for (int i=0;i<taglist_sz;i++) if (taglist[i] == tag) return 1;
    return 0;
    }

///
/// Parse tlv tag stream, return node chain
/// @param data Pointer to data byte stream
/// @param len  Length data from data stream
/// @return Pointer to TLV structure with parsed result or NULL pointer if fail or poor data.
/// Wrapped consummed data in grandparrent tag = 0x00. Tag chain bind all discovered tags and 
/// child tag data also was been reparsed 
/// 
TLV* parse_data( byte *data, int len) 
{
    int head_sz=0;          // tag head_sz
    TLV this, x = (TLV) {.child =NULL, .data=NULL, .first=NULL, .last=NULL, .len=-1, .next = NULL, .parrent=NULL,
                        .prev = NULL, .self_data=NULL, .self_size=0, .tag = 0, .isDataTLV=1},
     *r =NULL, *father=NULL, *line = NULL;   // TLV context, tmp, result pointer

    this = x;           // build grandparent tag tree
    this.tag = 0;
    this.data = data;   // start init
    this.len = len;

    if (!data || len<=2) return NULL;   // not valid input length field must have

do  {
    if (isTag(data[0])) // if not catch single byte tag, assume 2 byte tag 
    {
    x.tag =  len>0 ?  data[0] : 0;        // assume 1 byte tag
    x.len =  len>1 ?  data[1] : 0;        // if len acessible
    x.data = len>2  ? &data[2] : NULL;    // if data acessible
    x.data = x.len>0 ? x.data : NULL;     // and if data present  
    head_sz = len-1>0 ? 2 : len;  
    }
      else
           {
            x.tag =  len>1  ? DB_TG(data) : 0;    // assume 2 byte tag
            x.len =  len>2  ? data[2]: 0;         // if len acessible
            x.data = len>3  ? &data[3]: NULL;     // if data acessible
            x.data = x.len>0  ? x.data : NULL;    // if data present  
            head_sz = len-2>0 ? 3 : len;        
           }
    len -=head_sz;          // crop head data from len             
                            //  !! len  contain max lenght rest data !!      
    x.self_data = data;
    x.self_size = x.len > len ? (len+head_sz)  : (x.len + head_sz); // len contain remain data without head part
    x.data = x.len>len  ? NULL : x.data;  // failure data len , if data size oversize avaliable data data part
                                                 // then data NULL pointer use as indicator to not valid parsing
                                                 // to terminate parsing, x.data == NULL, - invalid tag attribute
                                                 // x.len >0 and x.data == NULL - fail tag else valid tag
                                                  
    if (x.len>len && father) father->isDataTLV = 0;
    x.isDataTLV = x.len>len ? 0 : 1 ;            // set, unset valid TLV data flag it datas not valid TVL   

    if ( x.data == NULL && x.len == 0 ||        // valid tag  condition , else all other fail data filter
         x.data && x.len>0   &&                 // if tag with data, than it contain data usefull for reparsing
         len >= 0  )                       // else it contain only tag present and len present field without data                  
        {
            x.prev = r ? r : NULL;                       // bind with previous parsing chain, if present
            x.parrent = father ? father : x.parrent;     // set parrent if avaliable
            r = (TLV*) malloc(sizeof (TLV));             // get new place for new tag   
                x.first = x.first ? x.first : r;             // if x first absent set it this tag storage - r, else uncange it  
            if (father)
               father->child = father->child ? father->child : r;  // r contain actual current tag reference to location 
            if  ( len-x.len <= 0 )                       // x - this final tag in chain  
                {                                        // update last property for all prev tags bind     
                    x.last = r;                          // save last child tag ref at time after parent tag change      
                    if (line=x.prev)                     // work with prev the ones parent chain tags
                      {
                        line ->next = r;
                        while ( line  && line->parrent == line->next->parrent ) // if parrent not change in chains
                        {                                                       // all child to this parrent
                        line->last = x.last;
                        line = line->prev;             // go next node chain
                        }
                      }
                }    
            r[0] = x;                                  // save parsing result                                        
            if (r[0].prev)                             // if present
                r[0].prev->next = r;                   // update prev tag next reference 
        }

    if (len-x.len <= 0 || (x.len>0  && x.data == NULL)) // end chain, or invalid unfull tag data present, 
                                                        // load data from prev parsing result 
                                                        // this logic path only flow if valid parsing flow presented
                                                        // until all data in flow, from tag tree has been parsed
        {    
        father = father ? father : r;   // init <parsed> variable with absent value  
        TLV *lr = r;                    // local result pointer
        while (father->prev) father=father->prev;       // rewind father object pointer to start chain
        while (father)
                 {  // catch valid and unparsed data, child == NULL attrubute mark unparsed tag
                    // if x.data  present (not NULL), discover data to parsing and break
                    if (father->data != NULL && father->child == NULL && father->isDataTLV !=0) break;              
                    else father = father->next ? father->next : lr ;    // catch next unparsed tag from next tag chain 
                                                                        // or get tag from head chain - by ref. r
                    if (father == NULL) break;      // missing unparsed data chain and r chain              
                    if (father == r) lr = NULL ;    // r chain assign worked out pointer, set it unacessible     
                 }                                  // father == NULL in this condition assume that all data
        if (father)                                 // in chain processed
            {
                // load new data poiners from aceptable tag from result chain
                data = father->data;
                len = father->len;
                father->child = NULL; // explice cler child value
                x.parrent = NULL;   // reset parrent ref to recalc in next loop
                x.last = NULL;      // reset first ref
                x.first = NULL;     // reset last ref
                x.child = NULL;      // reset child ref
                continue;           // forced go new parsing loop                    
            }                                           
        }
    len -= x.len ;          // cutting data portion
    data += x.self_size;    // shift data pointer to new data portion    
        
}   while(father || len >1); // remains unparsed data , minimal tag 2 bytes 

#define DBG
while (r->prev != NULL) r = r->prev;            // r - all tags chain, rewind to start tag and return result
#ifdef DBG
TLV *tmp = r;
int k=0;
while (r->next) { printf(">:%p->Id:%p->:%p %-4x pt:%-4x n:%-3i",r->parrent, r, r->child ,r->tag, r->parrent? r->parrent->tag : 0, k++ );
                  printf(" len:%-3x isTLV:%-2i",r->len,r->isDataTLV);
                  printf(" data:");
                   for(int n=0;n<r->len;n++) { printf ("%x",r->data[n]);}  r=r->next;
                  printf("\n"); 
                }
r=tmp;
#endif
#undef DBG
r->parrent = (TLV *) malloc (sizeof(TLV));      // save prandparrent pointer tag
(r->parrent)[0] = this;                         // load actual values from grandfather
r->parrent->child = r;                          // bind to father
r->prev = r->parrent;                           // append to chain
                                                // r contain parsed data tag chain 
while (r->next != NULL && r->next->parrent == NULL )
    {
        r->next->parrent = r->parrent;          // r->parrent = grandparrent
        r = r->next;                            // set grandparrent pointer in descedants 
    }

r = r->prev->parrent;                           // set grandparrent pointer 
r->next = r->child;                             // bind to general chain
r->last = r;
r->first = r;

return r->child;                                 // return parsing result
                                                 // with grandparrent wrap
}

void job (  )
{
    jobn++;
    /// Main processing
    /// Read response, setup response fields.
    unsigned char SW1 = ((unsigned char *) cdata)[cdata_sz-2];
    unsigned char SW2 = ((unsigned char *) cdata)[cdata_sz-1];
    unsigned short SW = ((short *) &((unsigned char *)cdata)[cdata_sz-2])[0];
    unsigned char *bt = ((unsigned char *)cdata);   //byte pointer 
    unsigned short *sh = ((unsigned short *)cdata);  //short int pointer
 
    TLV *r = NULL;

    unsigned char * apps = NULL; 
    unsigned char app_cnt = 0;

    struct applist  {
        const byte *app_RID;
        byte  app_priopity;
        struct applist *next;
        struct applist *prev;
        } *a=NULL,*b=NULL;

 
    if (SW == 0x0090)   // response ok
      {
       r = parse_data(cdata,cdata_sz-2);      // parse data
       int i =0;
       a = malloc(sizeof(struct applist));  // get storage location for applist object
       a->app_priopity = 0x00;              // set default setting app list item
       a->app_RID =NULL;
       a->next =NULL;
       a->prev =NULL;
       while (r)                           // dicovered _61 tag,and collect all interested child tags
                {
                  if  (r->tag == _61_APP_TEMPLATE && r->child != NULL)
                                                
                    {
                        TLV *tr = r->child;     // temp router for walk chain, grab data
                        do
                        {                                           // store tags _4F and _87 if present
                        a->app_RID = (tr->tag == _4F_ADF &&  memcmp(tr->data,rs_RID,5) == 0 ) ?  // =0 ok compare
                                     rs_RID : a->app_RID;           
                        // ----- store only data from _61_tag if  its supported given reader RID
                        a->app_priopity = tr->tag == _87_APPLICATION_PRIORITY_INDICATOR ? tr->data[0] : a->app_priopity;
                        tr = tr->next;                              // next view child tag chain
                        }
                            while (r->child->last == tr->last);
                    
                    if (a->app_RID == rs_RID)
                        {   // if sucessful grab supported reader allocate new item list storage
                        a->next = malloc(sizeof(struct applist));   // get new list item
                        a->next->prev = a;                          // bind with next list item  
                        a = a->next;                                // set pointer to list head
                        }
                        // if not rewrite this item, not allocate new list item
                    }             
                  if (r->next == NULL) break;
                  r = r->next;       // to next view tag chain  
                }     
    /// end grab information from card response, do sort by _app priority data
    /// _61 tag with unsupportrd RID was be skip from processing
     
    if (a->app_RID !=rs_RID) // cuttof unsupported app list item if it present in head items
    {
        a=a->prev;
        free(a->next);   // free head item
    }
    if (a)  // start sorting
        {
            struct applist *s_max=NULL, *final=NULL, *start=NULL, *top=NULL, *t, *t2;
            final = a;                                                   // set final point
            start = a; while (start->prev!=NULL) start = start->prev;    // set start point
            top = start;
            while (1)
            { 
            s_max = s_max ? s_max : top;           // init s_max from NULL to exist value                 

                while (top->next) 
                        {
                s_max = (s_max->app_priopity & 0x0F <= top->app_priopity & 0x0F) ||
                        (top->app_priopity & 0x0F == 0) ? s_max : top;      // find most maximum, move above  
                                                                            // start pointer
                top = top->next;                                                    
                        }            
            
            if (s_max != start)             // if not start, than do position moving in chain
                {
                if (s_max->prev)
                    s_max->prev->next = s_max->prev ? s_max->next : NULL;    // sew together app list or finished chain
                s_max->next = start->next;                                   // save start-next ref
            
                start->next = s_max;                // move s_max to start
                s_max->prev = start;                // bind to start chain
                }
            top = start;                            // go text loop               
            if (s_max == start) break; 
            else s_max = start;
            }
            
        }
    // Print result 

    while ((a = a->prev ? a->prev : a)->prev) ;   // go to start app list
    
        printf ("Apps on the card:\n");
        do 
        {
            printf("RID:%hhx%hhx%hhx%hhx%hhx\n",a->app_RID[0],a->app_RID[1],a->app_RID[2],a->app_RID[3],a->app_RID[4]);
            printf("App priority: %hhx\n",a->app_priopity&0x0F);
        }
        while  ((a = a->next ? a->next : a )->next);

      }
    else
    printf ("Not valid operation result %x",SW );

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

