#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>

int sat; // produce a sat or unsat problem
int nnode;
float pedge; // probability of an edge

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

float
takeFloat(char *buffer, int *pos, int length) {
    for (; *pos < length && (buffer[*pos] < '0' || buffer[*pos] > '9') && buffer[*pos] != '-'; ++(*pos));
    int sign = 1;
    float magnitude = 0;
    if (buffer[*pos] == '-') {
        sign = -1;
        ++(*pos);
    }
    for (; (*pos) < length && buffer[*pos] >= '0' && buffer[*pos] <= '9'; ++(*pos)) magnitude=magnitude*10.0+((float) (buffer[*pos]-'0'));
    if (*pos == length) {
        return magnitude*sign;
    }
    if (buffer[*pos] != '.') {
        return magnitude*sign;
    }
    (*pos)++;
    float magnitudeDec = 0;
    float places = 1;
    for (; (*pos) < length && buffer[*pos] >= '0' && buffer[*pos] <= '9'; ++(*pos)) {
        magnitudeDec=magnitudeDec*10.0+((float) (buffer[*pos]-'0'));
        places *= 10.0;
    }
    return magnitude*sign + magnitudeDec/places;
}

void
generateColoring(int *pairs, int npairs, int ncolors) {
    // i*ncolors+j+1 = variable i has color j
    int nvars = nnode*ncolors;
    int nclauses = nnode + nnode*ncolors*(ncolors-1) + npairs*ncolors;
    printf("p cnf %d %d\n", nvars, nclauses);

    // every variable must have a color
    for (int i = 0; i < nnode; ++i) {
        for (int j = 0; j < ncolors; ++j) {
            printf("%d ", i*ncolors+j+1);
        }
        printf("0\n");
    }

    // every variable must have no more than one color
    for (int i = 0; i < nnode; ++i) {
        for (int j = 0; j < ncolors; ++j) {
            for (int k = 0; k < ncolors; ++k) {
                if (j != k) {
                    printf("-%d -%d 0\n", i*ncolors+j+1, i*ncolors+k+1);
                }
            }
        }
    }

    // every variable must not share a color with its neighbors
    for (int i = 0; i < npairs; ++i) {
        for (int c = 0; c < ncolors; ++c) {
            printf("-%d -%d 0\n", pairs[2*i]*ncolors+c+1, pairs[2*i+1]*ncolors+c+1);
        }
    }
}

int 
main(int argc, char *argv[]) {
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "-s")) {
            sat = 1;
        }
        if (!strcmp(argv[i], "-n"))  {
            i++;
            if (i == argc) {
                return -1;
            }
            int length = 0;
            nnode = takeNumber(argv[i], &length, strlen(argv[i]));
        }
        if (!strcmp(argv[i], "-p")) {
            i++;
            if (i == argc) {
                return -1;
            }
            int length = 0;
            pedge = takeFloat(argv[i], &length, strlen(argv[i]));
        }
    }
    
    
    // set up memeory to load in the clauses
    int *block = mmap(NULL, 2000000000, PROT_READ|PROT_WRITE, MAP_ANON|MAP_SHARED, -1, 0);
    if (!block) {
        printf("Could not allocate enough space\n");
        return -1;
    }

    int c = 0;
    for (int i = 0; i < nnode; ++i) {
        for (int j = i+1; j < nnode; ++j) {
            if ((((float)rand()/(float)(RAND_MAX))) < pedge) {
                block[2*c]   = i;
                block[2*c+1] = j;
                ++c;
            }
        }
    }

    generateColoring(block, c, 4);
}