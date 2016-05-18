#include <stdio.h>
#include "varint.h"
#include "index.h"
#include "buffer.h"
#include <assert.h>
#include <math.h>
#include <time.h>
#include "tokenize.h"
#include "spec.h"
#include "levenshtein.h"

#define TESTFUNC(f) \
                printf("Testing %s ...\t", __STRING(f)); \
                fflush(stdout); \
                if (f()) {printf ("FAIL\n"); exit(1); } else printf("PASS\n");
                
#define ASSERTM(expr, ...) if (!(expr)) { fprintf (stderr, "Assertion '%s' Failed: " __VA_ARGS__ "\n", __STRING(expr)); return -1; }
#define ASSERT(expr) if (!(expr)) { fprintf (stderr, "Assertion '%s' Failed\n", __STRING(expr)); return -1; }
#define ASSERT_EQUAL_INT(x,y,...) if (x!=y) { fprintf (stderr, "%d != %d: " __VA_ARGS__ "\n", x, y); return -1; }
                    
#define TEST_START() printf("Testing %s... ",  __FUNCTION__); fflush(stdout);
#define TEST_END() printf ("PASS!");


int testVarint() {
  VarintVectorWriter *vw = NewVarintVectorWriter(8);
  int expected[5] = {10, 1000,1020, 10000, 10020};
  for (int  i = 0; i < 5; i++) {
    VVW_Write(vw, expected[i]);    
  }
  
  
  // VVW_Write(vw, 100);
  printf("%ld %ld\n", BufferLen(vw->bw.buf), vw->bw.buf->cap);
  VVW_Truncate(vw);
  BufferSeek(vw->bw.buf, 0);
  VarintVectorIterator it = VarIntVector_iter(vw->bw.buf);
  int x = 0;
  
  
  while (VV_HasNext(&it)) {
    int n = VV_Next(&it);
    ASSERTM(n == expected[x++], "Wrong number decoded");
    printf("%d %d\n", x, n);
  }


  VVW_Free(vw);
  return 0;
}

int testDistance() {
    VarintVectorWriter *vw = NewVarintVectorWriter(8);
     VarintVectorWriter *vw2 = NewVarintVectorWriter(8);
    VVW_Write(vw, 1);
    VVW_Write(vw2, 4);
    
    //VVW_Write(vw, 9);
    VVW_Write(vw2, 7);
    
    VVW_Write(vw, 9);
    VVW_Write(vw, 13);
    
    VVW_Write(vw, 16);
    VVW_Write(vw, 22);
    VVW_Truncate(vw);
    VVW_Truncate(vw2);
    
    
    VarintVector v[2] = {*vw->bw.buf, *vw2->bw.buf};
    int delta = VV_MinDistance(v, 2);
    printf("%d\n", delta);
    
    VVW_Free(vw);
    VVW_Free(vw2);
    
    
    return 0;
    
    
}



int testIndexReadWrite() {
  IndexWriter *w = NewIndexWriter(10000);

  for (int i = 0; i < 100; i++) {
    // if (i % 10000 == 1) {
    //     printf("iw cap: %ld, iw size: %d, numdocs: %d\n", w->cap, IW_Len(w),
    //     w->ndocs);
    // }
    
    ForwardIndexEntry h;
    h.docId = i;
    h.flags = 0;
    h.freq = (1 + i % 100) / (float)101;
    h.docScore = (1 + (i + 2) % 30) / (float)31;
    
    h.vw = NewVarintVectorWriter(8);
    for (int n = 0; n < i % 4; n++) {
      VVW_Write(h.vw, n);
    }
    VVW_Truncate(h.vw);
    
    IW_WriteEntry(w, &h);
    printf("doc %d, score %f offset %zd\n", h.docId, h.docScore, w->bw.buf->offset);
    VVW_Free(h.vw);
  }

  LG_INFO("iw cap: %ld, iw size: %ld, numdocs: %d\n", w->bw.buf->cap, IW_Len(w),
         w->ndocs);
        
  LG_INFO("Score writer: numEntries: %d, minscore: %f\n", w->scoreWriter.header.numEntries, w->scoreWriter.header.lowestScore);
  ScoreIndex *si = NewScoreIndex(w->scoreWriter.bw.buf);
  for (int i = 0; i < si->header.numEntries; i++) {
      printf("Entry %d, offset %d, score %f docId %d\n", i, si->entries[i].offset, si->entries[i].score, si->entries[i].docId);
  }
  ASSERT(w->skipIndexWriter.buf->offset > 0);
  IW_Close(w);
  ScoreIndex_Free(si);
  
  //IW_MakeSkipIndex(w, NewMemoryBuffer(8, BUFFER_WRITE));
  
//   for (int x = 0; x < w->skipIdx.len; x++) {
//     printf("Skip entry %d: %d, %d\n", x, w->skipIdx.entries[x].docId, w->skipIdx.entries[x].offset);
//   }
  printf("iw cap: %ld, iw size: %ld, numdocs: %d\n", w->bw.buf->cap, IW_Len(w),
         w->ndocs);

  
  
  
  int n = 0;

  for (int xx = 0; xx < 1; xx++) {
    SkipIndex* si = NewSkipIndex(w->skipIndexWriter.buf);
    printf("si: %d\n", si->len);
    IndexReader *ir = NewIndexReader(w->bw.buf->data, w->bw.buf->cap, &si, NULL, 1);
    IndexHit h = NewIndexHit();
    

    struct timespec start_time, end_time;
    while (IR_HasNext(ir)) {
        IR_Read(ir, &h);
        //printf("%d\n", h.docId);
    }
    // for (int z= 0; z < 10; z++) {
    // clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start_time);

    // IR_SkipTo(ir, 900001, &h);
    
    
    
    // clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &end_time);
    // long diffInNanos = end_time.tv_nsec - start_time.tv_nsec;
    

    // printf("Time elapsed: %ldnano\n", diffInNanos);
    // //IR_Free(ir);
    // }
  }
  IW_Free(w);
  
  return 0;
}

IndexWriter *createIndex(int size, int idStep) {
   IndexWriter *w = NewIndexWriter(100);
   
  t_docId id = idStep;
  for (int i = 0; i < size; i++) {
    // if (i % 10000 == 1) {
    //     printf("iw cap: %ld, iw size: %d, numdocs: %d\n", w->cap, IW_Len(w),
    //     w->ndocs);
    // }
    ForwardIndexEntry h;
    h.docId = id;
    h.flags = 0;
    h.freq = i % 10;
    
    h.vw = NewVarintVectorWriter(8);
    for (int n = idStep; n < idStep + i % 4; n++) {
      VVW_Write(h.vw, n);
    }
    IW_WriteEntry(w, &h);
    VVW_Free(h.vw);
    
    id += idStep;
  }


   printf("BEFORE: iw cap: %ld, iw size: %zd, numdocs: %d\n", w->bw.buf->cap, IW_Len(w),
         w->ndocs);
  
  
  w->bw.Truncate(w->bw.buf, 0);
  
//  IW_MakeSkipIndex(w, NewMemoryBuffer(100, BUFFER_WRITE));
  IW_Close(w);
  
  printf("AFTER iw cap: %ld, iw size: %zd, numdocs: %d\n", w->bw.buf->cap, IW_Len(w),
         w->ndocs);
  return w;
}


typedef struct {
    int maxFreq;
    int counter;
} IterationContext;

int printIntersect(void *ctx, IndexHit *hits, int argc) {
    
    printf("intersect: %d\n", hits[0].docId);
    return 0;
}

int testReadIterator() {
    IndexWriter *w = createIndex(10, 1);
    
    
    IndexReader *r1 = NewIndexReaderBuf(w->bw.buf, NULL, NULL, 0, NULL);
    IndexHit h = NewIndexHit();
            
    IndexIterator *it = NewReadIterator(r1);
    int i = 1;
    while(it->HasNext(it->ctx)) {
        if (it->Read(it->ctx, &h) == INDEXREAD_EOF) {
            return -1;
        }
        printf("Iter got %d\n", h.docId);
        ASSERT(h.docId == i++);
        
    }
    return i == 11 ? 0 : -1;
}


int testUnion() {
    IndexWriter *w = createIndex(10, 2);
    SkipIndex *si = NewSkipIndex(w->skipIndexWriter.buf);
    IndexReader *r1 = NewIndexReader(w->bw.buf->data,  IW_Len(w), si, NULL, 1);
    IndexWriter *w2 = createIndex(10, 3);
    si = NewSkipIndex(w2->skipIndexWriter.buf);
    IndexReader *r2 = NewIndexReader(w2->bw.buf->data , IW_Len(w2), si, NULL, 1);
    printf("Reading!\n");
    IndexIterator *irs[] = {NewReadIterator(r1), NewReadIterator(r2)};
    IndexIterator *ui =  NewUnionIterator(irs, 2, NULL);
    IndexHit h = NewIndexHit();
    int expected[] = {2, 3, 4, 6, 8, 9, 10, 12, 14, 15, 16, 18, 20, 21, 24, 27, 30};
    int i = 0;
    while (ui->Read(ui->ctx, &h) != INDEXREAD_EOF) {
        ASSERT(h.docId == expected[i++]);
         //printf("%d, ", h.docId);
    }
    // IndexWriter *w3 = createIndex(30, 5);
    // IndexReader *r3 = NewIndexReader(w3->bw.buf->data,  IW_Len(w3), &w3->skipIdx);
    
    
    // IndexIterator *irs2[] = {ui, NewIndexIterator(r3)};
    // // while (ui->Read(ui->ctx, &h) != INDEXREAD_EOF) {
    // //     printf("Read %d\n", h.docId);
    // // }
    //  IterationContext ctx = {0,0};
    // int count = IR_Intersect2(irs2, 2, printIntersect, &ctx);
    // printf("%d\n", count);
    ui->Free(ui);
    return 0;
}

int testIntersection() {
    
    IndexWriter *w = createIndex(100000, 4);
    SkipIndex *si = NewSkipIndex(w->skipIndexWriter.buf);
    IndexReader *r1 = NewIndexReader(w->bw.buf->data,  IW_Len(w), si, NULL, 0);
    IndexWriter *w2 = createIndex(100000, 2);
    si = NewSkipIndex(w2->skipIndexWriter.buf);
    IndexReader *r2 = NewIndexReader(w2->bw.buf->data,  IW_Len(w2), si, NULL, 0);
    
    // IndexWriter *w3 = createIndex(10000, 3);
    // IndexReader *r3 = NewIndexReader(w3->bw.buf->data,  IW_Len(w3), &w3->skipIdx);
    
    IterationContext ctx = {0,0};
    
    
    IndexIterator *irs[] = {NewReadIterator(r1), NewReadIterator(r2)};//,NewIndexTerator(r2)};
    
    printf ("Intersecting...\n");
    
    int count = 0;
    IndexIterator *ii = NewIntersecIterator(irs, 2, 0, NULL);
    struct timespec start_time, end_time;
    clock_gettime(CLOCK_REALTIME, &start_time);
    IndexHit h = NewIndexHit();
    
    while (ii->Read(ii->ctx, &h) != INDEXREAD_EOF) {
        //printf("%d\n", h.docId);
        ++count;
    }    
    
    //int count = IR_Intersect(r1, r2, onIntersect, &ctx);
    clock_gettime(CLOCK_REALTIME, &end_time);
    long diffInNanos = end_time.tv_nsec - start_time.tv_nsec;

    printf("%d intersections in %ldns\n", count, diffInNanos); 
    printf("top freq: %d\n", ctx.maxFreq);
    ASSERT(count == 50000)
    return 0;
}

int testMemBuffer() {
    //TEST_START();
    
    BufferWriter w = NewBufferWriter(NewMemoryBuffer(2, BUFFER_WRITE));
    ASSERTM( w.buf->cap == 2, "Wrong capacity");
    ASSERT (w.buf->data != NULL);
    ASSERT( BufferLen(w.buf) == 0);
    ASSERT( w.buf->data == w.buf->pos );
    return 0;
    
    const char *x = "helo";
    size_t l = w.Write(w.buf, (void*)x, strlen(x)+1);
    
    ASSERT( l == strlen(x)+1);
    ASSERT( BufferLen(w.buf) == l);
    ASSERT (w.buf->cap == 8);
    
    l = WriteVarint(1337, &w);
    ASSERT (l == 2);
    ASSERT( BufferLen(w.buf) == 7);
    ASSERT( w.buf->cap == 8);
    
    w.Truncate(w.buf, 0);
    ASSERT( w.buf->cap == 7);
    
    Buffer *b = NewBuffer(w.buf->data, w.buf->cap, BUFFER_READ);
    ASSERT(b->cap = w.buf->cap);
    ASSERT(b->data = w.buf->data);
    ASSERT(b->pos == b->data);
    
    
    char *y = malloc(5);
    l = BufferRead(b, y, 5);
    ASSERT(l == l);
    
    ASSERT( strcmp(y, "helo") == 0 );
    ASSERT( BufferLen(b) == 5);
    
    free(y);
    
    int n = ReadVarint(b);
    ASSERT (n == 1337);
    
    w.Release(w.buf);
    //TEST_END();
    return 0;
}


typedef struct{
    int num;
    char **expected;
    
} tokenContext;

int tokenFunc(void *ctx, Token t) {
    tokenContext *tx = ctx;
    
    assert( strcmp(t.s, tx->expected[tx->num++]) == 0);
    assert(t.len == strlen(t.s));
    assert(t.fieldId == 1);
    assert(t.pos > 0);
    assert(t.score == 1);
    return 0;    
}

int testTokenize() {
    
    char *txt = strdup("Hello? world...   ? __WAZZ@UP? שלום");
    tokenContext ctx = {0};
    const char *expected[] = {"hello", "world", "wazz", "up", "שלום"};
    ctx.expected = (char **)expected;
    
    tokenize(txt, 1, 1, &ctx, tokenFunc);
    ASSERT(ctx.num == 5);
    
    free(txt);
    
    return 0;
    
}

int testForwardIndex() {
    
    ForwardIndex *idx = NewForwardIndex(1,  1.0);
    char *txt = strdup("Hello? world...  hello hello ? __WAZZ@UP? שלום");
    tokenize(txt, 1, 1, idx, forwardIndexTokenFunc);

    return 0;
}

int testIndexSpec() {
    
    const char *args[4] = {"title", "0.1", "body", "2.0" };
    IndexSpec s;
    
    int rc = IndexSpec_Parse(&s, args, 4);
    ASSERT(rc == REDISMODULE_OK);
    ASSERT( s.numFields == 2)
    
    FieldSpec *f = IndexSpec_GetField(&s, args[0], strlen(args[0]));
    ASSERT( f != NULL);
    assert( strcmp(f->name, args[0]) == 0);
    assert( f->weight == 0.1 );
    
    ASSERT (IndexSpec_GetField(&s, "foo", 3) == NULL)
    
    return 0;
    
}

int testQueryTokenize() {
    
    char *text = strdup("hello \"world wat\" wat");
    QueryTokenizer qt = NewQueryTokenizer(text, strlen(text));
    
    char *expected[] = {"hello", NULL, "world", "wat", NULL, "wat"};
    QueryTokenType etypes[] = {T_WORD, T_QUOTE, T_WORD, T_WORD, T_QUOTE, T_WORD, T_END};
    int i = 0;
    while (QueryTokenizer_HasNext(&qt)) {
        QueryToken t = QueryTokenizer_Next(&qt);
        printf ("%d Token text: %*s, token type %d\n", i, t.len,t.s, t.type);
        
        //ASSERT((t.s == NULL && expected[i] == NULL) ||  (t.s != NULL && expected[i] && !strcmp(t.s, expected[i])))
        if (t.s != NULL) {
            ASSERT(expected[i] != NULL)
            ASSERT( !strncmp(expected[i], t.s, t.len))
        }
        ASSERT( t.type == etypes[i] )
        i++;
        if (t.type == T_END) {
            break;
        }
    }
    
    
   
    return 0;
}

int testFuzzy() {
    
    TrieNode root;
    TrieNode_Init(&root, 0);
    
    TrieNode_Add(&root, "hello", strlen("hello"));
    TrieNode_Add(&root, "jello", strlen("hello"));
    TrieNode_Add(&root, "hello world", strlen("hello world"));
    
    TrieNode_FuzzyMatches(&root, "yello", strlen("yello"), 2);
    
    return 0;
}

typedef union {
  int i;
  float f;
 } u;
 
int main(int argc, char **argv) {
//   u u1;
//  u1.f = 3.0;
//  /* now u1.i refers to the int version of the float */
//  printf("%d",u1.i);

     LOGGING_INIT(L_INFO);
    // LOGGING_LEVEL = L_DEBUG;
    //     TESTFUNC(testVarint);
    //     TESTFUNC(testDistance);
    //TESTFUNC(testIndexReadWrite);
    //TESTFUNC(testIntersection);
    //  TESTFUNC(testReadIterator);
    //   TESTFUNC(testUnion);

    //   TESTFUNC(testMemBuffer);
    //   TESTFUNC(testTokenize);
    //   TESTFUNC(testForwardIndex);
    //   TESTFUNC(testIndexSpec);
    //TESTFUNC(testQueryTokenize);
    TESTFUNC(testFuzzy)
  return 0;
}