#pragma once
#include <mutex>
#include <string>

// Only the scanner's configuration surface. This does not test production Config;
// reads return copies, matching its synchronized optional interface.
template<class T> class ScanTestOption
{
  public:
    explicit ScanTestOption(T value) : value_(value) {}
    T value_or_default() const { std::lock_guard<std::mutex> lock(mutex_); return value_; }
    void set(T value) { std::lock_guard<std::mutex> lock(mutex_); value_ = value; }
  private:
    mutable std::mutex mutex_;
    T value_;
};

class Config
{
  public:
    static Config* Instance() { static Config config; return &config; }
    struct Runtime { bool enabled; };
    Runtime GetDlssNrRuntimeSnapshot() const { return {true}; }
    ScanTestOption<unsigned int> DlssNrWhitePointSource {2};
    ScanTestOption<bool> DlssNrScanExposure {false};
    ScanTestOption<std::string> DlssNrScanAnchors {std::string()};
    ScanTestOption<float> DlssNrScanAnchorValue {0.0f};
    ScanTestOption<float> DlssNrScanAnchorWhitePoint {0.0f};
};
