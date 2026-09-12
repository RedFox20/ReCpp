// Constructs an rpp::concurrent_queue from an import alone, which gcc-14 crashed on at -O1
// and above until BUGS.md B18 was fixed. The umbrella carries the same module, so both run.
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
