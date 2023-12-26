#include <bits/stdc++.h>

#include <parlay/parallel.h>
#include <parlay/primitives.h>
#include <parlay/sequence.h>

using namespace parlay;
using namespace std;

const int N = 500;
const int RUNS = 5;

struct Graph
{
    // sequence<sequence<int>> adjacency_list;
    int n;
    int vertices;

    Graph(int _n)
    {
        n = _n;
        vertices = n * n * n;
    }

    int get_index(int i, int j, int k, int n)
    {
        return i * n * n + j * n + k;
    }

    sequence<int> adjacency_list(int v) const
    {
        sequence<int> adjacency_list;
        adjacency_list.reserve(3);
        int i = v / (n * n);
        int j = (v / n) % n;
        int k = v % n;

        int index = i * n * n + j * n + k;

        if (k < n - 1)
            adjacency_list.push_back(i * n * n + j * n + k + 1);

        if (j < n - 1)
            adjacency_list.push_back(i * n * n + (j + 1) * n + k);

        if (i < n - 1)
            adjacency_list.push_back((i + 1) * n * n + j * n + k);

        return adjacency_list;
    }

    size_t adjacency_list_size(int v) const
    {
        int i, j, k;
        i = v / (n * n);
        j = (v / n) % n;
        k = v % n;

        uint32_t result = 0;
        if (k < n - 1)
            result++;
        if (j < n - 1)
            result++;
        if (i < n - 1)
            result++;
        return result;
    }
};


struct _Graph
{
    sequence<sequence<int>> _adjacency_list;
    int vertices;

    _Graph(sequence<sequence<int>> __adjacency_list)
    {
        _adjacency_list = __adjacency_list;
        vertices = __adjacency_list.size();
    }


    sequence<int> adjacency_list(int v) const
    {
        return _adjacency_list[v];
    }

    size_t adjacency_list_size(int v) const
    {
        return _adjacency_list[v].size();
    }
};


template <typename G>
sequence<int> bfs_sequential(const G &graph, int start)
{
    queue<int> queue;
    queue.push(start);

    sequence<bool> visited(graph.vertices, false);
    visited[start] = true;

    sequence<int> d(graph.vertices, INT_MAX);
    d[start] = 0;

    while (!queue.empty())
    {
        int u = queue.front();
        queue.pop();

        for (auto v : graph.adjacency_list(u)) {
            if (!visited[v]) {
                visited[v] = true;
                d[v] = d[u] + 1;
                queue.push(v);
            }
        }
    }

    return d;
}

void print(sequence<int> s)
{
    for (int i = 0; i < s.size(); i++)
    {
        cout << "s[" << i << "] = " << s[i] << endl;
    }
    cout << endl;
}

void print(sequence<size_t> s)
{
    for (int i = 0; i < s.size(); i++)
    {
        cout << "s[" << i << "] = " << s[i] << endl;
    }
    cout << endl;
}

template <typename G>
sequence<int> bfs_parallel(const G &graph, int start)
{
    sequence<int> frontier;
    frontier.push_back(start);

    vector<atomic_bool> visited(graph.vertices);
    visited[start] = true;

    sequence<int> d(graph.vertices, INT_MAX);
    d[start] = 0;

    int cur_d = 1;
    while (frontier.size() > 0)
    {
        auto frontier_degrees = parlay::map(frontier, [&graph](int v)
                                            { return graph.adjacency_list_size(v); });

        auto new_frontier_size = parlay::scan_inplace(frontier_degrees, parlay::plus<int>());
        sequence<int> new_frontier(new_frontier_size, INT_MAX);
        parlay::parallel_for(0, frontier.size(), [&frontier, &graph, &visited, &new_frontier, &frontier_degrees, &d, &cur_d](size_t i)
                             {
            int u = frontier[i];
            for (auto v : graph.adjacency_list(u)) {
                bool wanted = false;
                if (visited[v].compare_exchange_strong(wanted, true)) {
                    new_frontier[frontier_degrees[i]++] = v;
                    d[v] = cur_d;
                }
            } });

        ++cur_d;
        frontier = filter(new_frontier, [](int v)
                          { return v != INT_MAX; });
    }

    return d;
}

void Experiment()
{
    long parallel_duration = 0;
    long sequential_duration = 0;
    Graph graph(N);
    int s = 0;

    for (int run = 1; run <= RUNS; run++)
    {
        auto start = std::chrono::high_resolution_clock::now();
        auto d_par = bfs_parallel(graph, s);
        auto finish = std::chrono::high_resolution_clock::now();
        std::cout << "Run #" << run << ": par took "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(finish - start).count() << "ms" << std::endl;
        parallel_duration += std::chrono::duration_cast<std::chrono::milliseconds>(finish - start).count();

        start = std::chrono::high_resolution_clock::now();
        auto d_seq = bfs_sequential(graph, s);
        finish = std::chrono::high_resolution_clock::now();
        std::cout << "Run #" << run << ": seq took "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(finish - start).count() << "ms" << std::endl;
        sequential_duration += std::chrono::duration_cast<std::chrono::milliseconds>(finish - start).count();

        for (size_t i = 0; i < d_par.size(); ++i)
        {
            assert(d_par[i] == d_seq[i]);
        }
    }

    double average_parallel_result = double(parallel_duration) / double(RUNS);
    std::cout << "Average parallel result: " << average_parallel_result << "ms\n";
    double average_sequential_result = double(sequential_duration) / double(RUNS);
    std::cout << "Average sequential result: " << average_sequential_result << "ms\n";
    std::cout << "Boost: " << average_sequential_result / average_parallel_result << "\n";
}

void Test() {
    _Graph graph({{1, 2}, {2, 3}, {3}, {4}, {0, 1, 5}, {}});
    int s = 0;
    auto d_par = bfs_parallel(graph, s);
    auto d_seq = bfs_sequential(graph, s);

    for (size_t i = 0; i < d_par.size(); ++i)
    {
        assert(d_par[i] == d_seq[i]);
    }
}

int main()
{
    Experiment();
    Test();
}