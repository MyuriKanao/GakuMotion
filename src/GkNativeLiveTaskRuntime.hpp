#pragma once
namespace NativeLiveRuntime
{
    struct Task
    {
        r::Root value;
        void Start(const void* method, void* target, void** args)
        {
            r::Require(!value, "owned task is already pending");
            value = r::Root(r::Call(method, target, args));
        }
        GakuMotion::native_live::Operation Poll()
        {
            using GakuMotion::native_live::Operation;
            if (!value) return Operation::Ready;
            void* task = g_safeBones.unbox(value.Get()); r::Require(task != nullptr, "owned task unbox failed");
            const int status = r::Read<int>(taskStatus, task);
            if (status == pendingStatus) return Operation::Pending;
            // Consume once on success, fault AND cancellation; UniTask sources
            // may be pooled and must not be polled after GetResult.
            r::Arena roots;
            void* awaiter = roots.Keep(r::Call(getAwaiter, task));
            void* data = awaiter ? g_safeBones.unbox(awaiter) : nullptr;
            r::Require(data != nullptr, "owned awaiter unbox failed");
            value.Reset();
            try { r::Call(getResult, data); }
            catch (...) { return Operation::Failed; }
            return status == successStatus ? Operation::Ready : Operation::Failed;
        }
    };
}
