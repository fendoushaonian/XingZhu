#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <vector>
#include <string>
#include <functional>
#include <atomic>
#include <thread>
#include <mutex>

namespace sf {

// 内存搜索结果
struct MemoryResult {
    uintptr_t address;
    std::vector<uint8_t> value;
    std::string displayValue;
};

// 数值类型
enum class ValueType {
    Int8,
    Int16,
    Int32,
    Int64,
    Float,
    Double,
    String,
    ByteArray
};

// 搜索条件
enum class ScanCondition {
    Exact,          // 精确值
    Greater,        // 大于
    Less,           // 小于
    Between,        // 范围内
    Unknown,        // 未知初始值
    Increased,      // 增加了
    Decreased,      // 减少了
    Changed,        // 变化了
    Unchanged       // 未变化
};

// 内存区域信息
struct MemoryRegion {
    uintptr_t baseAddress;
    size_t size;
    DWORD protection;
    bool isWritable;
    bool isExecutable;
};

// 锁定的地址
struct LockedAddress {
    uintptr_t address;
    ValueType type;
    std::vector<uint8_t> value;
    std::string name;
    bool enabled;
};

// 游戏内存修改器核心
class MemoryEditor {
public:
    MemoryEditor();
    ~MemoryEditor();

    // 附加到进程
    bool AttachProcess(DWORD processId);
    bool AttachProcess(const std::string& processName);
    void DetachProcess();
    bool IsAttached() const { return processHandle_ != nullptr; }
    DWORD GetAttachedPid() const { return attachedPid_; }
    std::string GetProcessName() const { return processName_; }

    // 内存扫描
    void StartScan(ValueType type, ScanCondition condition,
                   const std::string& value1, const std::string& value2 = "");
    void NextScan(ScanCondition condition,
                  const std::string& value1, const std::string& value2 = "");
    void ResetScan();

    // 扫描状态
    bool IsScanning() const { return isScanning_; }
    float GetScanProgress() const { return scanProgress_; }
    size_t GetResultCount() const;
    std::vector<MemoryResult> GetResults(size_t offset, size_t count);

    // 内存读写
    bool ReadMemory(uintptr_t address, void* buffer, size_t size);
    bool WriteMemory(uintptr_t address, const void* data, size_t size);

    // 便捷读写
    template<typename T>
    T ReadValue(uintptr_t address) {
        T value{};
        ReadMemory(address, &value, sizeof(T));
        return value;
    }

    template<typename T>
    bool WriteValue(uintptr_t address, T value) {
        return WriteMemory(address, &value, sizeof(T));
    }

    // 地址锁定
    void AddLockedAddress(const LockedAddress& addr);
    void RemoveLockedAddress(size_t index);
    void SetLockedEnabled(size_t index, bool enabled);
    void UpdateLockedValue(size_t index, const std::vector<uint8_t>& value);
    std::vector<LockedAddress>& GetLockedAddresses() { return lockedAddresses_; }

    // 锁定更新（在主循环调用）
    void UpdateLocks();

    // 获取进程列表
    static std::vector<std::pair<DWORD, std::string>> GetProcessList();

    // 获取内存区域
    std::vector<MemoryRegion> GetMemoryRegions();

    // 工具函数
    static std::string ValueToString(const std::vector<uint8_t>& data, ValueType type);
    static std::vector<uint8_t> StringToValue(const std::string& str, ValueType type);
    static size_t GetValueSize(ValueType type);
    static const char* GetValueTypeName(ValueType type);

private:
    void ScanThread();
    void ScanRegion(const MemoryRegion& region);
    bool MatchValue(const uint8_t* data, size_t size);
    int CompareValues(const uint8_t* oldData, const uint8_t* newData, size_t size);

    HANDLE processHandle_ = nullptr;
    DWORD attachedPid_ = 0;
    std::string processName_;

    // 扫描状态
    std::atomic<bool> isScanning_{false};
    std::atomic<float> scanProgress_{0.0f};
    std::thread scanThread_;

    // 扫描参数
    ValueType scanType_ = ValueType::Int32;
    ScanCondition scanCondition_ = ScanCondition::Exact;
    std::vector<uint8_t> scanValue1_;
    std::vector<uint8_t> scanValue2_;
    bool isFirstScan_ = true;

    // 扫描结果
    std::vector<MemoryResult> results_;
    std::mutex resultsMutex_;

    // 锁定地址
    std::vector<LockedAddress> lockedAddresses_;
    std::mutex locksMutex_;
    DWORD lastLockUpdate_ = 0;
};

// 全局访问
MemoryEditor& GetMemoryEditor();

} // namespace sf
