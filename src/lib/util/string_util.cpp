#include "string_util.h"
#include <algorithm>
#include <cstring>
#include <iconv.h>
#include <set>
#include <uchardet.h>
#include <vector>

const std::set<std::string> g_chinese_encoding_priorities
    = {"UTF-8", "UTF-16", "GB2312", "GBK", "GB18030"};

static uchardet_t cChardetHandle = uchardet_new();

__attribute__((destructor)) void chardet_destroy_handle() {
    if (nullptr != cChardetHandle) {
        uchardet_delete(cChardetHandle);
        cChardetHandle = nullptr;
    }
}

// BOM 判断UTF16编码
static std::string detect_utf16_bom(const std::string_view str) {
    if (str.size() < 2) {
        // UTF16 BOM至少2字节，长度不足直接返回
        return "";
    }
    const unsigned char* bom = reinterpret_cast<const unsigned char*>(str.data());
    // UTF-16LE BOM (0xFF 0xFE)
    if (bom[0] == 0xFF && bom[1] == 0xFE) {
        return "UTF-16LE";
    }
    // UTF-16BE BOM (0xFE 0xFF)
    else if (bom[0] == 0xFE && bom[1] == 0xFF) {
        return "UTF-16BE";
    }
    // 无BOM，返回空
    return "";
}

static std::vector<std::string> get_iconv_candidate_encodings(const char* src_encoding) {
    std::vector<std::string> candidates;
    if (src_encoding == nullptr) {
        return candidates;
    }
    std::string enc = src_encoding;
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

// 转UTF8
std::string
    mediaxx::stringxx::convert_to_utf8(const std::string_view src, const char* src_encoding) {
    // 已是UTF8/空字符串，直接返回
    if (std::strcmp(src_encoding, "UTF-8") == 0 || std::strcmp(src_encoding, "utf8") == 0) {
        return std::string{src};
    }
    if (src.empty()) {
        return "";
    }

    auto candidate_encs = get_iconv_candidate_encodings(src_encoding);
    if (candidate_encs.empty()) {
        return "";
    }

    // 遍历候选编码，逐个尝试
    std::string target_enc = "UTF-8//TRANSLIT//IGNORE";
    iconv_t     cd         = (iconv_t)-1;
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
        return "";
    }

    // 缓冲区
    size_t src_len = src.size();
    // UTF8单字符最大6字节
    size_t            dst_buf_size = src_len * 6;
    std::vector<char> dst_buf(dst_buf_size);
    char*             dst_ptr    = dst_buf.data();
    size_t            dst_remain = dst_buf_size;

    // 编码转换
    const char* src_ptr    = src.data();
    size_t      src_remain = src_len;
    int         ret = iconv(cd, const_cast<char**>(&src_ptr), &src_remain, &dst_ptr, &dst_remain);

    std::string utf8_str;
    if (ret != -1 && src_remain == 0) {
        utf8_str = std::string(dst_buf.data(), dst_buf_size - dst_remain);
    }

    iconv_close(cd);
    return utf8_str;
}

bool mediaxx::stringxx::chardet_convert_encoding(
    const std::string_view str,
    std::string&           encoding,
    std::string&           result
) {
    const auto handle = cChardetHandle;
    if (handle == nullptr) {
        return false;
    }

    // 手动检测UTF16 BOM
    std::string bom_enc = detect_utf16_bom(str);
    if (!bom_enc.empty()) {
        encoding = bom_enc;
        result   = convert_to_utf8(str, encoding.c_str());
        if (!result.empty() || str.empty()) {
            return true;
        }
    }

    // uchardet检测
    uchardet_reset(handle);
    int ret = uchardet_handle_data(handle, str.data(), str.size());
    if (ret != 0) {
        return false;
    }
    uchardet_data_end(handle);

    int n_candidates = uchardet_get_n_candidates(handle);
    if (n_candidates <= 0) {
        return false;
    }
    if (n_candidates > 3) {
        n_candidates = 3;
    }
    std::vector<std::string> detected_candidates;
    for (int i = 0; i < n_candidates; ++i) {
        const char* enc = uchardet_get_encoding(handle, i);
        if (enc && std::strlen(enc) > 0) {
            detected_candidates.emplace_back(enc);
        }
    }
    if (detected_candidates.empty()) {
        return false;
    }

    std::string selected_enc;
    if (str.size() < 20) {
        // 短字符串，趋向取常见编码
        bool haveCheckUtf8 = false;
        // detected_candidates 中可能出现多次 utf8
        for (const auto& item : detected_candidates) {
            if (g_chinese_encoding_priorities.contains(item)) {
                if (item == "UTF-8") {
                    // 检查utf8有效性
                    if (haveCheckUtf8
                        || false == mediaxx::stringxx::utf8GetLengthCheckAvail(str.data())) {
                        haveCheckUtf8 = true;
                        continue;
                    }
                }
                selected_enc = item;
                break;
            }
        }
    }
    if (selected_enc.empty()) {
        // 无优先，取第一个
        selected_enc = detected_candidates[0];
    }

    encoding = selected_enc;
    if (encoding == "UTF-16") {
        // 通用UTF-16, 强制转为UTF-16LE，适配Windows
        encoding = "UTF-16LE";
    }

    result = convert_to_utf8(str, encoding.c_str());
    if (result.empty() && !str.empty()) {
        return false;
    }

    return true;
}