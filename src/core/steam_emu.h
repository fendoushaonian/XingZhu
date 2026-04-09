#pragma once
#include <string>
#include <functional>

namespace sf {

// Steam Emulator (Goldberg) 管理
class SteamEmu {
public:
    static SteamEmu& Get();

    // 检查 emulator 是否已安装
    bool IsAvailable() const;

    // 确保已安装（自动下载）
    void EnsureInstalled(std::function<void(bool success, const std::string& error)> callback);

    // 是否正在安装
    bool IsSettingUp() const { return settingUp_; }

    // 为游戏应用 Steam Emulator
    // 会备份原始 steam_api.dll 并替换为 emulator 版本
    // 同时创建必要的配置文件
    bool ApplyToGame(const std::string& gameDir, const std::string& appId, const std::string& gameName);

    // 检查游戏是否需要 Steam DRM（有 steam_api.dll）
    bool GameNeedsSteamDRM(const std::string& gameDir);

    // 获取 emulator 目录
    std::string GetEmuDir() const { return emuDir_; }

private:
    SteamEmu();
    ~SteamEmu() = default;

    void DownloadEmulator(std::function<void(bool, const std::string&)> callback);

    std::string emuDir_;
    bool settingUp_ = false;
};

} // namespace sf
