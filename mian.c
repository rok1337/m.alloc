#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define HEAP_SIZE 1024 * 1024 * 1024 // 1GB
static char heap[HEAP_SIZE];
static void *heap_end = heap;

void *sbrk(intptr_t increment){
    //static void *heap_start = heap;
    void *prev_heap_end = heap_end;

    if((char *) heap_end + increment > heap + HEAP_SIZE || (char *) heap_end + increment < heap){
        return (void *) -1;
    }

    heap_end = (char *) heap_end + increment;
    return prev_heap_end;
}

struct block_meta {
    size_t size;
    unsigned is_free;
    struct header_t *next;
};

typedef  char ALIGN[16];

union header {
    struct {
        size_t size;
        unsigned is_free;
        union header *next;
    } s;
};

typedef union header header_t;

// Global variables
header_t *head = NULL, *tail = NULL;
pthread_mutex_t global_malloc_lock;

header_t *get_free_block(size_t size);

//implementing malloc
void *malloc(size_t size) {
    size_t total_size;
    void *block;
    header_t *header;

    if (!size) {
        return NULL;
    }

    pthread_mutex_lock(&global_malloc_lock);
    header = get_free_block(size);

    if (header) {
        header->s.is_free = 0;
        pthread_mutex_unlock(&global_malloc_lock);
        return (void*)(header + 1);
    }

    total_size = sizeof(header_t) + size;
    block = sbrk(total_size);

    if (block == (void*) -1) {
        pthread_mutex_unlock(&global_malloc_lock);
        return NULL;
    }

    header = block;
    header->s.size = size;
    header->s.is_free = 0;
    header->s.next = NULL;

    if (!head) {
        head = header;
    }

    if (tail) {
        tail->s.next = header;
    }

    tail = header;
    pthread_mutex_unlock(&global_malloc_lock);
    return (void*)(header + 1);
}

//implementing free
void free(void *block) {
    header_t *header, *tmp;
    void *programbreak;

    if (!block) {
        return;
    }

    pthread_mutex_lock(&global_malloc_lock);
    header = (header_t*)block - 1;

    programbreak = sbrk(0);

    if ((char*)block + header->s.size == programbreak) {
        if (head == tail) {
            head = tail = NULL;
        } else {
            tmp = head;
            while (tmp) {
                if (tmp->s.next == tail) {
                    tmp->s.next = NULL;
                    tail = tmp;
                }
                tmp = tmp->s.next;
            }
        }

        sbrk(0 - sizeof(header_t) - header->s.size);
        pthread_mutex_unlock(&global_malloc_lock);
        return;
    }

    header->s.is_free = 1;
    pthread_mutex_unlock(&global_malloc_lock);
}

//implementing calloc   
void *calloc(size_t num, size_t nsize) {
    size_t size;
    void *block;

    if (!num || !nsize) {
        return NULL;
    }

    size = num * nsize;
    if (nsize != size / num) {
        return NULL;
    }

    block = malloc(size);
    if (!block) {
        return NULL;
    }

    memset(block, 0, size);
    return block;
}

//implementing realloc
void *realloc(void *block, size_t size) {
    header_t *header;
    void *ret;
    if (!block || !size) {
        return malloc(size);
    }

    header = (header_t*)block - 1;
    if (header->s.size >= size) {
        return block;
    }

    ret = malloc(size);
    if (ret) {
        memcpy(ret, block, header->s.size);
        free(block);
    }
    return ret;
}

//get free block    
header_t *get_free_block(size_t size) {
    header_t *curr = head;
    while (curr) {
        if (curr->s.is_free && curr->s.size >= size) {
            return curr;
        }
        curr = curr->s.next;
    }
    return NULL;
}

//main function
int main() {
    printf("testing malloc\n");
    printf("malloc: \n");
    int *p = (int *)malloc(10 * sizeof(int));
    if(p){
        for (int i = 0; i < 10; i++) {
            p[i] = i + 1;
        }
    }
    for(int i = 0; i < 10; i++){
        printf("%d\n", p[i]);
    }
  
    //calloc
    int *q = (int *)calloc(10, sizeof(int));
    if(q){
        printf("calloc: \n");
        for(int i = 0; i < 10; i++){
            printf("%d\n", q[i]);
        }
    }


    //reealloc
    q = (int *)realloc(q, 20 * sizeof(int));
    if (q)
    {
        for(int i = 0; i < 20; i++){
            printf("%d\n", q[i]);
        }
        printf("realloc: \n");
        for(int i = 10; i < 20; i++){
            q[i] = i + 1;
        }

    }
    free(q);
    printf("free: \n");
    return 0;    
}