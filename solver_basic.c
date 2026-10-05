/*
 * 14 碼字串 → parse() → state_t → solve() → 嘗試 R/B/D 各種轉法
 * → 找到 solved state → 印出答案 → 再自己驗證一次
 */
#include <stdio.h>

#define C 7                                                /* 2階魔方原本有8顆corner，固定其中一顆，記錄另外 7 顆 */
#define MAX_STEPS 11                                       /* 最多找到(深度)11步 */

/* 沒有給命令列參數時，使用這個預設狀態 */
#define DEFAULT_INPUT "25346712313322"


/*state_t描述魔方狀態*/
typedef struct {
    unsigned char p[C];                                    /* p[7]每個位置的角塊編號，內部會將 1~7 → 0~6 (index) */
    unsigned char o[C];                                    /* o[7]每個位置的方向，內部為 0~2 */
} state_t;

/* 每一層保存自己的狀態和下一個要嘗試的動作 */
static state_t stack[MAX_STEPS + 1];                       /* stack[]: 每一層搜尋時的魔方狀態 */
static unsigned char next_move[MAX_STEPS + 1];             /* next_move[]: 該層的下一個動作為何，"+1"因為char後面要放'\0' */
static unsigned char path[MAX_STEPS];                      /* path[]: 如何走到目前狀態的 */
static const char move_name[9][3] = {                      /* 儲存9組，每組為"字串"('R','\0'；'R','2','\0'；'R','`' '\0'...字串長度皆為3) */
    "R", "R2", "R'", "B", "B2", "B'", "D", "D2", "D'"
};

/*
 * 檢查輸入是否合法，然後將字串轉成state_t 
 */
static int parse(const char *text, state_t *state)
{
    /* seen: 用於bit mask檢查，記錄了七個小塊中，哪個小塊已經出現在輸入中。如果某個小塊重複出現，則表示輸入不符合正立方體的規則，因此解析器會拒絕該輸入 */
    /* sum: 用於檢查orientation是否符合魔方規則 */
    unsigned seen = 0, sum = 0;

    /* 先檢查長度是否為14個字元，避免讀到短字串後面的空間。 */
    int length = 0;
    while (length < 14 && text[length] != '\0') ++length;

    if (length != 14 || text[14] != '\0') return 0;

    /* 只要在陣列裡面(i < 7)，就把每一個(當下的)狀態寫進陣列裡(儲存)*/
    for (int i = 0; i < C; ++i) {
        unsigned p = (unsigned)(text[i] - '1');          /* p只是暫時用於儲存該次迴圈讀到的角塊編號(並非陣列)，且因字串內容為0 ~ 6，又非整數相減一律用ASCII進行(e.g. '4'-'1' = 52-49 = 3)，故4(字元)在p[]被儲存為3 */
        unsigned o = (unsigned)(text[i + C] - '1');      /* o被存在陣列第7~13的位置 */
        if (p >= C || o >= 3 || ((seen >> p) & 1U)) return 0;           /* 檢查格式是否正確 , e.g. p >= 7 或  o >= 3 或     */
        state->p[i] = (unsigned char)p;                  /* p存到state(指向state_t的指標)這個結構裡的p[i] */
        state->o[i] = (unsigned char)o;                  /* o存到state這個結構裡的o[i] */
        seen |= 1U << p;                                 /* seen(=0) 和 unsigned 1(左移p bits) 做"or" */
        sum += o;
    }

    while (sum >= 3) sum -= 3;                           /* sum若超過3，則mod3 */

    return sum == 0;
}

/* 
 * 輸入一個魔方(狀態)s，和一個面f，將s這個魔方的f面，順時針轉90度
 * f=0 → R，f=1 → B，f=2 → D；正對該面(R,B,D)順時針轉 90 度。
 */
static state_t turn(state_t s, int f)
{
    static const unsigned char source[3][C] = {
        {1, 4, 2, 0, 3, 5, 6},                            /* 做一次R後的角塊狀態 */
        {0, 1, 2, 4, 5, 6, 3},                            /* 做一次B後的角塊狀態 */
        {0, 2, 5, 3, 1, 4, 6}                             /* 做一次D後的角塊狀態 */
    };

    static const unsigned char twist[3][C] = {
        {1, 2, 0, 2, 1, 0, 0},                            /* 做一次R後的面朝向狀態 */
        {0, 0, 0, 1, 2, 1, 2},                            /* 做一次B後的面朝向狀態 */
        {0, 0, 0, 0, 0, 0, 0}                             /* 做一次D後的面朝向狀態 */
    };

    state_t result;
    for (int i = 0; i < C; ++i) {
        int from = source[f][i];                          /* from用來存取二維陣列當下的角塊狀態；新的position，要從舊的position拿角塊；source[0] → R 的規則，source[0][2] → 2 */
        unsigned o = s.o[from] + twist[f][i];             /* s.o[from]，s裡面的o[from] = ?，s為state_s(裡面有char p[C]和char o[C]) */
                                                          /* "s.o"為struct state_s裡面的orientation陣列，故s.o[from] = 值(0~2)  */
                                                          /* 新o(當下面朝向哪) = 舊orientation + 這次R造成的狀態*/

        if (o >= 3) o -= 3;                               /* 若新的o大於等於3，mod3 */

        result.p[i] = s.p[from];                          /* 把舊位置from的corner搬到新位置i；將s(struct)裡面的p[from](第from個)陣列值，給result(這個struct)裡面的p[i](第i個) */
        result.o[i] = (unsigned char)o;                   /* 將當下角塊的o值，傳入o陣列的第i元素 */
                                                          /* C 語言裡，char、unsigned char、short 這些比較小的整數型別，在做算術運算時通常會先進行 integer promotion ，也就是說不是直接用unsigned char，而是unsigned int */
    }
    return result;                                        /* 回傳輸入f面順時針旋轉90度的結果 */
}

/*
 * 0 1 2 → R
 * 3 4 5 → B
 * 6 7 8 → D
 * 且回傳值:
 * 0 = R
 * 1 = B
 * 2 = D
 */
static int face_of(int move)
{
    if (move < 3) return 0;                                 /* 若小於3，則回傳0(代表R)*/
    if (move < 6) return 1;                                 /* 若小於6，則回傳1(代表B)*/
    return 2;                                               /* 其餘(>6)，則回傳2(代表D)*/
}


/*
 * 我們只有寫一個「順時針轉 90°」的 turn()
 * 要怎麼利用它做出 R、R2、R'、B、B2、B'、D、D2、D' 這九種動作？
 */
static state_t apply_move(state_t s, int move)
{
    int face = face_of(move);                               /* 先找是哪一個面，face = int */
    int turns = move - (face + face + face) + 1;            /* turns:對該面轉動(90度)次數(1~3)；move:對某一面轉幾次的編號(0~8)；face:對哪一面(0~2) */
                                                            /* 若將R,B,D轉法序列轉成一維[0,1,2,3,4,5,6,7,8](move)，則col_idx = row_len * row_idx，且因turns(代表轉90,180,270度)，不能不轉，故從0開始(+1) */
                                                            /* 例:4在三階方陣在[1,1]，在一維idx = 5，故5 / 3 = 1(第1行)，5 - 3 = 2(第2位) */

    for (int n = 0; n < turns; ++n)  s = turn(s, face);     /* 總共重複turns次，每次都把face指定的面順時鐘旋轉90度；在不同面，執行同樣的動作(順時針旋轉90度)，從而做到R2,R'等動作 */

    return s;
}


/*
 * 檢查現在狀態是否被解完
 */
static int is_solved(state_t s)
{
    for (int i = 0; i < C; ++i)
        if (s.p[i] != i || s.o[i] != 0) return 0;            /* 如果s(struct state_t)的p[i] != i(陣列元素)或是o[i] != 0 ，則回傳0，否則回傳1 */
    return 1;
}


/* 
 * 一個動作最多影響四個角塊。
 * 若有 k 個角塊尚未完成，至少還要 ceil(k/4) 步。
 * 這只是簡單的下限，不會把真正的最短解排除。
 */
static int lower_bound(state_t s)
{
    unsigned wrong = 0;

    for (int i = 0; i < C; ++i)
        if (s.p[i] != i || s.o[i] != 0) ++wrong;             /* 只要狀態的p[i]或是o[i]不對的話，則wrong先加一再回傳值(計算幾個corner是錯的) */

    return (int)((wrong + 3U) >> 2);                         /* 估計目前的魔方狀態，估計至少還需要幾步才能解完，即為heuristic [h(n)] */
                                                             /* ">>"2(除以4)，也可以寫做"/4"，因為一次face move最多影響4顆角塊，理論上最快可以一步解決 */
                                                             /* 若wrong = 5, wrong / 4 = 1，但是1步無法解決，故要"向上取整"，又"ceil(n / d) = (n + d - 1) / 4"(其中d = 4) */
                                                             /* 簡化後可得"(wrong + 3) / 4" */
}


/*
 * 使用帶有 heuristic 的 "Iterative Deepening Depth-First Search" (接近於IDA*)
 * 這份程式不是單純的 DFS，也不是單純的 IDDFS；它使用「逐步提高步數上限 + g+h 剪枝」來搜尋
 * 所以本質上就是 IDA* 的做法，只是它的 threshold(limit) 是 0,1,2,...,11 逐一增加，而不是每輪直接跳到下一個超過 threshold 的最小 f 值
 * 其中limit = 這一輪允許使用的"最大步數"
 */
static int solve(state_t start)
{
    /* 依序問：0 步能解嗎？ 不行，代表第0步沒有解
     *        1 步呢？ 不行，第1步沒有解
     *        ...
     *        一直到 i 步，找到解答。
     * 最多走11步。
     * 第一次找到的解就是最短解，因為前面的步數都沒有解。
     */
    for (int limit = 0; limit <= MAX_STEPS; ++limit) {
        int depth = 0;                                        /* 紀錄深度，代表已經走了幾步，迴圈的最開始 */
        stack[0] = start;                                     /* 第0層，即為原始魔方 */
        next_move[0] = 0;                                     /* 第0層下一個先試move 0，也就是R，next_move[]存每層下一個還沒試過的move是哪一個 */

        /*
         * 手動做DFS，一般DFS常用recursion
         * 但這邊用stack[], depth, next_move[]來模擬function call back
         */
        while (depth >= 0) {
            if (is_solved(stack[depth])) return depth;         /* 如果已經解完，回傳depth，如果還沒救繼續下一行，stack[]存每個depth的魔方狀態 */

            /* 
             * 做backtrack，若
             * 1."已走步數"(depth)快超出可以走的"最大步數"
             * 2."已走步數" + "預計要走步數" 已超過可走的 "最大步數"
             * 3.九種動作都試完，因為move_name[9][3]={...}，且???
             * 則退回上一層(--depth)，繼續下一行。 
             */
            if (depth == limit || depth + lower_bound(stack[depth]) > limit || next_move[depth] == 9) {
                --depth;
                continue;
            }

            int move = next_move[depth]++;                     /* move為下一個動作的index(0~8)，因為DFS會搜尋同一層內的所有合法路徑 */

            /* 
             * 不允許連續轉同一面"face_of(move) == face_of(path[depth - 1)"，必定可以合併，不可能是最短解必需的形式
             * 例: R接下來不可能是"R, R2, R'"
             * R*R = R2, R*R2 = R, R*R' = I
             */
            if (depth > 0 && face_of(move) == face_of(path[depth - 1]))
                continue;

            path[depth] = (unsigned char)move;                  /* path[]紀錄到置個depth為止執行哪些動作(R,R2,...) */
            stack[depth + 1] = apply_move(stack[depth], move);  /* 拿現在的魔方，執行move後，產生出下一個魔方 */

            ++depth;                                            /* 進入下一層 */
            next_move[depth] = 0;                               /* 第depth層的動作從R(idx 0)開始執行 */
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
