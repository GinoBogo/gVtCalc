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
 * This function evaluates a mathematical expression represented as a string
 * `expr`. It supports basic arithmetic operations and returns the result as a
 * double.
 *
 * @param expr Pointer to the expression string.
 * @return The result of the evaluated expression.
 */
double gb_calc(const char *expr);

#endif // GB_CALC_H

/* *****************************************************************************
 End of File
 */
