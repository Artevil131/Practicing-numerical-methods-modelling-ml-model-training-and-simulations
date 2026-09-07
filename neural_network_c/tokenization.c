/*
 * tokenization.c — EXERCISE 1: character-level tokenizer
 *
 * Compile:  cc -Wall -Wextra -O2 -o tokenization tokenization.c
 * Run:      ./tokenization
 *
 * What a tokenizer is: a bijection between text and integers.
 *   "hello"  --encode-->  [7, 4, 11, 11, 14]  --decode-->  "hello"
 * The model never sees text, only these ints (then an embedding table turns
 * each int into a vector — that's Phase 2).
 *
 * ---------------------------------------------------------------------------
 * PART A — char-level (do this first, ~40 lines)
 * ---------------------------------------------------------------------------
 * 1. Build the vocabulary from a training string:
 *      - every DISTINCT byte that appears gets an id 0..vocab_size-1
 *      - hint: a char is a number 0..255, so `int char_to_id[256]` initialised
 *        to -1 is your whole "dict". `char id_to_char[256]` is the reverse.
 *      - walk the string; if char_to_id[(unsigned char)c] == -1, assign next id.
 *
 * 2. encode(const char *text, int *out) -> int
 *      - writes ids into `out`, returns how many it wrote.
 *      - what should happen on a char not in the vocab? Decide. (GPT-2 avoids this
 *        by using bytes as the base vocab — 256 entries, nothing is ever unknown.)
 *
 * 3. decode(const int *ids, int n, char *out)
 *      - writes chars, then the '\0' terminator. Don't forget it.
 *
 * 4. In main(): train on a sentence, print the vocab, encode "hello", print the
 *    ids, decode them back, assert the round-trip is identical (strcmp == 0).
 *
 * ---------------------------------------------------------------------------
 * PART B — byte-pair encoding (BPE; what GPT actually uses)
 * ---------------------------------------------------------------------------
 * Start from Part A's ids as a sequence. Repeat `num_merges` times:
 *   1. count how often every ADJACENT PAIR (ids[i], ids[i+1]) occurs
 *   2. find the most frequent pair (a, b)
 *   3. create new token id = vocab_size++ ; record merge (a, b) -> new id
 *   4. replace every occurrence of a,b in the sequence with the new id
 * Encoding new text = apply merges in the order they were learned.
 * Decoding = recursively expand ids > 255 back to their pair until only bytes remain.
 *
 * C lessons hidden inside Part B:
 *   - the sequence shrinks as you merge → you need a growable IntVec (see 00_c_basics.c)
 *   - counting pairs: for a small vocab a 2D array counts[vocab][vocab] works;
 *     for a real vocab that's 50k*50k*4 bytes = 10 GB → you need a hash map.
 *     Write the 2D array version first, feel the wall, then write the hash map.
 *
 * Reference to check yourself against: Karpathy's minbpe (Python). Feed both the
 * same text and the same num_merges; the merge lists must be identical.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* your code here */

int main(void) {
    const char *train = "the quick brown fox jumps over the lazy dog";
    (void)train;   /* delete this line once you use `train` — it just silences the unused warning */

    return 0;
}
