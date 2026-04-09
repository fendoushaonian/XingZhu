#include "core/memory_editor.h"
#include <tlhelp32.h>
#include <psapi.h>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <algorithm>

#pragma comment(lib, "psapi.lib")

namespace sf {

static MemoryEditor g_memoryEditor;

MemoryEditor& GetMemoryEditor() {
    return g_memoryEditor;
}

MemoryEditor::MemoryEditor() {}

MemoryEditor::~MemoryEditor() {
    DetachProcess();
}

bool MemoryEditor::AttachProcess(DWORD processId) {
    DetachProcess();

    processHandle_ = OpenProcess(
        PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION | PROCESS_QUERY_INFORMATION,
        FALSE, processId);

    if (!processHandle_) {
        return false;
    }

    attachedPid_ = processId;

    // 获取进程名
    char name[MAX_PATH] = {};
    HMODULE hMod;
    DWORD cbNeeded;
    if (EnumProcessModules(processHandle_, &hMod, sizeof(hMod), &cbNeeded)) {
        GetModuleBaseNameA(processHandle_, hMod, name, sizeof(name));
    }
    processName_ = name;

    return true;
}

bool MemoryEditor::AttachProcess(const std::string& processName) {
    auto processes = GetProcessList();
    for (const auto& [pid, name] : processes) {
        if (name == processName) {
            return AttachProcess(pid);
        }
    }
    return false;
}

void MemoryEditor::DetachProcess() {
    // 等待扫描完成
    if (isScanning_) {
        isScanning_ = false;
        if (scanThread_.joinable()) {
            scanThread_.join();
        }
    }

    if (processHandle_) {
        CloseHandle(processHandle_);
        processHandle_ = nullptr;
    }
    attachedPid_ = 0;
    processName_.clear();
    ResetScan();
}

std::vector<std::pair<DWORD, std::string>> MemoryEditor::GetProcessList() {
    std::vector<std::pair<DWORD, std::string>> result;

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return result;
    }

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);

    if (Process32FirstW(snapshot, &pe)) {
        do {
            // 转换宽字符到多字节
            char name[MAX_PATH];
            WideCharToMultiByte(CP_UTF8, 0, pe.szExeFile, -1, name, MAX_PATH, nullptr, nullptr);

            // 跳过系统进程
            if (pe.th32ProcessID > 4) {
                result.emplace_back(pe.th32ProcessID, name);
            }
        } while (Process32NextW(snapshot, &pe));
    }

    CloseHandle(snapshot);

    // 按名称排序
    std::sort(result.begin(), result.end(),
              [](const auto& a, const auto& b) { return a.second < b.second; });

    return result;
}

std::vector<MemoryRegion> MemoryEditor::GetMemoryRegions() {
    std::vector<MemoryRegion> regions;

    if (!processHandle_) return regions;

    MEMORY_BASIC_INFORMATION mbi;
    uintptr_t address = 0;

    while (VirtualQueryEx(processHandle_, (LPCVOID)address, &mbi, sizeof(mbi))) {
        if (mbi.State == MEM_COMMIT &&
            (mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY |
                           PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY))) {

            MemoryRegion region;
            region.baseAddress = (uintptr_t)mbi.BaseAddress;
            region.size = mbi.RegionSize;
            region.protection = mbi.Protect;
            region.isWritable = (mbi.Protect & (PAGE_READWRITE | PAGE_WRITECOPY |
                                                PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) != 0;
            region.isExecutable = (mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ |
                                                  PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) != 0;
            regions.push_back(region);
        }

        address = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
        if (address < (uintptr_t)mbi.BaseAddress) break; // 溢出检查
    }

    return regions;
}

void MemoryEditor::StartScan(ValueType type, ScanCondition condition,
                             const std::string& value1, const std::string& value2) {
    if (isScanning_ || !processHandle_) return;

    ResetScan();

    scanType_ = type;
    scanCondition_ = condition;
    scanValue1_ = StringToValue(value1, type);
    scanValue2_ = StringToValue(value2, type);
    isFirstScan_ = true;

    isScanning_ = true;
    scanProgress_ = 0.0f;

    scanThread_ = std::thread(&MemoryEditor::ScanThread, this);
}

void MemoryEditor::NextScan(ScanCondition condition,
                            const std::string& value1, const std::string& value2) {
    if (isScanning_ || !processHandle_ || results_.empty()) return;

    scanCondition_ = condition;
    scanValue1_ = StringToValue(value1, scanType_);
    scanValue2_ = StringToValue(value2, scanType_);
    isFirstScan_ = false;

    isScanning_ = true;
    scanProgress_ = 0.0f;

    scanThread_ = std::thread(&MemoryEditor::ScanThread, this);
}

void MemoryEditor::ResetScan() {
    if (isScanning_) {
        isScanning_ = false;
        if (scanThread_.joinable()) {
            scanThread_.join();
        }
    }

    std::lock_guard<std::mutex> lock(resultsMutex_);
    results_.clear();
    isFirstScan_ = true;
}

size_t MemoryEditor::GetResultCount() const {
    return results_.size();
}

std::vector<MemoryResult> MemoryEditor::GetResults(size_t offset, size_t count) {
    std::lock_guard<std::mutex> lock(resultsMutex_);

    std::vector<MemoryResult> result;
    if (offset >= results_.size()) return result;

    size_t end = (std::min)(offset + count, results_.size());
    for (size_t i = offset; i < end; i++) {
        // 更新当前值
        MemoryResult r = results_[i];
        ReadMemory(r.address, r.value.data(), r.value.size());
        r.displayValue = ValueToString(r.value, scanType_);
        result.push_back(r);
    }

    return result;
}

bool MemoryEditor::ReadMemory(uintptr_t address, void* buffer, size_t size) {
    if (!processHandle_) return false;

    SIZE_T bytesRead;
    return ReadProcessMemory(processHandle_, (LPCVOID)address, buffer, size, &bytesRead) &&
           bytesRead == size;
}

bool MemoryEditor::WriteMemory(uintptr_t address, const void* data, size_t size) {
    if (!processHandle_) return false;

    SIZE_T bytesWritten;
    return WriteProcessMemory(processHandle_, (LPVOID)address, data, size, &bytesWritten) &&
           bytesWritten == size;
}

void MemoryEditor::AddLockedAddress(const LockedAddress& addr) {
    std::lock_guard<std::mutex> lock(locksMutex_);
    lockedAddresses_.push_back(addr);
}

void MemoryEditor::RemoveLockedAddress(size_t index) {
    std::lock_guard<std::mutex> lock(locksMutex_);
    if (index < lockedAddresses_.size()) {
        lockedAddresses_.erase(lockedAddresses_.begin() + index);
    }
}

void MemoryEditor::SetLockedEnabled(size_t index, bool enabled) {
    std::lock_guard<std::mutex> lock(locksMutex_);
    if (index < lockedAddresses_.size()) {
        lockedAddresses_[index].enabled = enabled;
    }
}

void MemoryEditor::UpdateLockedValue(size_t index, const std::vector<uint8_t>& value) {
    std::lock_guard<std::mutex> lock(locksMutex_);
    if (index < lockedAddresses_.size()) {
        lockedAddresses_[index].value = value;
    }
}

void MemoryEditor::UpdateLocks() {
    if (!processHandle_) return;

    DWORD now = GetTickCount();
    if (now - lastLockUpdate_ < 10) return; // 100Hz更新
    lastLockUpdate_ = now;

    std::lock_guard<std::mutex> lock(locksMutex_);
    for (auto& addr : lockedAddresses_) {
        if (addr.enabled && !addr.value.empty()) {
            WriteMemory(addr.address, addr.value.data(), addr.value.size());
        }
    }
}

void MemoryEditor::ScanThread() {
    if (isFirstScan_) {
        // 首次扫描：扫描所有内存区域
        auto regions = GetMemoryRegions();
        size_t totalSize = 0;
        for (const auto& r : regions) totalSize += r.size;

        size_t scannedSize = 0;
        std::vector<MemoryResult> newResults;

        for (const auto& region : regions) {
            if (!isScanning_) break;

            // 分块读取
            const size_t chunkSize = 64 * 1024; // 64KB
            std::vector<uint8_t> buffer(chunkSize);

            for (size_t offset = 0; offset < region.size && isScanning_; offset += chunkSize) {
                size_t readSize = (std::min)(chunkSize, region.size - offset);
                uintptr_t addr = region.baseAddress + offset;

                SIZE_T bytesRead;
                if (ReadProcessMemory(processHandle_, (LPCVOID)addr, buffer.data(), readSize, &bytesRead)) {
                    size_t valueSize = GetValueSize(scanType_);

                    for (size_t i = 0; i + valueSize <= bytesRead; i++) {
                        if (MatchValue(buffer.data() + i, valueSize)) {
                            MemoryResult result;
                            result.address = addr + i;
                            result.value.assign(buffer.data() + i, buffer.data() + i + valueSize);
                            result.displayValue = ValueToString(result.value, scanType_);
                            newResults.push_back(result);

                            // 限制结果数量
                            if (newResults.size() >= 100000) {
                                break;
                            }
                        }
                    }
                }

                scannedSize += readSize;
                scanProgress_ = (float)scannedSize / totalSize;

                if (newResults.size() >= 100000) break;
            }

            if (newResults.size() >= 100000) break;
        }

        std::lock_guard<std::mutex> lock(resultsMutex_);
        results_ = std::move(newResults);
    } else {
        // 后续扫描：只扫描已有结果
        std::vector<MemoryResult> newResults;
        size_t total = results_.size();
        size_t processed = 0;

        for (const auto& oldResult : results_) {
            if (!isScanning_) break;

            size_t valueSize = GetValueSize(scanType_);
            std::vector<uint8_t> currentValue(valueSize);

            if (ReadMemory(oldResult.address, currentValue.data(), valueSize)) {
                bool match = false;

                switch (scanCondition_) {
                    case ScanCondition::Exact:
                        match = MatchValue(currentValue.data(), valueSize);
                        break;
                    case ScanCondition::Increased:
                        match = CompareValues(oldResult.value.data(), currentValue.data(), valueSize) < 0;
                        break;
                    case ScanCondition::Decreased:
                        match = CompareValues(oldResult.value.data(), currentValue.data(), valueSize) > 0;
                        break;
                    case ScanCondition::Changed:
                        match = memcmp(oldResult.value.data(), currentValue.data(), valueSize) != 0;
                        break;
                    case ScanCondition::Unchanged:
                        match = memcmp(oldResult.value.data(), currentValue.data(), valueSize) == 0;
                        break;
                    default:
                        match = MatchValue(currentValue.data(), valueSize);
                        break;
                }

                if (match) {
                    MemoryResult result;
                    result.address = oldResult.address;
                    result.value = currentValue;
                    result.displayValue = ValueToString(currentValue, scanType_);
                    newResults.push_back(result);
                }
            }

            processed++;
            scanProgress_ = (float)processed / total;
        }

        std::lock_guard<std::mutex> lock(resultsMutex_);
        results_ = std::move(newResults);
    }

    isScanning_ = false;
    scanProgress_ = 1.0f;
}

bool MemoryEditor::MatchValue(const uint8_t* data, size_t size) {
    if (scanValue1_.empty()) return false;

    switch (scanCondition_) {
        case ScanCondition::Exact:
            return memcmp(data, scanValue1_.data(), size) == 0;

        case ScanCondition::Greater:
            return CompareValues(scanValue1_.data(), data, size) < 0;

        case ScanCondition::Less:
            return CompareValues(scanValue1_.data(), data, size) > 0;

        case ScanCondition::Between:
            return CompareValues(scanValue1_.data(), data, size) <= 0 &&
                   CompareValues(data, scanValue2_.data(), size) <= 0;

        case ScanCondition::Unknown:
            return true; // 首次扫描时记录所有值

        default:
            return memcmp(data, scanValue1_.data(), size) == 0;
    }
}

int MemoryEditor::CompareValues(const uint8_t* a, const uint8_t* b, size_t size) {
    switch (scanType_) {
        case ValueType::Int8:
            return (int)*(int8_t*)a - (int)*(int8_t*)b;
        case ValueType::Int16:
            return (int)*(int16_t*)a - (int)*(int16_t*)b;
        case ValueType::Int32:
            return *(int32_t*)a < *(int32_t*)b ? -1 : (*(int32_t*)a > *(int32_t*)b ? 1 : 0);
        case ValueType::Int64:
            return *(int64_t*)a < *(int64_t*)b ? -1 : (*(int64_t*)a > *(int64_t*)b ? 1 : 0);
        case ValueType::Float:
            return *(float*)a < *(float*)b ? -1 : (*(float*)a > *(float*)b ? 1 : 0);
        case ValueType::Double:
            return *(double*)a < *(double*)b ? -1 : (*(double*)a > *(double*)b ? 1 : 0);
        default:
            return memcmp(a, b, size);
    }
}

std::string MemoryEditor::ValueToString(const std::vector<uint8_t>& data, ValueType type) {
    if (data.empty()) return "";

    std::ostringstream ss;

    switch (type) {
        case ValueType::Int8:
            ss << (int)*(int8_t*)data.data();
            break;
        case ValueType::Int16:
            ss << *(int16_t*)data.data();
            break;
        case ValueType::Int32:
            ss << *(int32_t*)data.data();
            break;
        case ValueType::Int64:
            ss << *(int64_t*)data.data();
            break;
        case ValueType::Float:
            ss << std::fixed << std::setprecision(2) << *(float*)data.data();
            break;
        case ValueType::Double:
            ss << std::fixed << std::setprecision(4) << *(double*)data.data();
            break;
        case ValueType::String:
            ss << std::string((char*)data.data(), data.size());
            break;
        case ValueType::ByteArray:
            for (size_t i = 0; i < data.size(); i++) {
                if (i > 0) ss << " ";
                ss << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << (int)data[i];
            }
            break;
    }

    return ss.str();
}

std::vector<uint8_t> MemoryEditor::StringToValue(const std::string& str, ValueType type) {
    std::vector<uint8_t> result;
    if (str.empty()) return result;

    try {
        switch (type) {
            case ValueType::Int8: {
                int8_t v = (int8_t)std::stoi(str);
                result.resize(sizeof(v));
                memcpy(result.data(), &v, sizeof(v));
                break;
            }
            case ValueType::Int16: {
                int16_t v = (int16_t)std::stoi(str);
                result.resize(sizeof(v));
                memcpy(result.data(), &v, sizeof(v));
                break;
            }
            case ValueType::Int32: {
                int32_t v = std::stoi(str);
                result.resize(sizeof(v));
                memcpy(result.data(), &v, sizeof(v));
                break;
            }
            case ValueType::Int64: {
                int64_t v = std::stoll(str);
                result.resize(sizeof(v));
                memcpy(result.data(), &v, sizeof(v));
                break;
            }
            case ValueType::Float: {
                float v = std::stof(str);
                result.resize(sizeof(v));
                memcpy(result.data(), &v, sizeof(v));
                break;
            }
            case ValueType::Double: {
                double v = std::stod(str);
                result.resize(sizeof(v));
                memcpy(result.data(), &v, sizeof(v));
                break;
            }
            case ValueType::String:
                result.assign(str.begin(), str.end());
                break;
            case ValueType::ByteArray: {
                std::istringstream iss(str);
                std::string byte;
                while (iss >> byte) {
                    result.push_back((uint8_t)std::stoi(byte, nullptr, 16));
                }
                break;
            }
        }
    } catch (...) {
        result.clear();
    }

    return result;
}

size_t MemoryEditor::GetValueSize(ValueType type) {
    switch (type) {
        case ValueType::Int8: return 1;
        case ValueType::Int16: return 2;
        case ValueType::Int32: return 4;
        case ValueType::Int64: return 8;
        case ValueType::Float: return 4;
        case ValueType::Double: return 8;
        default: return 4;
    }
}

const char* MemoryEditor::GetValueTypeName(ValueType type) {
    switch (type) {
        case ValueType::Int8: return "1 Byte";
        case ValueType::Int16: return "2 Bytes";
        case ValueType::Int32: return "4 Bytes";
        case ValueType::Int64: return "8 Bytes";
        case ValueType::Float: return "Float";
        case ValueType::Double: return "Double";
        case ValueType::String: return "String";
        case ValueType::ByteArray: return "Byte Array";
        default: return "Unknown";
    }
}

} // namespace sf
