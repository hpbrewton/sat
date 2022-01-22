#include <unistd.h>
#include <stdio.h>

int 
rcc(int row, int clm, int clr) {
    return row*90+clm*10+clr+1;
}

int
main(int argc, char *argv[]) {
    char problem[81];
    int toRead = 81;
    for(;(toRead -= read(0, problem, toRead)) > 0;);

    // row*9*10+col*10+n = row, col has color n -- 10 colors (1-9) + 0

    // a square must have numbers
    for (int r = 0; r < 9; r++) {
        for (int c = 0; c < 9; c++) {
            for (int n = 1; n < 10; n++) printf("%d ", rcc(r,c,n));
            printf("0\n");
        }
    }

    // a square must not have two numbers
    for (int r = 0; r < 9; r++) 
        for (int c = 0; c < 9; c++) 
            for (int n1 = 1; n1 < 10; n1++) 
                for (int n2 = 1; n2 < 10; n2++)
                    if (n1 != n2)
                        printf("-%d -%d 0\n", rcc(r, c, n1), rcc(r, c, n2));

    // boxes must disagree
    for (int br = 0; br < 3; br++)
        for (int bc = 0; bc < 3; bc++)
            for (int bir1 = 0; bir1 < 3; bir1++)
                for (int bic1 = 0; bic1 < 3; bic1++)
                    for (int bir2 = 0; bir2 < 3; bir2++)
                        for (int bic2 = 0; bic2 < 3; bic2++)
                            for (int n = 1; n < 10; ++n)
                                if (bir1 != bir2 && bic1 != bic2)
                                    printf("-%d -%d 0\n", rcc(br*3+bir1,bc*3+bic1,n), rcc(br*3+bir2,bc*3+bic2, n));

    // cols must disagree
    for (int c = 0; c < 9; c++)
        for (int r1 = 0; r1 < 9; r1++)
            for (int r2 = 0; r2 < 9; r2++)
                if (r1 != r2)
                    for (int n = 1; n < 10; ++n)
                        printf("-%d -%d 0\n", rcc(r1, c, n), rcc(r2, c, n));

    // rows must disagree
    // there's a forgone fusion with the preivous
    for (int r = 0; r < 9; r++)
        for (int c1 = 0; c1 < 9; c1++)
            for (int c2 = 0; c2 < 9; c2++)
                if (c1 != c2)
                    for (int n = 1; n < 10; ++n)
                        printf("-%d -%d 0\n", rcc(r, c1, n), rcc(r, c2, n));

    // must match the original problem 
    for (int r = 0; r < 9; r++)
        for (int c = 0; c < 9; c++)
            if (problem[r*9+c] != '0')
                printf("%d 0\n", rcc(r, c, problem[r*9+c]-'0'));
}