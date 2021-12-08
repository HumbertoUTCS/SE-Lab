/*
 * cache.c - A cache simulator that can replay traces from Valgrind
 *     and output statistics such as number of hits, misses, and
 *     evictions, both dirty and clean.  The replacement policy is LRU. 
 *     The cache is a writeback cache. 
 * 
 * Updated 2021: M. Hinton
 */
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include "cache.h"

#define ADDRESS_LENGTH 64
//#define GRAB_SET_INDEX(uword_t x) (5)

/* Counters used to record cache statistics in printSummary().
   test-cache uses these numbers to verify correctness of the cache. */

//Increment when a miss occurs
int miss_count = 0;

//Increment when a hit occurs
int hit_count = 0;

//Increment when a dirty eviction occurs
int dirty_eviction_count = 0;

//Increment when a clean eviction occurs
int clean_eviction_count = 0;

/* TODO: add more globals, structs, macros if necessary */
uword_t globalLru = 0;

/*
 * Initialize the cache according to specified arguments
 * Called by cache-runner so do not modify the function signature
 *
 * The code provided here shows you how to initialize a cache structure
 * defined above. It's not complete and feel free to modify/add code.
 */
cache_t *create_cache(int s_in, int b_in, int E_in, int d_in)
{
    /* see cache-runner for the meaning of each argument */
    cache_t *cache = malloc(sizeof(cache_t));
    cache->s = s_in;
    cache->b = b_in;
    cache->E = E_in;
    cache->d = d_in;
    unsigned int S = (unsigned int) pow(2, cache->s);
    unsigned int B = (unsigned int) pow(2, cache->b);

    cache->sets = (cache_set_t*) calloc(S, sizeof(cache_set_t));
    for (unsigned int i = 0; i < S; i++){
        cache->sets[i].lines = (cache_line_t*) calloc(cache->E, sizeof(cache_line_t));
        for (unsigned int j = 0; j < cache->E; j++){
            cache->sets[i].lines[j].valid = 0;
            cache->sets[i].lines[j].tag   = 0;
            cache->sets[i].lines[j].lru   = 0;
            cache->sets[i].lines[j].dirty = 0;
            cache->sets[i].lines[j].data  = calloc(B, sizeof(byte_t));
        }
    }

    /* TODO: add more code for initialization */

    return cache;
}

cache_t *create_checkpoint(cache_t *cache) {
    unsigned int S = (unsigned int) pow(2, cache->s);
    unsigned int B = (unsigned int) pow(2, cache->b);
    cache_t *copy_cache = malloc(sizeof(cache_t));
    memcpy(copy_cache, cache, sizeof(cache_t));
    copy_cache->sets = (cache_set_t*) calloc(S, sizeof(cache_set_t));
    for (unsigned int i = 0; i < S; i++) {
        copy_cache->sets[i].lines = (cache_line_t*) calloc(cache->E, sizeof(cache_line_t));
        for (unsigned int j = 0; j < cache->E; j++) {
            memcpy(&copy_cache->sets[i].lines[j], &cache->sets[i].lines[j], sizeof(cache_line_t));
            copy_cache->sets[i].lines[j].data = calloc(B, sizeof(byte_t));
            memcpy(copy_cache->sets[i].lines[j].data, cache->sets[i].lines[j].data, sizeof(byte_t));
        }
    }
    
    return copy_cache;
}

void display_set(cache_t *cache, unsigned int set_index) {
    unsigned int S = (unsigned int) pow(2, cache->s);
    if (set_index < S) {
        cache_set_t *set = &cache->sets[set_index];
        for (unsigned int i = 0; i < cache->E; i++) {
            printf ("Valid: %d Tag: %llx Lru: %lld Dirty: %d\n", set->lines[i].valid, 
                set->lines[i].tag, set->lines[i].lru, set->lines[i].dirty);
        }
    } else {
        printf ("Invalid Set %d. 0 <= Set < %d\n", set_index, S);
    }
}

/*
 * Free allocated memory. Feel free to modify it
 */
void free_cache(cache_t *cache)
{
    unsigned int S = (unsigned int) pow(2, cache->s);
    for (unsigned int i = 0; i < S; i++){
        for (unsigned int j = 0; j < cache->E; j++) {
            free(cache->sets[i].lines[j].data);
        }
        free(cache->sets[i].lines);
    }
    free(cache->sets);
    free(cache);
}

/* TODO: CHECK MARK x2
 * Get the line for address contained in the cache
 * On hit, return the cache line holding the address
 * On miss, returns NULL
 */
cache_line_t *get_line(cache_t *cache, uword_t addr)
{
    /* your implementation */

    // unsigned long bigS = pow(2, cache -> s);
    // unsigned long bigB = pow(2, cache -> b);
    uword_t setIndex = addr >> cache -> b;
    setIndex = setIndex & ((uword_t)pow(2, cache -> s) - 1);

    for (unsigned int j = 0; j < cache -> E; j++){
        //Going through all lines hopefully
        if (cache -> sets[setIndex].lines[j].tag == ((addr >> (cache -> s + cache -> b)) & ((uword_t)pow(2, ADDRESS_LENGTH - cache -> s - cache -> b) - 1)) && cache -> sets[setIndex].lines[j].valid) {
            //XXX : why the dots and the arrows; all arrows ?
            (cache -> sets[setIndex].lines[j]).lru = globalLru;
            globalLru++;
            // hit_count++;
            // cache -> sets[setIndex].lines[j].valid = true;
            return &(cache -> sets[setIndex].lines[j]);
        }
    }
    // miss_count++;
    return NULL;
}

/* TODO: CHECK MARK
 * Select the line to fill with the new cache line
 * Return the cache line selected to filled in by addr
 */
cache_line_t *select_line(cache_t *cache, uword_t addr)
{
    /* your implementation */

    uword_t setIndex = addr >> cache -> b;
    setIndex = setIndex & ((uword_t)pow(2, cache -> s) - 1);



    for (unsigned int j = 0; j < cache -> E; j++){
        //Going through all lines hopefully
        if (cache -> sets[setIndex].lines[j].valid == 0) { //TODO check 0 or 1
             //XXX : why the dots and the arrows; all arrows ?
            //  (cache -> sets[setIndex].lines[j]).lru = globalLru;
            //  globalLru++;
             return &(cache -> sets[setIndex].lines[j]);
        }

    }

    //Made it this far? all lines are invalid
    //get oldest one (lru)
    uword_t oldestLru = cache -> sets[setIndex].lines[0].lru;
    cache_line_t * oldestLine = &(cache -> sets[setIndex].lines[0]);

    for (unsigned int j = 0; j < cache -> E; j++){
        //Going through all lines hopefully
        if (cache -> sets[setIndex].lines[j].lru < oldestLru) {
             oldestLru = cache -> sets[setIndex].lines[j].lru;
             oldestLine = &(cache -> sets[setIndex].lines[j]);
        }
    }
    // oldestLine -> lru = globalLru;
    // globalLru++;
    return oldestLine;
}

/* TODO: CHECK MARK
 * Check if the address is hit in the cache, updating hit and miss data.
 * Return true if pos hits in the cache.
 */
bool check_hit(cache_t *cache, uword_t addr, operation_t operation)
{
  
    cache_line_t * possibleLine = get_line(cache, addr);
    if(possibleLine == NULL) {
        miss_count++;
        return false;
    } else if(possibleLine -> valid) {
        hit_count++;
        if(operation == WRITE) {
            possibleLine -> dirty = 1;
        }
        return true;
    }
    miss_count++;
    return false;
}

/* TODO:
 * Handles Misses, evicting from the cache if necessary.
 * Fill out the evicted_line_t struct with info regarding the evicted line.
 */
evicted_line_t *handle_miss(cache_t *cache, uword_t addr, operation_t operation, byte_t *incoming_data)
{
    size_t B = (size_t)pow(2, cache->b);
    evicted_line_t *evicted_line = malloc(sizeof(evicted_line_t));
    evicted_line->data = (byte_t *) calloc(B, sizeof(byte_t));
    /* your implementation */
    
    // we know its a miss; make it dependent on operation
    cache_line_t * selectedLine = (cache_line_t *) select_line(cache, addr);
    if(selectedLine -> valid && selectedLine -> dirty) {
        dirty_eviction_count++;
    } else if(selectedLine -> valid && !(selectedLine -> dirty)) {
        clean_eviction_count++;
    }

    //Tis bad
    //Tis good
    //Tis really good
    //TIS REALLY BAD
    
    //selectedLine -> data = incoming_data;
    if(selectedLine -> data != NULL) {
        memcpy(evicted_line -> data, selectedLine -> data, B);
    }
    if(incoming_data != NULL) {
        memcpy(selectedLine -> data, incoming_data, B);
    }
    
    evicted_line -> valid = selectedLine -> valid;
    evicted_line -> dirty = selectedLine -> dirty;
    if(operation == READ) {
        selectedLine -> dirty = false;
    } else { //OPERATION WRITE
        selectedLine -> dirty = true;
    }
    // evicted_line -> addr = addr;
    //change addr to keep the set index since the tag and the offset bits(0) don't match; grab from selected_line -> tag 
    evicted_line -> addr = ((selectedLine -> tag) << (cache -> s + cache -> b)) + (((addr >> (cache -> b)) & ((uword_t)pow(2,cache -> s)-1)) << (cache -> b));


    selectedLine -> valid = true;
    selectedLine -> tag = (addr >> (cache -> s + cache -> b));// & ((uword_t)pow(2, ADDRESS_LENGTH - cache -> s - cache -> b) - 1);
    selectedLine -> lru = globalLru;
    globalLru++;

    return evicted_line;
}

/* TODO:
 * Get a byte from the cache and write it to dest.
 * Preconditon: pos is contained within the cache.
 */
void get_byte_cache(cache_t *cache, uword_t addr, byte_t *dest)
{
    /* your implementation */
    // //size_t B = (size_t)pow(2, cache->b);
    //XXX: NO CHANGE WITH OR WITHOUT ANYHTING
    size_t offset = addr & ((uword_t)pow(2, cache -> b) - 1);
    cache_line_t * gottenLine = get_line(cache, addr);
    memcpy(dest, &(gottenLine -> data[offset]), 1);
}


/* TODO:
 * Get 8 bytes from the cache and write it to dest.
 * Preconditon: pos is contained within the cache.
 */
void get_word_cache(cache_t *cache, uword_t addr, word_t *dest) {

    /* your implementation */
    // cache_line_t * gottenLine = get_line(cache, addr);
    // // memcpy(dest, gottenLine -> tag, ADDRESS_LENGTH - cache -> s - cache -> b);
    // dest =(word_t *)(gottenLine -> tag);//TODO Is this WRITE?
    //size_t offset = addr & ((uword_t)pow(2, cache -> b) - 1);
    // cache_line_t * gottenLine = get_line(cache, addr);
    //memcpy(dest, &(gottenLine -> data[offset]), 8);
    for(int i=0; i<8; i++) {
        get_byte_cache(cache, addr+i, ((byte_t*)(dest))+i);
    }
}


/* TODO:
 * Set 1 byte in the cache to val at pos.
 * Preconditon: pos is contained within the cache.
 */
void set_byte_cache(cache_t *cache, uword_t addr, byte_t val)
{
    /* your implementation */
    //size_t B = (size_t)pow(2, cache->b);
    //XXX: NO CHANGE WITH OR WITHOUT????
    size_t offset = addr & ((uword_t)pow(2, cache -> b) - 1);
    cache_line_t * gottenLine = get_line(cache, addr);
    memcpy(&(gottenLine -> data[offset]), &val, 1);
}


/* TODO:
 * Set 8 bytes in the cache to val at pos.
 * Preconditon: pos is contained within the cache.
 */
void set_word_cache(cache_t *cache, uword_t addr, word_t val)
{
    /* your implementation */
    // cache_line_t * gottenLine = get_line(cache, addr);
    // // memcpy(gottenLine -> tag, val, ADDRESS_LENGTH - cache -> s - cache -> b);
    // gottenLine -> tag= val;
    //size_t offset = addr & ((uword_t)pow(2, cache -> b) - 1);
    //cache_line_t * gottenLine = get_line(cache, addr);
    
    //ANDREW SAID 8 INSTEAD OF 1
    //memcpy(&((gottenLine -> data)[offset]), &val, 1);
    // memcpy(&(gottenLine -> data[offset]), &val, 8);
    for(int i=0; i<8; i++) {
        set_byte_cache(cache, addr+i, ((byte_t*)(&val))[i]);
    }
}

/*
 * Access data at memory address addr
 * If it is already in cache, increast hit_count
 * If it is not in cache, bring it in cache, increase miss count
 * Also increase eviction_count if a line is evicted
 *
 * Called by cache-runner; no need to modify it if you implement
 * check_hit() and handle_miss()
 */
void access_data(cache_t *cache, uword_t addr, operation_t operation)
{
    if(!check_hit(cache, addr, operation))
        free(handle_miss(cache, addr, operation, NULL));
}