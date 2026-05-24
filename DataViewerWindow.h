#ifndef DATAVIEWERWINDOW_H
#define DATAVIEWERWINDOW_H

#include <QMainWindow>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QValueAxis>
#include <QComboBox>
#include <QVector>
#include <QPushButton>
#include <QPointer>
#include <QThread>

struct TelemetryRecord {
    qint64 time;
    double current;
    double temp;
    double flow;
    double hall;
    double ioncurrent;
    double pressure;
};

class DataViewerWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit DataViewerWindow(const QString& filePath, QWidget *parent = nullptr);
    ~DataViewerWindow() = default;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onMetricChanged(int index);
    void resetZoom();

private:
    void loadData(const QString& filePath);
    void updateChart();
    void showCustomTooltip(const QPointF& mousePixelPos);

    QString m_filePath;
    QVector<TelemetryRecord> m_dataRecords;
    QVector<QPointF> m_currentPoints;

    QComboBox* m_metricCombo;
    QPushButton* m_resetZoomBtn;
    
    QChart* m_chart;
    QChartView* m_chartView;
    QLineSeries* m_series;
    QDateTimeAxis* m_axisX;
    QValueAxis* m_axisY;
};

#endif // DATAVIEWERWINDOW_H