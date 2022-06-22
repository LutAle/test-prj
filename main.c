 #include <stdio.h>   
 #include <string.h>
 #include <limits.h>
 #include <stdlib.h>
 #include "EMV 4.3 Book 3 Application Specification.h"

// #define DBG                      // Print debug parsing card response
 #define ONLY_SUPPORTED_APP       // Print only app that support reader


/// Two byte tag 
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
/// Return tag length if value standart tag 1 or 2 or return 0, else.
/// @param *tag reference on data to testing.
/// @return Tag length 1 or 2, if is standart tag, else return 0 - unknown tag or arbitrary data
///
int tagL(byte *tag)
    {
    int t = tag[0];
    int tt = DB_TG(tag);

    for (int i=0;i<taglist_sz;i++) if (taglist[i] == t) return 1;
    for (int i=0;i<taglist_sz;i++) if (taglist[i] == tt) return 2;
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
    
    TLV  x = (TLV) {.child =NULL, .data=NULL, .first=NULL, .last=NULL, .len=-1, .next = NULL, .parrent=NULL,
                        .prev = NULL, .self_data=NULL, .self_size=0, .tag = 0, .isDataTLV=1},
     *r =NULL, *father=NULL, *line = NULL;   // TLV context, tmp, result pointer

    r = malloc(sizeof(TLV));        // allocate new tag
    r->child =NULL;                 // build (root)grandparent tag 
    r->data = data;
    r->first = r;
    r->isDataTLV = 1;
    r->last = r;
    r->len =len;
    r->next =NULL;
    r->parrent = NULL;
    r->prev =NULL;
    r->self_data = NULL;            // root tag marker
    r->self_size = len+3;
    r->tag = 0;

    if (!data || len<=2) return NULL;   // not valid input length field must have

    father = r;             // set parrent data src tag

do  {
    if (tagL(data) == 1) // if not catch single byte tag, assume 2 byte tag 
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


    if (x.len>len) father->isDataTLV = 0;       // reset data valid TLV  
    x.isDataTLV = x.len>len || x.len < 2 && tagL(x.self_data) == 1 ||
                  x.len<3   ? 0 : 1 ;            // set, unset TLV data length validation flag or it datas not valid TVL   

    if ( father && father->isDataTLV)            // Save valid parsing
     if ( x.data == NULL && x.len == 0 ||        // valid tag  condition , else all other fail data filter
          x.data && x.len>0 )                    // if tag with data, than it contain data usefull for reparsing
                                                 // else it contain only tag present and len present field without data                  
        {
            x.prev = r ? r : NULL;                       // bind with previous parsing chain, if present
            x.parrent = father ? father : x.parrent;     // set parrent if avaliable
            r = (TLV*) malloc(sizeof (TLV));             // get new place for new tag   
            x.first = x.first ? x.first : r;             // if x first absent set it this tag storage - r, else uncange it  

            father->child = father->child ? father->child : r;  // r contain actual current tag reference to location 
            if  ( len-x.len == 0 || len == 0 )           // x - this final tag in chain  
                {                                        // update last property for all prev tags bind     
                    x.last = r;                          // save last child tag ref at time after parent tag change      
                    if (line=x.prev)                     // work with prev the ones parent chain tags
                      {
                        line->next = r;
                        while ( line  && line->parrent == father ) // if parrent not change in chains
                        {                                          // all child to this parrent
                        line->last = x.last;
                        line = line->prev;             // set in prev node chain, go backward
                        }
                      }
                }    
            r[0] = x;                                  // save parsing result                                        
            if (r[0].prev)                             // if present
                r[0].prev->next = r;                   // update prev tag next reference 
        }                                                          
    if (len-x.len == 0 || len == 0 || father->isDataTLV == 0)  ///  normal finalise data parsing, load new data from chain
        {                                                       //  or prev loop with parrent data been parsing fail   
            if (father->isDataTLV == 0)
                {
                    father->child = NULL;                       // reset not valid parsing child ref 
                    while (r  && r->parrent->isDataTLV == 0)  {r=r->prev; free(r->next);}  // remove not valid tag chain
                    r->next = NULL;
                }
            while (father->prev) father=father->prev;           // rewind father object pointer to start chain
            while (father)      // give new unparsed father tag
                 {  // catch valid and unparsed data, child == NULL attrubute mark unparsed tag
                    // if x.data  present (not NULL), discover data to parsing and break
                    
                    int  iscanparse = father->isDataTLV && (father->len>2 || father->len>1 && tagL(father->data) == 1);  
                                                                                                // tag and length fields must beens present
                    if (father->data != NULL && father->child == NULL && iscanparse ) break;    // data content minimal validation
                                                                                       
                    else father = father->next;         // catch next unparsed tag from next tag chain 
                 }                                      // or be father == NULL that equive missing unparsed data in chain              

            if (father)      
		        {           // load new data poiners from aceptable tag, if present
                data = father->data;
                len = father->len;

                x.parrent = father;     
                x.last = NULL;          // reset first ref
                x.first = NULL;         // reset last ref
                x.child = NULL;         // reset child ref
                continue;               // forced go new parsing loop                                                               
                }
        }
    
    int parseok = len==0 || len >1 && tagL(data) == 1 || len>2 ;   // data for parsing present and data amount   
                                                                   // above  minimal threshold 
                                                                   // to contine parse smal TLV
                                                                   // ol all data parsed.     
    if (parseok)                // contine parsing    
        {    
        len -= x.len ;          // cutting data portion
        data += x.self_size;    // shift data pointer to new data portion
        }                                                       
    else                        // Reset parrernt data parsing  
        if (father) father->isDataTLV = 0;      // mark data not valid TLV
                               // next parsing loop set new parrent and remove invalid childs                                                                      
}   while(father); 

while (r->prev != NULL) r = r->prev;    // r - all tags chain, rewind to start tag and return result


#ifdef DBG
TLV *tmp = r;
int k=0;
printf("\nData parse the results:\n");
while (r) {
            printf("f:%p l:%p ", r->first, r->last);
            printf("p:%p<-Id:%p->c:%p %-4x pt:%-4x n:%-3i",r->parrent, r, r->child ,r->tag, r->parrent? r->parrent->tag : 0, k++ );
            printf(" self_size:%-2x",r->self_size);
            printf(" len:%-3x isTLV:%-2i",r->len,r->isDataTLV);
            printf(" data[16]:");
            for(int n=0;n<r->len && n<16;n++) {printf ("%-2x ",r->data[n]);}
            r=r->next;
            printf("\n"); 
        }
r=tmp;
#endif
#undef DBG

return r->child;        // return parsing result with grandparrent
}

void job (  )
{
    jobn++;
    /// Main processing
    /// Read response, setup response fields.
    unsigned char SW1 = ((unsigned char *) cdata)[cdata_sz-2];
    unsigned char SW2 = ((unsigned char *) cdata)[cdata_sz-1];
    unsigned short SW = ((short *) &((unsigned char *)cdata)[cdata_sz-2])[0];

    TLV *r = NULL;

    struct applist  {
        const byte *app_RID;
        byte  app_priopity;
        struct applist *next;
        struct applist *prev;
        } *a = NULL;

    if (SW == 0x0090)   // response ok
    {
        r = parse_data(cdata,cdata_sz-2);   // parse data

        while (r)                           // dicovered _61 tag,and collect all interested child tags
            {
                if  (r->tag == _61_APP_TEMPLATE && r->child != NULL)                                                 
                {
                TLV *tr = r->child;             // temp router for walk chain, grab data
                                                // get storage location for applist object, liitle hard pointer pinpong   
                if (a)  a = ((a->next = malloc(sizeof(struct applist)))->prev = a)->next;   // renew new item a in chain
                else   (a = malloc(sizeof(struct applist)))->prev = NULL;    // simple get new allocaton, and set tail NULL   
                a->next = NULL;         // set new field
                a->app_RID = NULL;      // value that has exist at new app item born
                a->app_priopity = 0;    // default
                do
                    {
                    if (tr->tag == _4F_ADF)      // store tags _4F and _87 if present
                        a->app_RID = (memcmp(tr->data,rs_RID,5) == 0 ) ? rs_RID : tr->data; // =0 ok compare
                    if (tr->tag == _87_APPLICATION_PRIORITY_INDICATOR)
                        a->app_priopity = tr->data[0] & 0x0F;
                    tr = tr->next;              // view next childs chain tag
                    }
                      while (tr && r == tr->parrent);   // do until that pareent present in chain  
                    
                }
                if (r->next == NULL) break; // finish or
                r = r->next;                // go to search new _61 tag   
            } 
        
        // end grab information from card response, do sort by _app priority data
        // _61 tag with unsupportrd RID 
        
        if (a)  // start sorting and print
            {
                while(a->prev) a = a->prev;         // go to start chain
                
                struct applist *start=a, *top=start; // local pointers
                
                while (top)         // start chain sorting exchange
                    {               // find supported app 
                    if (top && start && top != start)  // do exchange with different items only
                    if (top->app_RID == rs_RID)         // move to up position supported apps
                        if (start->app_RID != rs_RID)   // insert before start item if exist start item not supported app 
                            {
                                // swap data or swap ref?  - swap ref
                                if (top->next) top->next->prev = top->prev;
                                if (top->prev) top->prev->next = top->next;
                                top->next = start;                                  // ... prev, before <start> after ,next ...
                                top->prev = start->prev;
                                start->prev = top; 
                                start = top;             // set new start position
                                top = start;             //
                            }
                        else // supported app present and need do insertion with its priority order by app_priority field control
                            {
                                if (top->app_priopity != 0) // find item with max priority order (small value have largest priority)
                                   {
                                    struct  applist *tmp = start;       // temp chain store 
                                                                        // find max position and ignore 0 app priority                                                            
                                    while (start && (start->app_priopity > top->app_priopity || start->app_priopity == 0))
                                           {
                                            if (start->next == NULL) break;
                                            start = start->prev;           // stopped in max binding position for top
                                           }

                                    if (top->next) top->next->prev=top->prev;   // insert after start position
                                    if (top->prev) top->prev->next = top->next;
                                    top->prev = start;
                                    top->next = start->next;
                                    start->next = top;
                                    start = tmp;                                // restore initial start position
                                    top = start;                                // go to new pass, renew top pointer            
                                   }
                                else        // insert after rel start app position, top app with 0 ptiority
                                    {       // set in tail app chain, actual start app presented,
                                            // not supported app no move througth app list and shift to end chains
                                    if (top->next) top->next->prev=top->prev;
                                    if (top->prev) top->prev->next = top->next;
                                    top->next = start->next;
                                    top->prev = start;
                                    start->next = top;
                                    start = top;    // renew start pointer
                                    }
                            }
                        top=top->next;      // go test new item from was be reaind tail chain
                    }                       // at missing new items to test, finish chain rebuilding
                    
                // Print result 

                while (a->prev) a = a->prev;   // go to start app list
                printf ("\nApps on the card:\n");
                int reclamationout = 1;
                while (a) 
                    {
                    
                    #ifdef ONLY_SUPPORTED_APP                 // set to printout only supported app    
                    if ( !a->app_RID || a->app_RID != rs_RID)
                        {
                            if (a->next == NULL) break;         // go print next or go relax 
                            a=a->next;
                            continue;
                        }
                    #endif
                    
                    if (a->app_RID) 
                        {
                        if (a->app_RID != rs_RID && reclamationout )
                           {printf ("Unsupported ! app:\n"); reclamationout = 0; }
                        printf("RID:%hhx%hhx%hhx%hhx%hhx\n",a->app_RID[0],a->app_RID[1],a->app_RID[2],a->app_RID[3],a->app_RID[4]);
                        }
                    else
                        printf("RID:NULL - !RID not present in card response\n" ); 
                    printf("App priority: %hhx\n",a->app_priopity);

                    if (a->next == NULL) break;         // go print next or go relax 
                    a=a->next;
                    } 
            }
            else 
                printf("\nMissing apps in card response\n");
    }
    else
        printf ("Not sucessful response from card %#x\n",SW );

    if (r) while (r->prev) r=r->prev;          // go start
    if (a) while (a->prev) a=a->prev;
    while (r) {if(r->prev) free(r->prev); r=r->next;}   // deocupate memory
    while (a) {if(a->prev) free(a->prev); a=a->next;}       

    printf("Job processing card data is %i number.\n",jobn);


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

