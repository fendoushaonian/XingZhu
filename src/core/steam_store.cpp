#include "core/steam_store.h"
#include "core/texture_manager.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winhttp.h>
#include <shlobj.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <cstdio>
#include <set>
#include <algorithm>
#include <fstream>

#pragma comment(lib, "winhttp.lib")

namespace sf {

using json = nlohmann::json;

// ---- Safe JSON helpers (used throughout) ----
static int SafeJsonInt(const json& obj, const char* key, int def = 0) {
    if (!obj.contains(key) || obj[key].is_null()) return def;
    if (obj[key].is_number()) return obj[key].get<int>();
    return def;
}
static std::string SafeJsonStr(const json& obj, const char* key, const std::string& def = "") {
    if (!obj.contains(key) || obj[key].is_null()) return def;
    if (obj[key].is_string()) return obj[key].get<std::string>();
    return def;
}

// ---- API Key ----
static std::string g_steamApiKey;

void SetStoreApiKey(const std::string& key) { g_steamApiKey = key; }

// ---- State ----
static std::mutex g_storeMutex;
static std::unordered_map<std::string, SteamStoreData> g_storeCache;
static std::queue<std::string> g_storeQueue;
static std::set<std::string> g_storeRequested;
static std::atomic<bool> g_storeRunning{false};

// ---- Helper: case-insensitive prefix match ----
static bool StartsWithCI(const std::string& s, size_t pos, const char* prefix) {
    for (size_t i = 0; prefix[i]; i++) {
        if (pos + i >= s.size()) return false;
        if (tolower((unsigned char)s[pos + i]) != tolower((unsigned char)prefix[i])) return false;
    }
    return true;
}

// ---- Strip BBCode tags from Steam news content ----
static std::string StripBBCode(const std::string& text) {
    std::string result;
    result.reserve(text.size());

    auto tagStartsWith = [](const std::string& tagLow, const char* prefix) -> bool {
        size_t len = strlen(prefix);
        return tagLow.size() >= len && tagLow.compare(0, len, prefix) == 0;
    };

    for (size_t i = 0; i < text.size(); i++) {
        if (text[i] != '[') {
            result += text[i];
            continue;
        }

        // Found '[', look for closing ']'
        size_t close = text.find(']', i + 1);
        if (close == std::string::npos || close - i > 120) {
            result += text[i];
            continue;
        }

        std::string tag = text.substr(i + 1, close - i - 1);
        std::string tagLow = tag;
        for (auto& ch : tagLow) ch = (char)tolower((unsigned char)ch);

        // [img] / [img=...] / [img src="..."] - skip entire content until [/img]
        if (tagLow == "img" || tagStartsWith(tagLow, "img=") || tagStartsWith(tagLow, "img ")) {
            size_t endTag = text.find("[/img]", close + 1);
            if (endTag == std::string::npos) endTag = text.find("[/IMG]", close + 1);
            if (endTag != std::string::npos && endTag - close < 500) {
                i = endTag + 5;
            } else {
                i = close;
            }
            continue;
        }

        // [previewyoutube...] - skip content
        if (tagLow == "previewyoutube" || tagStartsWith(tagLow, "previewyoutube=")) {
            size_t endTag = text.find("[/previewyoutube]", close + 1);
            if (endTag != std::string::npos) {
                result += "\n";
                i = endTag + 16;
            } else {
                i = close;
            }
            continue;
        }

        // [video ...] / [video=...] - skip entire content until [/video]
        if (tagLow == "video" || tagStartsWith(tagLow, "video ") || tagStartsWith(tagLow, "video=")) {
            size_t endTag = text.find("[/video]", close + 1);
            if (endTag == std::string::npos) endTag = text.find("[/VIDEO]", close + 1);
            if (endTag != std::string::npos) {
                result += "\n";
                i = endTag + 7;
            } else {
                i = close;
            }
            continue;
        }

        // Block-level tags -> newline
        const char* blockTags[] = {
            "h1", "h2", "h3", "h4", "h5",
            "/h1", "/h2", "/h3", "/h4", "/h5",
            "p", "/p",
            "list", "/list", "olist", "/olist",
            "hr", "/hr", "section", "/section",
            nullptr
        };
        bool isBlock = false;
        for (int t = 0; blockTags[t]; t++) {
            if (tagLow == blockTags[t]) { isBlock = true; break; }
        }

        // [*] bullet item, [/*] end of bullet
        if (tagLow == "*") {
            result += "\n  \xe2\x80\xa2 ";
            i = close;
            continue;
        }
        if (tagLow == "/*") {
            i = close;
            continue;
        }

        if (isBlock) {
            // Only add newline if last char isn't already one
            if (!result.empty() && result.back() != '\n')
                result += '\n';
            i = close;
            continue;
        }

        // Inline tags to strip silently (exact match)
        const char* inlineTags[] = {
            "b", "/b", "i", "/i", "u", "/u",
            "strike", "/strike", "spoiler", "/spoiler",
            "noparse", "/noparse", "code", "/code",
            "/img", "/url", "/video",
            "table", "/table", "tr", "/tr", "td", "/td", "th", "/th",
            "expand", "/expand", "quote", "/quote",
            "/previewyoutube",
            "c", "/c",  // color tags
            "o", "/o",  // outline tags
            "s", "/s",  // strikethrough
            "sub", "/sub", "sup", "/sup",  // subscript/superscript
            "center", "/center", "left", "/left", "right", "/right",  // alignment
            "justify", "/justify",
            "indent", "/indent",
            "br",  // line break
            nullptr
        };
        bool isInline = false;
        for (int t = 0; inlineTags[t]; t++) {
            if (tagLow == inlineTags[t]) { isInline = true; break; }
        }
        if (isInline) {
            i = close;
            continue;
        }

        // Tags with parameters: [url=...], [url="..."], [h1=...], [expand=...], etc.
        const char* paramPrefixes[] = {
            "url=", "h1=", "h2=", "h3=", "h4=",
            "expand=", "quote=", "spoiler=",
            "c=", "color=",  // color with parameter
            "o=",  // outline with parameter
            "font=", "size=",  // font styling
            "b=", "i=", "u=",  // styled variants
            "strike=", "s=",
            "sub=", "sup=",
            "center=", "left=", "right=", "justify=",
            "indent=",
            "table=", "tr=", "td=", "th=",
            nullptr
        };
        bool isParamTag = false;
        for (int t = 0; paramPrefixes[t]; t++) {
            if (tagStartsWith(tagLow, paramPrefixes[t])) {
                isParamTag = true;
                break;
            }
        }
        if (isParamTag) {
            i = close;
            continue;
        }

        // Unknown tag - keep the '[' character
        result += text[i];
    }

    return result;
}

// ---- HTML tag stripper + entity decoder (also strips BBCode first) ----
static std::string StripHtml(const std::string& raw) {
    // First strip BBCode
    std::string html = StripBBCode(raw);
    std::string result;
    result.reserve(html.size());
    bool inTag = false;
    bool lastWasSpace = false;

    for (size_t i = 0; i < html.size(); i++) {
        char c = html[i];
        if (c == '<') {
            // <br>, <br/>, <br /> -> newline
            if (StartsWithCI(html, i, "<br")) {
                size_t j = i + 3;
                while (j < html.size() && (html[j] == ' ' || html[j] == '/')) j++;
                if (j < html.size() && html[j] == '>') {
                    result += '\n';
                    lastWasSpace = false;
                    i = j;
                    continue;
                }
            }
            // Closing block tags -> newline
            if (i + 1 < html.size() && html[i + 1] == '/') {
                size_t end = html.find('>', i);
                if (end != std::string::npos) {
                    std::string closeTag = html.substr(i + 2, end - i - 2);
                    for (auto& ch : closeTag) ch = (char)tolower((unsigned char)ch);
                    if (closeTag == "p" || closeTag == "li" || closeTag == "h1" ||
                        closeTag == "h2" || closeTag == "h3" || closeTag == "h4" ||
                        closeTag == "div" || closeTag == "tr" || closeTag == "table" ||
                        closeTag == "blockquote" || closeTag == "ul" || closeTag == "ol") {
                        result += '\n';
                        lastWasSpace = false;
                    }
                }
            }
            // Opening block tags -> newline + bullet for <li>
            if (i + 1 < html.size() && html[i + 1] != '/' && html[i + 1] != '!') {
                size_t end = html.find('>', i);
                if (end != std::string::npos) {
                    size_t nameEnd = i + 1;
                    while (nameEnd < end && html[nameEnd] != ' ' && html[nameEnd] != '/')
                        nameEnd++;
                    std::string openTag = html.substr(i + 1, nameEnd - i - 1);
                    for (auto& ch : openTag) ch = (char)tolower((unsigned char)ch);
                    if (openTag == "p" || openTag == "h1" || openTag == "h2" ||
                        openTag == "h3" || openTag == "h4" || openTag == "li") {
                        if (!result.empty() && result.back() != '\n')
                            result += '\n';
                        lastWasSpace = false;
                    }
                    if (openTag == "li") {
                        result += "  \xe2\x80\xa2 ";
                        lastWasSpace = false;
                    }
                }
            }
            inTag = true;
            continue;
        }
        if (c == '>') {
            inTag = false;
            continue;
        }
        if (inTag) continue;

        // Handle HTML entities
        if (c == '&') {
            if (html.compare(i, 6, "&nbsp;") == 0) { result += ' '; i += 5; continue; }
            if (html.compare(i, 5, "&amp;") == 0) { result += '&'; i += 4; continue; }
            if (html.compare(i, 4, "&lt;") == 0) { result += '<'; i += 3; continue; }
            if (html.compare(i, 4, "&gt;") == 0) { result += '>'; i += 3; continue; }
            if (html.compare(i, 6, "&quot;") == 0) { result += '"'; i += 5; continue; }
            if (html.compare(i, 6, "&apos;") == 0) { result += '\''; i += 5; continue; }
            if (html.compare(i, 8, "&hellip;") == 0) { result += "\xe2\x80\xa6"; i += 7; continue; }
            if (html.compare(i, 7, "&mdash;") == 0) { result += "\xe2\x80\x94"; i += 6; continue; }
            if (html.compare(i, 7, "&ndash;") == 0) { result += "\xe2\x80\x93"; i += 6; continue; }
            if (html.compare(i, 7, "&laquo;") == 0) { result += "\xc2\xab"; i += 6; continue; }
            if (html.compare(i, 7, "&raquo;") == 0) { result += "\xc2\xbb"; i += 6; continue; }
            if (html.compare(i, 8, "&lsquo;") == 0) { result += "\xe2\x80\x98"; i += 7; continue; }
            if (html.compare(i, 8, "&rsquo;") == 0) { result += "\xe2\x80\x99"; i += 7; continue; }
            if (html.compare(i, 8, "&ldquo;") == 0) { result += "\xe2\x80\x9c"; i += 7; continue; }
            if (html.compare(i, 8, "&rdquo;") == 0) { result += "\xe2\x80\x9d"; i += 7; continue; }
            // Numeric entities: &#123; or &#x1F;
            if (i + 2 < html.size() && html[i + 1] == '#') {
                size_t semi = html.find(';', i + 2);
                if (semi != std::string::npos && semi - i < 12) {
                    unsigned long cp = 0;
                    bool valid = false;
                    if (html[i + 2] == 'x' || html[i + 2] == 'X') {
                        cp = strtoul(html.c_str() + i + 3, nullptr, 16);
                        valid = (cp > 0);
                    } else {
                        cp = strtoul(html.c_str() + i + 2, nullptr, 10);
                        valid = (cp > 0);
                    }
                    if (valid && cp < 0x80) {
                        result += (char)cp;
                    } else if (valid && cp < 0x800) {
                        result += (char)(0xC0 | (cp >> 6));
                        result += (char)(0x80 | (cp & 0x3F));
                    } else if (valid && cp < 0x10000) {
                        result += (char)(0xE0 | (cp >> 12));
                        result += (char)(0x80 | ((cp >> 6) & 0x3F));
                        result += (char)(0x80 | (cp & 0x3F));
                    } else if (valid) {
                        result += (char)(0xF0 | (cp >> 18));
                        result += (char)(0x80 | ((cp >> 12) & 0x3F));
                        result += (char)(0x80 | ((cp >> 6) & 0x3F));
                        result += (char)(0x80 | (cp & 0x3F));
                    }
                    if (valid) { i = semi; continue; }
                }
            }
        }

        // Collapse whitespace
        if (c == '\r') continue;
        if (c == '\n' || c == '\t') c = ' ';

        if (c == ' ' && lastWasSpace) continue;
        lastWasSpace = (c == ' ');
        result += c;
    }

    // Trim trailing whitespace and collapse multiple newlines
    std::string cleaned;
    int newlines = 0;
    for (char c : result) {
        if (c == '\n') {
            newlines++;
            if (newlines <= 2) cleaned += c;
        } else {
            newlines = 0;
            cleaned += c;
        }
    }

    // Trim
    while (!cleaned.empty() && (cleaned.back() == ' ' || cleaned.back() == '\n'))
        cleaned.pop_back();
    while (!cleaned.empty() && (cleaned.front() == ' ' || cleaned.front() == '\n'))
        cleaned.erase(0, 1);

    // Filter out unsupported Unicode characters (emoji, etc.)
    // ImGui with standard fonts only supports up to U+FFFF, and some ranges are missing
    std::string filtered;
    filtered.reserve(cleaned.size());
    for (size_t i = 0; i < cleaned.size(); ) {
        unsigned char c = (unsigned char)cleaned[i];
        if (c < 0x80) {
            // ASCII - always valid
            filtered += cleaned[i++];
        } else if ((c & 0xE0) == 0xC0 && i + 1 < cleaned.size()) {
            // 2-byte UTF-8 (U+0080 to U+07FF) - generally supported
            filtered += cleaned[i++];
            filtered += cleaned[i++];
        } else if ((c & 0xF0) == 0xE0 && i + 2 < cleaned.size()) {
            // 3-byte UTF-8 (U+0800 to U+FFFF)
            unsigned int cp = ((c & 0x0F) << 12) |
                              (((unsigned char)cleaned[i+1] & 0x3F) << 6) |
                              ((unsigned char)cleaned[i+2] & 0x3F);
            // Skip Private Use Area (U+E000-U+F8FF) except our icon range
            // Skip some problematic ranges
            if (cp >= 0xE000 && cp <= 0xF8FF) {
                i += 3; // Skip PUA characters (often cause issues)
            } else {
                filtered += cleaned[i++];
                filtered += cleaned[i++];
                filtered += cleaned[i++];
            }
        } else if ((c & 0xF8) == 0xF0 && i + 3 < cleaned.size()) {
            // 4-byte UTF-8 (U+10000 to U+10FFFF) - emoji and beyond
            // These are not supported by standard ImGui fonts, skip them
            i += 4;
        } else {
            // Invalid UTF-8 sequence, skip
            i++;
        }
    }

    return filtered;
}

// ---- Rich content parser: mixed HTML+BBCode -> segments ----
static std::string TrimStr(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\n' || s[a] == '\r' || s[a] == '\t')) a++;
    while (b > a && (s[b-1] == ' ' || s[b-1] == '\n' || s[b-1] == '\r' || s[b-1] == '\t')) b--;
    return s.substr(a, b - a);
}

static std::vector<SteamStoreData::RichSegment> ParseRichContent(const std::string& raw) {
    std::vector<SteamStoreData::RichSegment> segs;
    std::string cur;  // accumulate current text

    auto flushText = [&]() {
        std::string t = StripHtml(cur);
        t = TrimStr(t);
        if (!t.empty()) {
            SteamStoreData::RichSegment seg;
            seg.type = SteamStoreData::RichSegment::TEXT;
            seg.text = t;
            segs.push_back(std::move(seg));
        }
        cur.clear();
    };

    auto addImage = [&](const std::string& url) {
        flushText();
        std::string imgUrl = TrimStr(url);
        // Resolve Steam clan image placeholder
        size_t clanPos = imgUrl.find("{STEAM_CLAN_IMAGE}");
        if (clanPos != std::string::npos) {
            std::string rest = imgUrl.substr(clanPos + 18); // 18 = strlen("{STEAM_CLAN_IMAGE}")
            if (!rest.empty() && rest[0] == '/') rest = rest.substr(1);
            imgUrl = "https://clan.akamai.steamstatic.com/images/" + rest;
        }
        size_t locPos = imgUrl.find("{STEAM_CLAN_LOC_IMAGE}");
        if (locPos != std::string::npos) {
            std::string rest = imgUrl.substr(locPos + 22);
            if (!rest.empty() && rest[0] == '/') rest = rest.substr(1);
            imgUrl = "https://clan.akamai.steamstatic.com/images/" + rest;
        }
        // Skip empty or too-short URLs
        if (imgUrl.size() < 10) return;
        // Ensure URL has protocol
        if (imgUrl.find("//") == 0) imgUrl = "https:" + imgUrl;
        if (imgUrl.find("http") != 0) return; // skip non-http URLs

        SteamStoreData::RichSegment seg;
        seg.type = SteamStoreData::RichSegment::IMAGE;
        seg.text = imgUrl;
        segs.push_back(std::move(seg));
    };

    auto addHeading = [&](const std::string& text, int level) {
        flushText();
        std::string t = StripHtml(text);
        t = TrimStr(t);
        if (!t.empty()) {
            SteamStoreData::RichSegment seg;
            seg.type = SteamStoreData::RichSegment::HEADING;
            seg.text = t;
            seg.headingLevel = level;
            segs.push_back(std::move(seg));
        }
    };

    size_t len = raw.size();
    for (size_t i = 0; i < len; ) {
        // === BBCode tags ===
        if (raw[i] == '[') {
            size_t close = raw.find(']', i + 1);
            if (close == std::string::npos || close - i > 120) {
                cur += raw[i++];
                continue;
            }

            std::string tag = raw.substr(i + 1, close - i - 1);
            std::string tagLow = tag;
            for (auto& ch : tagLow) ch = (char)tolower((unsigned char)ch);

            // [img]url[/img], [img=url], or [img src="url"] formats
            if (tagLow == "img" || (tagLow.size() > 4 && tagLow.compare(0, 4, "img=") == 0)
                || (tagLow.size() > 4 && tagLow.compare(0, 4, "img ") == 0)) {

                // Check for [img src="url"] (self-closing with attribute)
                if (tagLow.size() > 4 && tagLow.compare(0, 4, "img ") == 0) {
                    // Extract src from tag attributes
                    size_t srcPos = tagLow.find("src=");
                    if (srcPos != std::string::npos) {
                        srcPos += 4; // skip "src="
                        std::string srcPart = tag.substr(srcPos);
                        std::string imgUrl;
                        char q = (!srcPart.empty() && (srcPart[0] == '"' || srcPart[0] == '\'')) ? srcPart[0] : 0;
                        if (q) {
                            size_t endQ = srcPart.find(q, 1);
                            if (endQ != std::string::npos)
                                imgUrl = srcPart.substr(1, endQ - 1);
                        } else {
                            size_t endS = srcPart.find_first_of(" ]");
                            imgUrl = srcPart.substr(0, endS);
                        }
                        addImage(imgUrl);
                    }
                    // Skip optional [/img] if present right after
                    size_t endTag = raw.find("[/img]", close + 1);
                    if (endTag != std::string::npos && endTag - close < 10)
                        i = endTag + 6;
                    else
                        i = close + 1;
                    continue;
                }

                // Standard [img]url[/img] or [img=url]
                size_t endTag = raw.find("[/img]", close + 1);
                if (endTag == std::string::npos) endTag = raw.find("[/IMG]", close + 1);
                if (endTag != std::string::npos) {
                    std::string imgUrl = TrimStr(raw.substr(close + 1, endTag - close - 1));
                    // For [img=url], the URL is in the tag itself
                    if (imgUrl.empty() && tagLow.compare(0, 4, "img=") == 0) {
                        imgUrl = tag.substr(4);
                    }
                    addImage(imgUrl);
                    i = endTag + 6;
                } else {
                    // No closing tag - might be [img=url] self-closing
                    if (tagLow.compare(0, 4, "img=") == 0) {
                        addImage(tag.substr(4));
                    }
                    i = close + 1;
                }
                continue;
            }

            // [h1]...[/h1], [h2]...[/h2], [h3]...[/h3]
            for (int hl = 1; hl <= 4; hl++) {
                char openTag[8], closeTagStr[8];
                snprintf(openTag, sizeof(openTag), "h%d", hl);
                snprintf(closeTagStr, sizeof(closeTagStr), "[/h%d]", hl);
                // Also match [h1=...] style
                bool isThis = (tagLow == openTag) ||
                              (tagLow.size() > 3 && tagLow[0] == 'h' && tagLow[1] == ('0'+hl) && tagLow[2] == '=');
                if (isThis) {
                    size_t endTag = raw.find(closeTagStr, close + 1);
                    if (endTag != std::string::npos) {
                        std::string hText = raw.substr(close + 1, endTag - close - 1);
                        addHeading(hText, hl);
                        i = endTag + 5;
                    } else {
                        i = close + 1;
                    }
                    goto bbcontinue;
                }
            }

            // [previewyoutube...][/previewyoutube] - skip
            if (tagLow == "previewyoutube" || (tagLow.size() > 16 && tagLow.compare(0, 16, "previewyoutube=") == 0)) {
                size_t endTag = raw.find("[/previewyoutube]", close + 1);
                if (endTag != std::string::npos) {
                    i = endTag + 17;
                } else {
                    i = close + 1;
                }
                continue;
            }

            // [video webm="url" mp4="url"][/video] - extract video URLs
            if (tagLow == "video" || (tagLow.size() > 6 && (tagLow.compare(0, 6, "video ") == 0 || tagLow.compare(0, 6, "video=") == 0))) {
                flushText();
                // Extract webm and mp4 attributes from tag
                auto extractBBAttr = [&](const std::string& attrName) -> std::string {
                    size_t pos = tagLow.find(attrName + "=");
                    if (pos == std::string::npos) return "";
                    pos += attrName.size() + 1;
                    std::string part = tag.substr(pos);
                    char q = (!part.empty() && (part[0] == '"' || part[0] == '\'')) ? part[0] : 0;
                    if (q) {
                        size_t endQ = part.find(q, 1);
                        return (endQ != std::string::npos) ? part.substr(1, endQ - 1) : "";
                    }
                    size_t endS = part.find_first_of(" ]");
                    return part.substr(0, endS);
                };

                std::string webmUrl = extractBBAttr("webm");
                std::string mp4Url = extractBBAttr("mp4");

                // Use webm as primary, mp4 as fallback
                std::string primaryUrl = !webmUrl.empty() ? webmUrl : mp4Url;
                if (!primaryUrl.empty()) {
                    // Ensure URL has protocol
                    if (primaryUrl.find("//") == 0) primaryUrl = "https:" + primaryUrl;
                    if (mp4Url.find("//") == 0) mp4Url = "https:" + mp4Url;

                    SteamStoreData::RichSegment seg;
                    seg.type = SteamStoreData::RichSegment::VIDEO;
                    seg.text = primaryUrl;
                    seg.videoUrl2 = (primaryUrl != mp4Url) ? mp4Url : "";
                    segs.push_back(std::move(seg));
                }

                // Skip to [/video]
                size_t endTag = raw.find("[/video]", close + 1);
                if (endTag == std::string::npos) endTag = raw.find("[/VIDEO]", close + 1);
                if (endTag != std::string::npos)
                    i = endTag + 8;
                else
                    i = close + 1;
                continue;
            }

            // Other BBCode tags - pass through to cur for StripHtml to handle
            cur += raw.substr(i, close - i + 1);
            i = close + 1;
            continue;

            bbcontinue:
            continue;
        }

        // === HTML tags ===
        if (raw[i] == '<') {
            size_t close = raw.find('>', i + 1);
            if (close == std::string::npos || close - i > 500) {
                cur += raw[i++];
                continue;
            }

            std::string htmlTag = raw.substr(i + 1, close - i - 1);
            std::string htmlTagLow = htmlTag;
            for (auto& ch : htmlTagLow) ch = (char)tolower((unsigned char)ch);

            // Helper: extract attribute value from tag string
            auto extractAttr = [&](const std::string& tagStr, const std::string& tagStrLow,
                                   const std::string& attrName) -> std::string {
                size_t pos = tagStrLow.find(attrName + "=");
                if (pos == std::string::npos) return "";
                pos += attrName.size() + 1;
                std::string part = tagStr.substr(pos);
                char q = (!part.empty() && (part[0] == '"' || part[0] == '\'')) ? part[0] : 0;
                if (q) {
                    size_t endQ = part.find(q, 1);
                    return (endQ != std::string::npos) ? part.substr(1, endQ - 1) : "";
                }
                size_t endS = part.find_first_of(" >/");
                return part.substr(0, endS);
            };

            // <img src="url"> or <img src="url"/>
            if (htmlTagLow.compare(0, 3, "img") == 0 && (htmlTagLow.size() == 3 || htmlTagLow[3] == ' ' || htmlTagLow[3] == '/')) {
                std::string imgUrl = extractAttr(htmlTag, htmlTagLow, "src");
                if (!imgUrl.empty()) addImage(imgUrl);
                i = close + 1;
                continue;
            }

            // <video ... poster="url"> — extract poster as static image
            if (htmlTagLow.compare(0, 5, "video") == 0 && (htmlTagLow.size() == 5 || htmlTagLow[5] == ' ')) {
                std::string posterUrl = extractAttr(htmlTag, htmlTagLow, "poster");
                if (!posterUrl.empty()) addImage(posterUrl);
                // Skip to </video> closing tag
                size_t endTag = raw.find("</video>", close + 1);
                if (endTag == std::string::npos) endTag = raw.find("</VIDEO>", close + 1);
                if (endTag != std::string::npos)
                    i = endTag + 8;
                else
                    i = close + 1;
                continue;
            }

            // <h1>...<h4> opening tags
            for (int hl = 1; hl <= 4; hl++) {
                char htag[4];
                snprintf(htag, sizeof(htag), "h%d", hl);
                // Check for <h1> or <h1 class=...>
                if (htmlTagLow == htag || (htmlTagLow.size() > 2 && htmlTagLow[0] == 'h' && htmlTagLow[1] == ('0'+hl) && htmlTagLow[2] == ' ')) {
                    char closeStr[8];
                    snprintf(closeStr, sizeof(closeStr), "</h%d>", hl);
                    size_t endTag = raw.find(closeStr, close + 1);
                    if (endTag != std::string::npos) {
                        std::string hText = raw.substr(close + 1, endTag - close - 1);
                        addHeading(hText, hl);
                        i = endTag + 5;
                    } else {
                        i = close + 1;
                    }
                    goto htmlcontinue;
                }
            }

            // <li> — flush text before it and mark as bullet
            if (htmlTagLow == "li" || (htmlTagLow.size() > 2 && htmlTagLow[0] == 'l' && htmlTagLow[1] == 'i' && htmlTagLow[2] == ' ')) {
                flushText();
                // Find </li> closing
                size_t endTag = raw.find("</li>", close + 1);
                if (endTag == std::string::npos) endTag = raw.find("</LI>", close + 1);
                if (endTag != std::string::npos) {
                    std::string liText = StripHtml(raw.substr(close + 1, endTag - close - 1));
                    liText = TrimStr(liText);
                    if (!liText.empty()) {
                        SteamStoreData::RichSegment seg;
                        seg.type = SteamStoreData::RichSegment::BULLET;
                        seg.text = liText;
                        segs.push_back(std::move(seg));
                    }
                    i = endTag + 5;
                } else {
                    i = close + 1;
                }
                continue;
            }

            // Other HTML tags - pass through for StripHtml
            cur += raw.substr(i, close - i + 1);
            i = close + 1;
            continue;

            htmlcontinue:
            continue;
        }

        // Normal character
        cur += raw[i++];
    }

    flushText();
    return segs;
}

// ---- WinHTTP fetch ----
static bool HttpGet(const std::string& url, std::string& response) {
    std::string u = url;
    bool isHttps = true;

    if (u.find("https://") == 0) u = u.substr(8);
    else if (u.find("http://") == 0) { u = u.substr(7); isHttps = false; }

    size_t slashPos = u.find('/');
    std::string host, path;
    if (slashPos == std::string::npos) { host = u; path = "/"; }
    else { host = u.substr(0, slashPos); path = u.substr(slashPos); }

    std::wstring wHost(host.begin(), host.end());
    std::wstring wPath(path.begin(), path.end());

    HINTERNET hSession = WinHttpOpen(L"SteamForge/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                      WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return false;

    // Set timeouts: 10s connect, 15s send/receive
    WinHttpSetTimeouts(hSession, 10000, 10000, 15000, 15000);

    HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(),
        isHttps ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return false; }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", wPath.c_str(), NULL,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, isHttps ? WINHTTP_FLAG_SECURE : 0);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return false; }

    // Enable automatic decompression (gzip/deflate)
    DWORD decompFlags = WINHTTP_DECOMPRESSION_FLAG_ALL;
    WinHttpSetOption(hRequest, WINHTTP_OPTION_DECOMPRESSION, &decompFlags, sizeof(decompFlags));

    BOOL ok = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                  WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (!ok) { WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return false; }

    ok = WinHttpReceiveResponse(hRequest, NULL);
    if (!ok) { WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return false; }

    // Check HTTP status code
    DWORD statusCode = 0, statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        NULL, &statusCode, &statusSize, NULL);
    if (statusCode != 200) {
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        return false;
    }

    response.clear();
    DWORD dwSize = 0, dwDownloaded = 0;
    do {
        dwSize = 0;
        WinHttpQueryDataAvailable(hRequest, &dwSize);
        if (dwSize == 0) break;
        std::vector<char> buf(dwSize + 1, 0);
        WinHttpReadData(hRequest, buf.data(), dwSize, &dwDownloaded);
        response.append(buf.data(), dwDownloaded);
    } while (dwSize > 0);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return true;
}

// ---- Parse store API response ----
static bool ParseAppDetails(const std::string& appId, const std::string& jsonStr, SteamStoreData& out) {
    try {
        auto root = json::parse(jsonStr);
        if (!root.contains(appId) || !root[appId].contains("success") || root[appId]["success"].is_null() || !root[appId]["success"].get<bool>())
            return false;

        auto& data = root[appId]["data"];

        out.name        = SafeJsonStr(data, "name");
        out.shortDesc   = SafeJsonStr(data, "short_description");
        // Use API-provided header_image URL (more reliable for new games)
        // The API returns the correct URL format for each game
        out.headerImage = SafeJsonStr(data, "header_image");
        if (out.headerImage.empty()) {
            // Fallback to CDN pattern if API doesn't provide header_image
            out.headerImage = "https://cdn.akamai.steamstatic.com/steam/apps/" + appId + "/header.jpg";
        }

        // About / description
        {
            std::string aboutRaw = SafeJsonStr(data, "about_the_game");
            if (aboutRaw.empty()) aboutRaw = SafeJsonStr(data, "detailed_description");
            out.aboutGame = StripHtml(aboutRaw);
            out.aboutRich = ParseRichContent(aboutRaw);
        }

        // Developers / Publishers
        if (data.contains("developers") && data["developers"].is_array()) {
            for (auto& d : data["developers"]) {
                if (d.is_string()) {
                    if (!out.developer.empty()) out.developer += ", ";
                    out.developer += d.get<std::string>();
                }
            }
        }
        if (data.contains("publishers") && data["publishers"].is_array()) {
            for (auto& p : data["publishers"]) {
                if (p.is_string()) {
                    if (!out.publisher.empty()) out.publisher += ", ";
                    out.publisher += p.get<std::string>();
                }
            }
        }

        // Release date
        if (data.contains("release_date") && data["release_date"].is_object()) {
            out.releaseDate = SafeJsonStr(data["release_date"], "date");
            // Check if game is coming soon (not yet released)
            if (data["release_date"].contains("coming_soon") && data["release_date"]["coming_soon"].is_boolean()) {
                out.comingSoon = data["release_date"]["coming_soon"].get<bool>();
            }
        }

        // Screenshots
        if (data.contains("screenshots") && data["screenshots"].is_array()) {
            for (auto& ss : data["screenshots"]) {
                out.screenshotUrls.push_back(SafeJsonStr(ss, "path_full"));
                out.screenshotThumbs.push_back(SafeJsonStr(ss, "path_thumbnail"));
            }
        }

        // Movies/Trailers
        if (data.contains("movies") && data["movies"].is_array()) {
            for (auto& m : data["movies"]) {
                SteamStoreData::Movie movie;
                movie.name = SafeJsonStr(m, "name");
                movie.thumbnailUrl = SafeJsonStr(m, "thumbnail");
                movie.highlight = m.contains("highlight") && !m["highlight"].is_null() && m["highlight"].get<bool>();
                // New Steam format: DASH/HLS streaming
                movie.hlsUrl = SafeJsonStr(m, "hls_h264");
                movie.dashUrl = SafeJsonStr(m, "dash_h264");
                // Legacy format fallback
                if (m.contains("mp4") && m["mp4"].is_object()) {
                    movie.mp4Url = SafeJsonStr(m["mp4"], "max");
                    if (movie.mp4Url.empty()) movie.mp4Url = SafeJsonStr(m["mp4"], "480");
                }
                fprintf(stderr, "[STORE] Movie: %s hls=%d dash=%d mp4=%d\n",
                        movie.name.c_str(), !movie.hlsUrl.empty(), !movie.dashUrl.empty(), !movie.mp4Url.empty());
                out.movies.push_back(std::move(movie));
            }
        }

        // Genres
        if (data.contains("genres") && data["genres"].is_array()) {
            for (auto& g : data["genres"]) {
                std::string desc = SafeJsonStr(g, "description");
                if (!desc.empty()) out.genres.push_back(desc);
            }
        }

        // Categories (single-player, multiplayer, achievements, etc.)
        if (data.contains("categories") && data["categories"].is_array()) {
            for (auto& c : data["categories"]) {
                std::string desc = SafeJsonStr(c, "description");
                if (!desc.empty()) out.categories.push_back(desc);
            }
        }

        // Reviews
        if (data.contains("recommendations") && data["recommendations"].is_object())
            out.totalReviews = SafeJsonInt(data["recommendations"], "total", 0);
        if (data.contains("metacritic") && data["metacritic"].is_object())
            out.reviewScore = SafeJsonInt(data["metacritic"], "score", 0);

        // Price
        if (data.contains("price_overview") && data["price_overview"].is_object()) {
            auto& po = data["price_overview"];
            out.priceFormatted = SafeJsonStr(po, "final_formatted");
            out.discountPercent = SafeJsonInt(po, "discount_percent", 0);
            out.originalPrice = SafeJsonStr(po, "initial_formatted");
            // Check if final price is 0 (free)
            int finalPrice = SafeJsonInt(po, "final", -1);
            if (finalPrice == 0) out.isFree = true;
        } else if (data.contains("is_free") && !data["is_free"].is_null() && data["is_free"].get<bool>()) {
            out.priceFormatted = u8"\u514D\u8D39\u5F00\u73A9"; // 免费开玩
            out.isFree = true;
        }

        // System requirements
        if (data.contains("pc_requirements")) {
            auto& req = data["pc_requirements"];
            if (req.is_object()) {
                std::string minReq = SafeJsonStr(req, "minimum");
                if (!minReq.empty()) {
                    out.reqMinimum = StripHtml(minReq);
                    // Parse disk space from requirements (look for "Storage:" or "硬盘:" patterns)
                    // Common formats: "Storage: 50 GB", "硬盘空间: 20 GB", "Hard Drive: 10 GB"
                    std::string lowerReq = minReq;
                    for (auto& c : lowerReq) c = (char)tolower((unsigned char)c);

                    // Find storage value - look for patterns like "50 gb", "20gb", "1500 mb"
                    size_t storagePos = lowerReq.find("storage");
                    if (storagePos == std::string::npos) storagePos = lowerReq.find("hard drive");
                    if (storagePos == std::string::npos) storagePos = lowerReq.find("hard disk");
                    if (storagePos == std::string::npos) storagePos = lowerReq.find("disk space");
                    // Chinese patterns
                    if (storagePos == std::string::npos) storagePos = minReq.find(u8"硬盘");
                    if (storagePos == std::string::npos) storagePos = minReq.find(u8"存储");
                    if (storagePos == std::string::npos) storagePos = minReq.find(u8"磁盘");

                    if (storagePos != std::string::npos) {
                        // Search for number followed by GB or MB after the storage keyword
                        size_t searchStart = storagePos;
                        size_t searchEnd = (std::min)(minReq.size(), storagePos + 100);
                        for (size_t i = searchStart; i < searchEnd; i++) {
                            if (isdigit((unsigned char)minReq[i])) {
                                // Found a digit, parse the number
                                double value = 0;
                                size_t numEnd = i;
                                while (numEnd < searchEnd && (isdigit((unsigned char)minReq[numEnd]) || minReq[numEnd] == '.' || minReq[numEnd] == ',')) {
                                    if (minReq[numEnd] != ',') {
                                        if (minReq[numEnd] == '.') {
                                            numEnd++;
                                            double decimal = 0.1;
                                            while (numEnd < searchEnd && isdigit((unsigned char)minReq[numEnd])) {
                                                value += (minReq[numEnd] - '0') * decimal;
                                                decimal *= 0.1;
                                                numEnd++;
                                            }
                                            break;
                                        } else {
                                            value = value * 10 + (minReq[numEnd] - '0');
                                        }
                                    }
                                    numEnd++;
                                }
                                // Skip whitespace
                                while (numEnd < searchEnd && (minReq[numEnd] == ' ' || minReq[numEnd] == '\t')) numEnd++;
                                // Check for GB or MB
                                std::string unit;
                                if (numEnd + 1 < searchEnd) {
                                    unit = minReq.substr(numEnd, 2);
                                    for (auto& c : unit) c = (char)tolower((unsigned char)c);
                                }
                                if (unit == "gb") {
                                    out.diskSpaceBytes = (long long)(value * 1024LL * 1024LL * 1024LL);
                                    break;
                                } else if (unit == "mb") {
                                    out.diskSpaceBytes = (long long)(value * 1024LL * 1024LL);
                                    break;
                                } else if (unit == "tb") {
                                    out.diskSpaceBytes = (long long)(value * 1024LL * 1024LL * 1024LL * 1024LL);
                                    break;
                                }
                                // Not a valid unit, continue searching
                            }
                        }
                    }
                }
                std::string recReq = SafeJsonStr(req, "recommended");
                if (!recReq.empty()) out.reqRecommended = StripHtml(recReq);
            }
        }

        // Languages
        if (data.contains("supported_languages") && data["supported_languages"].is_string())
            out.languages = StripHtml(data["supported_languages"].get<std::string>());

        // Platforms
        if (data.contains("platforms") && data["platforms"].is_object()) {
            auto& p = data["platforms"];
            out.platformWindows = p.contains("windows") && !p["windows"].is_null() && p["windows"].get<bool>();
            out.platformMac = p.contains("mac") && !p["mac"].is_null() && p["mac"].get<bool>();
            out.platformLinux = p.contains("linux") && !p["linux"].is_null() && p["linux"].get<bool>();
        }

        // Website
        if (data.contains("website") && data["website"].is_string())
            out.website = data["website"].get<std::string>();

        // DLC - get ALL IDs
        if (data.contains("dlc") && data["dlc"].is_array()) {
            auto& dlcArr = data["dlc"];
            out.dlcCount = (int)dlcArr.size();
            for (int di = 0; di < out.dlcCount; di++) {
                SteamStoreData::DlcItem dlc;
                if (dlcArr[di].is_number())
                    dlc.appId = std::to_string(dlcArr[di].get<int>());
                else if (dlcArr[di].is_string())
                    dlc.appId = dlcArr[di].get<std::string>();
                if (!dlc.appId.empty())
                    out.dlcItems.push_back(std::move(dlc));
            }
        }

        // Type
        out.type = SafeJsonStr(data, "type", "game");

        out.loaded = true;
        return true;

    } catch (const std::exception& e) {
        fprintf(stderr, "[STORE] JSON parse error for %s: %s\n", appId.c_str(), e.what());
        return false;
    }
}

// ---- Cache dir ----
static std::wstring GetStoreCacheDir() {
    wchar_t path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, path))) {
        std::wstring dir = std::wstring(path) + L"\\SteamForge\\store_cache";
        std::filesystem::create_directories(dir);
        return dir;
    }
    std::filesystem::create_directories(L"store_cache");
    return L"store_cache";
}

static std::wstring g_storeCacheDir;

// ---- Worker thread ----
// Process a single appId: fetch details + news, store immediately, then DLCs
static void FetchOneStoreEntry(const std::string& appId) {
    // Check disk cache first
    std::wstring appIdW(appId.begin(), appId.end());
    std::wstring cacheFile = g_storeCacheDir + L"\\" + appIdW + L".json";
    std::wstring regionCacheFile = g_storeCacheDir + L"\\" + appIdW + L"_region.txt";

    std::string jsonStr;
    bool fromCache = false;
    bool regionRestricted = false;

    // Check if we have cached region restriction info
    if (std::filesystem::exists(regionCacheFile)) {
        FILE* f = _wfopen(regionCacheFile.c_str(), L"rb");
        if (f) {
            char buf[16] = {0};
            fread(buf, 1, 15, f);
            fclose(f);
            regionRestricted = (strcmp(buf, "restricted") == 0);
        }
    }

    if (std::filesystem::exists(cacheFile)) {
        FILE* f = _wfopen(cacheFile.c_str(), L"rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long sz = ftell(f);
            fseek(f, 0, SEEK_SET);
            jsonStr.resize(sz);
            fread(&jsonStr[0], 1, sz, f);
            fclose(f);
            fromCache = true;
        }
    }

    if (!fromCache) {
        // First try with China region
        std::string url = "https://store.steampowered.com/api/appdetails?appids=" + appId + "&l=schinese&cc=CN";
        if (HttpGet(url, jsonStr) && !jsonStr.empty()) {
            // Check if China request returned success=false (potential region restriction)
            try {
                auto root = json::parse(jsonStr);
                if (root.contains(appId) && root[appId].contains("success") &&
                    !root[appId]["success"].is_null() && !root[appId]["success"].get<bool>()) {
                    // China region failed, try US region to check if it's region restricted
                    std::string usJson;
                    std::string usUrl = "https://store.steampowered.com/api/appdetails?appids=" + appId + "&l=english&cc=US";
                    if (HttpGet(usUrl, usJson) && !usJson.empty()) {
                        auto usRoot = json::parse(usJson);
                        if (usRoot.contains(appId) && usRoot[appId].contains("success") &&
                            !usRoot[appId]["success"].is_null() && usRoot[appId]["success"].get<bool>()) {
                            // US region works but China doesn't - this is region restricted
                            regionRestricted = true;
                            jsonStr = usJson;  // Use US data for display
                            fprintf(stderr, "[STORE] AppId %s is region restricted in China\n", appId.c_str());
                        }
                    }
                }
            } catch (...) {}

            FILE* f = _wfopen(cacheFile.c_str(), L"wb");
            if (f) { fwrite(jsonStr.data(), 1, jsonStr.size(), f); fclose(f); }

            // Cache region restriction status
            FILE* rf = _wfopen(regionCacheFile.c_str(), L"wb");
            if (rf) {
                const char* status = regionRestricted ? "restricted" : "available";
                fwrite(status, 1, strlen(status), rf);
                fclose(rf);
            }
        } else {
            fprintf(stderr, "[STORE] HTTP request failed for appId %s\n", appId.c_str());
        }
    }

    if (jsonStr.empty()) {
        fprintf(stderr, "[STORE] Empty response for appId %s\n", appId.c_str());
        std::lock_guard<std::mutex> lock(g_storeMutex);
        SteamStoreData failData; failData.failed = true;
        g_storeCache[appId] = failData;
        return;
    }

    SteamStoreData storeData;
    if (!ParseAppDetails(appId, jsonStr, storeData)) {
        fprintf(stderr, "[STORE] Parse failed for appId %s\n", appId.c_str());
        std::lock_guard<std::mutex> lock(g_storeMutex);
        SteamStoreData failData; failData.failed = true;
        g_storeCache[appId] = failData;
        return;
    }

    // Set region restriction flag
    storeData.regionRestricted = regionRestricted;

    fprintf(stderr, "[STORE] Loaded appId %s: %s, screenshots=%d, movies=%d, regionRestricted=%d\n",
            appId.c_str(), storeData.name.c_str(),
            (int)storeData.screenshotUrls.size(), (int)storeData.movies.size(), regionRestricted ? 1 : 0);

    // Request screenshot/movie textures
    for (int i = 0; i < std::min(5, (int)storeData.screenshotUrls.size()); i++)
        RequestTextureFromUrl("ss_" + appId + "_" + std::to_string(i), storeData.screenshotUrls[i]);
    for (int i = 0; i < std::min(3, (int)storeData.movies.size()); i++)
        if (!storeData.movies[i].thumbnailUrl.empty())
            RequestTextureFromUrl("mov_" + appId + "_" + std::to_string(i), storeData.movies[i].thumbnailUrl);

    // Fetch news
    {
        std::string newsUrl = "https://api.steampowered.com/ISteamNews/GetNewsForApp/v2/?appid=" + appId + "&count=5&maxlength=0&format=json";
        std::string newsJson;
        if (HttpGet(newsUrl, newsJson) && !newsJson.empty()) {
            try {
                auto nRoot = json::parse(newsJson);
                if (nRoot.contains("appnews") && nRoot["appnews"].contains("newsitems")) {
                    for (auto& item : nRoot["appnews"]["newsitems"]) {
                        SteamStoreData::NewsItem ni;
                        ni.title = SafeJsonStr(item, "title");
                        std::string rawContent = SafeJsonStr(item, "contents");
                        ni.contents = StripHtml(rawContent);
                        ni.richContent = ParseRichContent(rawContent);
                        for (auto& seg : ni.richContent)
                            if (seg.type == SteamStoreData::RichSegment::IMAGE && !seg.text.empty()) {
                                ni.thumbnailUrl = seg.text; break;
                            }
                        ni.url = SafeJsonStr(item, "url");
                        ni.feedLabel = SafeJsonStr(item, "feedlabel");
                        if (item.contains("date") && !item["date"].is_null() && item["date"].is_number())
                            ni.date = item["date"].get<long long>();
                        if (!ni.title.empty())
                            storeData.news.push_back(std::move(ni));
                    }
                    for (int ni = 0; ni < std::min(5, (int)storeData.news.size()); ni++) {
                        if (!storeData.news[ni].thumbnailUrl.empty())
                            RequestTextureFromUrl("news_thumb_" + appId + "_" + std::to_string(ni), storeData.news[ni].thumbnailUrl);
                        int imgIdx = 0;
                        for (auto& seg : storeData.news[ni].richContent)
                            if (seg.type == SteamStoreData::RichSegment::IMAGE && !seg.text.empty())
                                RequestTextureFromUrl("news_inline_" + appId + "_" + std::to_string(ni) + "_" + std::to_string(imgIdx++), seg.text);
                    }
                }
            } catch (...) {}
        }
    }

    // Fetch current player count
    {
        std::string playersUrl = "https://api.steampowered.com/ISteamUserStats/GetNumberOfCurrentPlayers/v1/?appid=" + appId + "&format=json";
        std::string playersJson;
        if (HttpGet(playersUrl, playersJson) && !playersJson.empty()) {
            try {
                auto pRoot = json::parse(playersJson);
                if (pRoot.contains("response") && pRoot["response"].contains("player_count"))
                    storeData.currentPlayers = SafeJsonInt(pRoot["response"], "player_count", 0);
            } catch (...) {}
        }
    }

    // Fetch global achievement percentages (no API key required)
    {
        std::string achUrl = "https://api.steampowered.com/ISteamUserStats/GetGlobalAchievementPercentagesForApp/v2/?gameid=" + appId + "&format=json";
        std::string achJson;
        if (HttpGet(achUrl, achJson) && !achJson.empty()) {
            try {
                auto aRoot = json::parse(achJson);
                if (aRoot.contains("achievementpercentages") && aRoot["achievementpercentages"].contains("achievements")) {
                    std::unordered_map<std::string, float> achPercents;
                    for (auto& ach : aRoot["achievementpercentages"]["achievements"]) {
                        std::string name = SafeJsonStr(ach, "name");
                        float pct = 0.0f;
                        if (ach.contains("percent") && ach["percent"].is_number())
                            pct = ach["percent"].get<float>();
                        if (!name.empty())
                            achPercents[name] = pct;
                    }
                    storeData.totalAchievements = (int)achPercents.size();

                    // Fetch achievement schema (names, descriptions, icons) if API key available
                    if (!g_steamApiKey.empty()) {
                        std::string schemaUrl = "https://api.steampowered.com/ISteamUserStats/GetSchemaForGame/v2/?key=" + g_steamApiKey + "&appid=" + appId + "&l=schinese&format=json";
                        std::string schemaJson;
                        if (HttpGet(schemaUrl, schemaJson) && !schemaJson.empty()) {
                            try {
                                auto sRoot = json::parse(schemaJson);
                                if (sRoot.contains("game") && sRoot["game"].contains("availableGameStats") &&
                                    sRoot["game"]["availableGameStats"].contains("achievements")) {
                                    for (auto& ach : sRoot["game"]["availableGameStats"]["achievements"]) {
                                        SteamStoreData::Achievement achData;
                                        achData.apiName = SafeJsonStr(ach, "name");
                                        achData.displayName = SafeJsonStr(ach, "displayName");
                                        achData.description = SafeJsonStr(ach, "description");
                                        achData.iconUrl = SafeJsonStr(ach, "icon");
                                        achData.iconGrayUrl = SafeJsonStr(ach, "icongray");
                                        achData.hidden = SafeJsonInt(ach, "hidden", 0) != 0;
                                        auto it = achPercents.find(achData.apiName);
                                        if (it != achPercents.end())
                                            achData.globalPercent = it->second;
                                        storeData.achievements.push_back(std::move(achData));
                                    }
                                    // Sort by global percent (rarest first for display)
                                    std::sort(storeData.achievements.begin(), storeData.achievements.end(),
                                        [](const SteamStoreData::Achievement& a, const SteamStoreData::Achievement& b) {
                                            return a.globalPercent < b.globalPercent;
                                        });
                                    // Request icons for top achievements
                                    for (int ai = 0; ai < std::min(12, (int)storeData.achievements.size()); ai++) {
                                        if (!storeData.achievements[ai].iconUrl.empty())
                                            RequestTextureFromUrl("ach_" + appId + "_" + std::to_string(ai), storeData.achievements[ai].iconUrl);
                                    }
                                }
                            } catch (...) {}
                        }
                    }
                }
            } catch (...) {}
        }
    }

    // Save DLC appId list before storing
    std::vector<std::string> dlcAppIds;
    for (auto& dlc : storeData.dlcItems)
        dlcAppIds.push_back(dlc.appId);

    // *** STORE IMMEDIATELY *** — UI can show game detail page now
    {
        std::lock_guard<std::mutex> lock(g_storeMutex);
        g_storeCache[appId] = std::move(storeData);
    }

    // Now fetch DLC details in background (load all DLCs)
    // Limit to 50 DLCs max to avoid excessive API calls
    int dlcMax = std::min(50, (int)dlcAppIds.size());
    for (int di = 0; di < dlcMax; di++) {
        if (!g_storeRunning) break;

        // Add delay BEFORE each request (except first) to avoid rate limiting
        if (di > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
        }

        const std::string& dlcId = dlcAppIds[di];
        std::string dlcUrl = "https://store.steampowered.com/api/appdetails?appids=" + dlcId + "&l=schinese&cc=CN";
        std::string dlcJson;
        if (!HttpGet(dlcUrl, dlcJson) || dlcJson.empty()) continue;

        try {
            auto dRoot = json::parse(dlcJson);
            if (!dRoot.contains(dlcId) || !dRoot[dlcId].contains("success")
                || !dRoot[dlcId]["success"].get<bool>()
                || !dRoot[dlcId].contains("data")) continue;
            auto& dd = dRoot[dlcId]["data"];

            std::lock_guard<std::mutex> lock(g_storeMutex);
            auto it = g_storeCache.find(appId);
            if (it == g_storeCache.end()) break;
            for (auto& dlc : it->second.dlcItems) {
                if (dlc.appId != dlcId) continue;
                dlc.name = SafeJsonStr(dd, "name", "DLC");
                dlc.headerImage = SafeJsonStr(dd, "header_image");
                dlc.shortDesc = SafeJsonStr(dd, "short_description");
                std::string aboutRaw = SafeJsonStr(dd, "about_the_game");
                if (aboutRaw.empty()) aboutRaw = SafeJsonStr(dd, "detailed_description");
                dlc.aboutGame = StripHtml(aboutRaw);
                if (dd.contains("price_overview") && dd["price_overview"].is_object()) {
                    auto& pr = dd["price_overview"];
                    dlc.price = SafeJsonStr(pr, "final_formatted");
                    dlc.originalPrice = SafeJsonStr(pr, "initial_formatted");
                    dlc.discountPercent = SafeJsonInt(pr, "discount_percent", 0);
                } else if (dd.contains("is_free") && dd["is_free"].get<bool>()) {
                    dlc.price = u8"\u514D\u8D39";
                }
                dlc.loaded = true;
                if (!dlc.headerImage.empty())
                    RequestTextureFromUrl("dlc_" + dlcId, dlc.headerImage);
                break;
            }
        } catch (...) {}
    }
}

// Multiple worker threads for parallel fetching
static const int STORE_WORKER_COUNT = 3;
static std::thread g_storeWorkers[STORE_WORKER_COUNT];

static void StoreWorkerThread() {
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (g_storeCacheDir.empty()) g_storeCacheDir = GetStoreCacheDir();

    while (g_storeRunning) {
        std::string appId;
        {
            std::lock_guard<std::mutex> lock(g_storeMutex);
            if (!g_storeQueue.empty()) {
                appId = g_storeQueue.front();
                g_storeQueue.pop();
            }
        }

        if (appId.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        FetchOneStoreEntry(appId);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    CoUninitialize();
}

// ---- Public API ----

void InitStoreData() {
    g_storeRunning = true;
    g_storeCacheDir = GetStoreCacheDir();
    for (int i = 0; i < STORE_WORKER_COUNT; i++)
        g_storeWorkers[i] = std::thread(StoreWorkerThread);
}

void ShutdownStoreData() {
    g_storeRunning = false;
    for (int i = 0; i < STORE_WORKER_COUNT; i++)
        if (g_storeWorkers[i].joinable())
            g_storeWorkers[i].join();
}

void RequestStoreData(const std::string& appId) {
    if (appId.empty()) return;
    std::lock_guard<std::mutex> lock(g_storeMutex);
    // Skip if already loaded successfully
    auto it = g_storeCache.find(appId);
    if (it != g_storeCache.end() && it->second.loaded) return;
    // Skip if already in queue (but not failed)
    if (g_storeRequested.count(appId) && (it == g_storeCache.end() || !it->second.failed)) return;
    // If previously failed, allow retry
    if (it != g_storeCache.end() && it->second.failed) {
        g_storeCache.erase(it);
    }
    g_storeRequested.insert(appId);
    g_storeQueue.push(appId);
}

const SteamStoreData* GetStoreData(const std::string& appId) {
    std::lock_guard<std::mutex> lock(g_storeMutex);
    auto it = g_storeCache.find(appId);
    if (it != g_storeCache.end() && it->second.loaded)
        return &it->second;
    return nullptr;
}

bool IsStoreDataLoading(const std::string& appId) {
    std::lock_guard<std::mutex> lock(g_storeMutex);
    return g_storeRequested.count(appId) && !g_storeCache.count(appId);
}

// ---- Store Listings (featured, specials, new releases, etc.) ----

static std::mutex g_listingsMutex;
static SteamStoreListings g_listings;
static std::atomic<bool> g_listingsRequested{false};
static std::atomic<bool> g_listingsLoading{false};
static std::thread g_listingsWorker;

static std::string DecodeHtmlEntities(const std::string& s) {
    std::string r;
    r.reserve(s.size());
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '&') {
            if (s.compare(i, 5, "&amp;") == 0) { r += '&'; i += 4; continue; }
            if (s.compare(i, 4, "&lt;") == 0) { r += '<'; i += 3; continue; }
            if (s.compare(i, 4, "&gt;") == 0) { r += '>'; i += 3; continue; }
            if (s.compare(i, 6, "&quot;") == 0) { r += '"'; i += 5; continue; }
            if (s.compare(i, 6, "&apos;") == 0) { r += '\''; i += 5; continue; }
            if (s.compare(i, 8, "&trade;") == 0 || s.compare(i, 7, "&#8482;") == 0) { r += u8"\u2122"; i += (s[i+1]=='#'?6:7); continue; }
            if (s.compare(i, 6, "&reg;") == 0) { r += u8"\u00AE"; i += 5; continue; }
        }
        r += s[i];
    }
    return r;
}

static SteamListingGame ParseListingItem(const json& item) {
    SteamListingGame g;
    g.appId = std::to_string(SafeJsonInt(item, "id", 0));
    g.name = DecodeHtmlEntities(SafeJsonStr(item, "name"));

    // Price (some fields may be null in Steam API)
    int finalPrice = SafeJsonInt(item, "final_price", 0);
    int originalPrice = SafeJsonInt(item, "original_price", 0);
    int discPct = SafeJsonInt(item, "discount_percent", 0);
    bool isFree = (finalPrice == 0);

    if (isFree) {
        g.price = u8"\u514D\u8D39\u5F00\u73A9"; // 免费开玩
    } else {
        char buf[32];
        snprintf(buf, sizeof(buf), u8"\u00A5%d", finalPrice / 100);
        g.price = buf;
    }

    if (discPct > 0) {
        char dbuf[16];
        snprintf(dbuf, sizeof(dbuf), "-%d%%", discPct);
        g.discount = dbuf;
        g.discountPercent = discPct;

        if (originalPrice > 0) {
            char obuf[32];
            snprintf(obuf, sizeof(obuf), u8"\u00A5%d", originalPrice / 100);
            g.originalPrice = obuf;
        }
    }

    // Header image - use API-provided URLs (more reliable for new games)
    // Try multiple API fields in order of preference
    g.headerImage = SafeJsonStr(item, "large_capsule_image");
    if (g.headerImage.empty())
        g.headerImage = SafeJsonStr(item, "header_image");
    if (g.headerImage.empty())
        g.headerImage = SafeJsonStr(item, "small_capsule_image");

    // Fallback to CDN pattern if API doesn't provide any image URL
    if (g.headerImage.empty() && !g.appId.empty() && g.appId != "0") {
        g.headerImage = "https://cdn.akamai.steamstatic.com/steam/apps/" + g.appId + "/header.jpg";
    }

    return g;
}

static void ListingsWorkerThread() {
    CoInitializeEx(NULL, COINIT_MULTITHREADED);

    // Fetch featured games
    {
        std::string resp;
        if (HttpGet("https://store.steampowered.com/api/featured/?l=schinese&cc=CN", resp) && !resp.empty()) {
            try {
                auto j = json::parse(resp);
                std::lock_guard<std::mutex> lock(g_listingsMutex);

                auto addFeatured = [&](const json& arr, int maxCount) {
                    for (auto& item : arr) {
                        if ((int)g_listings.featured.size() >= maxCount) break;
                        auto g = ParseListingItem(item);
                        if (!g.appId.empty() && g.appId != "0") {
                            if (!g.headerImage.empty())
                                RequestTextureFromUrl("hdr_" + g.appId, g.headerImage);
                            RequestGameTexture(g.appId);
                            g_listings.featured.push_back(std::move(g));
                        }
                    }
                };

                if (j.contains("large_capsules") && j["large_capsules"].is_array())
                    addFeatured(j["large_capsules"], 10);
                if (j.contains("featured_win") && j["featured_win"].is_array())
                    addFeatured(j["featured_win"], 12);

                fprintf(stderr, "[STORE] Featured: %d games\n", (int)g_listings.featured.size());
            } catch (const std::exception& e) {
                fprintf(stderr, "[STORE] Featured parse error: %s\n", e.what());
            }
        }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Fetch categories (specials, top_sellers, new_releases, coming_soon)
    {
        std::string resp;
        if (HttpGet("https://store.steampowered.com/api/featuredcategories/?l=schinese&cc=CN", resp) && !resp.empty()) {
            try {
                auto j = json::parse(resp);
                std::lock_guard<std::mutex> lock(g_listingsMutex);

                auto parseCategory = [&](const char* key, std::vector<SteamListingGame>& out) {
                    if (j.contains(key) && j[key].contains("items") && j[key]["items"].is_array()) {
                        for (auto& item : j[key]["items"]) {
                            auto g = ParseListingItem(item);
                            if (!g.appId.empty() && g.appId != "0") {
                                // Use direct header_image URL if available, fall back to CDN pattern
                                if (!g.headerImage.empty()) {
                                    RequestTextureFromUrl("hdr_" + g.appId, g.headerImage);
                                }
                                RequestGameTexture(g.appId);
                                out.push_back(std::move(g));
                            }
                        }
                    }
                };

                parseCategory("specials", g_listings.specials);
                parseCategory("top_sellers", g_listings.topSellers);
                parseCategory("new_releases", g_listings.newReleases);
                parseCategory("coming_soon", g_listings.comingSoon);

                g_listings.loaded = true;
                fprintf(stderr, "[STORE] Categories loaded: specials=%d, top=%d, new=%d, coming=%d\n",
                        (int)g_listings.specials.size(), (int)g_listings.topSellers.size(),
                        (int)g_listings.newReleases.size(), (int)g_listings.comingSoon.size());
            } catch (const std::exception& e) {
                fprintf(stderr, "[STORE] Categories parse error: %s\n", e.what());
                // Still mark as loaded even if some items failed
                std::lock_guard<std::mutex> lock(g_listingsMutex);
                g_listings.loaded = true;
            }
        } else {
            std::lock_guard<std::mutex> lock(g_listingsMutex);
            g_listings.failed = true;
        }
    }

    g_listingsLoading = false;
    CoUninitialize();
}

void RequestStoreListings() {
    // 如果已经加载成功，不再请求
    {
        std::lock_guard<std::mutex> lock(g_listingsMutex);
        if (g_listings.loaded && !g_listings.featured.empty()) return;
    }

    // 如果正在加载中，不再请求
    if (g_listingsLoading.load()) return;

    // 重置状态以允许重试
    g_listingsRequested = true;
    g_listingsLoading = true;
    g_listingsWorker = std::thread(ListingsWorkerThread);
    g_listingsWorker.detach();
}

const SteamStoreListings* GetStoreListings() {
    std::lock_guard<std::mutex> lock(g_listingsMutex);
    if (g_listings.loaded)
        return &g_listings;
    return nullptr;
}

bool IsStoreListingsLoading() {
    return g_listingsLoading.load();
}

// ---- Search ----

static std::mutex g_searchMutex;
static SteamSearchResults g_searchResults;
static std::atomic<bool> g_searchLoading{false};
static std::thread g_searchWorker;
static std::string g_searchPending; // next query to run (if user types fast)
static std::mutex g_searchPendingMutex;

// ---- Game Alias Mapping ----
static std::unordered_map<std::string, std::string> g_aliasToEnglish;  // 中文别名 -> 英文名
static bool g_aliasLoaded = false;
static std::mutex g_aliasMutex;

static void LoadGameAliases() {
    std::lock_guard<std::mutex> lock(g_aliasMutex);
    if (g_aliasLoaded) return;
    g_aliasLoaded = true;

    // 获取exe所在目录
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::filesystem::path exeDir = std::filesystem::path(exePath).parent_path();
    std::filesystem::path aliasFile = exeDir / "data" / "game_aliases.json";

    // 也尝试从源码目录加载（开发时）
    if (!std::filesystem::exists(aliasFile)) {
        aliasFile = exeDir.parent_path() / "data" / "game_aliases.json";
    }
    if (!std::filesystem::exists(aliasFile)) {
        aliasFile = "c:/Users/lixin/Desktop/SteamForge/data/game_aliases.json";
    }

    fprintf(stderr, "[ALIAS] Loading from: %s\n", aliasFile.string().c_str());

    std::ifstream file(aliasFile);
    if (!file.is_open()) {
        fprintf(stderr, "[ALIAS] Failed to open alias file\n");
        return;
    }

    try {
        json j = json::parse(file);
        if (j.contains("aliases") && j["aliases"].is_object()) {
            for (auto& [englishName, aliases] : j["aliases"].items()) {
                if (aliases.is_array()) {
                    for (auto& alias : aliases) {
                        if (alias.is_string()) {
                            std::string aliasStr = alias.get<std::string>();
                            // 转小写存储以便不区分大小写匹配
                            std::string aliasLower = aliasStr;
                            for (auto& c : aliasLower) c = (char)tolower((unsigned char)c);
                            g_aliasToEnglish[aliasLower] = englishName;
                            // 也存原始大小写版本
                            g_aliasToEnglish[aliasStr] = englishName;
                        }
                    }
                }
            }
        }
        fprintf(stderr, "[ALIAS] Loaded %d alias mappings\n", (int)g_aliasToEnglish.size());
    } catch (const std::exception& e) {
        fprintf(stderr, "[ALIAS] Parse error: %s\n", e.what());
    }
}

// 查找别名对应的英文名
static std::string FindEnglishNameByAlias(const std::string& term) {
    LoadGameAliases();

    std::lock_guard<std::mutex> lock(g_aliasMutex);

    // 先精确匹配
    auto it = g_aliasToEnglish.find(term);
    if (it != g_aliasToEnglish.end()) {
        return it->second;
    }

    // 转小写后匹配
    std::string termLower = term;
    for (auto& c : termLower) c = (char)tolower((unsigned char)c);
    it = g_aliasToEnglish.find(termLower);
    if (it != g_aliasToEnglish.end()) {
        return it->second;
    }

    // 部分匹配（搜索词包含在别名中，或别名包含在搜索词中）
    for (auto& [alias, english] : g_aliasToEnglish) {
        if (alias.find(termLower) != std::string::npos ||
            termLower.find(alias) != std::string::npos) {
            return english;
        }
    }

    return "";
}

static void SearchWorkerThread(std::string term, int start, int count) {
    CoInitializeEx(NULL, COINIT_MULTITHREADED);

    // Proper URL encoding for all characters (including Chinese)
    auto urlEncode = [](const std::string& str) -> std::string {
        std::string encoded;
        for (unsigned char c : str) {
            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
                encoded += c;
            } else if (c == ' ') {
                encoded += '+';
            } else {
                char hex[4];
                snprintf(hex, sizeof(hex), "%%%02X", c);
                encoded += hex;
            }
        }
        return encoded;
    };

    std::string encoded = urlEncode(term);

    // 检测是否包含中文字符
    bool hasChinese = false;
    for (unsigned char c : term) {
        if (c >= 0x80) {  // UTF-8 多字节字符（可能是中文）
            hasChinese = true;
            break;
        }
    }

    SteamSearchResults results;
    results.query = term;

    // 搜索函数
    auto doSearch = [&](const std::string& lang, const std::string& encodedTerm) {
        char urlBuf[1024];
        snprintf(urlBuf, sizeof(urlBuf),
            "https://store.steampowered.com/api/storesearch/?term=%s&l=%s&cc=CN&start=%d&count=%d",
            encodedTerm.c_str(), lang.c_str(), start, count);

        fprintf(stderr, "[SEARCH] Requesting: %s\n", urlBuf);

        std::string resp;
        std::vector<SteamListingGame> items;

        if (HttpGet(urlBuf, resp) && !resp.empty()) {
            fprintf(stderr, "[SEARCH] Got response, length=%d\n", (int)resp.size());
            try {
                auto j = json::parse(resp);
                int total = SafeJsonInt(j, "total", 0);
                if (total > results.total) results.total = total;

                if (j.contains("items") && j["items"].is_array()) {
                    for (auto& item : j["items"]) {
                        SteamListingGame g;
                        g.appId = std::to_string(SafeJsonInt(item, "id", 0));
                        g.name = DecodeHtmlEntities(SafeJsonStr(item, "name"));
                        g.headerImage = SafeJsonStr(item, "tiny_image");

                        // Price
                        if (item.contains("price") && item["price"].is_object()) {
                            auto& pr = item["price"];
                            int finalP = SafeJsonInt(pr, "final", 0);
                            int initP = SafeJsonInt(pr, "initial", 0);
                            int disc = SafeJsonInt(pr, "discount_percent", 0);

                            if (finalP == 0) {
                                g.price = u8"\u514D\u8D39\u5F00\u73A9";
                            } else {
                                char buf[32];
                                snprintf(buf, sizeof(buf), u8"\u00A5%d", finalP / 100);
                                g.price = buf;
                            }
                            if (disc > 0) {
                                char dbuf[16];
                                snprintf(dbuf, sizeof(dbuf), "-%d%%", disc);
                                g.discount = dbuf;
                                g.discountPercent = disc;
                                if (initP > 0) {
                                    char obuf[32];
                                    snprintf(obuf, sizeof(obuf), u8"\u00A5%d", initP / 100);
                                    g.originalPrice = obuf;
                                }
                            }
                        }

                        if (!g.appId.empty() && g.appId != "0") {
                            items.push_back(std::move(g));
                        }
                    }
                }
            } catch (const std::exception& e) {
                fprintf(stderr, "[SEARCH] Parse error (%s): %s\n", lang.c_str(), e.what());
            }
        }
        return items;
    };

    // 先用中文搜索
    auto chineseResults = doSearch("schinese", encoded);

    // 如果搜索词包含中文，也用英文搜索（可能用户输入的是游戏的中文名）
    // 如果搜索词是英文，也用中文搜索（可能用户想找有中文名的游戏）
    std::vector<SteamListingGame> englishResults;
    if (hasChinese) {
        // 中文搜索词，额外用英文搜索
        englishResults = doSearch("english", encoded);
    } else {
        // 英文搜索词，额外用中文搜索（已经搜过了，这里用英文再搜一次）
        englishResults = doSearch("english", encoded);
    }

    // 检查别名映射，如果找到对应的英文名，额外搜索
    std::string aliasEnglishName = FindEnglishNameByAlias(term);
    std::vector<SteamListingGame> aliasResults;
    if (!aliasEnglishName.empty() && aliasEnglishName != term) {
        fprintf(stderr, "[SEARCH] Found alias mapping: '%s' -> '%s'\n", term.c_str(), aliasEnglishName.c_str());
        std::string aliasEncoded = urlEncode(aliasEnglishName);
        aliasResults = doSearch("english", aliasEncoded);
        // 也用中文搜索英文名
        auto aliasChineseResults = doSearch("schinese", aliasEncoded);
        for (auto& g : aliasChineseResults) {
            aliasResults.push_back(std::move(g));
        }
    }

    // 合并结果，去重
    std::set<std::string> seenAppIds;

    // 优先显示别名匹配的结果
    for (auto& g : aliasResults) {
        if (seenAppIds.find(g.appId) == seenAppIds.end()) {
            seenAppIds.insert(g.appId);
            if (!g.headerImage.empty())
                RequestTextureFromUrl("hdr_" + g.appId, g.headerImage);
            RequestGameTexture(g.appId);
            results.items.push_back(std::move(g));
        }
    }

    for (auto& g : chineseResults) {
        if (seenAppIds.find(g.appId) == seenAppIds.end()) {
            seenAppIds.insert(g.appId);
            // Request textures
            if (!g.headerImage.empty())
                RequestTextureFromUrl("hdr_" + g.appId, g.headerImage);
            RequestGameTexture(g.appId);
            results.items.push_back(std::move(g));
        }
    }
    for (auto& g : englishResults) {
        if (seenAppIds.find(g.appId) == seenAppIds.end()) {
            seenAppIds.insert(g.appId);
            // Request textures
            if (!g.headerImage.empty())
                RequestTextureFromUrl("hdr_" + g.appId, g.headerImage);
            RequestGameTexture(g.appId);
            results.items.push_back(std::move(g));
        }
    }

    results.loaded = true;
    fprintf(stderr, "[SEARCH] '%s' => %d results (alias: %d, chinese: %d, english: %d)\n",
            term.c_str(), (int)results.items.size(),
            (int)aliasResults.size(), (int)chineseResults.size(), (int)englishResults.size());

    {
        std::lock_guard<std::mutex> lock(g_searchMutex);
        g_searchResults = std::move(results);
    }
    g_searchLoading = false;
    CoUninitialize();
}

void RequestStoreSearch(const std::string& term, int start, int count) {
    if (g_searchLoading.load()) {
        // A search is already running; queue this one
        std::lock_guard<std::mutex> lock(g_searchPendingMutex);
        g_searchPending = term;
        return;
    }
    g_searchLoading = true;
    // Clear old results
    {
        std::lock_guard<std::mutex> lock(g_searchMutex);
        g_searchResults = SteamSearchResults();
    }
    // Detach previous worker if joinable
    if (g_searchWorker.joinable()) g_searchWorker.detach();
    g_searchWorker = std::thread(SearchWorkerThread, term, start, count);
    g_searchWorker.detach();
}

const SteamSearchResults* GetSearchResults() {
    std::lock_guard<std::mutex> lock(g_searchMutex);
    if (g_searchResults.loaded)
        return &g_searchResults;
    return nullptr;
}

bool IsSearchLoading() {
    // Check if pending search needs to fire
    if (!g_searchLoading.load()) {
        std::lock_guard<std::mutex> lock(g_searchPendingMutex);
        if (!g_searchPending.empty()) {
            std::string next = g_searchPending;
            g_searchPending.clear();
            // Fire it (unlock first to avoid deadlock)
            lock.~lock_guard();
            RequestStoreSearch(next);
            return true;
        }
    }
    return g_searchLoading.load();
}

void ClearSearchResults() {
    std::lock_guard<std::mutex> lock(g_searchMutex);
    g_searchResults = SteamSearchResults();
}

// ---- Browse All Games (IStoreService/GetAppList) ----

static std::mutex g_browseMutex;
static SteamBrowsePage g_browsePage;
static std::atomic<bool> g_browseLoading{false};
static std::thread g_browseWorker;

static void BrowseWorkerThread(int lastAppId, int count) {
    CoInitializeEx(NULL, COINIT_MULTITHREADED);

    if (g_steamApiKey.empty()) {
        fprintf(stderr, "[BROWSE] No API key set\n");
        std::lock_guard<std::mutex> lock(g_browseMutex);
        g_browsePage.loaded = true;
        g_browsePage.loading = false;
        g_browseLoading = false;
        CoUninitialize();
        return;
    }

    char urlBuf[512];
    snprintf(urlBuf, sizeof(urlBuf),
        "https://api.steampowered.com/IStoreService/GetAppList/v1/"
        "?key=%s&include_games=true&include_dlc=false&max_results=%d&last_appid=%d",
        g_steamApiKey.c_str(), count, lastAppId);

    std::string resp;
    if (HttpGet(urlBuf, resp) && !resp.empty()) {
        try {
            auto j = json::parse(resp);
            auto& response = j["response"];

            bool hasMore = false;
            int newLastAppId = lastAppId;

            if (response.contains("have_more_results") && !response["have_more_results"].is_null())
                hasMore = response["have_more_results"].get<bool>();
            if (response.contains("last_appid") && !response["last_appid"].is_null())
                newLastAppId = response["last_appid"].get<int>();

            std::vector<SteamListingGame> newGames;
            if (response.contains("apps") && response["apps"].is_array()) {
                for (auto& app : response["apps"]) {
                    SteamListingGame g;
                    if (app.contains("appid") && app["appid"].is_number())
                        g.appId = std::to_string(app["appid"].get<int>());
                    g.name = SafeJsonStr(app, "name");

                    if (g.appId.empty() || g.appId == "0" || g.name.empty())
                        continue;

                    // Construct CDN header image URL (use high-res library_hero)
                    g.headerImage = "https://cdn.akamai.steamstatic.com/steam/apps/" + g.appId + "/library_hero.jpg";

                    // Request texture
                    RequestTextureFromUrl("hdr_" + g.appId, g.headerImage);
                    RequestGameTexture(g.appId);

                    newGames.push_back(std::move(g));
                }
            }

            fprintf(stderr, "[BROWSE] Loaded %d games (lastAppId=%d, hasMore=%d)\n",
                    (int)newGames.size(), newLastAppId, hasMore);

            std::lock_guard<std::mutex> lock(g_browseMutex);
            // Append to existing results
            for (auto& g : newGames)
                g_browsePage.games.push_back(std::move(g));
            g_browsePage.lastAppId = newLastAppId;
            g_browsePage.hasMore = hasMore;
            g_browsePage.loaded = true;
            g_browsePage.loading = false;

        } catch (const std::exception& e) {
            fprintf(stderr, "[BROWSE] Parse error: %s\n", e.what());
            std::lock_guard<std::mutex> lock(g_browseMutex);
            g_browsePage.loaded = true;
            g_browsePage.loading = false;
        }
    } else {
        fprintf(stderr, "[BROWSE] HTTP failed\n");
        std::lock_guard<std::mutex> lock(g_browseMutex);
        g_browsePage.loaded = true;
        g_browsePage.loading = false;
    }

    g_browseLoading = false;
    CoUninitialize();
}

void RequestBrowseGames(int lastAppId, int count) {
    if (g_browseLoading.load()) return; // already loading
    g_browseLoading = true;
    {
        std::lock_guard<std::mutex> lock(g_browseMutex);
        g_browsePage.loading = true;
    }
    if (g_browseWorker.joinable()) g_browseWorker.detach();
    g_browseWorker = std::thread(BrowseWorkerThread, lastAppId, count);
    g_browseWorker.detach();
}

const SteamBrowsePage* GetBrowsePage() {
    std::lock_guard<std::mutex> lock(g_browseMutex);
    if (g_browsePage.loaded)
        return &g_browsePage;
    return nullptr;
}

bool IsBrowseLoading() {
    return g_browseLoading.load();
}

// ═══════════════════════════════════════════════════════════════
//  TRANSLATION API (Google Translate - free endpoint)
// ═══════════════════════════════════════════════════════════════

static std::mutex g_translationMutex;
static std::unordered_map<std::string, TranslationResult> g_translations;
static std::unordered_map<std::string, std::atomic<bool>*> g_translationLoading;

// URL encode for query parameters
static std::string UrlEncode(const std::string& s) {
    std::string result;
    result.reserve(s.size() * 3);
    for (unsigned char c : s) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            result += c;
        } else {
            char buf[4];
            snprintf(buf, sizeof(buf), "%%%02X", c);
            result += buf;
        }
    }
    return result;
}

// Translate a single text using Google Translate free API
static std::string TranslateSingle(const std::string& text, const std::string& targetLang = "zh-CN") {
    if (text.empty()) return "";

    // Google Translate free endpoint
    std::string host = "translate.googleapis.com";
    std::string path = "/translate_a/single?client=gtx&sl=auto&tl=" + targetLang +
                       "&dt=t&q=" + UrlEncode(text);

    std::wstring wHost(host.begin(), host.end());
    std::wstring wPath(path.begin(), path.end());

    HINTERNET hSession = WinHttpOpen(L"SteamForge/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                      WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return text;

    WinHttpSetTimeouts(hSession, 10000, 10000, 15000, 15000);

    HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return text; }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", wPath.c_str(), NULL,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return text; }

    BOOL ok = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                  WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (!ok) { WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return text; }

    ok = WinHttpReceiveResponse(hRequest, NULL);
    if (!ok) { WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return text; }

    std::string response;
    DWORD bytesAvailable = 0;
    while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0) {
        std::vector<char> buf(bytesAvailable + 1);
        DWORD bytesRead = 0;
        if (WinHttpReadData(hRequest, buf.data(), bytesAvailable, &bytesRead)) {
            response.append(buf.data(), bytesRead);
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    // Parse Google Translate JSON response: [[["translated","original",...],...]...]
    // The response is a nested array, first element contains translation segments
    try {
        auto j = json::parse(response);
        if (j.is_array() && !j.empty() && j[0].is_array()) {
            std::string result;
            for (auto& seg : j[0]) {
                if (seg.is_array() && !seg.empty() && seg[0].is_string()) {
                    result += seg[0].get<std::string>();
                }
            }
            if (!result.empty()) return result;
        }
    } catch (...) {}

    return text;  // Return original on failure
}

static void TranslationWorkerThread(std::vector<std::string> texts, std::string requestId) {
    CoInitializeEx(NULL, COINIT_MULTITHREADED);

    TranslationResult result;
    result.translatedTexts.reserve(texts.size());

    for (const auto& text : texts) {
        std::string translated = TranslateSingle(text);
        result.translatedTexts.push_back(translated);
        Sleep(100);  // Small delay to avoid rate limiting
    }

    result.loaded = true;

    {
        std::lock_guard<std::mutex> lock(g_translationMutex);
        g_translations[requestId] = std::move(result);
        if (g_translationLoading.count(requestId)) {
            g_translationLoading[requestId]->store(false);
        }
    }

    CoUninitialize();
}

void RequestTranslation(const std::vector<std::string>& texts, const std::string& requestId) {
    std::lock_guard<std::mutex> lock(g_translationMutex);

    // Check if already loading or loaded
    if (g_translationLoading.count(requestId) && g_translationLoading[requestId]->load()) {
        return;  // Already loading
    }
    if (g_translations.count(requestId) && g_translations[requestId].loaded) {
        return;  // Already loaded
    }

    // Create loading flag
    if (!g_translationLoading.count(requestId)) {
        g_translationLoading[requestId] = new std::atomic<bool>(true);
    } else {
        g_translationLoading[requestId]->store(true);
    }

    // Start worker thread
    std::thread worker(TranslationWorkerThread, texts, requestId);
    worker.detach();
}

const TranslationResult* GetTranslationResult(const std::string& requestId) {
    std::lock_guard<std::mutex> lock(g_translationMutex);
    auto it = g_translations.find(requestId);
    if (it != g_translations.end() && it->second.loaded) {
        return &it->second;
    }
    return nullptr;
}

bool IsTranslationLoading(const std::string& requestId) {
    std::lock_guard<std::mutex> lock(g_translationMutex);
    auto it = g_translationLoading.find(requestId);
    if (it != g_translationLoading.end()) {
        return it->second->load();
    }
    return false;
}

void ClearTranslation(const std::string& requestId) {
    std::lock_guard<std::mutex> lock(g_translationMutex);
    g_translations.erase(requestId);
    if (g_translationLoading.count(requestId)) {
        g_translationLoading[requestId]->store(false);
    }
}

} // namespace sf
