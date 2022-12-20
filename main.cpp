#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
#include <queue>
#include <future>

using namespace std;

template<typename T>
class safe_queue
{
    queue<T> m_items;
    mutex mt;
public:
    void push(T &item)
    {
        lock_guard<mutex> lk(mt);
        m_items.push(move(item));
    }
    T pop()
    {
        lock_guard<mutex> lk(mt);
        auto first = move(m_items.front());
        m_items.pop();
        return first;
    }
    bool has_items()
    {
        return !m_items.empty();
    }
};

typedef packaged_task<int()> func;
typedef pair<func,string> task;
typedef tuple<int,bool,shared_future<int>> info;

class thread_pool
{
    safe_queue<task> tasks;
    vector<thread> pool;
    vector<info> res;
    thread handler;
    thread logger;
    mutex mt;
public:
    thread_pool(int size)
    {
        pool = vector<thread>(size);
        res = vector<info>(size);

        for (int n=0; n<size; n++)
        {
            get<0>(res[n]) = 0;
            get<1>(res[n]) = true;
            get<2>(res[n]) = async([]{return 0;});
        }

        handler = thread(&thread_pool::work, this);
        logger = thread(&thread_pool::report, this);
    }
    ~thread_pool()
    {
        handler.join();
        logger.join();
    }
    void work()
    {
        while (true)
        {
            for (int n=0; n<pool.size(); n++)
            {
                if (tasks.has_items())
                if (get<1>(res[n]) == true)
                if (get<2>(res[n]).wait_for(0ms) == future_status::ready)
                {
                    lock_guard<mutex> lk(mt);
                    auto pop_pair = tasks.pop();
                    auto task = move(pop_pair.first);
                    auto tag = move(pop_pair.second);
                    get<2>(res[n]) = task.get_future();
                    get<1>(res[n]) = false;
                    get<0>(res[n]) = n;
                    cout << "Thread " << n << " started function " << tag << endl;
                    pool[n] = thread(move(task));
                }
            }
        }
    }
    void report()
    {
        while (true)
        {
            for (int n=0; n<pool.size(); n++)
            {
                if (get<1>(res[n]) == false)
                if (get<2>(res[n]).wait_for(0ms) == future_status::ready)
                {
                    lock_guard<mutex> lk(mt);
                    auto id = get<0>(res[n]);
                    auto result = get<2>(res[n]).get();
                    cout << "Thread " << id << " completed with value " << result << endl;
                    get<1>(res[n]) = true;
                    pool[n].join();
                }
            }
        }
    }
    void submit(func &foo, const string &tag)
    {
        task this_pair(move(foo),move(tag));
        tasks.push(this_pair);
    }
};

int main()
{
    thread_pool p(4);
    auto my_func_1 = [&]() 
    {
        cout << "first_func_running..." << endl;
        this_thread::sleep_for(600ms);
        return 1;
    };
    auto my_func_2 = [&]() 
    {
        cout << "second_func_running..." << endl;
        this_thread::sleep_for(800ms);
        return 2;
    };
    
    while (true)
    {
        func first_foo(my_func_1);
        func second_foo(my_func_2);
        p.submit(first_foo, "func_1");
        p.submit(second_foo, "func_2");
        this_thread::sleep_for(1000ms);
    }

    return 0;
}