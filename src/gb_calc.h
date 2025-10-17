/* ************************************************************************** */
/*
    @file
        gb_calc.h

    @date
        October, 2023

    @author
        Gino Francesco Bogo (ᛊᛟᚱᚱᛖ ᛗᛖᚨ ᛁᛊᛏᚨᛗᛁ ᚨcᚢᚱᛉᚢ)

    @license
        MIT
 */
/* ************************************************************************** */

#ifndef GB_CALC_H
#define GB_CALC_H

// *****************************************************************************
// *****************************************************************************
// Public Functions
// *****************************************************************************
// *****************************************************************************

/**
 * @brief Evaluates a mathematical expression.
 *
 * This function evaluates a mathematical expression represented as a string. It
 * supports arithmetic operators, parentheses, and a set of mathematical
 * functions.
 *
 * @param[in] expr A null-terminated string containing the mathematical
 *                 expression to be evaluated.
 *
 * @return The result of the expression as a double. In case of an error, it
 *         returns INFINITY and prints an error message to stderr.
 */
double gb_calc(const char *expr);

#endif // GB_CALC_H

/* *****************************************************************************
 End of File
 */
