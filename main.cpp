#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
#include <queue>

using namespace std;

typedef vector<bool>::reference status;
typedef function<void(status complete)> func;

mutex m1, m2;

class safe_queue
{
public:
    queue<func> tasks;

    void push(func &f)
    {
        m1.lock();
        tasks.push(f);
        m1.unlock();
    }
    func pop()
    {
        m1.lock();
        auto next = tasks.front();
        tasks.pop();
        m1.unlock();
        return next;
    }
    bool has_items()
    {
        return !tasks.empty();
    }
};

class thread_pool
{
public:
    vector<thread> pool;
    vector<bool> ready;
    safe_queue tasks;
    thread handle;
public:
    thread_pool(int size)
    {
        pool = vector<thread>(size);
        ready = vector<bool>(size, true);
        handle = thread(&thread_pool::work, this);
    }
    ~thread_pool()
    {
        handle.detach();
    }
    void work()
    {
        while (true)
        {
            for (int n=0; n<pool.size(); n++)
            {
                if (tasks.has_items() == true && ready[n] == true)
                {
                    ready[n] = false;
                    pool[n] = thread(tasks.pop(), ready[n]);
                    pool[n].detach();
                }
            }
        }
    }
    void submit(func &f)
    {
        tasks.push(ref(f));
    }
};

int main()
{
    thread_pool p(4);

    func first = [&](status complete) {
        m2.lock();
        cout << "first_func_running..." << endl;
        m2.unlock();
        complete = true;
    };
    func second = [&](status complete) {
        m2.lock();
        cout << "second_func_running..." << endl;
        m2.unlock();
        complete = true;
    };

    while (true)
    {
        p.submit(first);
        p.submit(second);
        this_thread::sleep_for(1s);
    }

    return 0;
}