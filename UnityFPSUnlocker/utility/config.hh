#ifndef CONFIG_HEADER
#define CONFIG_HEADER

#include <jni.h>
#include <sys/mman.h>

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>

#include <absl/status/status.h>
#include <absl/status/statusor.h>

#include "logger.hh"

class ConfigValue {
public:
    int delay_ = 5;
    int fps_ = 0;
    bool mod_opcode_ = false;
    float scale_ = -1;
    bool render_interval_ = false;

    ConfigValue(){};
    ConfigValue(int delay, int fps, bool mod_opcode, float scale, bool render_interval = false)
        : delay_(delay),
          fps_(fps),
          mod_opcode_(mod_opcode),
          scale_(scale),
          render_interval_(render_interval){};
    ConfigValue(const ConfigValue& lhs) {
        delay_ = lhs.delay_;
        fps_ = lhs.fps_;
        mod_opcode_ = lhs.mod_opcode_;
        scale_ = lhs.scale_;
        render_interval_ = lhs.render_interval_;
    }

    void DebugPrint() const {
        LOG("\tdelay: %d | fps: %d | mod_opcode: %d | scale: %f", delay_, fps_, mod_opcode_, scale_);
    }
};

namespace Utility {
    absl::StatusOr<rapidjson::Document> LoadJsonFromFile(const char*);
    jobject GetApplication(JNIEnv* env);
    absl::StatusOr<jobject> GetApplicationInfo(JNIEnv* env);
    absl::StatusOr<std::string> GetLibraryPath(JNIEnv* env, jobject application_info);
    absl::StatusOr<JavaVM*> GetVM();
    int ChangeMemPermission(void* p, size_t n, int permission = PROT_READ | PROT_WRITE | PROT_EXEC);
    void NopFunc(unsigned char* ptr);
}; // namespace Utility

#endif // config.hh