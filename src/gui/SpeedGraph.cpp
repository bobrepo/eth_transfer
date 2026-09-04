#include "SpeedGraph.h"
#include <QPainter>
#include <QPainterPath>
#include <algorithm>

namespace FastTransfer {

SpeedGraph::SpeedGraph(QWidget* parent)
    : QWidget(parent) {
    setMinimumHeight(120);
    m_points.fill(0.0, MAX_POINTS);
}

void SpeedGraph::addSpeedSample(double speedBps) {
    double speedMbs = speedBps / (1024.0 * 1024.0);
    if (m_points.size() >= MAX_POINTS) {
        m_points.removeFirst();
    }
    m_points.append(speedMbs);

    // Track dynamic max
    m_maxSpeedSeen = std::max(100.0, speedMbs * 1.15);

    update();
}

void SpeedGraph::clear() {
    m_points.fill(0.0, MAX_POINTS);
    m_maxSpeedSeen = 120.0;
    update();
}

void SpeedGraph::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    int w = width();
    int h = height();

    // Background card
    painter.setPen(QPen(QColor("#232835"), 1));
    painter.setBrush(QColor("#13161D"));
    painter.drawRoundedRect(0, 0, w - 1, h - 1, 8, 8);

    if (w < 40 || h < 40) return;

    int leftMargin = 55;
    int rightMargin = 15;
    int topMargin = 15;
    int bottomMargin = 20;

    int plotW = w - leftMargin - rightMargin;
    int plotH = h - topMargin - bottomMargin;
    if (plotW <= 0 || plotH <= 0) return;

    // Grid lines
    painter.setPen(QPen(QColor("#202635"), 1, Qt::DashLine));
    painter.setFont(QFont("Segoe UI", 8));

    for (int step = 0; step <= 4; ++step) {
        double val = m_maxSpeedSeen * (step / 4.0);
        int y = topMargin + plotH - static_cast<int>((val / m_maxSpeedSeen) * plotH);

        painter.drawLine(leftMargin, y, leftMargin + plotW, y);

        // Label
        painter.setPen(QColor("#64748B"));
        QString lbl = QString("%1").arg(val, 0, 'f', 0);
        painter.drawText(8, y + 4, 42, 14, Qt::AlignRight, lbl);
        painter.setPen(QPen(QColor("#202635"), 1, Qt::DashLine));
    }

    // Units label
    painter.setPen(QColor("#475569"));
    painter.drawText(8, topMargin - 2, "MB/s");

    if (m_points.isEmpty()) return;

    // Build curve path
    QPainterPath linePath;
    QPainterPath fillPath;

    int n = m_points.size();
    double stepX = static_cast<double>(plotW) / (MAX_POINTS - 1);

    for (int i = 0; i < n; ++i) {
        double px = leftMargin + i * stepX;
        double clampedVal = std::clamp(m_points[i], 0.0, m_maxSpeedSeen);
        double py = topMargin + plotH - (clampedVal / m_maxSpeedSeen) * plotH;

        if (i == 0) {
            linePath.moveTo(px, py);
            fillPath.moveTo(px, topMargin + plotH);
            fillPath.lineTo(px, py);
        } else {
            linePath.lineTo(px, py);
            fillPath.lineTo(px, py);
        }
    }

    // Close fill path
    fillPath.lineTo(leftMargin + (n - 1) * stepX, topMargin + plotH);
    fillPath.closeSubpath();

    // Fill under curve
    QLinearGradient grad(0, topMargin, 0, topMargin + plotH);
    grad.setColorAt(0.0, QColor(37, 99, 235, 120));
    grad.setColorAt(1.0, QColor(37, 99, 235, 5));
    painter.fillPath(fillPath, grad);

    // Stroke curve line
    QPen linePen(QColor("#38BDF8"), 2.0);
    linePen.setCapStyle(Qt::RoundCap);
    painter.strokePath(linePath, linePen);

    // Draw last point dot
    if (n > 0) {
        double lastX = leftMargin + (n - 1) * stepX;
        double lastVal = std::clamp(m_points.last(), 0.0, m_maxSpeedSeen);
        double lastY = topMargin + plotH - (lastVal / m_maxSpeedSeen) * plotH;

        painter.setPen(QPen(QColor("#FFFFFF"), 2));
        painter.setBrush(QColor("#0284C7"));
        painter.drawEllipse(QPointF(lastX, lastY), 4, 4);
    }
}

} // namespace FastTransfer
