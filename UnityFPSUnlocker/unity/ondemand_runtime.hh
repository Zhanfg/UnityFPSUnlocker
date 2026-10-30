#ifndef UNITY_ONDEMAND_RUNTIME_HH
#define UNITY_ONDEMAND_RUNTIME_HH

#include <atomic>
#include <cstdint>

namespace UnityOnDemand {

// Runtime metadata APIs are resolved dynamically. We never assume a
// version-specific Il2CppClass / FieldInfo / MethodInfo binary layout.
struct Api {
    void* (*domain_get)() = nullptr;
    void* (*domain_assembly_open)(void*, const char*) = nullptr;
    const void* (*assembly_get_image)(const void*) = nullptr;
    void* (*class_from_name)(const void*, const char*, const char*) = nullptr;
    void* (*class_get_field_from_name)(void*, const char*) = nullptr;
    void (*field_static_get_value)(void*, void*) = nullptr;
    void (*field_static_set_value)(void*, void*) = nullptr;
};
struct Observation {
    bool available = false;
    bool changed = false;
    int before = -1;
    int after = -1;
};

class Runtime {
public:
    void Init(const Api& api) { api_ = api; }

    // Call ONLY while forwarding an original game-originated Unity setter.
    // Force=1 is opt-in per game; otherwise the field is read only.
    Observation Observe(bool force_one) {
        Observation o;
        if (!api_.domain_get || !api_.domain_assembly_open ||
            !api_.assembly_get_image || !api_.class_from_name ||
            !api_.class_get_field_from_name || !api_.field_static_get_value) {
            return o;
        }
        void* field = field_.load(std::memory_order_acquire);
        if (!field) {
            void* domain = api_.domain_get();
            if (!domain) return o;
            const char* assemblies[] = {
                "UnityEngine.CoreModule", "UnityEngine.CoreModule.dll", "UnityEngine", nullptr
            };
            for (const char** name = assemblies; *name && !field; ++name) {
                const void* assembly = api_.domain_assembly_open(domain, *name);
                if (!assembly) continue;
                const void* image = api_.assembly_get_image(assembly);
                if (!image) continue;
                void* klass = api_.class_from_name(image, "UnityEngine.Rendering", "OnDemandRendering");
                if (klass) field = api_.class_get_field_from_name(klass, "m_RenderFrameInterval");
            }
            if (!field) return o;
            field_.store(field, std::memory_order_release);
        }
        api_.field_static_get_value(field, &o.before);
        o.after = o.before;
        o.available = true;
        if (force_one && api_.field_static_set_value && o.before != 1) {
            int one = 1;
            api_.field_static_set_value(field, &one);
            api_.field_static_get_value(field, &o.after);
            o.changed = o.after == 1;
        }
        return o;
    }
private:
    Api api_{};
    std::atomic<void*> field_{nullptr};
};
} // namespace UnityOnDemand
#endif
