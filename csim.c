/**
 * Name: Patrick Wang
 * AndrewID: taowang
 */

#include "cachelab.h"
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct line_s line_t;
struct line_s {
    int dirty_bit;
    int valid_bit;
    long tag;
    line_t *next;
};

typedef struct {
    int line_count;
    line_t *first_line;
    line_t *last_line;
} set_t;

#define COLD_MISS_TYPE -1
#define CAPACITY_MISS_TYPE -2
#define UNIT_SIZE 1
#define BUFFER_SIZE 50

/**
 * Pass parameters from command line to corresponding global variables
 */
void set_global_values(int argc, char *argv[]);

/**
 * Parsing each line in the file to get certain info to position a cache line
 */
void parsing_line(char *s, int *is_load, long *tag, long *set_num);

/**
 * Safely use calloc, when calloc is not able to allocate memory
 */
void *xcalloc(size_t num, size_t size);

/**
 * when return value >= 0, there is a conflict miss, return value is its
 * positon; when return value is -1, there is a cold miss; when return value is
 * -2, there is a capacity miss.
 */
int miss_type(set_t set, long set_num, long tag);

/**
 * Initialize or update this line
 */
void init_line(line_t *line, long tag);

/**
 * Get line by its position in a certain set.
 */
line_t *get_line(set_t set, int pos);

/**
 * Free all memory that have allocated in heap
 */
void free_mem(set_t *sets, csim_stats_t *stats);

/**
 * Get the result of pow(2,index)
 * The input and return value are integers.
 */
int pow2(int index);

/**
 * Safely open file in read-only mode
 */
FILE *file_open(char *file_name);

int s;
int E;
int b;
int verbose = 0;
int help = 0;
int total_sets;
int total_bytes;
char file_name[50];
long set_mask;

int main(int argc, char *argv[]) {
    set_global_values(argc, argv);
    set_t *sets = xcalloc(total_sets, sizeof(set_t));
    csim_stats_t *stats = xcalloc(UNIT_SIZE, sizeof(csim_stats_t));
    char buffer[BUFFER_SIZE];
    FILE *stream = file_open(file_name);
    int i = 1;

    while (fgets(buffer, 50, stream)) // read from each line
    {
        // load = 1, store = 0
        int is_load;
        long tag, set_num;
        parsing_line(buffer, &is_load, &tag, &set_num);
        if (verbose == 1)
            printf("line_num = %d,is_load = %d, set_num = %ld, tag = %lx", i++,
                   is_load, set_num, tag);

        set_t *this_set = sets + set_num;
        int type = miss_type(*this_set, set_num, tag);

        // miss
        if (type == COLD_MISS_TYPE) {
            if (verbose == 1)
                printf(" miss\n");
            // init a new line
            line_t *new_line = xcalloc(UNIT_SIZE, sizeof(line_t));
            init_line(new_line, tag);

            if (this_set->line_count == 0) {
                this_set->first_line = new_line;
                this_set->last_line = new_line;
                this_set->line_count = 1;
            } else {
                this_set->line_count = this_set->line_count + 1;
                this_set->last_line->next = new_line;
                this_set->last_line = new_line;
            }

            if (is_load == 0) {
                stats->dirty_bytes = stats->dirty_bytes + total_bytes;
                new_line->dirty_bit = 1;
            }

            // update stats info
            stats->misses = stats->misses + 1;
        }

        // miss eviction
        if (type == CAPACITY_MISS_TYPE) {
            if (verbose == 1)
                printf(" miss eviction\n");

            // init a new line
            line_t *new_line = xcalloc(UNIT_SIZE, sizeof(line_t));
            init_line(new_line, tag);

            // use LRU
            line_t *first_line = this_set->first_line;
            if (sets[set_num].line_count == 1) {
                this_set->first_line = new_line;
                this_set->last_line = new_line;
            } else {
                this_set->first_line = first_line->next;
                this_set->last_line->next = new_line;
                this_set->last_line = new_line;
            }

            // update stats info
            stats->misses = stats->misses + 1;
            stats->evictions = stats->evictions + 1;
            // evict dirty update stats info
            if (first_line->dirty_bit == 1) {
                stats->dirty_evictions = stats->dirty_evictions + total_bytes;
                stats->dirty_bytes = stats->dirty_bytes - total_bytes;
            }

            if (is_load == 0) {
                stats->dirty_bytes = stats->dirty_bytes + total_bytes;
                new_line->dirty_bit = 1;
            }

            // free memory
            free(first_line);
        }

        // hit
        if (type >= 0) {
            if (verbose == 1)
                printf(" hit\n");

            // update new_line's position
            line_t *prev_line, *cur_line;
            if (type == this_set->line_count - 1)
                cur_line = this_set->last_line;
            else if (type == 0) {
                // LRU, first line
                cur_line = this_set->first_line;
                this_set->first_line = cur_line->next;

                this_set->last_line->next = cur_line;
                this_set->last_line = cur_line;
                cur_line->next = NULL;
            } else {
                // LRU, other line
                prev_line = get_line(*this_set, type - 1);
                cur_line = prev_line->next;

                prev_line->next = cur_line->next;
                this_set->last_line->next = cur_line;
                this_set->last_line = cur_line;
                cur_line->next = NULL;
            }

            // update stats info
            stats->hits = stats->hits + 1;
            if (is_load == 0 && cur_line->dirty_bit == 0) {
                stats->dirty_bytes = stats->dirty_bytes + total_bytes;
                // update dirty_bit in the current line
                cur_line->dirty_bit = 1;
            }
        }
    }
    fclose(stream);
    printSummary(stats);
    free_mem(sets, stats);
}

void set_global_values(int argc, char *argv[]) {
    // Read values from command line
    int opt;
    /* looping over arguments */
    while ((opt = getopt(argc, argv, "s:E:b:t:vh")) > 0) {
        switch (opt) {
        case 'h':
            help = 1;
            break;
        case 'v':
            verbose = 1;
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
            printf("wrong argument\n");
            break;
        }
    }

    // set other global values
    total_sets = pow2(s);
    total_bytes = pow2(b);
    set_mask = total_sets - 1;
}

void parsing_line(char *buffer, int *is_load, long *tag, long *set_num) {
    long addr;
    int byte_num;
    char c;
    sscanf(buffer, "%c %lx %*c %d", &c, &addr, &byte_num);

    // set load
    *is_load = 1;
    if (c == 'S')
        *is_load = 0;

    // set set_num
    addr = addr >> b;
    *set_num = addr & set_mask;

    // set tag
    *tag = (addr >> s);
}

void *xcalloc(size_t num, size_t size) {
    void *p = calloc(num, size);
    if (p == NULL)
        abort();
    return p;
}

int miss_type(set_t set, long set_num, long tag) {

    int line_count = set.line_count;
    // miss
    if (line_count == 0)
        return -1;

    line_t *line = set.first_line;

    for (int i = 0; i < line_count; i++) {
        if (line->tag == tag) { // hit
            return i;
        }
        line = line->next;
    }

    // miss eviction
    if (line_count == E)
        return -2;

    // miss
    return -1;
}

void init_line(line_t *line, long tag) {
    line->tag = tag;
    line->valid_bit = 1;
    line->dirty_bit = 0;
}

line_t *get_line(set_t set, int pos) {
    line_t *line = set.first_line;
    for (int i = 0; i < pos; i++)
        line = line->next;
    return line;
}

void free_mem(set_t *sets, csim_stats_t *stats) {

    free(stats);

    // free all lines
    for (int i = 0; i < total_sets; i++) {
        set_t set = sets[i];
        if (set.line_count > 0) {
            line_t *cur = set.first_line;
            line_t *next = cur->next;
            free(cur);
            while (next) {
                cur = next;
                next = next->next;
                free(cur);
            }
        }
    }

    // free sets
    free(sets);
}

int pow2(int index) {
    if (index == 0)
        return 1;
    int half_res = pow2(index / 2);
    if (index % 2 == 0)
        return half_res * half_res;
    else
        return 2 * half_res * half_res;
}

FILE *file_open(char *file_name) {
    FILE *stream = fopen(file_name, "r");
    if (stream == NULL)
        abort();
    return stream;
}