#pragma once

#include <cstdint>
#include <string_view>

/// Safe GPU mode.
///
/// The compiler of the shaders in the driver of a GPU can crash the whole process (SIGSEGV in libGLES_mali.so on Mali-G57, a try/catch does not see
/// it). The program cannot survive such a crash, but it can learn from it:
///  - while the shaders are compiled, the file `safegpu.lock` exists and has the name of the shader that the driver got last;
///  - the lock that is still there at the next start means the previous start died in the compiler of the driver: the program writes
///    `safegpu.mode` (the GPU, the level, the shader) and from now on uses the safe variants of the shaders for that GPU;
///  - the safe variant has the same bindings and the same result, it only does not use what the compiler of the driver cannot take
///    (`textureQueryLevels` and samplers passed to functions in clouds_lut). The other GPUs keep the original shaders.
/// A crash at the next start again (the lock and a fresh crash.log) raises the level: the optional pipelines (ray tracing, mesh shaders,
/// bindless, virtual shadows) are off too.
///
/// The command line `-safegpu auto|off|on|minimal` overrides the file; `off` deletes it. The mode is bound to the name of the GPU:
/// another GPU (or an update of the driver that changes its name) starts in the normal mode.
class SafeGpu final {
  public:
    enum Level : int {
      Normal  = 0,
      Safe    = 1,  // safe variants of the shaders that are known to crash drivers
      Minimal = 2,  // + no optional pipelines
      };

    /// reads the state that the previous start has left; call it once, before the first shader is compiled
    static void  init(std::string_view gpuName);

    static Level level();
    static bool  isSafe()    { return level()>=Safe;    }
    static bool  isMinimal() { return level()>=Minimal; }

    /// the occluders and the pyramid of HiZ are not made, the main view is culled without the test of HiZ (a bit more is drawn): at the minimal
    /// level (it draws no occluders, and the pyramid of an empty depth killed the drivers of Adreno 618 and 650), and after a death in the build
    /// of HiZ (a command of the dispatch, not the compiler: no lock, but the crash.log of the previous start names Renderer::buildHiZ)
    static bool  noHiZ();

    /// the shader that is going to be compiled by the driver now; the name stays in the lock if the driver kills the process
    static void  trace(std::string_view shader);

    /// a pipeline that the driver compiles when the game draws it (Tempest, in VPipeline::instance), not in the compilation of the shaders at the start:
    /// the same lock, the same name. Tempest tells about it in its log: `vkCreateGraphicsPipelines: <shaders>` and `vkCreateGraphicsPipelines: done`
    static void  driverCompile(std::string_view shaders);
    static void  driverDone();

    /// handles the lines of the log of Tempest above; returns false for any other line
    static bool  onLog(std::string_view line);

    /// the lock exists while there is a scope alive; compilations of the shaders on other threads can overlap
    class Scope final {
      public:
        Scope();
        ~Scope();
        Scope(const Scope&) = delete;
        Scope& operator = (const Scope&) = delete;
      };
  };
