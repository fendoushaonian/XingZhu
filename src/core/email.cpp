#include "core/email.h"
#include "utils/logger.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <random>
#include <thread>
#include <mutex>
#include <chrono>
#include <sstream>
#include <unordered_map>

namespace sf {

// ── SMTP config (QQ Mail) ──
static const char* kSmtpFrom     = "fendoushao_0609@qq.com";
static const char* kSmtpAuthCode = "rbdrmnqzjtgjdega";
static const char* kAppName      = u8"\u661F\u94F8"; // 星铸

// ── Pending codes ──
struct CodeEntry {
    std::string code;
    std::chrono::steady_clock::time_point sentAt;
};

static std::mutex g_codeMtx;
static std::unordered_map<std::string, CodeEntry> g_codes;

static constexpr int kCodeExpirySec  = 300; // 5 min
static constexpr int kCodeCooldownSec = 60; // 1 min between sends

static std::string GenerateCode() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(100000, 999999);
    return std::to_string(dis(gen));
}

// Send email via PowerShell .ps1 script file + .NET SmtpClient in background
static void SendEmailThread(std::string to, std::string subject, std::string body) {
    // Escape single quotes for PowerShell string literals
    auto esc = [](const std::string& s) {
        std::string out;
        for (char c : s) {
            if (c == '\'') out += "''";
            else out += c;
        }
        return out;
    };

    // Write a temp .ps1 script file (avoids cmd.exe escaping issues)
    char tempPath[MAX_PATH];
    GetTempPathA(MAX_PATH, tempPath);

    char tempFile[MAX_PATH];
    GetTempFileNameA(tempPath, "sfm", 0, tempFile);

    // Rename .tmp to .ps1
    std::string ps1Path = std::string(tempFile);
    // Replace extension
    auto dot = ps1Path.rfind('.');
    if (dot != std::string::npos) ps1Path = ps1Path.substr(0, dot);
    ps1Path += ".ps1";
    MoveFileA(tempFile, ps1Path.c_str());

    // Build PowerShell script content
    std::ostringstream ps;
    ps << "[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12\n"
       << "$smtp = New-Object System.Net.Mail.SmtpClient('smtp.qq.com', 587)\n"
       << "$smtp.EnableSsl = $true\n"
       << "$smtp.Credentials = New-Object System.Net.NetworkCredential('"
       << esc(kSmtpFrom) << "', '" << esc(kSmtpAuthCode) << "')\n"
       << "$msg = New-Object System.Net.Mail.MailMessage('"
       << esc(kSmtpFrom) << "', '" << esc(to) << "', '"
       << esc(subject) << "', '" << esc(body) << "')\n"
       << "$msg.IsBodyHtml = $true\n"
       << "$smtp.Send($msg)\n";

    // Write script to file with UTF-8 BOM (so PowerShell reads Chinese correctly)
    HANDLE hFile = CreateFileA(ps1Path.c_str(), GENERIC_WRITE, 0, NULL,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        Log(LogLevel::Error, "Failed to create temp script: %s", ps1Path.c_str());
        return;
    }
    // UTF-8 BOM
    const unsigned char bom[] = {0xEF, 0xBB, 0xBF};
    DWORD written;
    WriteFile(hFile, bom, 3, &written, NULL);

    std::string script = ps.str();
    WriteFile(hFile, script.c_str(), (DWORD)script.size(), &written, NULL);
    CloseHandle(hFile);

    // Build error log path next to the script
    std::string errLog = ps1Path + ".err.txt";

    // Execute: powershell -ExecutionPolicy Bypass -File <path>, redirect stderr to error log
    std::string fullCmd = "powershell.exe -ExecutionPolicy Bypass -NoProfile -NonInteractive -File \""
                          + ps1Path + "\" 2>\"" + errLog + "\"";
    std::vector<char> cmdBuf(fullCmd.begin(), fullCmd.end());
    cmdBuf.push_back('\0');

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = {};

    BOOL ok = CreateProcessA(
        NULL, cmdBuf.data(), NULL, NULL, FALSE,
        CREATE_NO_WINDOW, NULL, NULL, &si, &pi);

    if (ok) {
        WaitForSingleObject(pi.hProcess, 30000); // 30s timeout
        DWORD exitCode = 1;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        if (exitCode == 0) {
            Log(LogLevel::Info, "Verification email sent to %s", to.c_str());
        } else {
            Log(LogLevel::Error, "Email send failed (exit %lu) to %s", exitCode, to.c_str());
            // Read error log for diagnostics
            FILE* ef = fopen(errLog.c_str(), "r");
            if (ef) {
                char errBuf[512] = {};
                fread(errBuf, 1, sizeof(errBuf) - 1, ef);
                fclose(ef);
                if (errBuf[0]) Log(LogLevel::Error, "PowerShell error: %s", errBuf);
            }
        }
    } else {
        Log(LogLevel::Error, "CreateProcess failed for email send: %lu", GetLastError());
    }

    // Clean up temp files
    DeleteFileA(ps1Path.c_str());
    DeleteFileA(errLog.c_str());
}

bool SendVerifyCode(const std::string& toEmail) {
    if (toEmail.empty() || toEmail.find('@') == std::string::npos)
        return false;

    auto now = std::chrono::steady_clock::now();

    std::lock_guard<std::mutex> lk(g_codeMtx);

    // Check cooldown
    auto it = g_codes.find(toEmail);
    if (it != g_codes.end()) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - it->second.sentAt).count();
        if (elapsed < kCodeCooldownSec)
            return false; // still in cooldown
    }

    std::string code = GenerateCode();
    g_codes[toEmail] = {code, now};

    // Build HTML email body — Steam-style dark theme
    std::ostringstream html;
    html << "<div style=\"font-family:-apple-system,BlinkMacSystemFont,Segoe UI,Roboto,sans-serif;"
         << "max-width:520px;margin:0 auto;background:#171a21;border-radius:4px;overflow:hidden;\">"

         // ── Header banner ──
         << "<div style=\"background:linear-gradient(135deg,#1a9fff 0%,#0d5b9e 100%);"
         << "padding:28px 32px;text-align:center;\">"
         << "<div style=\"font-size:32px;font-weight:700;color:#ffffff;letter-spacing:2px;\">"
         << kAppName << "</div>"
         << "<div style=\"font-size:13px;color:rgba(255,255,255,0.7);margin-top:4px;letter-spacing:1px;\">"
         << "STEAMFORGE</div>"
         << "</div>"

         // ── Body ──
         << "<div style=\"padding:32px;\">"

         // Greeting
         << "<p style=\"font-size:16px;color:#c7d5e0;margin:0 0 20px 0;line-height:1.6;\">"
         << u8"\u60A8\u597D\uFF0C"  // 您好，
         << "</p>"

         << "<p style=\"font-size:15px;color:#8f98a0;margin:0 0 8px 0;line-height:1.6;\">"
         << u8"\u611F\u8C22\u60A8\u6CE8\u518C " // 感谢您注册
         << "<span style=\"color:#1a9fff;font-weight:600;\">" << kAppName << "</span>"
         << u8"\uFF0C\u8BF7\u4F7F\u7528\u4EE5\u4E0B\u9A8C\u8BC1\u7801\u5B8C\u6210\u8D26\u6237\u521B\u5EFA\uFF1A"  // ，请使用以下验证码完成账户创建：
         << "</p>"

         // ── Verification code box ──
         << "<div style=\"text-align:center;margin:28px 0;padding:24px 16px;"
         << "background:#0e141b;border-radius:8px;border:1px solid #2a3f5f;\">"
         << "<div style=\"font-size:11px;color:#556677;text-transform:uppercase;letter-spacing:3px;"
         << "margin-bottom:12px;\">"
         << u8"\u9A8C\u8BC1\u7801 / VERIFICATION CODE"  // 验证码
         << "</div>"
         << "<div style=\"font-size:42px;font-weight:700;letter-spacing:10px;color:#1a9fff;"
         << "font-family:Consolas,Monaco,monospace;\">"
         << code
         << "</div>"
         << "</div>"

         // ── Info section ──
         << "<div style=\"background:#1b2838;border-radius:6px;padding:16px 20px;margin:20px 0;\">"
         << "<table style=\"width:100%;border-collapse:collapse;\">"

         // Row 1: validity
         << "<tr>"
         << "<td style=\"padding:6px 0;color:#556677;font-size:13px;width:80px;\">"
         << u8"\u2022 \u6709\u6548\u671F" // • 有效期
         << "</td>"
         << "<td style=\"padding:6px 0;color:#c7d5e0;font-size:13px;\">"
         << u8"5 \u5206\u949F"  // 5 分钟
         << "</td></tr>"

         // Row 2: usage
         << "<tr>"
         << "<td style=\"padding:6px 0;color:#556677;font-size:13px;\">"
         << u8"\u2022 \u7528\u9014" // • 用途
         << "</td>"
         << "<td style=\"padding:6px 0;color:#c7d5e0;font-size:13px;\">"
         << u8"\u8D26\u6237\u6CE8\u518C\u9A8C\u8BC1"  // 账户注册验证
         << "</td></tr>"

         << "</table></div>"

         // ── Warning ──
         << "<div style=\"border-left:3px solid #c47e1a;padding:10px 16px;margin:20px 0;"
         << "background:rgba(196,126,26,0.08);border-radius:0 4px 4px 0;\">"
         << "<p style=\"font-size:13px;color:#c47e1a;margin:0;line-height:1.5;\">"
         << u8"\u26A0 \u9A8C\u8BC1\u7801\u4EC5\u7528\u4E8E\u672C\u4EBA\u64CD\u4F5C\uFF0C\u8BF7\u4E0D\u8981\u900F\u9732\u7ED9\u522B\u4EBA\u3002"  // ⚠ 验证码仅用于本人操作，请不要透露给别人。
         << "</p></div>"

         // ── Helpful text ──
         << "<p style=\"font-size:13px;color:#556677;margin:24px 0 0 0;line-height:1.5;\">"
         << u8"\u5982\u679C\u60A8\u6CA1\u6709\u8BF7\u6C42\u6B64\u9A8C\u8BC1\u7801\uFF0C\u8BF7\u5FFD\u7565\u6B64\u90AE\u4EF6\u3002"  // 如果您没有请求此验证码，请忽略此邮件。
         << "</p>"

         << "</div>" // end body

         // ── Footer ──
         << "<div style=\"padding:20px 32px;background:#0e141b;text-align:center;"
         << "border-top:1px solid #1b2838;\">"
         << "<p style=\"font-size:12px;color:#3d4f5f;margin:0;\">"
         << u8"\u00A9 2026 " << kAppName << " / SteamForge  \u00B7  "
         << u8"\u6B64\u90AE\u4EF6\u7531\u7CFB\u7EDF\u81EA\u52A8\u53D1\u9001\uFF0C\u8BF7\u52FF\u56DE\u590D"  // 此邮件由系统自动发送，请勿回复
         << "</p></div>"

         << "</div>"; // end outer container

    std::string subject = kAppName + std::string(u8" - \u6CE8\u518C\u9A8C\u8BC1\u7801"); // - 注册验证码
    std::string body = html.str();

    // Send async
    std::thread(SendEmailThread, toEmail, subject, body).detach();

    Log(LogLevel::Info, "Verification code generated for %s: %s", toEmail.c_str(), code.c_str());
    return true;
}

bool CheckVerifyCode(const std::string& email, const std::string& code) {
    std::lock_guard<std::mutex> lk(g_codeMtx);
    auto it = g_codes.find(email);
    if (it == g_codes.end()) return false;

    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - it->second.sentAt).count();

    if (elapsed > kCodeExpirySec) {
        g_codes.erase(it);
        return false; // expired
    }

    if (it->second.code == code) {
        g_codes.erase(it); // consume
        return true;
    }
    return false;
}

bool IsCodePending(const std::string& email) {
    std::lock_guard<std::mutex> lk(g_codeMtx);
    auto it = g_codes.find(email);
    if (it == g_codes.end()) return false;
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - it->second.sentAt).count();
    return elapsed <= kCodeExpirySec;
}

int GetCodeCooldown(const std::string& email) {
    std::lock_guard<std::mutex> lk(g_codeMtx);
    auto it = g_codes.find(email);
    if (it == g_codes.end()) return 0;
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - it->second.sentAt).count();
    int remain = kCodeCooldownSec - (int)elapsed;
    return remain > 0 ? remain : 0;
}

} // namespace sf
