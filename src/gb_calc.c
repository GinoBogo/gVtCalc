/* ************************************************************************** */
/*
    @file
        gb_calc.c

    @date
        October, 2023

    @author
        Gino Francesco Bogo (ᛊᛟᚱᚱᛖ ᛗᛖᚨ ᛁᛊᛏᚨᛗᛁ ᚨcᚢᚱᛉᚢ)

    @license
        MIT
 */
/* ************************************************************************** */

#include "gb_calc.h"

#include <ctype.h>   // isdigit, isspace
#include <math.h>    // INFINITY, M_PI, acos, asin, atan, cos, exp, fmod, log, pow, sin, sqrt, tan
#include <stdbool.h> // bool, false, true
#include <stdio.h>   // fprintf, size_t
#include <stdlib.h>  // strtod

#include "gb_utils.h"

// *****************************************************************************
// *****************************************************************************
// Local Types
// *****************************************************************************
// *****************************************************************************

typedef struct {
    const char *expr;
    double      num_lifo[32];
    int         num_top;
    char        op__lifo[32];
    int         op__top;
    int         i;
} calc_context_t;

// *****************************************************************************
// *****************************************************************************
// Local Functions
// *****************************************************************************
// *****************************************************************************

static bool _apply_unary_op(calc_context_t *ctx, double num) {
    bool result = false;

    if (ctx->op__top >= 0) {
        // Process unary operator (extended ID)
        char op = (char)((unsigned int)ctx->op__lifo[ctx->op__top] - 128);

        switch (op) {
            case '-': {
                ctx->num_lifo[++ctx->num_top] = -num;
                ctx->op__top--;
                result = true;
            } break;

            case '!': {
                ctx->num_lifo[++ctx->num_top] = !num;
                ctx->op__top--;
                result = true;
            } break;

            case '~': {
                ctx->num_lifo[++ctx->num_top] = ~((int)num);
                ctx->op__top--;
                result = true;
            } break;

            default: {
                // Not a unary operator
            }
        }
    }

    return result;
}

static bool _apply_unary_func(calc_context_t *ctx, double num) {
    bool result = false;

    if (ctx->op__top >= 0) {
        switch (ctx->op__lifo[ctx->op__top]) {
            case 's': { // sin
                ctx->num_lifo[++ctx->num_top] = sin(num);
                ctx->op__top--;
                result = true;
            } break;

            case 'S': { // asin
                ctx->num_lifo[++ctx->num_top] = asin(num);
                ctx->op__top--;
                result = true;
            } break;

            case 'c': { // cos
                ctx->num_lifo[++ctx->num_top] = cos(num);
                ctx->op__top--;
                result = true;
            } break;

            case 'C': { // acos
                ctx->num_lifo[++ctx->num_top] = acos(num);
                ctx->op__top--;
                result = true;
            } break;

            case 't': { // tan
                ctx->num_lifo[++ctx->num_top] = tan(num);
                ctx->op__top--;
                result = true;
            } break;

            case 'T': { // atan
                ctx->num_lifo[++ctx->num_top] = atan(num);
                ctx->op__top--;
                result = true;
            } break;

            case 'q': { // sqrt
                if (num < 0) {
                    fprintf(stderr, "Error: Square root of negative number\n");
                    ctx->num_lifo[++ctx->num_top] = INFINITY;
                } else {
                    ctx->num_lifo[++ctx->num_top] = sqrt(num);
                }
                ctx->op__top--;
                result = true;
            } break;

            case 'e': { // exp
                ctx->num_lifo[++ctx->num_top] = exp(num);
                ctx->op__top--;
                result = true;
            } break;

            case 'l': { // e-base log
                if (num <= 0) {
                    fprintf(stderr, "Error: Logarithm of non-positive number\n");
                    ctx->num_lifo[++ctx->num_top] = INFINITY;
                } else {
                    ctx->num_lifo[++ctx->num_top] = log(num);
                }
                ctx->op__top--;
                result = true;
            } break;

            case 'L': { // 2-base log
                if (num <= 0) {
                    fprintf(stderr, "Error: Logarithm of non-positive number\n");
                    ctx->num_lifo[++ctx->num_top] = INFINITY;
                } else {
                    ctx->num_lifo[++ctx->num_top] = log2(num);
                }
                ctx->op__top--;
                result = true;
            } break;

            default: {
                // Not a unary function
            }
        }
    }

    return result;
}

static bool _is_binary_op(char op) {
    switch (op) {
        case '+':
        case '-':
        case '*':
        case '/':
        case '%':
        case '^':
            return true;

        default:
            return false;
    }
}

static double _apply_binary_op(double a, double b, char op) {
    switch (op) {
        case '+':
            return (a + b);

        case '-':
            return (a - b);

        case '*':
            return (a * b);

        case '/':
            if (b == 0) {
                fprintf(stderr, "Error: Division by zero\n");
                return INFINITY;
            }
            return (a / b);

        case '%':
            if (b == 0) {
                fprintf(stderr, "Error: Modulo by zero\n");
                return INFINITY;
            }
            return fmod(a, b);

        case '^':
            return pow(a, b);

        default:
            fprintf(stderr, "Error: Unknown operator '%c'\n", op);
            return INFINITY;
    }
}

static bool _apply_operator(calc_context_t *ctx) {
    if (ctx->num_top < 0) {
        fprintf(stderr, "Error: Operator without operand(s)\n");
        return false;
    }

    double b = ctx->num_lifo[ctx->num_top--];

    if (_apply_unary_op(ctx, b)) {
        return true;
    }

    if (_apply_unary_func(ctx, b)) {
        return true;
    }

    if (ctx->num_top < 0) {
        fprintf(stderr, "Error: Operator without operand(s)\n");
        return false;
    }

    double a  = ctx->num_lifo[ctx->num_top--];
    char   op = ctx->op__lifo[ctx->op__top--];

    ctx->num_lifo[++ctx->num_top] = _apply_binary_op(a, b, op);

    return true;
}

static bool _process_operators(calc_context_t *ctx) {
    while (ctx->op__top >= 0) {
        if (ctx->op__lifo[ctx->op__top] == '(') {
            fprintf(stderr, "Error: Mismatched parentheses\n");
            return false;
        }

        if (!_apply_operator(ctx)) {
            return false;
        }
    }

    return true;
}

static int _get_precedence(char op) {
    // Precedence for 2-operand operators
    switch (op) {
        case '^':
            return 4;

        case '*':
        case '/':
        case '%':
            return 3;

        case '+':
        case '-':
            return 2;

        default:
            return 0;
    }
}

static bool _process_binary(calc_context_t *ctx) {
    const char *cp = &ctx->expr[ctx->i];
    const char  ch = *cp;

    if (!_is_binary_op(ch)) {
        return false;
    }

    while (ctx->op__top >= 0) {
        const int top_prec = _get_precedence(ctx->op__lifo[ctx->op__top]);
        const int cur_prec = _get_precedence(ch);

        if (cur_prec > top_prec) {
            break; // Current operator has higher precedence
        }

        double b  = ctx->num_lifo[ctx->num_top--];
        double a  = ctx->num_lifo[ctx->num_top--];
        char   op = ctx->op__lifo[ctx->op__top--];

        ctx->num_lifo[++ctx->num_top] = _apply_binary_op(a, b, op);
    }

    ctx->op__lifo[++ctx->op__top] = ch;
    ctx->i++;

    return true;
}

static bool _process_close_paren(calc_context_t *ctx) {
    const char *cp = &ctx->expr[ctx->i];
    const char  ch = *cp;

    if (ch != ')') {
        return false;
    }

    while ((ctx->op__top >= 0) && (ctx->op__lifo[ctx->op__top] != '(')) {
        if (!_apply_operator(ctx)) {
            return false;
        }
    }

    if ((ctx->op__top >= 0) && (ctx->op__lifo[ctx->op__top] == '(')) {
        ctx->op__top--; // Pop the '('

        double num = ctx->num_lifo[ctx->num_top--];

        if (!_apply_unary_op(ctx, num) && !_apply_unary_func(ctx, num)) {
            ctx->num_top++;
        }
    } else {
        fprintf(stderr, "Error: Mismatched parentheses\n");
        return false;
    }

    ctx->i++;
    return true;
}

static bool _process_open_paren(calc_context_t *ctx) {
    const char *cp = &ctx->expr[ctx->i];
    const char  ch = *cp;

    if (ch != '(') {
        return false;
    }

    ctx->op__lifo[++ctx->op__top] = ch;
    ctx->i++;
    return true;
}

static bool _process_function(calc_context_t *ctx) {
    const char *cp = &ctx->expr[ctx->i];

    if (gb_strncmp(cp, "sin", 3) == 0) {
        ctx->op__lifo[++ctx->op__top] = 's';
        ctx->i += 3;
        return true;
    }

    if (gb_strncmp(cp, "asin", 4) == 0) {
        ctx->op__lifo[++ctx->op__top] = 'S';
        ctx->i += 4;
        return true;
    }

    if (gb_strncmp(cp, "cos", 3) == 0) {
        ctx->op__lifo[++ctx->op__top] = 'c';
        ctx->i += 3;
        return true;
    }

    if (gb_strncmp(cp, "acos", 4) == 0) {
        ctx->op__lifo[++ctx->op__top] = 'C';
        ctx->i += 4;
        return true;
    }

    if (gb_strncmp(cp, "tan", 3) == 0) {
        ctx->op__lifo[++ctx->op__top] = 't';
        ctx->i += 3;
        return true;
    }

    if (gb_strncmp(cp, "atan", 4) == 0) {
        ctx->op__lifo[++ctx->op__top] = 'T';
        ctx->i += 4;
        return true;
    }

    if (gb_strncmp(cp, "sqrt", 4) == 0) {
        ctx->op__lifo[++ctx->op__top] = 'q';
        ctx->i += 4;
        return true;
    }

    if (gb_strncmp(cp, "exp", 3) == 0) {
        ctx->op__lifo[++ctx->op__top] = 'e';
        ctx->i += 3;
        return true;
    }

    if (gb_strncmp(cp, "log2", 4) == 0) {
        ctx->op__lifo[++ctx->op__top] = 'L';
        ctx->i += 4;
        return true;
    }

    if (gb_strncmp(cp, "log", 3) == 0) {
        ctx->op__lifo[++ctx->op__top] = 'l';
        ctx->i += 3;
        return true;
    }

    return false;
}

static bool _process_number(calc_context_t *ctx) {
    const char *cp = &ctx->expr[ctx->i];
    const char  ch = *cp;

    if (isdigit(ch) || (ch == '.')) {
        char  *ep;
        double num = strtod(cp, &ep);

        if (!_apply_unary_op(ctx, num)) {
            ctx->num_lifo[++ctx->num_top] = num;
        }

        ctx->i += (int)(ep - cp);
        return true;
    }

    return false;
}

static bool _process_constant(calc_context_t *ctx) {
    const char *cp = &ctx->expr[ctx->i];

    if (gb_strncmp(cp, "pi", 2) == 0) {
        if (!_apply_unary_op(ctx, M_PI)) {
            ctx->num_lifo[++ctx->num_top] = M_PI;
        }

        ctx->i += 2;
        return true;
    }

    return false;
}

static bool _is_unary_op(const char *cp, int i) {
    // The operator is unary if:
    // 1. It's at the start of the expression, or
    // 2. It follows another operator or opening parenthesis
    if (i == 0) {
        return true; // Unary operator at the expression start
    }

    char prev = *(cp - 1);

    return (prev == '!' || //
            prev == '%' || //
            prev == '(' || //
            prev == '*' || //
            prev == '+' || //
            prev == '-' || //
            prev == '/' || //
            prev == '^' || //
            prev == '~');
}

static bool _process_unary(calc_context_t *ctx) {
    const char *cp = &ctx->expr[ctx->i];
    const char  ch = *cp;

    if (_is_unary_op(cp, ctx->i)) {
        if (ch == '+') {
            // Skip unary plus operator
            ctx->i++;
            return true;
        }

        if ((ch == '!') || (ch == '-') || (ch == '~')) {
            // Push unary operator (extended ID)
            ctx->op__lifo[++ctx->op__top] = (char)(ch + 128);
            ctx->i++;
            return true;
        }
    }

    return false;
}

static bool _sanitize_expr(const char *src, char *dst, size_t dst_len) {
    if (!src || !*src) {
        fprintf(stderr, "Error: %s expression\n", (!src) ? "Null" : "Empty");
        return false;
    }

    if (!dst || dst_len < gb_strlen(src) + 1) {
        fprintf(stderr, "Error: Wrong destination buffer\n");
        return false;
    }

    while (*src) {
        if (!isspace(*src)) {
            *dst = *src;
            dst++;
        }
        src++;
    }
    *dst = '\0';

    return true;
}

// *****************************************************************************
// *****************************************************************************
// Public Functions
// *****************************************************************************
// *****************************************************************************

/**
 * @brief Evaluates a mathematical expression.
 *
 * This function evaluates a mathematical expression represented as a string
 * `expr`. It supports basic arithmetic operations and returns the result as a
 * double.
 *
 * @param expr Pointer to the expression string.
 * @return The result of the evaluated expression.
 */
double gb_calc(const char *expr) {
    char dst[256];

    if (!_sanitize_expr(expr, dst, sizeof(dst))) {
        return INFINITY;
    }

    calc_context_t ctx = {
        .expr    = dst,
        .num_top = -1,
        .op__top = -1,
        .i       = 0,
    };

    char ch;
    while ((ch = ctx.expr[ctx.i]) != '\0') {
        if (_process_unary(&ctx)) {
            continue;
        }

        if (_process_constant(&ctx)) {
            continue;
        }

        if (_process_number(&ctx)) {
            continue;
        }

        if (_process_function(&ctx)) {
            continue;
        }

        if (_process_open_paren(&ctx)) {
            continue;
        }

        if (_process_close_paren(&ctx)) {
            continue;
        }

        if (_process_binary(&ctx)) {
            continue;
        }

        fprintf(stderr, "Error: Invalid expression\n");
        return INFINITY;
    }

    if (!_process_operators(&ctx)) {
        fprintf(stderr, "Error: Invalid expression\n");
        return INFINITY;
    }

    if (ctx.num_top != 0) {
        fprintf(stderr, "Error: Invalid expression\n");
        return INFINITY;
    }

    return ctx.num_lifo[ctx.num_top];
}

/* *****************************************************************************
 End of File
 */
