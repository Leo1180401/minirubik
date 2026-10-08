/* Host validation for solver_basic.c. H1: lower-bound admissibility;
 * H2: no PDB (N/A); H3: optimal solve and independent replay.
 * --full is exhaustive and can be very slow. --tables-only skips solving.
 * This program does not measure target memory or RISC-V instructions. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#ifndef _WIN32
#include <sys/time.h>
#endif
#ifdef _WIN32
#include <windows.h>
#endif

/* Wall time, not CPU time. Windows is the measurement platform here. */
static double wall_seconds(void)
{
#ifdef _WIN32
    LARGE_INTEGER frequency, counter;
    if (!QueryPerformanceFrequency(&frequency) ||
        !QueryPerformanceCounter(&counter)) {
        fputs("FAIL: performance timer unavailable\n", stderr);
        exit(1);
    }
    return (double) counter.QuadPart / (double) frequency.QuadPart;
#else
    struct timeval t;
    if (gettimeofday(&t, NULL) != 0) exit(1);
    return (double)t.tv_sec + (double)t.tv_usec / 1e6;
#endif
}

/* Rename the included entry point; the solver source stays unchanged. */
#define main solver_basic_main
#include "solver_basic.c"
#undef main
#define CUBIES C


enum {
    PERMUTATIONS = 5040,
    ORIENTATIONS = 729,
    STATES = PERMUTATIONS * ORIENTATIONS,
    MOVES = 9,
    MAX_DEPTH = 11,
    UNVISITED = UINT8_MAX
};

static const uint8_t inverse_move[MOVES] = {2, 1, 0, 5, 4, 3, 8, 7, 6};

/* Baseline move rules copied from solver.c, kept independent of IDA*. */
static const uint8_t oracle_source[3][CUBIES] = {
    {1, 4, 2, 0, 3, 5, 6},
    {0, 1, 2, 4, 5, 6, 3},
    {0, 2, 5, 3, 1, 4, 6},
};
static const uint8_t oracle_twist[3][CUBIES] = {
    {1, 2, 0, 2, 1, 0, 0},
    {0, 0, 0, 1, 2, 1, 2},
    {0, 0, 0, 0, 0, 0, 0},
};

/* The three quarter-turns preserve the fixed front-upper-left corner. */
static state_t oracle_quarter_turn(state_t state, uint8_t face)
{
    state_t result;
    for (uint8_t i = 0; i < CUBIES; ++i) {
        uint8_t from = oracle_source[face][i];
        result.p[i] = state.p[from];
        result.o[i] = (uint8_t) ((state.o[from] + oracle_twist[face][i]) % 3U);
    }
    return result;
}

static state_t oracle_apply_move(state_t state, uint8_t move)
{
    uint8_t turns = (uint8_t) (move % 3U + 1U);
    for (uint8_t i = 0; i < turns; ++i)
        state = oracle_quarter_turn(state, (uint8_t) (move / 3U));
    return state;
}


/* Independent dense index: Lehmer permutation rank and six base-3 twists. */
static uint32_t rank_state(const state_t *state)
{
    uint32_t p = 0, o = 0;
    for (uint8_t i = 0; i < CUBIES; ++i) {
        uint8_t smaller = 0;
        for (uint8_t j = (uint8_t) (i + 1U); j < CUBIES; ++j)
            if (state->p[j] < state->p[i])
                ++smaller;
        p = p * (CUBIES - i) + smaller;
    }
    for (uint8_t i = 0; i < 6; ++i)
        o = o * 3U + state->o[i];
    return p * ORIENTATIONS + o;
}

static void unrank_state(uint32_t rank, state_t *state)
{
    uint8_t available[CUBIES] = {0, 1, 2, 3, 4, 5, 6};
    uint32_t p = rank / ORIENTATIONS, o = rank % ORIENTATIONS, f = 720;
    uint8_t sum = 0;
    for (uint8_t i = 0; i < CUBIES; ++i) {
        uint8_t q = (uint8_t) (p / f);
        p %= f;
        state->p[i] = available[q];
        for (uint8_t j = q; j + 1 < CUBIES - i; ++j)
            available[j] = available[j + 1U];
        if (i < 5)
            f /= 6U - i;
    }
    for (uint8_t i = 6; i-- > 0;) {
        state->o[i] = (uint8_t) (o % 3U);
        sum = (uint8_t) (sum + state->o[i]);
        o /= 3U;
    }
    state->o[6] = (uint8_t) ((3U - sum % 3U) % 3U);
}

static int valid(const state_t *state)
{
    uint8_t sum = 0;
    for (uint8_t i = 0; i < CUBIES; ++i) {
        if (state->p[i] >= CUBIES || state->o[i] >= 3)
            return 0;
        for (uint8_t j = 0; j < i; ++j)
            if (state->p[j] == state->p[i])
                return 0;
        sum = (uint8_t) (sum + state->o[i]);
    }
    return sum % 3U == 0;
}

/* Breadth-first search from solved over the whole space. On success returns
 * the move-toward-solved table and fills dist[rank] with the exact HTM
 * distance of every state; dist must hold STATES bytes.
 */
static uint8_t *build_table(uint8_t *dist, uint8_t *diameter)
{
    uint8_t *toward_solved = malloc(STATES);
    uint32_t *queue = malloc((size_t) STATES * sizeof *queue);
    uint16_t permutation[3][PERMUTATIONS], orientation[3][ORIENTATIONS];
    uint32_t head = 0, tail = 1, level_end = 1;
    state_t state;
    if (!toward_solved || !queue) {
        free(toward_solved);
        free(queue);
        return NULL;
    }
    for (uint16_t rank = 0; rank < PERMUTATIONS; ++rank) {
        unrank_state((uint32_t) rank * ORIENTATIONS, &state);
        for (uint8_t face = 0; face < 3; ++face) {
            state_t next = oracle_quarter_turn(state, face);
            permutation[face][rank] =
                (uint16_t) (rank_state(&next) / ORIENTATIONS);
        }
    }
    for (uint16_t rank = 0; rank < ORIENTATIONS; ++rank) {
        unrank_state(rank, &state);
        for (uint8_t face = 0; face < 3; ++face) {
            state_t next = oracle_quarter_turn(state, face);
            orientation[face][rank] =
                (uint16_t) (rank_state(&next) % ORIENTATIONS);
        }
    }
    memset(toward_solved, UNVISITED, STATES);
    memset(dist, UNVISITED, STATES);
    queue[0] = 0;
    toward_solved[0] = 0;
    dist[0] = 0;
    *diameter = 0;
    while (head < tail) {
        if (head == level_end) {
            level_end = tail;
            ++*diameter;
        }
        uint32_t here = queue[head++];
        uint16_t p = (uint16_t) (here / ORIENTATIONS);
        uint16_t o = (uint16_t) (here % ORIENTATIONS);
        for (uint8_t face = 0; face < 3; ++face) {
            uint16_t next_p = p, next_o = o;
            for (uint8_t turn = 0; turn < 3; ++turn) {
                next_p = permutation[face][next_p];
                next_o = orientation[face][next_o];
                uint32_t there = (uint32_t) next_p * ORIENTATIONS + next_o;
                if (toward_solved[there] == UNVISITED) {
                    uint8_t move = (uint8_t) (face * 3U + turn);
                    toward_solved[there] = inverse_move[move];
                    /* BFS reaches every state first along a shortest path. */
                    dist[there] = (uint8_t) (dist[here] + 1U);
                    queue[tail++] = there;
                }
            }
        }
    }
    free(queue);
    if (tail != STATES) {
        free(toward_solved);
        return NULL;
    }
    return toward_solved;
}

/* The dense index must be a bijection, or every per-rank check below would
 * be checking the wrong state.
 */
static int self_test(void)
{
    const state_t solved = {{0, 1, 2, 3, 4, 5, 6}, {0}};
    state_t state;
    for (uint8_t move = 0; move < MOVES; ++move) {
        state = apply_move(solved, move);
        state = apply_move(state, inverse_move[move]);
        if (memcmp(&solved, &state, sizeof solved))
            return 0;
    }
    for (uint32_t rank = 0; rank < STATES; ++rank) {
        unrank_state(rank, &state);
        if (!valid(&state) || rank_state(&state) != rank ||
            is_solved(state) != (rank == 0))
            return 0;
        char text[15];
        state_t parsed;
        for (int i = 0; i < CUBIES; ++i) {
            text[i] = (char)('1' + state.p[i]);
            text[i + CUBIES] = (char)('1' + state.o[i]);
        }
        text[14] = '\0';
        if (!parse(text, &parsed) || memcmp(&parsed, &state, sizeof state))
            return 0;
        for (uint8_t face = 0; face < 3; ++face) {
            state_t actual = turn(state, face);
            state_t expected = oracle_quarter_turn(state, face);
            if (memcmp(actual.p, expected.p, CUBIES) ||
                memcmp(actual.o, expected.o, CUBIES))
                return 0;
        }
    }
    return 1;
}

static int check_bfs(const uint8_t *dist, uint8_t diameter)
{
    uint32_t histogram[MAX_DEPTH + 2] = {0};
    int failures = 0;
    for (uint32_t rank = 0; rank < STATES; ++rank) {
        uint8_t d = dist[rank];
        ++histogram[d > MAX_DEPTH ? MAX_DEPTH + 1 : d];
    }
    printf("BFS: diameter %u\n", (unsigned) diameter);
    for (int d = 0; d <= MAX_DEPTH; ++d)
        printf("  depth %2d: %7lu\n", d, (unsigned long) histogram[d]);
    if (diameter != MAX_DEPTH || histogram[MAX_DEPTH + 1] != 0 ||
        histogram[0] != 1 || dist[0] != 0 || histogram[11] != 2644) {
        printf("FAIL: expected diameter %d with no deeper state\n", MAX_DEPTH);
        ++failures;
    }
    return failures;
}

/* H1: h(s) <= d(s) for every state s. */
static int check_h1(const uint8_t *dist)
{
    uint32_t violations = 0, first = 0;
    uint64_t sum_h = 0, sum_d = 0;
    unsigned max_h = 0;
    for (uint32_t rank = 0; rank < STATES; ++rank) {
        state_t state;
        unrank_state(rank, &state);
        unsigned h = (unsigned) lower_bound(state);
        if (h > max_h) max_h = h;
        sum_h += h;
        sum_d += dist[rank];
        if (h > dist[rank] && violations++ == 0)
            first = rank;
    }
    printf("H1: mean h %.3f, mean distance %.3f\n", (double) sum_h / STATES,
           (double) sum_d / STATES);
    state_t first_state;
    if (violations) {
        printf("H1 FAIL: %lu states overestimated, first rank %lu "
               "(h %u > d %u)\n",
               (unsigned long) violations, (unsigned long) first,
               (unsigned) lower_bound((unrank_state(first, &first_state), first_state)), (unsigned) dist[first]);
        return 1;
    }
    printf("H1 PASS: %d states; max lower_bound = %u\n", STATES, max_h);
    return 0;
}

static int check_h2(const uint8_t *dist)
{
    (void)dist;
    puts("H2: N/A -- solver_basic.c has no generated pattern databases.");
    return 0;
}

/* H3: solve() is optimal and its path is real, on every stride-th state. */
static int check_h3(const uint8_t *dist, uint32_t stride, int distance11_only)
{
    uint32_t checked = 0, wrong = 0;


    double start = wall_seconds();
    double last_progress = start;
    for (uint32_t rank = 0; rank < STATES; rank += stride) {
        if (distance11_only && dist[rank] != MAX_DEPTH)
            continue;
        state_t state, replay;

        unrank_state(rank, &state);
        int length = solve(state);
        int ok = length >= 0 && length <= MAX_DEPTH && length == dist[rank];
        if (ok) {
            replay = state;
            for (int i = 0; i < length; ++i) {
                if (path[i] >= MOVES) { ok = 0; break; }
                replay = oracle_apply_move(replay, path[i]);
            }
            ok = ok && valid(&replay) && rank_state(&replay) == 0;
        }
        if (!ok && wrong++ < 10)
            printf("H3 FAIL: rank %lu: length %d, distance %u\n",
                   (unsigned long) rank, length, (unsigned) dist[rank]);
        ++checked;
        double now = wall_seconds();
        if (now - last_progress >= 10.0) {
            printf("H3 progress: %lu checked; rank %lu/%d; %.1f wall seconds\n",
                    (unsigned long) checked, (unsigned long) rank, STATES, now - start);
            last_progress = now;
        }
        if (wrong) return 1;
    }
    double seconds = wall_seconds() - start;
    printf("H3: %lu states (stride %lu) in %.1f wall seconds\n",
           (unsigned long)checked, (unsigned long)stride, seconds);
    if (wrong) {
        printf("H3 FAIL: %lu states not solved optimally\n",
               (unsigned long) wrong);
        return 1;
    }
    if (!distance11_only && stride == 1 && checked == STATES)
        puts("H3 FULL PASS: all states checked");
    else
        puts("H3 SUBSET PASS ONLY: full-domain H3 remains incomplete");
    return 0;
}

/* The vector every submission reports, 21345671111111. */
static int check_reference_vector(void)
{
    const state_t state = {{1, 0, 2, 3, 4, 5, 6}, {0}};

    int length = solve(state);
    printf("Reference 21345671111111: length %d\n", length);
    if (length != MAX_DEPTH) return 1;
    state_t replay = state;
    for (int i = 0; i < length; ++i) {
        if (path[i] >= MOVES) return 1;
        replay = oracle_apply_move(replay, path[i]);
    }
    return !valid(&replay) || rank_state(&replay) != 0;
}

int main(int argc, char **argv)
{
    uint32_t stride = 100000;
    int full = 0, tables_only = 0, distance11_only = 0;
    int failures = 0;
    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc == 2 && !strcmp(argv[1], "--full")) {
        full = 1; stride = 1;
    } else if (argc == 2 && !strcmp(argv[1], "--tables-only")) {
        tables_only = 1;
    } else if (argc == 2 && !strcmp(argv[1], "--distance11")) {
        distance11_only = 1; stride = 1;
    } else if (argc == 3 && !strcmp(argv[1], "--sample")) {
        char *end;
        errno = 0;
        unsigned long n = strtoul(argv[2], &end, 10);
        if (errno || argv[2][0] < '0' || argv[2][0] > '9' || *end || n < 2 || n > STATES)
            goto usage;
        stride = (uint32_t) n;
    } else if (argc != 1) {
        goto usage;
    }
    printf("Mode: %s\n", full ? "FULL H3" : tables_only ? "H1/H2 only" :
           distance11_only ? "all distance-11 states (not full H3)" : "sample (not full H3)");
    puts("Host validation only; no target memory/performance measurement.");
    puts("Checking full-domain encodings, goal predicate and baseline move agreement...");
    if (!self_test()) {
        printf("FAIL: rank/unrank or move inverse self-test\n");
        return 1;
    }
    puts("PASS: encoding and baseline quarter-turn agreement for all states");
    uint8_t *dist = malloc(STATES);
    uint8_t diameter;
    uint8_t *table = dist ? build_table(dist, &diameter) : NULL;
    if (!table) {
        fprintf(stderr, "FAIL: could not build baseline table\n");
        free(dist);
        return 1;
    }
    failures += check_bfs(dist, diameter);
    failures += check_h1(dist);
    failures += check_h2(dist);
    puts("H4: N/A -- solver state uses unpacked corner arrays.");
    if (!failures && !tables_only) {
        failures += check_reference_vector();
        if (!failures) failures += check_h3(dist, stride, distance11_only);
    }
    if (failures) puts("FAILED");
    else if (full) puts("FULL HOST VERIFICATION PASS (H1/H3; H2/H4 N/A)");
    else puts("REQUESTED CHECKS PASSED; full H3 has NOT been completed by this run");
    free(table);
    free(dist);
    return failures ? 1 : 0;
usage:
    printf("usage: %s [--full | --tables-only | --distance11 | --sample STRIDE]\n", argv[0]);
    return 2;
}
