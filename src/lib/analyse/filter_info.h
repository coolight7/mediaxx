#pragma once

extern "C" {
#include <libavfilter/avfilter.h>
#include <libavutil/avutil.h>
}

#include "simdjson.h"
#include "util/string_util.h"
#include <array>
#include <string>
#include <string_view>

namespace mediaxx {

    class FilterInfo_c {
    public:

        /// 用 lavfi 前缀区分 mpv 自带过滤器与 libavfilter 同名过滤器（见 mpv 的
        /// check_unknown_entry）
        inline static constexpr std::string_view MPV_LAVFI_PREFIX = "lavfi-";

        /// mpv 自带的过滤器名
        ///
        /// 来自 mpv 源码 filters/user_filters.c 的 af_list / vf_list，与过滤器重名时
        /// mpv 的 af / vf 会先按自带过滤器解析，要用 libavfilter 的那个必须加
        /// `lavfi-` 前缀。
        /// - 注意 rubberband、vapoursynth、vdpaupp 等是否编译进来由 mpv 的编译配置决定，
        ///   与 ffmpeg 无关，所以这里只用来判断"名字重不重"，不表示名字一定可用
        inline static constexpr std::array<std::string_view, 15> DEF_MPV_FILTER_NAMES = {
            // 音频
            "lavfi",
            "lavfi-bridge",
            "scaletempo",
            "scaletempo2",
            "format",
            "rubberband",
            "lavcac3enc",
            "drop",
            // 视频（lavfi、lavfi-bridge、format 音频部分已列出）
            "sub",
            "fingerprint",
            "vapoursynth",
            "vdpaupp",
            "vavpp",
            "d3d11vpp",
            "gpu",
        };

        /// 过滤器名是否与 mpv 自带过滤器重名
        [[nodiscard]] static bool isMpvFilterName(std::string_view name) {
            for (const auto& item : DEF_MPV_FILTER_NAMES) {
                if (item == name) {
                    return true;
                }
            }
            return false;
        }

        /// 过滤器能不能直接写在 mpv 的 af / vf 上
        ///
        /// 判断与 mpv 源码 filters/f_lavfi.c 里的 is_usable 相同：
        /// - 输入、输出端口都不能超过一个（mpv 只接受单路进、单路出的过滤器，
        ///   多路进出的要改用 lavfi 的图语法）
        /// - 端口数为 1 时，端口类型要与 [mediaType] 一致
        /// - 端口数为 0 时，要求过滤器带 AVFILTER_FLAG_DYNAMIC_INPUTS /
        ///   AVFILTER_FLAG_DYNAMIC_OUTPUTS（如 asplit 这种输出个数不定的）
        [[nodiscard]] static bool isUsable(const AVFilter* filter, AVMediaType mediaType) {
            const int nbInputs  = avfilter_filter_pad_count(filter, 0);
            const int nbOutputs = avfilter_filter_pad_count(filter, 1);
            if (nbInputs > 1 || nbOutputs > 1) {
                return false;
            }
            bool inputOk  = filter->flags & AVFILTER_FLAG_DYNAMIC_INPUTS;
            bool outputOk = filter->flags & AVFILTER_FLAG_DYNAMIC_OUTPUTS;
            if (nbInputs == 1) {
                inputOk = avfilter_pad_get_type(filter->inputs, 0) == mediaType;
            }
            if (nbOutputs == 1) {
                outputOk = avfilter_pad_get_type(filter->outputs, 0) == mediaType;
            }
            return inputOk && outputOk;
        }

        /// 遍历链接进来的 ffmpeg，取出这一类的过滤器列表
        ///
        /// ## Args:
        /// - [mediaType] 过滤器类型，取 AVMEDIA_TYPE_AUDIO 为音频过滤器，
        ///   取 AVMEDIA_TYPE_VIDEO 为视频过滤器
        ///
        /// ## Return:
        /// - json 数组，每一项是一个过滤器：
        ///   {"name":名字, "desc":说明, "flags":AVFILTER_FLAG_* 位掩码, "mpv_name":mpv
        ///   里该用的名字}
        ///
        /// 说明：
        /// - 只列出 mpv 的 af / vf 能直接使用的（见 [isUsable]）；
        ///   mpv 自带的过滤器（format、scaletempo 等）没有列进来，它们由 mpv 编译，
        ///   可以读 mpv 的 option-info/af/choices、option-info/vf/choices
        /// - [desc] 可能为空，[mpv_name] 只在名字与 mpv 自带过滤器重名时才有
        static simdjson::builder::string_builder findAvailFilter(AVMediaType mediaType) {
            simdjson::builder::string_builder result{};
            const AVFilter*                   filter         = nullptr;
            void*                             iterationState = nullptr;

            result.start_array();
            bool isFirstItem = true;
            while ((filter = av_filter_iterate(&iterationState)) != nullptr) {
                if (false == isUsable(filter, mediaType)) {
                    continue;
                }

                if (false == isFirstItem) {
                    result.append_comma();
                }
                isFirstItem = false;

                const auto name = stringxx::toStringNotNull(filter->name);

                result.start_object();

                result.append_key_value<"name">(name);
                result.append_comma();

                if (nullptr != filter->description) {
                    result.append_key_value<"desc">(filter->description);
                    result.append_comma();
                }

                result.append_key_value<"flags">(int(filter->flags));

                if (isMpvFilterName(name)) {
                    // 与 mpv 自带过滤器重名，给出加 lavfi- 前缀后的名字
                    result.append_comma();
                    result.append_key_value<"mpv_name">(
                        std::string{MPV_LAVFI_PREFIX} + std::string{name}
                    );
                }

                result.end_object();
            }

            result.end_array();
            return result;
        }
    };
}; // namespace mediaxx
