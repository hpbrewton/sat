#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>

// #define PERCENT_LEFT_ESTIMATE 0
// #define IMPLICATION_GRAPH_DEBUG 0

/*
memory structure
| nclauses * int | nliterals * int | nvariables * int     |  nvariables * int        | ...               |
| clause sizes   |  literals       |  variables to handle |  round handled * sign    | graph buffer ...  |
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
int *model;
int nvariables;
int nclauses;
int *literal_counts;
int *literals;

void
show_model() {
    for (int i = 1; i <= nvariables; ++i) printf("%d: %d\n", i, model[i]); printf("\n");
}

int
unit_propagation() 
{
    int literals_to_propagate = 0;
    do {
        literals_to_propagate = 0;
        for (int clause = 0; clause < nclauses; ++clause) {
            int free_literal_count = literal_counts[clause+1]-literal_counts[clause];
            int last_free = -1;
            int satisfied = 0;
            for (int literaln = literal_counts[clause]; literaln < literal_counts[clause+1]; ++literaln) {
                int literal = literals[literaln];
                int assignment = model[abs(literal)];

                if (((literal <= 0) == (assignment <= 0)) && assignment) {
                    satisfied = 1;
                    break; // clause is satisfied
                }
                else if (!assignment) { last_free = literal; }// we have a free literal
                else {--free_literal_count; } // we have a lost literal
            }

            if (satisfied) continue;

            // can we prop?
            if (free_literal_count == 1) {
                model[abs(last_free)] = sign(last_free);
                literals_to_propagate = 1;
            }

            // contradiction
            if (free_literal_count == 0) {
                return -1; // contradiction
            }
        }
    } while(literals_to_propagate);

    return 0; // ok
}

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
    nvariables = 0;
    nclauses = 0;
    int i = 0;
    for (; i < length; ++i) {
        if (file[i] == 'c') for (; i < length && file[i] != '\n'; ++i); 
        if (file[i] == 'p') {
            nvariables = takeNumber(file, &i, length);
            nclauses = takeNumber(file, &i, length);
            for (; i < length && file[i] != '\n'; ++i);
            break;
        }
    }
    
    literal_counts = block;
    literal_counts[0] = 0;
    literals = block+sizeof(int)*(1+nclauses);
    model = literals+sizeof(int)*(1+nvariables);

    // page through the clauses, adding literals to a literal array, and keeping track of clause counts
    nclauses = 0;
    int nliteral = 0;
    for (; i < length; ++i) {
        // get a number
        int number = takeNumber(file, &i, length);
        if (number == 0) {
            literal_counts[++nclauses] = nliteral;
        } else {
            literals[nliteral++] = number;
        }
    }

    model = literals+sizeof(int)*(nliteral);
    memset(model, 0, sizeof(int)*(1+nvariables));
    model[1]=1;
    model[10]=1;
    int contradiction = unit_propagation();
    show_model();
}   