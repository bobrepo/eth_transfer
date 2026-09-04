#ifndef FASTTRANSFER_TRANSFER_TYPES_H
#define FASTTRANSFER_TRANSFER_TYPES_H

#include <QString>
#include <cstdint>
#include <algorithm>

namespace FastTransfer {

enum class TransferStatus {
    Idle,
    Connecting,
    Handshaking,
    Offering,
    WaitingApproval,
    Transferring,
    Paused,
    Completed,
    Failed,
    Cancelled
};

enum class TransferDirection {
    Send,
    Receive
};

inline QString formatBytes(uint64_t bytes) {
    constexpr double KB = 1024.0;
    constexpr double MB = KB * 1024.0;
    constexpr double GB = MB * 1024.0;
    constexpr double TB = GB * 1024.0;

    double dBytes = static_cast<double>(bytes);
    if (dBytes >= TB) {
        return QString::asprintf("%.2f TB", dBytes / TB);
    } else if (dBytes >= GB) {
        return QString::asprintf("%.2f GB", dBytes / GB);
    } else if (dBytes >= MB) {
        return QString::asprintf("%.1f MB", dBytes / MB);
    } else if (dBytes >= KB) {
        return QString::asprintf("%.1f KB", dBytes / KB);
    }
    return QString("%1 B").arg(bytes);
}

inline QString formatSpeed(double bytesPerSec) {
    constexpr double MB = 1024.0 * 1024.0;
    constexpr double KB = 1024.0;

    if (bytesPerSec >= MB) {
        return QString::asprintf("%.1f MB/s", bytesPerSec / MB);
    } else if (bytesPerSec >= KB) {
        return QString::asprintf("%.1f KB/s", bytesPerSec / KB);
    }
    return QString::asprintf("%.0f B/s", bytesPerSec);
}

inline QString formatDuration(int64_t seconds) {
    if (seconds < 0) return QStringLiteral("Calculating...");
    int64_t hrs = seconds / 3600;
    int64_t mins = (seconds % 3600) / 60;
    int64_t secs = seconds % 60;

    if (hrs > 0) {
        return QString::asprintf("%lldh %02lldm %02llds", hrs, mins, secs);
    } else if (mins > 0) {
        return QString::asprintf("%lldm %02llds", mins, secs);
    }
    return QString::asprintf("%llds", secs);
}

struct TransferMetrics {
    double currentSpeedBps = 0.0;
    double averageSpeedBps = 0.0;
    double peakSpeedBps = 0.0;
    int64_t etaSeconds = -1;
    int64_t elapsedSeconds = 0;

    uint64_t currentFileTransferred = 0;
    uint64_t currentFileSize = 0;
    uint64_t totalTransferred = 0;
    uint64_t totalBytes = 0;

    uint64_t currentFileIndex = 0;
    uint64_t totalFiles = 0;
    QString currentFileName;

    double currentFileProgressRatio() const {
        if (currentFileSize == 0) return 1.0;
        return std::clamp(static_cast<double>(currentFileTransferred) / static_cast<double>(currentFileSize), 0.0, 1.0);
    }

    double overallProgressRatio() const {
        if (totalBytes == 0) return 0.0;
        return std::clamp(static_cast<double>(totalTransferred) / static_cast<double>(totalBytes), 0.0, 1.0);
    }

    QString formattedCurrentSpeed() const {
        return formatSpeed(currentSpeedBps);
    }

    QString formattedAverageSpeed() const {
        return formatSpeed(averageSpeedBps);
    }

    QString formattedPeakSpeed() const {
        return formatSpeed(peakSpeedBps);
    }

    QString formattedEta() const {
        return formatDuration(etaSeconds);
    }

    QString formattedElapsed() const {
        return formatDuration(elapsedSeconds);
    }
};

} // namespace FastTransfer

#endif // FASTTRANSFER_TRANSFER_TYPES_H
