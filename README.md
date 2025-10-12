# gVtCalc
A lightweight, terminal-based calculator that I wrote in C, which I usually use on embedded systems in 'restricted areas' where standard calculators can't be installed.

![Fig. 0](./docs/images/gVtCalc_00.png)

## Features

### Mathematical Operations

These operations can be used within the `calc` command.

**Constants:**
*   `pi`: The mathematical constant π (pi).
*   `e`: The mathematical constant e (Euler's number).

**Operators:**
*   `+`: Addition
*   `-`: Subtraction
*   `*`: Multiplication
*   `/`: Division
*   `%`: Modulo
*   `^`: Power (e.g., `2^3` for 2 raised to the power of 3)
*   `-`: Unary negation (e.g., `-5`)
*   `!`: Logical NOT
*   `~`: Bitwise NOT

**Functions:**
*   `sin(x)`: Sine of x
*   `cos(x)`: Cosine of x
*   `tan(x)`: Tangent of x
*   `asin(x)`: Arc sine of x
*   `acos(x)`: Arc cosine of x
*   `atan(x)`: Arc tangent of x
*   `sqrt(x)`: Square root of x
*   `exp(x)`: Exponential function (e^x)
*   `log(x)`: Natural logarithm of x
*   `log2(x)`: Base-2 logarithm of x

### Terminal Commands

These are the commands you can use at the `gVtCalc` prompt.

**General Commands:**
*   `about`: Displays information about the calculator.
*   `clear`: Clears the terminal screen.
*   `exit`: Exits the `gVtCalc` application.
*   `help`: Displays a list of available commands.
*   `math`: Displays a list of math-related commands.

**Calculation and Conversion:**
*   `calc <expression>`: Evaluates a mathematical expression.
*   `bin2dec <number>` (or `b2d`): Converts a binary number to decimal.
*   `bin2hex <number>` (or `b2h`): Converts a binary number to hexadecimal.
*   `dec2bin <number>` (or `d2b`): Converts a decimal number to binary.
*   `dec2hex <number>` (or `d2h`): Converts a decimal number to hexadecimal.
*   `hex2bin <number>` (or `h2b`): Converts a hexadecimal number to binary.
*   `hex2dec <number>` (or `h2d`): Converts a hexadecimal number to decimal.