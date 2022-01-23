#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>

// #define PERCENT_LEFT_ESTIMATE 0
// #define IMPLICATION_GRAPH_DEBUG 0

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
sign(int a) {
    return a ? 1-2*(a < 0) : 0;
}

char *filename;

int
main(int argc, char *argv[]) {

    // mmap in the file
    filename = argv[1];
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

    // page through the file till the problem description (the 'p' row)
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

    // page through the clauses, adding literals to a literal array, and keeping track of clause counts
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

    int *variableStack = literalOffset+sizeof(int)*(1+nliteral);
    int *roundHandled  = variableStack+sizeof(int)*(1+nvariables);
    int graphBufferI   = 0;
    int *graphBuffer   = roundHandled+sizeof(int)*(1+nvariables);

    int stackPos = 1;
    memset(variableStack,0,sizeof(int)*(1+nvariables));
    memset(roundHandled,0,sizeof(int)*(1+nvariables));
    
    for (;;) {

        // if neutral, find next free variable
        if (variableStack[stackPos] == 0) {
            int v = 1;
            for (; v < nvariables && roundHandled[v]; ++v);
            if (v == nvariables) {
                printf("sat\n");
                break;
            }
            variableStack[stackPos] = v;
            roundHandled[v] = stackPos;
            graphBuffer[graphBufferI++] = v;
            graphBuffer[graphBufferI++] = 0; // we guessed it -- no implicants
        }

#ifdef PERCENT_LEFT_ESTIMATE
        // estimating percent done
        float percent_done = 0;
        for (int i = 1; i <= stackPos; ++i) {
            float delta = 1;
            for (int j = 0; j > ((variableStack[i] < 0) ? variableStack[i] : 0); --j) delta /= 2.0;
            if (variableStack[i] < 0) percent_done += delta;
        }
        printf("%.3f%%\n", percent_done*100.0);
#endif

        // propigate
        int contradiction = 0;
        int moreToFind;
        do {
            moreToFind = 0;
            
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
                    
                    if ((assigned < 0) == (literal < 0)) {
                        // this clause is satisfied
                        satisfied = 1;
                        break;
                    } 
                }

                if (satisfied) {
                    continue;
                }

                if (freeCount <= 0) {
                    contradiction = 1;
                    moreToFind = 0;
                    break;
                }

                if (freeCount == 1) {
                    graphBuffer[graphBufferI++] = lastLiteral;
                    graphBuffer[graphBufferI++] = (literalCountOffset[clause+1]-literalCountOffset[clause]-1);
                    for (int literaln = literalCountOffset[clause]; literaln < literalCountOffset[clause+1]; ++literaln)
                        if (literalOffset[literaln] != lastLiteral)
                            graphBuffer[graphBufferI++] = literalOffset[literaln];
                    roundHandled[abs(lastLiteral)] = stackPos*(1-2*(lastLiteral < 0));
                    moreToFind = 1;
                }
            }
        } while (moreToFind);

        if (contradiction) {
#ifdef IMPLICATION_GRAPH_DEBUG
            for (int i = 0; i < graphBufferI; ) {
                int v = graphBuffer[i++];
                int nimplicants = graphBuffer[i++];
                printf("%d: ", v);
                for (int j = 0; j < nimplicants; ++j) printf("%d ", graphBuffer[i++]);
                printf("\n");
            }
            return 0;
#endif

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

            // try next possibility
            roundHandled[variableStack[stackPos]] = -stackPos;
            variableStack[stackPos] *= -1;
        } else {
            stackPos++;
        }
    }

    for (int i = 1; i <= nvariables; ++i) printf("%d 0\n", sign(roundHandled[i])*i);
}   