#pragma once

#include <ostream>
#include <string>
#include <string_view>

class CrashLog final {
  public:
    static void setup();
    static void setGpu(std::string_view name);

    static void dumpStack(const char* sig, const char* extGpuLog);

    /// The memory of the process and of the system (Linux, Android): where a std::bad_alloc comes from, the process, the system or the limit of the mappings
    static std::string memoryInfo();

  private:
    static void tracebackStd(std::ostream& out);
    static void tracebackAndroid(std::ostream &out);
    static void tracebackLinux(std::ostream &out);
    static void tracebackGpu(std::ostream &out, const char* extGpuLog);
    static void writeSysInfo(std::ostream& fout);
  };

