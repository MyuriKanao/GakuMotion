#include "GkNativeLive.hpp"
#include <cstdio>
#include <cstring>
namespace NativeLiveRuntime
{
    struct FakeTask { int status{}; bool throwResult{}; int polls{}, awaits{}, consumes{}; };
    inline FakeTask* next{};
    inline bool failStatus{}, failAwait{};
    inline int calls[4]{}, pendingStatus{0}, successStatus{1};
    inline const void *taskStatus = &calls[0], *getAwaiter = &calls[1], *getResult = &calls[2], *start = &calls[3];
    namespace r
    {
        inline void Require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
        struct Root
        {
            void* value{}; Root() = default; explicit Root(void* input) : value(input) { Require(input, "missing root"); }
            void* Get() const { Require(value, "expired root"); return value; }
            void Reset() { value = nullptr; }
            explicit operator bool() const { return value != nullptr; }
        };
        struct Arena { void* Keep(void* value) { return value; } };
        inline void* Call(const void* method, void* target = nullptr, void** = nullptr)
        {
            if (method == start) return next;
            auto task = static_cast<FakeTask*>(target);
            if (method == getAwaiter) { ++task->awaits; Require(!failAwait, "awaiter failure"); return target; }
            if (method == getResult) { ++task->consumes; Require(!task->throwResult, "native fault or cancellation"); return nullptr; }
            throw std::runtime_error("unexpected method");
        }
        template<class T> T Read(const void*, void* target)
        { auto task = static_cast<FakeTask*>(target); ++task->polls; Require(!failStatus, "status failure"); return static_cast<T>(task->status); }
    }
}
inline struct { void* (*unbox)(void*) = [](void* value) { return value; }; } g_safeBones;
#include "GkNativeLiveTaskRuntime.hpp"
namespace
{
    int passed{}, failed{};
    void Check(bool ok, const char* message) { if (ok) ++passed; else { ++failed; std::printf("FAIL: %s\n", message); } }
    template<class F> bool Throws(F action) { try { action(); return false; } catch (...) { return true; } }
}
int main()
{
    using namespace NativeLiveRuntime;
    using GakuMotion::native_live::Operation;
    FakeTask success; next = &success;
    Task task; task.Start(start, nullptr, nullptr);
    Check(task.Poll() == Operation::Pending && task.value && success.consumes == 0, "pending async source stays rooted and unconsumed");
    Check(Throws([&] { task.Start(start, nullptr, nullptr); }), "pending task cannot be replaced");
    success.status = 1;
    Check(task.Poll() == Operation::Ready && !task.value && success.consumes == 1, "completed UniTask consumed exactly once");
    Check(task.Poll() == Operation::Ready && success.polls == 2 && success.consumes == 1, "pooled source never polled or consumed twice");
    for (int status : {2, 3})
    {
        FakeTask failedTask{status, true}; next = &failedTask; task.Start(start, nullptr, nullptr);
        Check(task.Poll() == Operation::Failed && !task.value && failedTask.consumes == 1, "faulted and canceled task GetResult both consumed");
        task.Poll(); Check(failedTask.consumes == 1, "fault/cancel consumption is also once-only");
    }
    FakeTask retry{1}; next = &retry; task.Start(start, nullptr, nullptr); failStatus = true;
    Check(Throws([&] { task.Poll(); }) && task.value && retry.consumes == 0, "status query failure preserves task for cleanup retry");
    failStatus = false; failAwait = true;
    Check(Throws([&] { task.Poll(); }) && task.value && retry.consumes == 0, "awaiter acquisition failure preserves task");
    failAwait = false;
    Check(task.Poll() == Operation::Ready && retry.consumes == 1, "cleanup can resume and consume after metadata invocation failure");
    std::printf("Native Live UniTask adapter: %d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
