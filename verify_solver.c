/*
 * Host-only verifier for the reviewed solver_basic.c interfaces:
 * state_t { unsigned char p[7], o[7]; }, parse(), apply_move(),
 * lower_bound(), solve(state_t), and global path[] (move IDs 0..8).
 * Place this file beside YOUR solver_basic.c. It includes that file without
 * changing it; its main is renamed. Compile ONLY this translation unit:
 *   gcc -O3 -std=c99 -Wall -Wextra -Wpedantic verify_solver.c -o verify_solver.exe
 *
 * PowerShell:
 *   .\verify_solver.exe --quick       # 3 cases, NOT exhaustive H3
 *   .\verify_solver.exe --audit       # all-state H1 + all 9 move comparisons
 *   .\verify_solver.exe --all         # exhaustive H3; potentially VERY slow
 *   .\verify_solver.exe --range 0 100 # H3 over ranks [0,100), PARTIAL only
 *   .\verify_solver.exe --state 67354123313333
 *   .\verify_solver.exe --export      # writes bfs_distances.bin, distance11.txt
 * Each mode first rebuilds the exact BFS table and checks H1 over all states.
 * --all additionally performs the full move audit before H3.
 * Exit: 0 selected checks passed, 1 validation/I/O failure, 2 usage error.
 * Exit 0 from a partial mode does NOT mean full H3 passed.
 *
 * Reference model: sysprog21/minirubik R/B/D source and twist convention.
 * Input is external 1-based: 7 permutation digits (1..7), 7 orientations
 * (1..3). Internal values are 0-based. Each of the 9 moves costs ONE step.
 * Independent reference moves are used for BFS and replay, never DUT moves.
 *
 * This verifier uses heap memory (~18.5 MB primary arrays); it is NOT the
 * resource-constrained target solver. Do not measure its size/instructions
 * as the target's. It does not verify Ripes performance or the 128 KiB cap.
 * H2: the reviewed solver has only fixed move/name tables, no generated
 * distance/heuristic tables. --audit checks move table behavior exhaustively.
 * H4: the reviewed solver has no packed accessors, so H4 is not applicable.
 * If you add new tables, heuristics, or packing, extend the relevant checks.
 *
 * Export format: bfs_distances.bin = exactly 3,674,160 unsigned bytes with
 * no header. Index = lexicographic permutation rank * 729 + orientation rank.
 * Orientation rank = o[0]*3^5 + ... + o[5]; o[6] = -sum(o[0..5]) mod 3.
 * The whole file is regenerated; --export overwrites these two output files.
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#ifndef SOLVER_FILE
#define SOLVER_FILE "solver_basic.c"
#endif
#define main verifier_original_solver_main
#include SOLVER_FILE
#undef main

enum { V_P = 5040, V_O = 729, V_N = 3674160, V_M = 9 };
static uint16_t v_pm[V_P][V_M], v_om[V_O][V_M];
static unsigned char *v_dist;
static const unsigned v_factorial[7] = {720,120,24,6,2,1,1};
static const char *const v_names[9] =
    {"R","R2","R'","B","B2","B'","D","D2","D'"};

static void v_fail(const char *message)
{
    fprintf(stderr, "FAIL: %s\n", message);
    free(v_dist);
    exit(1);
}

static int v_equal(state_t a, state_t b)
{
    for (int i=0;i<7;++i)
        if (a.p[i]!=b.p[i] || a.o[i]!=b.o[i]) return 0;
    return 1;
}

static int v_valid(state_t s)
{
    unsigned seen=0, sum=0;
    for (int i=0;i<7;++i) {
        if (s.p[i]>=7 || s.o[i]>=3) return 0;
        if (seen & (1U<<s.p[i])) return 0;
        seen |= 1U<<s.p[i]; sum += s.o[i];
    }
    return sum%3==0;
}

static void v_text(state_t s, char text[15])
{
    for (int i=0;i<7;++i) {
        text[i]=(char)('1'+s.p[i]); text[i+7]=(char)('1'+s.o[i]);
    }
    text[14]='\0';
}

static state_t v_unrank(unsigned id)
{
    state_t s;
    unsigned p=id/V_O, o=id%V_O, sum=0;
    unsigned char available[7]={0,1,2,3,4,5,6};
    for (int i=0;i<7;++i) {
        unsigned digit=p/v_factorial[i]; p%=v_factorial[i];
        s.p[i]=available[digit];
        for (unsigned j=digit;j<(unsigned)(6-i);++j)
            available[j]=available[j+1];
    }
    for (int i=5;i>=0;--i) {
        s.o[i]=(unsigned char)(o%3); sum+=s.o[i]; o/=3;
    }
    s.o[6]=(unsigned char)((3-sum%3)%3);
    return s;
}

static unsigned v_rank(state_t s)
{
    unsigned p=0, o=0;
    for (int i=0;i<7;++i) {
        unsigned smaller=0;
        for (int j=i+1;j<7;++j) smaller += s.p[j]<s.p[i];
        p+=smaller*v_factorial[i];
    }
    for (int i=0;i<6;++i) o=o*3+s.o[i];
    return p*V_O+o;
}

static state_t v_quarter(state_t s, int face)
{
    static const unsigned char src[3][7]={
        {1,4,2,0,3,5,6},{0,1,2,4,5,6,3},{0,2,5,3,1,4,6}};
    static const unsigned char twist[3][7]={
        {1,2,0,2,1,0,0},{0,0,0,1,2,1,2},{0,0,0,0,0,0,0}};
    state_t t;
    for (int i=0;i<7;++i) {
        t.p[i]=s.p[src[face][i]];
        t.o[i]=(unsigned char)((s.o[src[face][i]]+twist[face][i])%3);
    }
    return t;
}

static state_t v_move(state_t s, unsigned move)
{
    for (unsigned i=0;i<move%3+1;++i) s=v_quarter(s,(int)(move/3));
    return s;
}

static unsigned v_next(unsigned id, unsigned move)
{
    return (unsigned)v_pm[id/V_O][move]*V_O+v_om[id%V_O][move];
}

static void v_build(void)
{
    static const unsigned expected[12]={
        1,9,54,321,1847,9992,50136,227536,870072,1887748,623800,2644};
    unsigned histogram[12]={0};
    uint32_t *queue=malloc((size_t)V_N*sizeof *queue);
    v_dist=malloc(V_N);
    if (!queue || !v_dist) { free(queue); v_fail("BFS allocation failed"); }
    puts("Building independent exact BFS table..."); fflush(stdout);
    for (unsigned p=0;p<V_P;++p)
        for (unsigned m=0;m<V_M;++m)
            v_pm[p][m]=(uint16_t)(v_rank(v_move(v_unrank(p*V_O),m))/V_O);
    for (unsigned o=0;o<V_O;++o)
        for (unsigned m=0;m<V_M;++m)
            v_om[o][m]=(uint16_t)(v_rank(v_move(v_unrank(o),m))%V_O);
    memset(v_dist,255,V_N);
    unsigned head=0, tail=1; queue[0]=0; v_dist[0]=0;
    while (head<tail) {
        unsigned id=queue[head++];
        for (unsigned m=0;m<V_M;++m) {
            unsigned next=v_next(id,m);
            if (next>=V_N) { free(queue); v_fail("transition out of bounds"); }
            if (v_dist[next]==255) {
                if (tail>=V_N) { free(queue); v_fail("BFS queue overflow"); }
                v_dist[next]=(unsigned char)(v_dist[id]+1);
                queue[tail++]=next;
            }
        }
    }
    free(queue);
    if (tail!=V_N || v_dist[0]!=0) v_fail("BFS incomplete / solved entry wrong");
    for (unsigned id=0;id<V_N;++id) {
        if (v_dist[id]>11) v_fail("BFS distance outside 0..11");
        ++histogram[v_dist[id]];
    }
    for (unsigned d=0;d<12;++d) {
        printf("distance %2u: %u states\n",d,histogram[d]);
        if (histogram[d]!=expected[d]) v_fail("BFS histogram mismatch");
    }
    puts("BFS PASS: 3674160 states; diameter 11; distance-11 count 2644.");
}

static void v_h1(void)
{
    int maximum=0;
    puts("Checking H1 on every state..."); fflush(stdout);
    for (unsigned id=0;id<V_N;++id) {
        state_t s=v_unrank(id);
        if (!v_valid(s) || v_rank(s)!=id) v_fail("reference rank round-trip");
        int h=lower_bound(s);
        if (h<0 || h>(int)v_dist[id]) {
            char text[15]; v_text(s,text);
            fprintf(stderr,"state=%s rank=%u h=%d exact=%u\n",text,id,h,v_dist[id]);
            v_fail("H1 lower_bound is negative or overestimates");
        }
        if (h>maximum) maximum=h;
    }
    printf("H1 PASS: 3674160 states; max lower_bound = %d.\n",maximum);
    puts("H2 scope: reviewed solver has fixed move tables, no generated heuristic tables.");
    puts("Use --audit or --all for exhaustive move-table behavior comparison.");
    puts("H4: N/A for reviewed unpacked solver; new packed code needs extra tests.");
}

static void v_audit(void)
{
    puts("Auditing DUT parse and all 9 moves for every state..."); fflush(stdout);
    for (unsigned id=0;id<V_N;++id) {
        state_t s=v_unrank(id), parsed;
        char text[15]; v_text(s,text);
        if (!parse(text,&parsed) || !v_equal(s,parsed)) v_fail("DUT parse mismatch");
        if (!!is_solved(s)!=(id==0)) v_fail("DUT is_solved mismatch");
        for (unsigned m=0;m<V_M;++m) {
            state_t ref=v_move(s,m), got=apply_move(s,(int)m);
            if (!v_equal(ref,got) || !v_valid(ref) || v_rank(ref)!=v_next(id,m)) {
                fprintf(stderr,"state=%s move=%s\n",text,v_names[m]);
                v_fail("DUT/reference/coordinate transition mismatch");
            }
            int diff=(int)v_dist[id]-(int)v_dist[v_next(id,m)];
            if (diff < -1 || diff > 1) v_fail("BFS edge-distance inconsistency");
        }
        if ((id+1)%500000==0) { printf("Audit: %u/%u\n",id+1,V_N); fflush(stdout); }
    }
    for (unsigned m=0;m<V_M;++m)
        if (strcmp(move_name[m],v_names[m])) v_fail("DUT move-name mismatch");
    puts("MODEL AUDIT PASS: all states, all 9 moves, valid parsing and solved detection.");
    puts("Fixed move-table behavior PASS; no generated DUT tables to check in reviewed solver.");
}

static void v_check_one(unsigned id, int verbose)
{
    state_t s=v_unrank(id);
    char text[15]; v_text(s,text);
    if (verbose) { printf("Solving %s (exact distance %u)...\n",text,v_dist[id]); fflush(stdout); }
    int steps=solve(s);
    if (steps<0 || steps>11 || steps!=(int)v_dist[id]) {
        fprintf(stderr,"state=%s rank=%u got=%d expected=%u\n",text,id,steps,v_dist[id]);
        v_fail("H3 solution length mismatch");
    }
    if (verbose) printf("Solution:");
    for (int i=0;i<steps;++i) {
        unsigned m=path[i];
        if (m>=V_M) v_fail("invalid move ID in path");
        if (verbose) printf(" %s",v_names[m]);
        s=v_move(s,m); /* Independent replay, not DUT apply_move(). */
    }
    if (!v_equal(s,v_unrank(0))) v_fail("solution replay did not solve cube");
    if (verbose) printf("\nPASS: steps=%d, exact=%u, independent replay OK.\n",steps,v_dist[id]);
}

static unsigned v_parse_id(const char *text)
{
    state_t s;
    if (strlen(text)!=14) v_fail("state must contain exactly 14 digits");
    for (int i=0;i<7;++i) {
        if (text[i]<'1'||text[i]>'7'||text[i+7]<'1'||text[i+7]>'3')
            v_fail("state digits out of range");
        s.p[i]=(unsigned char)(text[i]-'1');
        s.o[i]=(unsigned char)(text[i+7]-'1');
    }
    if (!v_valid(s)) v_fail("duplicate cubies or invalid orientation sum");
    return v_rank(s);
}

static void v_h3(unsigned start, unsigned count)
{
    printf("H3 range [%u, %u); %u states. Each call uses the original solve().\n",
           start,start+count,count);
    puts("Some individual states may take a long time. Ctrl+C stops the run."); fflush(stdout);
    for (unsigned offset=0;offset<count;++offset) {
        unsigned id=start+offset;
        if (offset%100==0) {
            printf("H3: checked=%u/%u; next rank=%u, distance=%u\n",offset,count,id,v_dist[id]);
            fflush(stdout);
        }
        v_check_one(id,0);
    }
    if (start==0 && count==V_N)
        puts("H3 FULL PASS: 3674160/3674160; exact lengths and independent replay.");
    else
        printf("H3 PARTIAL PASS: ranks [%u,%u), %u states; NOT full H3.\n",start,start+count,count);
}

static void v_export(void)
{
    FILE *file=fopen("bfs_distances.bin","wb");
    if (!file) v_fail("cannot create bfs_distances.bin");
    size_t wrote=fwrite(v_dist,1,V_N,file);
    int close_result=fclose(file);
    if (wrote!=V_N || close_result!=0) v_fail("BFS export write failed");
    file=fopen("distance11.txt","w");
    if (!file) v_fail("cannot create distance11.txt");
    unsigned count=0;
    for (unsigned id=0;id<V_N;++id) if (v_dist[id]==11) {
        char text[15]; v_text(v_unrank(id),text);
        if (fprintf(file,"%s\n",text)<0) { fclose(file); v_fail("distance11 export failed"); }
        ++count;
    }
    if (fclose(file)!=0 || count!=2644) v_fail("distance11 export failed");
    puts("Exported bfs_distances.bin (3674160 bytes) and distance11.txt (2644 states).");
    puts("Export complete. H3 NOT RUN; no Ripes instruction counts measured.");
}

static int v_number(const char *text, unsigned *out)
{
    char *end;
    if (!*text || *text<'0' || *text>'9') return 0;
    errno=0;
    unsigned long n=strtoul(text,&end,10);
    if (errno || *end || n>V_N) return 0;
    *out=(unsigned)n; return 1;
}

int main(int argc, char **argv)
{
    const char *mode=argc==1 ? "--quick" : argv[1];
    unsigned start=0,count=V_N,id=0;
    int range=strcmp(mode,"--range")==0, one=strcmp(mode,"--state")==0;
    if ((range && (argc!=4 || !v_number(argv[2],&start) ||
                   !v_number(argv[3],&count) || count==0 || start>=V_N || count>V_N-start)) ||
        (one && argc!=3) ||
        (!range && !one && (argc>2 || (strcmp(mode,"--quick") && strcmp(mode,"--all") &&
          strcmp(mode,"--audit") && strcmp(mode,"--export"))))) {
        fprintf(stderr,"Usage: verify_solver [--quick|--audit|--all|--export|--range START COUNT|--state STATE]\n");
        return 2;
    }
    if (C!=7 || MAX_STEPS!=11) v_fail("unsupported solver constants");
    if (one) id=v_parse_id(argv[2]);
    clock_t began=clock();
    printf("Solver included at compile time: %s\n",SOLVER_FILE);
    puts("Host validation only; this is not a target memory/performance measurement.");
    v_build();
    v_h1();
    if (!strcmp(mode,"--all") || !strcmp(mode,"--audit")) v_audit();
    if (!strcmp(mode,"--all") || range) v_h3(start,count);
    else if (one) {
        v_check_one(id,1); puts("Single-state PASS; NOT full H3.");
    } else if (!strcmp(mode,"--quick")) {
        v_check_one(v_parse_id("12345671111111"),1);
        v_check_one(v_parse_id("67354123313333"),1);
        v_check_one(v_parse_id("21345671111111"),1);
        puts("Three-case PASS (0,5,11 steps); NOT full H3.");
    } else if (!strcmp(mode,"--export")) v_export();
    else puts("Audit complete; H3 NOT RUN.");
    free(v_dist); v_dist=NULL;
    printf("CPU seconds for selected checks: %.2f\n",(double)(clock()-began)/CLOCKS_PER_SEC);
    return fflush(stdout)!=0 || ferror(stdout);
}
