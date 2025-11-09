extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext.h>
}

#include "simdjson.h"
#include "util/log.h"
#include "util/string_util.h"
#include <vector>

class CodecInfo_c {
public:

    static simdjson::fallback::builder::string_builder findAvailCodec() {
        std::vector<const AVCodec*>                 hardware_codecs{};
        simdjson::fallback::builder::string_builder result{};
        const AVCodec*                              codec           = nullptr;
        void*                                       iteration_state = nullptr;

        result.start_array();

        bool isFirstItem = true;
        while ((codec = av_codec_iterate(&iteration_state)) != nullptr) {
            switch (codec->type) {
            case AVMediaType::AVMEDIA_TYPE_VIDEO:
            case AVMediaType::AVMEDIA_TYPE_AUDIO:
                {
                    if (false == isFirstItem) {
                        result.append_comma();
                    }
                    isFirstItem = false;
                    result.start_object();

                    result.append_key_value<"type">(int(codec->type));
                    result.append_comma();

                    result.append_key_value<"coder_type">(
                        int(av_codec_is_encoder(codec)   ? 1
                            : av_codec_is_decoder(codec) ? 2
                                                         : 0)
                    );
                    result.append_comma();

                    result.append_key_value<"name">(StringUtilxx_c::toStringNotNull(codec->name));
                    result.append_comma();

                    if (nullptr != codec->long_name) {
                        result.append_key_value<"long_name">(codec->long_name);
                        result.append_comma();
                    }

                    result.escape_and_append_with_quotes("hw");
                    result.append_colon();
                    result.start_array();
                    for (int i = 0;; i++) {
                        const AVCodecHWConfig* config = avcodec_get_hw_config(codec, i);
                        if (!config) {
                            break;
                        }

                        if (i > 0) {
                            result.append_comma();
                        }

                        const char* hw_type_str = av_hwdevice_get_type_name(config->device_type);
                        if (hw_type_str) {
                            result.escape_and_append_with_quotes(hw_type_str);
                        }
                    }
                    result.end_array();

                    result.end_object();
                }
                break;
            }
        }

        result.end_array();
        return result;
    }
};