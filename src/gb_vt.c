/* ************************************************************************** */
/*
    @file
        gb_vt.c

    @date
        January, 2025

    @author
        Gino Francesco Bogo (ᛊᛟᚱᚱᛖ ᛗᛖᚨ ᛁᛊᛏᚨᛗᛁ ᚨcᚢᚱᛉᚢ)

    @license
        MIT
 */
/* ************************************************************************** */

#include "gb_vt.h"

#include <ctype.h>   // isprint
#include <math.h>    // INFINITY
#include <pthread.h> // pthread_cancel, pthread_create, pthread_join, ...
#include <stdbool.h> // bool, false, true
#include <stdio.h>   // FILE, NULL, fclose, fflush, fgets, ...
#include <stdlib.h>  // strtoul
#include <string.h>  // memcpy, memset, strcmp, strncpy, strtok
#include <termios.h> // ECHO, ICANON, TCSANOW
#include <time.h>    // timespec, clock_gettime, CLOCK_MONOTONIC
#include <unistd.h>  // STDIN_FILENO

#include "gb_calc.h"
#include "gb_utils.h"

// *****************************************************************************
// *****************************************************************************
// Local Defines & Macros
// *****************************************************************************
// *****************************************************************************

// clang-format off
#define print_prompt()  fputs("\r\n$> ", stdout); fflush(stdout)
#define print_string(x) fputs("\r$> ", stdout); fputs(x, stdout); fflush(stdout)

#define error_unknown_cmd() fputs("\r\n  [ERROR] Unknown command!\r\n", stdout)
#define error_wrong_args()  fputs("\r\n  [ERROR] Wrong arguments\r\n", stdout)

#define move_cur_left()  fputs("\x1B[D", stdout) // ESC[D (move cursor left)
#define move_cur_right() fputs("\x1B[C", stdout) // ESC[C (move cursor right)
// clang-format on

#define MAX_ARG_LEN (24)
#define MAX_ARG_NUM (64)
#define MAX_CMD_LEN (32 + (MAX_ARG_NUM * MAX_ARG_LEN))
#define HISTORY_LEN (20)

// *****************************************************************************
// *****************************************************************************
// Local Types & Structures
// *****************************************************************************
// *****************************************************************************

typedef void (*vt_cmd_handler_t)(int argc);

typedef struct {
    const char      *name; // Command name
    size_t           size; // Size of the command name
    int              argc; // Number of arguments (excluding command name)
    vt_cmd_handler_t func; // Function pointer to the command handler
} vt_cmd_entry_t;

// *****************************************************************************
// *****************************************************************************
// Local Variables
// *****************************************************************************
// *****************************************************************************

pthread_t vt_thread;

char vt_arg[MAX_ARG_NUM][MAX_ARG_LEN];
char vt_cmd[MAX_CMD_LEN];
int  vt_cmd_len;
int  vt_cur_pos;
int  vt_esc_seq;

char vt_history[HISTORY_LEN][MAX_CMD_LEN];
int  vt_history_idx = 0;
int  vt_history_pos = 0;
int  vt_history_len = 0;

// *****************************************************************************
// *****************************************************************************
// Local Functions (Math)
// *****************************************************************************
// *****************************************************************************

static void __math_calc(int argc) {
    if (argc < 1) {
        error_wrong_args();
        return;
    }

    const char *task = vt_arg[0];
    const char *expr = vt_history[vt_history_pos - 1];

    size_t len = gb_strlen(task);
    while (len > 0) {
        if (*expr == *task) {
            ++task;
            --len;
        }
        ++expr;
    }

    double value = gb_calc(expr);

    if (value != INFINITY) {
        printf("%lf\r\n", value);
    }
}

static void __math_bin2dec(int argc) {
    if (argc != 1) {
        error_wrong_args();
        return;
    }

    char dec[80];

    if (gb_bin2dec(vt_arg[1], dec, sizeof(dec))) {
        printf("%s\r\n", dec);
    } else {
        error_wrong_args();
    }
}

static void __math_bin2hex(int argc) {
    if (argc != 1) {
        error_wrong_args();
        return;
    }

    char hex[64];

    if (gb_bin2hex(vt_arg[1], hex, sizeof(hex))) {
        printf("%s\r\n", hex);
    } else {
        error_wrong_args();
    }
}

static void __math_dec2bin(int argc) {
    if (argc != 1) {
        error_wrong_args();
        return;
    }

    char bin[128];

    if (gb_dec2bin(vt_arg[1], bin, sizeof(bin))) {
        printf("%s\r\n", bin);
    } else {
        error_wrong_args();
    }
}

static void __math_dec2hex(int argc) {
    if (argc != 1) {
        error_wrong_args();
        return;
    }

    char hex[64];

    if (gb_dec2hex(vt_arg[1], hex, sizeof(hex))) {
        printf("%s\r\n", hex);
    } else {
        error_wrong_args();
    }
}

static void __math_hex2bin(int argc) {
    if (argc != 1) {
        error_wrong_args();
        return;
    }

    char bin[128];

    if (gb_hex2bin(vt_arg[1], bin, sizeof(bin))) {
        printf("%s\r\n", bin);
    } else {
        error_wrong_args();
    }
}

static void __math_hex2dec(int argc) {
    if (argc != 1) {
        error_wrong_args();
        return;
    }

    char dec[80];

    if (gb_hex2dec(vt_arg[1], dec, sizeof(dec))) {
        printf("%s\r\n", dec);
    } else {
        error_wrong_args();
    }
}

// *****************************************************************************
// *****************************************************************************
// Local Functions (Virtual Terminal)
// *****************************************************************************
// *****************************************************************************

static void vt_add_history(const char *cmd) {
    strncpy(vt_history[vt_history_idx], cmd, MAX_CMD_LEN);

    vt_history[vt_history_idx][MAX_CMD_LEN - 1] = '\0';

    vt_history_idx = (vt_history_idx + 1) % HISTORY_LEN;
    vt_history_pos = (vt_history_idx + 0);

    if (vt_history_len < HISTORY_LEN) {
        vt_history_len++;
    }
}

static int __idx_vs_pos_distance(void) {
    int rvalue;

    if (vt_history_idx >= vt_history_pos) {
        rvalue = vt_history_idx - vt_history_pos;
    } else {
        rvalue = (vt_history_idx + HISTORY_LEN) - vt_history_pos;
    }

    return rvalue;
}

static const char *vt_get_history(bool arrow_up) {
    if (vt_history_len > 0) {
        const int distance = __idx_vs_pos_distance();
        if (arrow_up) {
            if (vt_history_len > distance) {
                vt_history_pos = (vt_history_pos - 1 + HISTORY_LEN) % HISTORY_LEN;
                return vt_history[vt_history_pos];
            }
        } else {
            if (distance > 1) {
                vt_history_pos = (vt_history_pos + 1) % HISTORY_LEN;
                return vt_history[vt_history_pos];
            }
        }
    }

    return NULL;
}

static int vt_split_string(char       *str, //
                           const char *sep,
                           char        arg[MAX_ARG_NUM][MAX_ARG_LEN]) {
    // remove comments
    char *ch = gb_strchr(str, '#');

    if (ch != NULL) {
        *ch = '\0';
    }

    int arg_num = 0;

    char *token = strtok(str, sep);

    while ((token != NULL) && (arg_num < MAX_ARG_NUM)) {
        size_t len = gb_strlen(token);

        if ((len > 0) && (len < MAX_ARG_LEN)) {
            memcpy(arg[arg_num++], token, len + 1);
        }

        token = strtok(NULL, sep);
    }

    if (token != NULL) {
        error_wrong_args();
    }

    return arg_num;
}

vt_cmd_entry_t vt_cmd_entry[] = {
    {   "calc", 4, 1,    __math_calc},
    {"bin2dec", 7, 1, __math_bin2dec},
    {"bin2hex", 7, 1, __math_bin2hex},
    {"dec2bin", 7, 1, __math_dec2bin},
    {"dec2hex", 7, 1, __math_dec2hex},
    {"hex2bin", 7, 1, __math_hex2bin},
    {"hex2dec", 7, 1, __math_hex2dec},
};

static bool vt_decode_task(int argc) {
    if (argc < 1) {
        return false;
    }

    const char *task = vt_arg[0];

    const size_t entries_num = sizeof(vt_cmd_entry) / sizeof(vt_cmd_entry_t);
    const size_t task_length = gb_strlen(task);

    for (size_t i = 0; i < entries_num; ++i) {
        if ((task_length == vt_cmd_entry[i].size) && !gb_strcmp(task, vt_cmd_entry[i].name)) {
            if (vt_cmd_entry[i].argc > (argc - 1)) {
                error_wrong_args();
                return true;
            }

            vt_cmd_entry[i].func(argc - 1);
            return true;
        }
    }

    return false;
}

static bool vt_decode_word(int argc) {
    if (argc != 1) {
        return false;
    }

    const char *word = vt_arg[0];

    if (!gb_strcmp(word, "about")) {
        VT_PrintAbout();
        return true;
    }

    if (!gb_strcmp(word, "clear")) {
        printf("\033c");
        return true;
    }

    if (!gb_strcmp(word, "exit")) {
        VT_Exit();
        return true;
    }

    if (!gb_strcmp(word, "help")) {
        VT_PrintHelp();
        return true;
    }

    if (!gb_strcmp(word, "math")) {
        VT_PrintMath();
        return true;
    }

    return false;
}

static void vt_decode_command(void) {
    vt_add_history(vt_cmd);

    const int argc = vt_split_string(vt_cmd, " ;", vt_arg);

    if (vt_decode_task(argc)) {
        print_prompt();
        return;
    }

    if (vt_decode_word(argc)) {
        print_prompt();
        return;
    }

    error_unknown_cmd();
    print_prompt();
}

static void vt_key_end(void) {
    if (vt_cmd_len > 0) {
        for (int i = vt_cur_pos; i < vt_cmd_len; ++i) {
            move_cur_right();
        }
        vt_cur_pos = vt_cmd_len;
    }
}

static void vt_key_home(void) {
    if (vt_cur_pos > 0) {
        for (int i = vt_cur_pos; i > 0; --i) {
            move_cur_left();
        }
        vt_cur_pos = 0;
    }
}

static void vt_key_backspace(void) {
    if (vt_cur_pos > 0) {
        if (vt_cur_pos < vt_cmd_len) {
            for (int i = vt_cur_pos; i < vt_cmd_len; ++i) {
                vt_cmd[i - 1] = vt_cmd[i];
            }
        }

        --vt_cmd_len;
        --vt_cur_pos;

        vt_cmd[vt_cmd_len] = '\0';

        print_string(vt_cmd);
        putchar(' ');

        for (int i = vt_cmd_len; i >= vt_cur_pos; --i) {
            move_cur_left();
        }
    }
}

static void vt_key_delete(void) {
    if (vt_cmd_len > 0) {
        if (vt_cur_pos < vt_cmd_len) {
            for (int i = vt_cur_pos; i < vt_cmd_len; ++i) {
                vt_cmd[i] = vt_cmd[i + 1];
            }

            --vt_cmd_len;

            vt_cmd[vt_cmd_len] = '\0';

            print_string(vt_cmd);
            putchar(' ');

            for (int i = vt_cmd_len; i >= vt_cur_pos; --i) {
                move_cur_left();
            }
        }
    }
}

static void vt_key_return(void) {
    vt_cmd[vt_cmd_len] = '\0';
    vt_cmd_len         = 0;
    vt_cur_pos         = 0;

    fputs("\r\n", stdout);

    vt_decode_command();

    memset(vt_cmd, 0, sizeof(vt_cmd));
}

static void vt_key_generic(int ch) {
    const bool check_1 = isprint(ch);
    const bool check_2 = vt_cmd_len < ((int)sizeof(vt_cmd) - 1);

    if (check_1 && check_2) {
        if (vt_cur_pos < vt_cmd_len) {
            for (int i = vt_cmd_len; i > vt_cur_pos; --i) {
                vt_cmd[i] = vt_cmd[i - 1];
            }
        }

        vt_cmd[vt_cur_pos] = (char)ch;

        ++vt_cur_pos;
        ++vt_cmd_len;

        vt_cmd[vt_cmd_len] = '\0';

        print_string(vt_cmd);
        putchar(' ');

        for (int i = vt_cmd_len; i >= vt_cur_pos; --i) {
            move_cur_left();
        }
    }
}

static bool vt_decode_escape_sequence(const int ch) {
    const bool arrow_up = (ch == 'A');
    const bool arrow_dw = (ch == 'B');

    // ESC[A or ESC[B
    if ((vt_esc_seq == 2) && (arrow_up || arrow_dw)) {
        const char *history_cmd = vt_get_history(arrow_up);

        if (history_cmd != NULL) {
            memset(vt_cmd, ' ', vt_cmd_len);

            print_string(vt_cmd);

            strncpy(vt_cmd, history_cmd, sizeof(vt_cmd) - 1);

            vt_cmd_len = (int)gb_strlen(vt_cmd);
            vt_cur_pos = vt_cmd_len;

            print_string(vt_cmd);
        }

        vt_esc_seq = 0;
        return true;
    }

    const bool arrow_rt = (ch == 'C');
    const bool arrow_lt = (ch == 'D');

    // ESC[C or ESC[D
    if ((vt_esc_seq == 2) && (arrow_rt || arrow_lt)) {
        if (arrow_rt) {
            if (vt_cur_pos < vt_cmd_len) {
                vt_cur_pos++;
                move_cur_right();
            }
        }

        else if (arrow_lt) {
            if (vt_cur_pos > 0) {
                vt_cur_pos--;
                move_cur_left();
            }
        }

        vt_esc_seq = 0;
        return true;
    }

    // ESC[F (End)
    if ((vt_esc_seq == 2) && (ch == 'F')) {
        vt_key_end();
        vt_esc_seq = 0;
        return true;
    }

    // ESC[H (Home)
    if ((vt_esc_seq == 2) && (ch == 'H')) {
        vt_key_home();
        vt_esc_seq = 0;
        return true;
    }

    return false;
}

static bool vt_is_escape_sequence(const int ch) {
    // ESC
    if (ch == 0x1B) {
        vt_esc_seq = 1;
        return true;
    }

    // ESC[
    if ((vt_esc_seq == 1) && (ch == '[')) {
        vt_esc_seq = 2;
        return true;
    }

    // ESC[<1-6>
    if ((vt_esc_seq == 2) && ('1' <= ch) && (ch <= '6')) {
        vt_esc_seq = ch;
        return true;
    }

    // ESC[<1-6>~
    if ((vt_esc_seq > 2) && (ch == '~')) {
        // ESC[3~ (Delete)
        if (vt_esc_seq == '3') {
            vt_key_delete();
        }

        vt_esc_seq = 0;
        return true;
    }

    return vt_decode_escape_sequence(ch);
}

static void *vt_keystroke_worker(void *args) {
    (void)args;

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

    while (!vt_exit) {
        const int ch = getchar();

        if (vt_is_escape_sequence(ch)) {
            continue;
        }

        switch (ch) {
            case 0x08:   // BACKSPACE
            case 0x7F: { // DEL
                vt_key_backspace();
            } break;

            case '\r': { // CARRIAGE RETURN 0x0D
                // do nothing
            } break;

            case '\n': { // LINE FEED 0x0A
                vt_key_return();
            } break;

            default: {
                vt_key_generic(ch);
            } break;
        }
    }

    return NULL;
}

// *****************************************************************************
// *****************************************************************************
// Public Variables
// *****************************************************************************
// *****************************************************************************

bool vt_exit = false;

// *****************************************************************************
// *****************************************************************************
// Public Functions
// *****************************************************************************
// *****************************************************************************

void VT_DisableBuffering(void) {
    struct termios term;

    tcgetattr(STDIN_FILENO, &term);
    term.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &term);
}

void VT_RestoreBuffering(void) {
    struct termios term;

    tcgetattr(STDIN_FILENO, &term);
    term.c_lflag |= (ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &term);
}

void VT_KeystrokeStart(void *args) {
    const int rvalue = pthread_create(&vt_thread, NULL, vt_keystroke_worker, args);

    if (!rvalue) {
        vt_exit = false;
    } else {
        fprintf(stderr, "ERROR: VT keystroke thread creation failure (%d)\n", rvalue);
    }
}

void VT_KeystrokeStop(void) {
    vt_exit = true;

    pthread_cancel(vt_thread);

    pthread_join(vt_thread, NULL);
}

static bool is_first_time = true;

void VT_PrintAbout(void) {
    memset(vt_cmd, 0, sizeof(vt_cmd));

    vt_cmd_len = 0;
    vt_cur_pos = 0;
    vt_esc_seq = 0;

    printf("\r\n");
    printf("----------------------------\r\n");
    printf("           gVtCalc          \r\n");
    printf("                            \r\n");
    printf("   author: Gino Bogo        \r\n");
    printf("  version: %d.%d.%d         \r\n", 0, 1, 0);
    printf("     date: %s               \r\n", "September, 2025");
    printf("----------------------------\r\n");

    if (is_first_time) {
        is_first_time = false;
        print_prompt();
    }
}

void VT_PrintHelp(void) {
    memset(vt_cmd, 0, sizeof(vt_cmd));

    vt_cmd_len = 0;
    vt_cur_pos = 0;
    vt_esc_seq = 0;

    printf("\r\n");
    printf("Commands list:\r\n");
    printf("  about\r\n");
    printf("  clear\r\n");
    printf("  exit\r\n");
    printf("  help\r\n");
    printf("  math\r\n");
}

void VT_PrintMath(void) {
    memset(vt_cmd, 0, sizeof(vt_cmd));

    vt_cmd_len = 0;
    vt_cur_pos = 0;
    vt_esc_seq = 0;

    // clang-format off
    printf("\r\n");
    printf("Math:\r\n");
    printf("  calc <expr>   - calculate the expression\r\n");
    printf("  bin2dec <num> - convert binary to decimal\r\n");
    printf("  bin2hex <num> - convert binary to hexadecimal\r\n");
    printf("  dec2bin <num> - convert decimal to binary\r\n");
    printf("  dec2hex <num> - convert decimal to hexadecimal\r\n");
    printf("  hex2bin <num> - convert hexadecimal to binary\r\n");
    printf("  hex2dec <num> - convert hexadecimal to decimal\r\n");
    // clang-format on
}

void VT_Exit(void) {
    vt_exit = true;
}

/*******************************************************************************
 End of File
*/
