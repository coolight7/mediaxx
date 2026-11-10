#include "util/log.h"
#include <memory>

// 调试构建: 把 cxx_utilxx_base 的日志输出接到 stderr (与之前直接写 stderr 的表观一致)。
// 发布/性能构建里 XX_LOG* 是空宏, 不会有日志进来, sink 也不注册。
#ifdef LUMENXX_BUILD_TYPE

namespace {

    /// 日志输出: onLog 在后台线程里串行调用, 这里直接写 stderr
    class StderrLogSink final : public utilxx_base::ThreadedLogSink {
    public:

        ~StderrLogSink() override {
            // 必须在本类析构里停线程: 基类析构时虚表已切到基类,
            // 线程再调 onLog 就是纯虚调用 (进程退出瞬间最容易触发)
            shutdownThread();
        }

    protected:

        void onLog(const utilxx_base::LogEntry& entry) override {
            std::cerr << entry.message << std::endl;
        }
    };

    /// 启动时把 sink 注册进全局分发器; 分发器只持 weak_ptr, 退出时不需要反注册
    struct SinkInstaller {
        std::shared_ptr<StderrLogSink> sink = std::make_shared<StderrLogSink>();

        SinkInstaller() {
            utilxx_base::LogDispatcher::instance().addSink(sink);
        }
    };

    SinkInstaller g_sinkInstaller;

} // namespace

#endif // LUMENXX_BUILD_TYPE
