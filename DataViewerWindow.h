#ifndef DATAVIEWERWINDOW_H
#define DATAVIEWERWINDOW_H

#include <QMainWindow>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QValueAxis>
#include <QComboBox>
#include <QJsonObject>
#include <QVector>
#include <QPushButton>

class DataViewerWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit DataViewerWindow(const QString& filePath, QWidget *parent = nullptr);
    ~DataViewerWindow() = default;

private slots:
    void onMetricChanged(int index);
    void onHovered(const QPointF &point, bool state);
    void resetZoom();

private:
    void loadData(const QString& filePath);
    void updateChart();

    QString m_filePath;
    QVector<QJsonObject> m_dataRecords;

    QComboBox* m_metricCombo;
    QPushButton* m_resetZoomBtn;
    
    QChart* m_chart;
    QChartView* m_chartView;
    QLineSeries* m_series;
    QDateTimeAxis* m_axisX;
    QValueAxis* m_axisY;
};

#endif // DATAVIEWERWINDOW_H