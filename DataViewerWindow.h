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
#include <QTimer>
#include <QLabel>

struct TelemetryRecord {
    qint64 time;
    std::optional<double> current;
    std::optional<double> temp;
    std::optional<double> flow;
    std::optional<double> hall;
    std::optional<double> ioncurrent;
    std::optional<double> iondetect;
    std::optional<double> pressure;
};

class DataViewerWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit DataViewerWindow(const QString& filePath, QWidget *parent = nullptr);
    ~DataViewerWindow();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onMetricChanged(int index);
    void resetZoom();

private:
    void loadAndShowData(const QString& filePath);
    void updateChart();
    void showCustomTooltip(const QPointF& mousePixelPos);

    QPoint m_lastMousePos;
    QLabel* m_tooltipWidget = nullptr;
    QTimer* m_tooltipTimer = nullptr;

    QString m_filePath;
    QVector<TelemetryRecord> m_dataRecords;

    QVector<QPointF> m_currentPoints1;
    QVector<QPointF> m_currentPoints2;

    QComboBox* m_metricCombo1;
    QComboBox* m_metricCombo2;
    QPushButton* m_resetZoomBtn;
    
    QChart* m_chart;
    QChartView* m_chartView;
    QList<QLineSeries*> m_seriesList;
    QDateTimeAxis* m_axisX;
    QValueAxis* m_axisY;
    QValueAxis* m_axisY2;

    QAbstractSeries* m_seriesMetric1 = nullptr;
    QAbstractSeries* m_seriesMetric2 = nullptr;

    std::shared_ptr<std::atomic<bool>> cancelFlag;
};

#endif // DATAVIEWERWINDOW_H