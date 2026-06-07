#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFile>
#include <QTextStream>
#include <QToolTip>
#include <QDateTime>
#include <QMouseEvent>
#include <QCursor>
#include <QLabel>
#include "DataViewerWindow.h"

class ChartViewPanZoom : public QChartView {
    bool m_isPanning = false;
    QPoint m_lastMousePos;
public:
    ChartViewPanZoom(QChart *chart, QWidget *parent = nullptr) : QChartView(chart, parent) {
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
    
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle("Просмотр логов данных: " + filePath.split('/').last());
    resize(1000, 700);

    cancelFlag = std::make_shared<std::atomic<bool>>(false);

    auto *centralWidget = new QWidget(this);
    auto *mainLayout = new QVBoxLayout(centralWidget);

    auto *topLayout = new QHBoxLayout();

    auto *comboLayout1 = new QHBoxLayout();
    QStringList metrics = {
        "Ток (PowerControl)", 
        "Температура (CoolControl)", 
        "Поток (CoolControl)", 
        "Датчик Холла (SensorControl)", 
        "Цилиндр Фарадея 1 (SensorControl)", 
        "Цилиндр Фарадея 2 (SensorControl)", 
        "Давление/Вакуум (SensorControl)"
    };
    QLabel* label1 = new QLabel("График 1 (<b style='color:#2196F3;'>Синий</b>):");
    label1->setTextFormat(Qt::RichText);
    comboLayout1->addWidget(label1);
    m_metricCombo1 = new QComboBox();
    m_metricCombo1->addItems(metrics);
    connect(m_metricCombo1, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DataViewerWindow::onMetricChanged);
    comboLayout1->addWidget(m_metricCombo1);

    auto *comboLayout2 = new QHBoxLayout();
    QLabel* label2 = new QLabel("График 2 (<b style='color:#F44336;'>Красный</b>):");
    label2->setTextFormat(Qt::RichText);
    comboLayout2->addWidget(label2);
    m_metricCombo2 = new QComboBox();
    QStringList metricsWithEmpty = {"Пусто"};
    metricsWithEmpty.append(metrics);
    m_metricCombo2->addItems(metricsWithEmpty);
    connect(m_metricCombo2, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DataViewerWindow::onMetricChanged);
    comboLayout2->addWidget(m_metricCombo2);

    m_resetZoomBtn = new QPushButton("Сбросить масштаб");
    connect(m_resetZoomBtn, &QPushButton::clicked, this, &DataViewerWindow::resetZoom);

    topLayout->addLayout(comboLayout1);
    topLayout->addSpacing(20);
    topLayout->addLayout(comboLayout2);
    topLayout->addWidget(m_resetZoomBtn);
    topLayout->addStretch();
    mainLayout->addLayout(topLayout);

    m_chart = new QChart();
    m_chart->legend()->hide();

    m_chart->setCacheMode(QChart::DeviceCoordinateCache);

    m_axisX = nullptr;
    m_axisY = nullptr;
    m_axisY2 = nullptr;

    m_chartView = new ChartViewPanZoom(m_chart);

    m_chartView->viewport()->installEventFilter(this);
    m_chartView->setMouseTracking(true);

    mainLayout->addWidget(m_chartView);
    setCentralWidget(centralWidget);

    loadAndShowData(m_filePath);
}

DataViewerWindow::~DataViewerWindow() {
    *cancelFlag = true;
}

void DataViewerWindow::loadAndShowData(const QString& filePath) {
    setWindowTitle("Загрузка данных... Пожалуйста, подождите.");
    m_metricCombo1->setEnabled(false);
    m_metricCombo2->setEnabled(false);
    m_resetZoomBtn->setEnabled(false);

    std::shared_ptr<std::atomic<bool>> _cancelFlag = this->cancelFlag;
    QPointer<DataViewerWindow> safeThis = this;

    QThread *thread = QThread::create([safeThis, filePath, _cancelFlag]() {
        QVector<TelemetryRecord> parsedRecords;
        QFile file(filePath);
        
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);

            auto parseOptional = [](const QString& tok) -> std::optional<double> {
                if (tok.trimmed().isEmpty()) return std::nullopt;
                bool ok;
                double val = tok.toDouble(&ok);
                return ok ? std::optional<double>(val) : std::nullopt;
            };

            while (!in.atEnd()) {
                if (*_cancelFlag) break; 

                QString line = in.readLine();
                QStringList tokens = line.split(',');
                
                if (tokens.isEmpty()) continue;
                bool isNumber = false;
                tokens[0].toLongLong(&isNumber);
                if (!isNumber) continue;

                TelemetryRecord rec;
                rec.time       = tokens[0].toLongLong();

                rec.current    = tokens.size() > 1 ? parseOptional(tokens[1]) : std::nullopt;
                rec.temp       = tokens.size() > 2 ? parseOptional(tokens[2]) : std::nullopt;
                rec.flow       = tokens.size() > 3 ? parseOptional(tokens[3]) : std::nullopt;
                rec.hall       = tokens.size() > 4 ? parseOptional(tokens[4]) : std::nullopt;
                rec.ioncurrent = tokens.size() > 5 ? parseOptional(tokens[5]) : std::nullopt;
                rec.iondetect  = tokens.size() > 6 ? parseOptional(tokens[6]) : std::nullopt;
                rec.pressure   = tokens.size() > 7 ? parseOptional(tokens[7]) : std::nullopt;

                parsedRecords.append(rec);
            }
        }

        if (!*_cancelFlag && safeThis) {
            QMetaObject::invokeMethod(safeThis, [safeThis, parsedRecords, filePath]() {
                if (safeThis) {
                    safeThis->m_dataRecords = parsedRecords;
                    safeThis->setWindowTitle("Просмотр логов данных: " + filePath.split('/').last());
                    safeThis->m_metricCombo1->setEnabled(true);
                    safeThis->m_metricCombo2->setEnabled(true);
                    safeThis->m_resetZoomBtn->setEnabled(true);
                    safeThis->updateChart();
                }
            }, Qt::QueuedConnection);
        }
    });

    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void DataViewerWindow::updateChart() {
    if (m_dataRecords.isEmpty()) return;

    for (QLineSeries* s : std::as_const(m_seriesList)) {
        m_chart->removeSeries(s);
        delete s;
    }
    m_seriesList.clear();

    if (m_axisX) { m_chart->removeAxis(m_axisX); delete m_axisX; m_axisX = nullptr; }
    if (m_axisY) { m_chart->removeAxis(m_axisY); delete m_axisY; m_axisY = nullptr; }
    if (m_axisY2) { m_chart->removeAxis(m_axisY2); delete m_axisY2; m_axisY2 = nullptr; }

    m_seriesMetric1 = nullptr;
    m_seriesMetric2 = nullptr;

    int metric1 = m_metricCombo1->currentIndex();
    int metric2 = m_metricCombo2->currentIndex() - 1;

    auto extractValue = [&](const TelemetryRecord& rec, int metricIndex) -> std::optional<double> {
        switch (metricIndex) {
            case 0: return rec.current;
            case 1: return rec.temp;
            case 2: return rec.flow;
            case 3: return rec.hall;
            case 4: return rec.ioncurrent;
            case 5: return rec.iondetect;
            case 6: return rec.pressure;
            default: return std::nullopt;
        }
    };

    struct MetricData {
        QList<QLineSeries*> seriesList;
        qreal minY = std::numeric_limits<qreal>::max();
        qreal maxY = std::numeric_limits<qreal>::lowest();
        QVector<QPointF> currentPoints;
    };

    auto generateSeries = [&](int metricIdx, QColor color, MetricData& outData) {
        outData.currentPoints.clear();
        outData.currentPoints.reserve(m_dataRecords.size());
        
        QVector<QPointF> currentSegment;
        qint64 lastTime = -1;

        auto flushSegment = [&]() {
            if (currentSegment.isEmpty()) return;
            QLineSeries* series = new QLineSeries();
            series->setColor(color);
            series->setUseOpenGL(true);
            series->replace(currentSegment);
            outData.seriesList.append(series);
            currentSegment.clear();
        };

        for (const TelemetryRecord& rec : std::as_const(m_dataRecords)) {
            std::optional<double> optY = extractValue(rec, metricIdx);
            if (!optY.has_value()) {
                flushSegment();
                lastTime = rec.time;
                continue;
            }

            qreal y = optY.value();
            if (y < outData.minY) outData.minY = y;
            if (y > outData.maxY) outData.maxY = y;

            QPointF pt(rec.time, y);
            outData.currentPoints.append(pt);

            if (lastTime != -1 && (rec.time - lastTime) > 10000) {
                flushSegment();
            }

            currentSegment.append(pt);
            lastTime = rec.time;
        }
        flushSegment();

        if (outData.minY > outData.maxY) {
            outData.minY = 0.0;
            outData.maxY = 1.0;
        }
    };

    MetricData data1, data2;
    generateSeries(metric1, QColor("#2196F3"), data1);
    m_currentPoints1 = data1.currentPoints;
    
    if (metric2 >= 0) {
        generateSeries(metric2, QColor("#F44336"), data2);
        m_currentPoints2 = data2.currentPoints;
    } else {
        m_currentPoints2.clear();
    }

    for (QLineSeries* s : std::as_const(data1.seriesList)) {
        m_chart->addSeries(s);
        m_seriesList.append(s);
        if (!m_seriesMetric1) m_seriesMetric1 = s;
    }
    if (metric2 >= 0) {
        for (QLineSeries* s : std::as_const(data2.seriesList)) {
            m_chart->addSeries(s);
            m_seriesList.append(s);
            if (!m_seriesMetric2) m_seriesMetric2 = s;
        }
    }

    m_axisX = new QDateTimeAxis();
    m_axisX->setFormat("HH:mm:ss");
    m_axisX->setTitleText("Время");
    m_chart->addAxis(m_axisX, Qt::AlignBottom);

    auto createYAxis = [&](const QString& title, QColor color, Qt::Alignment align) -> QValueAxis* {
        QValueAxis* axis = new QValueAxis();
        axis->setTitleText(title);
        axis->setLinePenColor(color);
        axis->setLabelsColor(color);
        axis->setTitleBrush(QBrush(color));
        m_chart->addAxis(axis, align);
        return axis;
    };

    m_axisY = createYAxis("Значение 1", "#2196F3", Qt::AlignLeft);

    if (metric2 >= 0) {
        m_axisY2 = createYAxis("Значение 2", "#F44336", Qt::AlignRight);
    }

    for (QLineSeries* s : std::as_const(data1.seriesList)) {
        s->attachAxis(m_axisX);
        s->attachAxis(m_axisY);
    }
    if (metric2 >= 0) {
        for (QLineSeries* s : std::as_const(data2.seriesList)) {
            s->attachAxis(m_axisX);
            s->attachAxis(m_axisY2);
        }
    }

    m_axisX->setRange(QDateTime::fromMSecsSinceEpoch(m_dataRecords.first().time), 
                      QDateTime::fromMSecsSinceEpoch(m_dataRecords.last().time));

    auto applyMargin = [](qreal min, qreal max, QValueAxis* axis) {
        qreal margin = (max - min) * 0.1;
        if (margin == 0) margin = 1.0;
        axis->setRange(min - margin, max + margin);
    };

    applyMargin(data1.minY, data1.maxY, m_axisY);
    if (metric2 >= 0) {
        applyMargin(data2.minY, data2.maxY, m_axisY2);
    }
}

void DataViewerWindow::onMetricChanged(int index) {
    Q_UNUSED(index);
    updateChart();
}

void DataViewerWindow::resetZoom() {
    m_chart->zoomReset();
    updateChart();
}

bool DataViewerWindow::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_chartView->viewport() && event->type() == QEvent::MouseMove) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
        showCustomTooltip(mouseEvent->pos());
    }
    return QMainWindow::eventFilter(watched, event);
}

void DataViewerWindow::showCustomTooltip(const QPointF& mousePixelPos) {
    qreal targetX = 0;
    if (m_seriesMetric1) {
        targetX = m_chart->mapToValue(mousePixelPos, m_seriesMetric1).x();
    } else if (m_seriesMetric2) {
        targetX = m_chart->mapToValue(mousePixelPos, m_seriesMetric2).x();
    } else {
        return;
    }

    auto findClosest = [&](const QVector<QPointF>& pts, QAbstractSeries* series, QPointF& outPt, QPointF& outPix) -> qreal {
        if (pts.isEmpty() || !series) return std::numeric_limits<qreal>::max();
        
        auto it = std::lower_bound(pts.begin(), pts.end(), targetX,
                                   [](const QPointF& p, qreal x) { return p.x() < x; });

        if (it == pts.end()) outPt = pts.last();
        else if (it == pts.begin()) outPt = pts.first();
        else {
            QPointF p1 = *(it - 1);
            QPointF p2 = *it;
            outPt = (std::abs(p1.x() - targetX) < std::abs(p2.x() - targetX)) ? p1 : p2;
        }

        outPix = m_chart->mapToPosition(outPt, series);

        return std::sqrt(std::pow(outPix.x() - mousePixelPos.x(), 2) + std::pow(outPix.y() - mousePixelPos.y(), 2));
    };

    QPointF pt1, pix1, pt2, pix2;
    qreal dist1 = findClosest(m_currentPoints1, m_seriesMetric1, pt1, pix1);
    qreal dist2 = findClosest(m_currentPoints2, m_seriesMetric2, pt2, pix2);
    
    if (dist1 > 30 && dist2 > 30) {
        QToolTip::hideText();
        return;
    } else {
        QString text = "";
        QDateTime dt = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(pt1.x()));
        if (dist1 < 30) {
            text += QString("<b style='color:#2196F3;'>Синий график</b><br>Время: %1<br>Значение: %2")
                    .arg(dt.toString("HH:mm:ss.zzz"))
                    .arg(pt1.y(), 0, 'f', 4);
        }
        if (dist2 < 30) {
            if (text != "") text += "<br>";
            text += QString("<b style='color:#F44336;'>Красный график</b><br>Время: %1<br>Значение: %2")
                    .arg(dt.toString("HH:mm:ss.zzz"))
                    .arg(pt2.y(), 0, 'f', 4);
        }
        QToolTip::showText(QCursor::pos(), text, m_chartView);
    }
}
