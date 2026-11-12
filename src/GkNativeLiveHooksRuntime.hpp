#pragma once
namespace NativeLiveRuntime
{
    namespace frame_hooks
    {
        using Callback = void (*)(void*, const void*);
        inline Callback originalUpdate{}, originalLate{}, originalReceiver{};
        inline void *targets[3]{};
        inline std::atomic<int> controllerId{}, receiverId{}, controllerScene{}, receiverScene{};
        inline bool IsOwned(void* value, int expected, int scene)
        {
            if (!expected || !ProtectionOnUnityThread() || !value) return false;
            // Only inspect this fresh callback argument. Never dereference an
            // old cached component; require both component and scene identity.
            try
            {
                if (r::Read<int>(StageLoader::instanceId, value) != expected) return false;
                r::Arena roots; void* go = roots.Keep(r::Call(StageLoader::componentGo, value));
                return r::Read<int>(StageLoader::goScene, go) == scene;
            }
            catch (...) { return false; }
        }
        inline void Update(void* value, const void* method)
        { if (!IsOwned(value, controllerId.load(), controllerScene.load()) && originalUpdate) originalUpdate(value, method); }
        inline void Late(void* value, const void* method)
        { if (!IsOwned(value, controllerId.load(), controllerScene.load()) && originalLate) originalLate(value, method); }
        inline void Receiver(void* value, const void* method)
        { if (!IsOwned(value, receiverId.load(), receiverScene.load()) && originalReceiver) originalReceiver(value, method); }
        inline void Release();
        inline bool Install()
        {
            if (targets[0] || targets[1] || targets[2]) return targets[0] && targets[1] && targets[2] && originalUpdate && originalLate && originalReceiver;
            if (!GakuMotion::host::hooks.acquireBatch || !GakuMotion::host::hooks.release) return false;
            const void* callbackMethods[]{equipmentUpdate, equipmentLate, receiverUpdate};
            void* addresses[3]{};
            for (int i = 0; i < 3; ++i) { if (!callbackMethods[i]) return false; addresses[i] = *static_cast<void* const*>(callbackMethods[i]); }
            if (!addresses[0] || !addresses[1] || !addresses[2] || addresses[0] == addresses[1] || addresses[1] == addresses[2] || addresses[0] == addresses[2]) return false;
            const GakuHookRequest requests[]{
                {addresses[0], reinterpret_cast<void*>(Update), reinterpret_cast<void**>(&originalUpdate)},
                {addresses[1], reinterpret_cast<void*>(Late), reinterpret_cast<void**>(&originalLate)},
                {addresses[2], reinterpret_cast<void*>(Receiver), reinterpret_cast<void**>(&originalReceiver)}};
            const auto status = GakuMotion::host::hooks.acquireBatch(GakuMotion::host::hooks.context, 3, requests);
            // Batch acquisition is atomic in the host contract. Preserve any
            // returned trampoline/lease even on an abnormal host response.
            Callback originals[]{originalUpdate, originalLate, originalReceiver};
            for (int i = 0; i < 3; ++i) if (status == GAKU_OK || originals[i]) targets[i] = addresses[i];
            const bool ready = status == GAKU_OK && originalUpdate && originalLate && originalReceiver;
            if (!ready) Release();
            return ready;
        }
        inline void Release()
        {
            controllerId.store(0); receiverId.store(0); controllerScene.store(0); receiverScene.store(0);
            for (auto& target : targets)
                if (target && GakuMotion::host::hooks.release &&
                    GakuMotion::host::hooks.release(GakuMotion::host::hooks.context, target) == GAKU_OK) target = nullptr;
            // Trampolines are process-lifetime, including failed hook release.
        }
    }
}
