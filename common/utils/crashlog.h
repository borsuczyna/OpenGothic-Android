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

    /// The biggest mappings of the address space, summed by the file (or the name) and the rights: what takes the virtual memory of the process.
    /// Empty when the mappings are smaller than minTotalGb. It does not allocate while it reads, so it is fit for the report of std::bad_alloc.
    /// Not thread-safe: the render thread and the crash handler only.
    static std::string mappingsSummary(size_t top, double minTotalGb = 0);

  private:
    static void tracebackStd(std::ostream& out);
    static void tracebackAndroid(std::ostream &out);
    static void tracebackLinux(std::ostream &out);
    static void tracebackGpu(std::ostream &out, const char* extGpuLog);
    static void writeSysInfo(std::ostream& fout);
  };

