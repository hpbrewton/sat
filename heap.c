int parent(int n) { return (i-1)/2; }
int child1(int n) { return 2*n+1; }
int child2(int n) { return 2*n+2; }

int siftup(int *data, int n) { 
    for(; data[n] > data[parent(n)];) {
        int temp = data[n];
        data[n] = data[parent(n)];
        data[parent(n)] = temp;
    }
}