#include "../UnityFPSUnlocker/unity/ondemand_runtime.hh"
#include <cassert>
#include <cstring>

namespace {
int interval_value = 6;
int static_set_calls = 0;
int image_marker = 0;
int klass_marker = 0;
int field_marker = 0;
int type_marker = 0;
int field_kind = 0x08;
unsigned field_flags = 0x0010U;
const void* get_field_type(void* field) {
    return field == &field_marker ? &type_marker : nullptr;
}
int get_type(const void* type) {
    return type == &type_marker ? field_kind : -1;
}
unsigned get_flags(void* field) {
    return field == &field_marker ? field_flags : 0;
}

void* get_domain() { return &image_marker; }
void* open_assembly(void*, const char* name) {
    return std::strcmp(name, "UnityEngine.CoreModule.dll") == 0
        ? &image_marker : nullptr;
}
const void* get_image(const void* assembly) {
    return assembly == &image_marker ? &image_marker : nullptr;
}
void* get_class(const void* image, const char* ns, const char* name) {
    if (image == &image_marker &&
        std::strcmp(ns, "UnityEngine.Rendering") == 0 &&
        std::strcmp(name, "OnDemandRendering") == 0) return &klass_marker;
    return nullptr;
}
void* get_field(void* klass, const char* name) {
    return klass == &klass_marker &&
        std::strcmp(name, "m_RenderFrameInterval") == 0 ? &field_marker : nullptr;
}
void read_field(void* field, void* dst) {
    assert(field == &field_marker);
    *static_cast<int*>(dst) = interval_value;
}
void write_field(void* field, void* src) {
    assert(field == &field_marker);
    interval_value = *static_cast<int*>(src);
    static_set_calls++;
}
}

int main() {
    UnityOnDemand::Runtime missing;
    assert(!missing.Observe(false).available);

    UnityOnDemand::Api api{};
    api.domain_get = get_domain;
    api.domain_assembly_open = open_assembly;
    api.assembly_get_image = get_image;
    api.class_from_name = get_class;
    api.class_get_field_from_name = get_field;
    api.field_get_type = get_field_type;
    api.type_get_type = get_type;
    api.field_get_flags = get_flags;
    api.field_static_get_value = read_field;
    api.field_static_set_value = write_field;

    UnityOnDemand::Runtime runtime;
    runtime.Init(api);
    auto probe = runtime.Observe(false);
    assert(probe.available && probe.before == 6 && probe.after == 6);
    assert(!probe.changed && static_set_calls == 0);
    auto force = runtime.Observe(true);
    assert(force.available && force.before == 6 && force.after == 1);
    assert(force.changed && static_set_calls == 1);
    auto repeat = runtime.Observe(true);
    assert(repeat.available && repeat.before == 1);
    assert(!repeat.changed && static_set_calls == 1);

    // If the runtime lacks write support, the mode MUST remain read-only.
    UnityOnDemand::Api readonly = api;
    readonly.field_static_set_value = nullptr;
    UnityOnDemand::Runtime restricted;
    restricted.Init(readonly);
    interval_value = 4;
    auto restricted_observation = restricted.Observe(true);
    assert(restricted_observation.available && restricted_observation.before == 4);
    assert(restricted_observation.after == 4 && !restricted_observation.changed);
    assert(static_set_calls == 1);

    // Reject a similarly named but non-int32 field before read/write.
    field_kind = 0x12; // IL2CPP_TYPE_CLASS
    UnityOnDemand::Runtime wrong_type;
    wrong_type.Init(api);
    const auto invalid_kind = wrong_type.Observe(true);
    assert(!invalid_kind.available);
    assert(static_set_calls == 1);

    field_kind = 0x08;
    field_flags = 0; // instance field, not a managed static
    UnityOnDemand::Runtime wrong_flags;
    wrong_flags.Init(api);
    const auto invalid_flags = wrong_flags.Observe(true);
    assert(!invalid_flags.available);
    assert(static_set_calls == 1);

    // Missing metadata accessors must disable even read-only observations.
    UnityOnDemand::Api no_metadata = api;
    no_metadata.field_get_flags = nullptr;
    UnityOnDemand::Runtime missing_metadata;
    missing_metadata.Init(no_metadata);
    assert(!missing_metadata.Observe(false).available);
    assert(!missing_metadata.Observe(true).available);

    return 0;
}
