#ifndef FASTTRANSFER_SPEED_GRAPH_H
#define FASTTRANSFER_SPEED_GRAPH_H

#include <QWidget>
#include <QVector>
#include <QPaintEvent>

namespace FastTransfer {

class SpeedGraph : public QWidget {
    Q_OBJECT
public:
    explicit SpeedGraph(QWidget* parent = nullptr);
    ~SpeedGraph() override = default;

    void addSpeedSample(double speedBps);
    void clear();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    static constexpr int MAX_POINTS = 80;
    QVector<double> m_points; // in MB/s
    double m_maxSpeedSeen = 120.0; // Default scale 120 MB/s for 1GbE
};

} // namespace FastTransfer

#endif // FASTTRANSFER_SPEED_GRAPH_H
