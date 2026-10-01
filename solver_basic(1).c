/*
 * 2x2x2 基礎版：先理解、確認正確，之後再優化。
 * gcc -O2 -std=c99 -Wall -Wextra solver_basic.c -o solver_basic
 *
 * 沿用小考的 state_t、parse、turn 概念與老師的 R/B/D 編碼。
 * 前上左角固定；輸入前 7 碼為角塊、後 7 碼為方向。
 * 沒有大型資料表、heap 或遞迴。
 * 以逐步增加深度的搜尋找最短解；最難測資可能很慢。
 * 尚未完成作業的全域 H3、Ripes 指令數門檻與 LED 顯示。
 */
#include <stdio.h>

#define C 7
#define MAX_STEPS 11

/* 沒有提供命令列參數時，使用這個預設狀態。 */
#define DEFAULT_INPUT "25346712313322"

typedef struct {
    unsigned char p[C];       /* 每個位置的角塊編號，內部為 0~6 */
    unsigned char o[C];       /* 每個位置的方向，內部為 0~2 */
} state_t;

/* 每一層保存自己的狀態和下一個要嘗試的動作，取代遞迴。 */
static state_t stack[MAX_STEPS + 1];
static unsigned char next_move[MAX_STEPS + 1];
static unsigned char path[MAX_STEPS];
static const char move_name[9][3] = {
    "R", "R2", "R'", "B", "B2", "B'", "D", "D2", "D'"
};

static int parse(const char *text, state_t *state)
{
    unsigned seen = 0, sum = 0;
    /* 先檢查長度，避免讀到短字串後面的空間。 */
    int length = 0;
    while (length < 14 && text[length] != '\0') ++length;
    if (length != 14 || text[14] != '\0') return 0;

    for (int i = 0; i < C; ++i) {
        unsigned p = (unsigned)(text[i] - '1');
        unsigned o = (unsigned)(text[i + C] - '1');
        if (p >= C || o >= 3 || ((seen >> p) & 1U)) return 0;
        state->p[i] = (unsigned char)p;
        state->o[i] = (unsigned char)o;
        seen |= 1U << p;
        sum += o;
    }
    while (sum >= 3) sum -= 3;
    return sum == 0;
}

/* f=0: R，f=1: B，f=2: D；正對該面順時針轉 90 度。
 * 這兩個小表描述轉動規則，不是預先算好的解答。
 */
static state_t turn(state_t s, int f)
{
    static const unsigned char source[3][C] = {
        {1, 4, 2, 0, 3, 5, 6},
        {0, 1, 2, 4, 5, 6, 3},
        {0, 2, 5, 3, 1, 4, 6}
    };
    static const unsigned char twist[3][C] = {
        {1, 2, 0, 2, 1, 0, 0},
        {0, 0, 0, 1, 2, 1, 2},
        {0, 0, 0, 0, 0, 0, 0}
    };
    state_t result;
    for (int i = 0; i < C; ++i) {
        int from = source[f][i];
        unsigned o = s.o[from] + twist[f][i];
        if (o >= 3) o -= 3;
        result.p[i] = s.p[from];
        result.o[i] = (unsigned char)o;
    }
    return result;
}

static int face_of(int move)
{
    if (move < 3) return 0;
    if (move < 6) return 1;
    return 2;
}

static state_t apply_move(state_t s, int move)
{
    int face = face_of(move);
    int turns = move - (face + face + face) + 1;
    for (int n = 0; n < turns; ++n) s = turn(s, face);
    return s;
}

static int is_solved(state_t s)
{
    for (int i = 0; i < C; ++i)
        if (s.p[i] != i || s.o[i] != 0) return 0;
    return 1;
}

/* 一個動作最多影響四個角塊。
 * 若有 k 個角塊尚未完成，至少還要 ceil(k/4) 步。
 * 這只是簡單的下限，不會把真正的最短解排除。
 */
static int lower_bound(state_t s)
{
    unsigned wrong = 0;
    for (int i = 0; i < C; ++i)
        if (s.p[i] != i || s.o[i] != 0) ++wrong;
    return (int)((wrong + 3U) >> 2);
}

static int solve(state_t start)
{
    /* 依序問：0 步能解嗎？1 步呢？一直到 11 步。
     * 第一次找到的解就是最短解。
     */
    for (int limit = 0; limit <= MAX_STEPS; ++limit) {
        int depth = 0;
        stack[0] = start;
        next_move[0] = 0;

        while (depth >= 0) {
            if (is_solved(stack[depth])) return depth;

            /* 超出步數、下限太大，或九種動作都試完：退回上一層。 */
            if (depth == limit ||
                depth + lower_bound(stack[depth]) > limit ||
                next_move[depth] == 9) {
                --depth;
                continue;
            }

            int move = next_move[depth]++;
            /* 相鄰同面轉動可以合併，不可能是最短解必需的形式。 */
            if (depth > 0 && face_of(move) == face_of(path[depth - 1]))
                continue;

            path[depth] = (unsigned char)move;
            stack[depth + 1] = apply_move(stack[depth], move);
            ++depth;
            next_move[depth] = 0;
        }
    }
    return -1;
}

int main(int argc, char *argv[])
{
    if (argc > 2) {
        fprintf(stderr, "Usage: %s [14-digit-cube-state]\n", argv[0]);
        return 2;
    }

    const char *input = (argc == 2) ? argv[1] : DEFAULT_INPUT;
    state_t state;
    if (!parse(input, &state)) {
        fprintf(stderr, "Invalid cube state: expected a legal 14-digit state.\n");
        return 2;
    }

    printf("Input: %s\nSearching...\n", input);
    fflush(stdout);
    int steps = solve(state);
    if (steps < 0) {
        puts("No solution found; check the model or search.");
        return 1;
    }

    printf("Solution:");
    for (int i = 0; i < steps; ++i) {
        printf(" %s", move_name[path[i]]);
        state = apply_move(state, path[i]);
    }
    if (steps == 0) printf(" (already solved)");
    printf("\nSteps: %d\n", steps);

    /* 程式自行驗證，不只靠人看輸出。 */
    if (!is_solved(state)) {
        puts("Verification: FAIL");
        return 1;
    }
    puts("Verification: PASS");
    return fflush(stdout) != 0 || ferror(stdout);
}
