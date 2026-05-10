#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFile>
#include <QTextStream>
#include <QJsonDocument>
#include <QToolTip>
#include <QDateTime>
#include <QMouseEvent>
#include <QCursor>
#include "DataViewerWindow.h"

// Кастомный QChartView для поддержки перетаскивания (Pan) правой кнопкой мыши
class ChartViewPanZoom : public QChartView {
    bool m_isPanning = false;
    QPoint m_lastMousePos;
public:
    ChartViewPanZoom(QChart *chart, QWidget *parent = nullptr) : QChartView(chart, parent) {
        // Включаем зум выделением (левая кнопка)
        setRubberBand(QChartView::RectangleRubberBand);
        setRenderHint(QPainter::Antialiasing);
    }
protected:
    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() == Qt::RightButton) {
            m_isPanning = true;
            m_lastMousePos = event->pos();
            setCursor(Qt::ClosedHandCursor);
        } else {
            QChartView::mousePressEvent(event);
        }
    }
    void mouseMoveEvent(QMouseEvent *event) override {
        if (m_isPanning) {
            QPoint delta = event->pos() - m_lastMousePos;
            chart()->scroll(-delta.x(), delta.y());
            m_lastMousePos = event->pos();
        } else {
            QChartView::mouseMoveEvent(event);
        }
    }
    void mouseReleaseEvent(QMouseEvent *event) override {
        if (event->button() == Qt::RightButton) {
            m_isPanning = false;
            setCursor(Qt::ArrowCursor);
        }
        QChartView::mouseReleaseEvent(event);
    }
};

DataViewerWindow::DataViewerWindow(const QString& filePath, QWidget *parent) 
    : QMainWindow(parent), m_filePath(filePath) {
    
    // При закрытии окна память автоматически освободится
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle("Просмотр логов данных: " + filePath.split('/').last());
    resize(900, 600);

    auto *centralWidget = new QWidget(this);
    auto *mainLayout = new QVBoxLayout(centralWidget);

    // Верхняя панель управления
    auto *topLayout = new QHBoxLayout();
    m_metricCombo = new QComboBox();
    m_metricCombo->addItems({
        "Ток (PowerControl)", 
        "Температура (CoolControl)", 
        "Поток (CoolControl)", 
        "Датчик Холла (SensorControl)", 
        "Цилиндр Фарадея (SensorControl)", 
        "Давление/Вакуум (SensorControl)"
    });
    connect(m_metricCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DataViewerWindow::onMetricChanged);

    m_resetZoomBtn = new QPushButton("Сбросить масштаб");
    connect(m_resetZoomBtn, &QPushButton::clicked, this, &DataViewerWindow::resetZoom);

    topLayout->addWidget(m_metricCombo);
    topLayout->addWidget(m_resetZoomBtn);
    topLayout->addStretch();
    mainLayout->addLayout(topLayout);

    // Настройка графика
    m_chart = new QChart();
    m_chart->legend()->hide();
    m_series = new QLineSeries();
    m_chart->addSeries(m_series);

    // Обработка наведения мыши
    connect(m_series, &QLineSeries::hovered, this, &DataViewerWindow::onHovered);

    m_axisX = new QDateTimeAxis();
    m_axisX->setFormat("HH:mm:ss");
    m_axisX->setTitleText("Время");
    m_chart->addAxis(m_axisX, Qt::AlignBottom);
    m_series->attachAxis(m_axisX);

    m_axisY = new QValueAxis();
    m_axisY->setTitleText("Значение");
    m_chart->addAxis(m_axisY, Qt::AlignLeft);
    m_series->attachAxis(m_axisY);

    m_chartView = new ChartViewPanZoom(m_chart);
    mainLayout->addWidget(m_chartView);
    setCentralWidget(centralWidget);

    // Загрузка данных
    loadData(m_filePath);
    updateChart();
}

void DataViewerWindow::loadData(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine();
        QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8());
        if (doc.isObject()) {
            m_dataRecords.append(doc.object());
        }
    }
}

void DataViewerWindow::updateChart() {
    if (m_dataRecords.isEmpty()) return;

    m_series->clear();
    int metricIndex = m_metricCombo->currentIndex();
    
    qreal minY = std::numeric_limits<qreal>::max();
    qreal maxY = std::numeric_limits<qreal>::lowest();

    for (const QJsonObject& obj : qAsConst(m_dataRecords)) {
        qint64 timeMs = static_cast<qint64>(obj["time"].toDouble());
        qreal y = 0.0;

        switch (metricIndex) {
            case 0: y = obj["powercontroller"].toObject()["current"].toDouble(); break;
            case 1: y = obj["coolcontroller"].toObject()["temp"].toDouble(); break;
            case 2: y = obj["coolcontroller"].toObject()["flow"].toDouble(); break;
            case 3: y = obj["sensorcontroller"].toObject()["hall"].toDouble(); break;
            case 4: y = obj["sensorcontroller"].toObject()["ioncurrent"].toDouble(); break;
            case 5: y = obj["sensorcontroller"].toObject()["pressure"].toDouble(); break;
        }

        m_series->append(timeMs, y);
        if (y < minY) minY = y;
        if (y > maxY) maxY = y;
    }

    // Масштабируем оси на весь файл
    m_axisX->setRange(QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(m_dataRecords.first()["time"].toDouble())), 
                      QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(m_dataRecords.last()["time"].toDouble())));
    
    // Даем отступ по Y (10%), чтобы график не прилипал к границами
    qreal margin = (maxY - minY) * 0.1;
    if (margin == 0) margin = 1.0; 
    m_axisY->setRange(minY - margin, maxY + margin);
}

void DataViewerWindow::onMetricChanged(int index) {
    Q_UNUSED(index);
    updateChart();
}

void DataViewerWindow::resetZoom() {
    m_chart->zoomReset();
    updateChart(); // Возвращаем к изначальным границам
}

void DataViewerWindow::onHovered(const QPointF &point, bool state) {
    if (state) {
        // 1. Получаем все реальные точки текущего графика
        const QVector<QPointF> points = m_series->pointsVector();
        if (points.isEmpty()) return;

        // 2. Ищем ближайшую точку по оси X (времени)
        qreal minDistance = std::numeric_limits<qreal>::max();
        QPointF closestPoint = points.first();

        for (const QPointF &p : points) {
            qreal distance = std::abs(p.x() - point.x());
            if (distance < minDistance) {
                minDistance = distance;
                closestPoint = p;
            }
        }

        // 3. Выводим ВРЕМЯ и ЗНАЧЕНИЕ
        QDateTime dt = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(closestPoint.x()));
        
        QString text = QString("Время: %1\nЗначение: %2")
                       .arg(dt.toString("HH:mm:ss.zzz"))
                       .arg(closestPoint.y(), 0, 'f', 4);
                       
        QToolTip::showText(QCursor::pos(), text, m_chartView);
    } else {
        QToolTip::hideText();
    }
}
