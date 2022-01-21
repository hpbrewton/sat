#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>

/*
| nclauses * int | nliterals * int | nvariables * int     |  nvariables * int        |
  clause sizes   |  literals       |  variables to handle |  round handled * sign    |
*/

int 
abs(int a) {
    return (a < 0) ?  (-a) : (a);
}

int 
takeNumber(char *buffer, int *pos, int length) {
    for (; *pos < length && (buffer[*pos] < '0' || buffer[*pos] > '9') && buffer[*pos] != '-'; ++(*pos));
    int sign = 1;
    int magnitude = 0;
    if (buffer[*pos] == '-') {
        sign = -1;
        ++(*pos);
    }
    for (; (*pos) < length && buffer[*pos] >= '0' && buffer[*pos] <= '9'; ++(*pos)) magnitude=magnitude*10+(buffer[*pos]-'0');
    return sign*magnitude;
}

int
main(int argc, char *argv[]) {
    // mmap in the file
    //char *filename = "/Users/harrison/dextrose/fermat-907547022132073.cnf";
    char *filename = "ex2.cnf";
    int cnf_file = open(filename, O_RDONLY);
    if (cnf_file < 0) {
        printf("Could not open the CNF file\n");
        return -1;
    }
    int length = lseek(cnf_file, 0, SEEK_END);
    lseek(cnf_file, 0, SEEK_SET);
    char *file = (char*) mmap(NULL, length, PROT_READ, MAP_SHARED, cnf_file, 0);
    
    // set up memeory to load in the clauses
    int *block = mmap(NULL, 2000000000, PROT_READ|PROT_WRITE, MAP_ANON|MAP_SHARED, -1, 0);
    if (!block) {
        printf("Could not allocate enough space\n");
        return -1;
    }

    // page through the file
    int nvariables = 0;
    int nclauses = 0;
    int i = 0;
    for (; i < length; ++i) {
        if (file[i] == 'c') for (; i < length && file[i] != '\n'; ++i); 
        if (file[i] == 'p') {
            nvariables = takeNumber(file, &i, length);
            nclauses = takeNumber(file, &i, length);
            break;
        }
    }
    
    int *literalCountOffset = block;
    literalCountOffset[0] = 0;
    int *literalOffset = block+sizeof(int)*(1+nclauses);
    int *variableStack = literalOffset+sizeof(int)*(1+nvariables);
    int *roundHandled = variableStack+sizeof(int)*(1+nvariables);

    // page through the file (differently)
    int nclause = 0;
    int nliteral = 0;
    for (; i < length; ++i) {
        // get a number
        int number = takeNumber(file, &i, length);
        if (number == 0) {
            literalCountOffset[++nclause] = nliteral;
        } else {
            literalOffset[nliteral++] = number;
        }
    }

    int stackPos = 1;
    memset(variableStack,0,sizeof(int)*(1+nvariables));
    memset(roundHandled,0,sizeof(int)*(1+nvariables));
    
    for (;;) {
        if (stackPos > nvariables) {
            printf("sat");
            break;
        }

        // if neutral, find next free variable
        if (variableStack[stackPos] == 0) {
            int v = 1;
            for (; v < nvariables && roundHandled[v]; ++v);
            variableStack[stackPos] = v;
            roundHandled[v] = stackPos;
        }

        // propigate
        int contradiction = 0;
        int moreToFind;
        do {
            moreToFind = 0;

            for (int i = 0; i < nvariables; ++i) printf("%d,", roundHandled[i]);
            printf("\n");
            
            // propigate step
            for (int clause = 0; clause < nclauses; ++clause) {
                int lastLiteral = 0;
                int freeCount = 0;
                int satisfied = 0;
                for (int literaln = literalCountOffset[clause]; literaln < literalCountOffset[clause+1]; ++literaln) {
                    int literal = literalOffset[literaln];
                    int assigned = roundHandled[abs(literal)];

                    if (!assigned) {
                        // this literal is not assigned
                        lastLiteral = literal;
                        freeCount++;
                        continue;
                    }
                    
                    printf("\t\ta: %d, l: %d\n", assigned, literal);
                    if ((assigned < 0) == (literal < 0)) {
                        // this clause is satisfied
                        satisfied = 1;
                        break;
                    } 
                }

                printf("\tc: %d, s: %d, fc: %d\n", clause, satisfied, freeCount);

                if (satisfied) {
                    continue;
                }

                if (freeCount <= 0) {
                    contradiction = 1;
                    moreToFind = 0;
                    break;
                }

                if (freeCount == 1) {
                    roundHandled[abs(lastLiteral)] = stackPos*(1-2*(lastLiteral < 0));
                    printf("learned %d %d\n",lastLiteral, roundHandled[abs(lastLiteral)]);
                    moreToFind = 1;
                }
            }
        } while (moreToFind);

        printf("X: %d\n", contradiction);
        if (contradiction) {
            // forget everything learned in round
            for (int i = 1; i <= nvariables; ++i) 
                if (abs(roundHandled[i]) >= stackPos)
                    roundHandled[i] = 0;
            // clear to last positive on the stack
            for (; stackPos >= 0 && variableStack[stackPos] < 0; stackPos--) variableStack[stackPos] = 0;
            if (stackPos < 1) {
                printf("unsat\n");
                return 0;
            }
            roundHandled[variableStack[stackPos]] = -stackPos;
            variableStack[stackPos] *= -1;
        } else {
            stackPos++;
        }
    }
}   