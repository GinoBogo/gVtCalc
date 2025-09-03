/* ************************************************************************** */
/*
    @file
        main.c

    @date
        September, 2025

    @author
        Gino Francesco Bogo (ᛊᛟᚱᚱᛖ ᛗᛖᚨ ᛁᛊᛏᚨᛗᛁ ᚨcᚢᚱᛉᚢ)

    @license
        MIT
 */
/* ************************************************************************** */

#include <stdio.h>
#include <unistd.h> // NULL, sleep

#include "gb_vt.h"

int main(void) {
    VT_DisableBuffering();
    VT_KeystrokeStart(NULL);

    VT_PrintAbout();

    while (!vt_exit) {
        sleep(1);
    }

    VT_KeystrokeStop();
    VT_RestoreBuffering();

    fprintf(stdout, "\r\n... done!");
    return 0;
}

/*******************************************************************************
 End of File
*/
