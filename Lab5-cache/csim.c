#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include "cachelab.h"
#define MAXLEN 1000   //max length of the name of file
const int m = 64;   

typedef struct{
    int valid;
    int tag;
    int last_used;
}Cache_line;

typedef Cache_line** Cache;

Cache cache;
int help, verbous, time, t, s, S, E, b, B;
char file_name[MAXLEN];
int hit_count, miss_count, eviction_count;

void init_cache();
void free_cache();
void print_usage();
void read_trace();
void update(unsigned int addr);

int main(int argc, char** argv)
{
    help = verbous = 0;
    
    int opt;
    while((opt = getopt(argc, argv, "hvs:E:b:t:")) != -1){  //"s", "E", "b", "t" must be included
        switch (opt)
        {
        case 'h':
            help = 1;
            print_usage();
            break;
        case 'v':
            verbous = 1;
            break;
        case 's':
            s = atoi(optarg);
            break;
        case 'E':
            E = atoi(optarg);
            break;
        case 'b':
            b = atoi(optarg);
            break;
        case 't':
            strcpy(file_name, optarg);
            break;
        default:
            print_usage();
            break;
        }
    }

    if(s <= 0 || E <= 0 || b <= 0 || file_name == NULL){ 
        printf("invalid arguments\n");
        exit(-1);
    }
    S = 1 << s;
    B = 1 << b;
    
    init_cache();

    read_trace();
    
    free_cache();

    printSummary(hit_count, miss_count, eviction_count);
    return 0;
}

void print_usage()
{
    printf("Usage: ./csim-ref [-hv] -s <num> -E <num> -b <num> -t <file>\n"
            "Options:\n"
            "  -h         Print this help message.\n"
            "  -v         Optional verbose flag.\n"
            "  -s <num>   Number of set index bits.\n"
            "  -E <num>   Number of lines per set.\n"
            "  -b <num>   Number of block offset bits.\n"
            "  -t <file>  Trace file.\n\n"
            "Examples:\n"
            "  linux>  ./csim-ref -s 4 -E 1 -b 4 -t traces/yi.trace\n"
            "  linux>  ./csim-ref -v -s 8 -E 2 -b 4 -t traces/yi.trace\n");
}

void init_cache()
{
    hit_count = miss_count = eviction_count = 0; 
    time = 0;
    cache = (Cache)malloc(S * sizeof(Cache_line*));
    for(int i = 0; i < S; i++){
        cache[i] = (Cache_line*)malloc(E * sizeof(Cache_line));
        cache[i]->valid = 0;
        cache[i]->tag = -1;
        cache[i]->last_used = 0;
    }
}

void free_cache()
{
    for(int i = 0; i < S; i++)
        free(cache[i]);
    free(cache);
}

void read_trace()
{
    FILE* fp = fopen(file_name, "r");
    if(fp == NULL){
        printf("No such file or directory\n");
        exit(-1);
    }

    char op;
    unsigned int addr;
    int size;
    while(fscanf(fp, " %c %x,%d\n", &op, &addr, &size) != -1){
        if(verbous)
            printf("%c %x,%d", op, addr, size);
        switch(op){
            case 'M':
                update(addr);
            case 'L':
            case 'S':
                update(addr);
                break;
            default:
                break;
        }
        if(verbous)
            putchar('\n');
    }
    fclose(fp);
}

void update(unsigned int addr)
{
    int tag = (addr >> (s + b));
    int idx = (addr >> b) & ((-1U) >> (m - s));
    ++time;
    for(int i = 0; i < E; i++){
        if(cache[idx][i].valid && cache[idx][i].tag == tag){    //hit
            ++hit_count;
            if(verbous)
                printf(" hit");
            cache[idx][i].last_used = time;
            return;
        }
    }

    for(int i = 0; i < E; i++){
        if(!cache[idx][i].valid){   //empty cache line
            ++miss_count;
            if(verbous)
                printf(" miss");
            cache[idx][i].valid = 1;
            cache[idx][i].tag = tag;
            cache[idx][i].last_used = time;
            return;
        }
    }

    ++miss_count;
    ++eviction_count;
    if(verbous)
        printf(" miss eviction");
    int least_recentently_used = 1000000000;
    int least_idx = 0;
    for(int i = 0; i < E; i++){
        if(cache[idx][i].valid && cache[idx][i].last_used < least_recentently_used){
            least_recentently_used = cache[idx][i].last_used;
            least_idx = i;
        }
    }
    cache[idx][least_idx].tag = tag;
    cache[idx][least_idx].last_used = time;
}
