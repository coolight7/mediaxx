#include "util/string_util.h"
#include "uchardet/uchardet.h"
#include <algorithm>
#include <cstring>
#include <iconv.h>
#include <map>
#include <mutex>
#include <set>
#include <vector>

const mediaxx::stringxx::IgnoreCaseSet g_encoding_priorities = {
    "UTF-8",
    "UTF-16",
    "GB2312",
    "GBK",
    "GB18030",
};
const mediaxx::stringxx::IgnoreCaseMap<std::string> g_encoding_shift = {
    {"UTF8",   "UTF-8"  },
    {"UTF16",  "UTF-16" },
    {"GB2312", "GB18030"},
    {"GBK",    "GB18030"},
};
const size_t defShortStringLength = 30;

static const thread_local struct ChardetHolder {
    uchardet_t handle;

    ChardetHolder() :
        handle(uchardet_new()) {}

    ~ChardetHolder() {
        if (handle) {
            uchardet_delete(handle);
        }
    }
} g_chardetHandle;

// BOM 判断UTF16编码
static std::string detectUtfBom(std::string_view str) {
    if (str.size() < 2) {
        // UTF16 BOM至少2字节，长度不足直接返回
        return "";
    }
    const unsigned char* bom = reinterpret_cast<const unsigned char*>(str.data());
    if (bom[0] == 0xFF && bom[1] == 0xFE) {
        // UTF-16LE BOM (0xFF 0xFE)
        return "UTF-16LE";
    } else if (bom[0] == 0xFE && bom[1] == 0xFF) {
        // UTF-16BE BOM (0xFE 0xFF)
        return "UTF-16BE";
    } else if (str.size() >= 3 && bom[0] == 0xEF && bom[1] == 0xBB && bom[2] == 0xBF) {
        return "UTF-8";
    }
    // 无BOM，返回空
    return "";
}

static std::vector<std::string> getIconvCandidateEncodings(std::string_view src_encoding) {
    std::vector<std::string> candidates;
    if (src_encoding.empty()) {
        return candidates;
    }
    std::string enc{src_encoding};
    // UTF-16LE，Windows默认
    if (enc == "UTF-16LE" || enc == "UTF16LE" || enc == "UTF-16") {
        candidates = {"UTF-16LE", "UCS-2LE", "UTF16LE"};
    } else if (enc == "UTF-16BE" || enc == "UTF16BE") {
        // UTF-16BE
        candidates = {"UTF-16BE", "UCS-2BE", "UTF16BE"};
    } else {
        // 其他编码
        candidates.push_back(enc);
    }
    return candidates;
}

std::tuple<bool, std::optional<std::string>> mediaxx::stringxx::convertCharset(
    std::string_view src,
    std::string_view srcEncoding,
    std::string_view targetEncoding
) {
    // 已是目标编码/空字符串，直接返回
    if (mediaxx::stringxx::isIgnoreCaseEqual(srcEncoding, targetEncoding)) {
        return {true, std::nullopt};
    }
    if (src.empty()) {
        return {false, std::nullopt};
    }

    auto candidate_encs = getIconvCandidateEncodings(srcEncoding);
    if (candidate_encs.empty()) {
        return {false, std::nullopt};
    }

    // 遍历候选编码，逐个尝试
    auto target_enc
        = fmt::format("{}//TRANSLIT//IGNORE", mediaxx::stringxx::toUpper(targetEncoding));
    iconv_t cd = (iconv_t)-1;
    // 记录实际使用的成功编码名
    std::string used_src_enc;
    for (const auto& enc : candidate_encs) {
        std::string src_enc = enc + "//TRANSLIT//IGNORE";
        cd                  = iconv_open(target_enc.c_str(), src_enc.c_str());
        if (cd != (iconv_t)-1) {
            used_src_enc = enc;
            break;
        }
    }
    if (cd == (iconv_t)-1) {
        // 所有候选编码名都失败，直接返回
        return {false, std::nullopt};
    }

    // 缓冲区: 初始按单字节转换为 8 字节编码预估, 不足 (E2BIG) 时自动扩容重试
    size_t      src_len    = src.size();
    size_t      dst_offset = 0;
    std::string targetStr;
    targetStr.resize(src_len * 8);

    const char* src_ptr    = src.data();
    size_t      src_remain = src_len;
    bool        ok         = false;
    while (true) {
        char*  dst_ptr    = targetStr.data() + dst_offset;
        size_t dst_remain = targetStr.size() - dst_offset;
        auto   ret = iconv(cd, const_cast<char**>(&src_ptr), &src_remain, &dst_ptr, &dst_remain);
        dst_offset = targetStr.size() - dst_remain;
        if (ret != static_cast<size_t>(-1)) {
            // 转换结束, 仅当输入完全消耗才算成功
            ok = (src_remain == 0);
            break;
        }
        if (errno == E2BIG) {
            // 输出缓冲区不足, 扩容后继续
            targetStr.resize(targetStr.size() * 2);
            continue;
        }
        // EILSEQ / EINVAL 等不可恢复错误
        break;
    }

    iconv_close(cd);
    if (!ok) {
        return {false, std::nullopt};
    }
    targetStr.resize(dst_offset);
    return {true, targetStr};
}

/// <isSuccess, result>
std::tuple<bool, std::optional<std::string>> mediaxx::stringxx::autoConvertCharset(
    std::string_view str,
    std::string&     encoding,
    std::string_view targetEncoding
) {
    if (str.empty()) {
        return {true, std::nullopt};
    }

    // 手动检测UTF16 BOM
    std::string bom_enc = detectUtfBom(str);
    if (!bom_enc.empty()) {
        encoding                 = bom_enc;
        auto [isSuccess, result] = mediaxx::stringxx::convertCharset(str, encoding, targetEncoding);
        if (isSuccess) {
            return {true, result};
        }
    }

    // uchardet检测
    uchardet_reset(g_chardetHandle.handle);
    int ret = uchardet_handle_data(g_chardetHandle.handle, str.data(), str.size());
    if (ret != 0) {
        return {false, std::nullopt};
    }
    uchardet_data_end(g_chardetHandle.handle);

    auto n_candidates = uchardet_get_n_candidates(g_chardetHandle.handle);
    if (n_candidates <= 0) {
        return {false, std::nullopt};
    }
    // if (n_candidates > 5) {
    //     n_candidates = 5;
    // }
    std::vector<std::string> detected_candidates;
    for (size_t i = 0; i < n_candidates; ++i) {
        const char* enc = uchardet_get_encoding(g_chardetHandle.handle, i);
        if (enc && std::strlen(enc) > 0) {
            detected_candidates.emplace_back(enc);
        }
    }
    if (detected_candidates.empty()) {
        return {false, std::nullopt};
    }

    std::string selected_enc;
    {
        bool haveCheckUtf8 = false;
        // detected_candidates 中可能出现多次 utf8
        for (const auto& item : detected_candidates) {
            if (g_encoding_priorities.contains(item)) {
                auto item_ptr = &item;

                auto shiftItemIt = g_encoding_shift.find(item);
                if (shiftItemIt != g_encoding_shift.end()) {
                    item_ptr = &(shiftItemIt->second);
                }

                if (str.size() < defShortStringLength) {
                    // 短字符串容易不准确，需要检查utf8有效性
                    if (*item_ptr == std::string_view{"UTF-8"}) {
                        if (haveCheckUtf8
                            || false == mediaxx::stringxx::utf8GetLengthCheckAvail(str)) {
                            haveCheckUtf8 = true;
                            continue;
                        }
                    }
                }
                selected_enc = *item_ptr;
                break;
            }
        }
    }
    if (selected_enc.empty()) {
        // 无优先，取第一个
        if (str.size() < defShortStringLength) {
            // 短字符串直接取 gb
            selected_enc = "GB18030";
        } else {
            selected_enc = detected_candidates[0];
        }
    }

    encoding = selected_enc;
    if (encoding == "UTF-16") {
        // 通用UTF-16, 强制转为UTF-16LE，适配Windows
        encoding = "UTF-16LE";
    }

    return mediaxx::stringxx::convertCharset(str, encoding, targetEncoding);
}

std::tuple<bool, std::optional<std::string>>
    mediaxx::stringxx::autoConvertToUtf8(std::string_view str, bool _) {
    if (str.empty()) {
        return {true, std::nullopt};
    }
    std::string encoding;
    return autoConvertCharset(str, encoding, "UTF-8");
}

bool mediaxx::stringxx::autoConvertToUtf8(std::string& str) {
    if (str.empty()) {
        return true;
    }
    auto [isSuccess, result] = autoConvertToUtf8(std::string_view{str}, true);
    if (isSuccess && result.has_value()) {
        str = std::move(result.value());
    }
    return isSuccess;
}

std::string mediaxx::stringxx::autoTryConvertToUtf8(std::string_view str) {
    auto [isSuccess, result] = autoConvertToUtf8(str, true);
    if (isSuccess && result.has_value()) {
        return std::move(result.value());
    }
    return std::string{str};
}