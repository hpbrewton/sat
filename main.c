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
int model_size;
int *model;
int nvariables;
int nclauses;
int *literal_counts;
int *literals;
int *decisions;
int ndecisions;
int current_level = 2;

// data for conflict analysis
int *impl_map_start;
int *impl_map;
int *impl_stack;
int impl_stack_pos;
int *impl_resolution;
int impl_resolution_pos;

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
                int var = abs(last_free);
                model[var] = sign(last_free)*current_level; ++model_size;
                impl_map[var] = clause;
                literals_to_propagate = 1;
            }

            // contradiction
            if (free_literal_count == 0) {
                return clause; // contradiction
            }
        }
    } while(literals_to_propagate);

    return 0; // ok
}

void 
conflict_clause(int clause)
{
    impl_stack_pos = 0;
    impl_resolution_pos = 0;

    // while stack
    do {

        // add clause's literals to stack
        if (clause > 0)
            for (int literaln = literal_counts[clause]; literaln < literal_counts[clause+1]; ++literaln)
                impl_stack[impl_stack_pos++] = literals[literaln];

        for (int i = 0; i < impl_stack_pos; ++i) printf("%d, ", impl_stack[i]); printf("|");
        for (int i = 0; i < impl_resolution_pos; ++i) printf("%d, ", impl_resolution[i]); printf("|");
        printf(clause > 0 ? "w%d\n" : "%d\n", clause);

        // pop a literal
        int literal = impl_stack[--impl_stack_pos];
        clause = impl_map[literal];

        int decision_level = abs(model[abs(literal)]);
        if (decision_level != current_level) {
            impl_resolution[impl_resolution_pos++] = literal;
            clause = -1;
        } else {
            impl_map[literal] = -1;
        }

    }  while(impl_stack_pos);

    // now, the resolution is a clause to add
    for (int i = 0; i < impl_stack_pos; ++i) printf("%d, ", impl_stack[i]); printf("|");
    for (int i = 0; i < impl_resolution_pos; ++i) printf("%d, ", impl_resolution[i]); printf("|");
    printf(clause > 0 ? "w%d\n" : "%d\n", clause);

    // decide level to backtrack to
    int backtrack_level = nvariables+1;
    for (int i = 0; i < impl_resolution_pos; ++i) {
        int curr_backtrack = abs(model[impl_resolution[i]]);
        if (curr_backtrack < backtrack_level) backtrack_level = curr_backtrack;
    }

    // now, do some clean up of the systems for a backtrack

    // shrink the model
    for (int i = 0; i < nvariables+1; ++i) {
        if (abs(model[i]) >= backtrack_level) {
            model_size--;
            model[i] = 0;
        }
    }

    // move to the backtrack level 
    for (int i = backtrack_level; i < ndecisions; ++i) decisions[i] = 0;
    current_level = backtrack_level;    
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
    impl_map        = model+sizeof(int)*(1+nvariables);
    impl_stack      = impl_map+sizeof(int)*(1+nvariables);
    impl_resolution = impl_stack+sizeof(int)*(1+nvariables);
    decisions = impl_resolution+sizeof(int)*(1+nvariables);
    for (int i = 0; i < 1+nvariables; ++i) impl_map[i] = -1;
    
    int contradiction = unit_propagation();
    if (contradiction) {
        printf("unsat\n");
        return -1;
    } else {
        while (model_size < nvariables && current_level > 0) {
            printf("%d\n", model_size);
            int chosen_variable = -1;
            for (int i = model_size+1; i < nvariables+1; ++i) {
                if (!model[i]) {
                    chosen_variable = i;
                    break;
                }
            }
            decisions[current_level++] = chosen_variable;
            model_size++;
            int contradiction = unit_propagation();
            if (contradiction) conflict_clause(contradiction);
        }
        if (model_size == nvariables && current_level > 0) printf("sat\n");
        else printf("unsat\n");
    }
}   