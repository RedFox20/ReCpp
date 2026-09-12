// Constructs an rpp::concurrent_queue from an import alone, the shape which crashes gcc-14
// at -O1 and above, see BUGS.md C27.
#ifdef MAMA_HAS_MODULES

import rpp.threading;

int main()
{
    rpp::concurrent_queue<int> q;
    for (int i = 0; i < 64; ++i) // enough pushes to grow the ring, which is where memmove runs
        q.push(i);

    int value = -1;
    if (!q.try_pop(value) || value != 0) return 1;
    if (q.size() != 63) return 2;
    return 0;
}
#else
int main() { return 0; } // the header build does not exercise the module
#endif
