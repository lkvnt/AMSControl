#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFile>
#include <QTextStream>
#include <QToolTip>
#include <QDateTime>
#include <QMouseEvent>
#include <QCursor>
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
    resize(900, 600);

    cancelFlag = std::make_shared<std::atomic<bool>>(false);

    auto *centralWidget = new QWidget(this);
    auto *mainLayout = new QVBoxLayout(centralWidget);

    auto *topLayout = new QHBoxLayout();
    m_metricCombo = new QComboBox();
    m_metricCombo->addItems({
        "Ток (PowerControl)", 
        "Температура (CoolControl)", 
        "Поток (CoolControl)", 
        "Датчик Холла (SensorControl)", 
        "Цилиндр Фарадея 1 (SensorControl)", 
        "Цилиндр Фарадея 2 (SensorControl)", 
        "Давление/Вакуум (SensorControl)"
    });
    connect(m_metricCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DataViewerWindow::onMetricChanged);

    m_resetZoomBtn = new QPushButton("Сбросить масштаб");
    connect(m_resetZoomBtn, &QPushButton::clicked, this, &DataViewerWindow::resetZoom);

    topLayout->addWidget(m_metricCombo);
    topLayout->addWidget(m_resetZoomBtn);
    topLayout->addStretch();
    mainLayout->addLayout(topLayout);

    m_chart = new QChart();
    m_chart->legend()->hide();

    m_axisX = nullptr;
    m_axisY = nullptr;

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
    m_metricCombo->setEnabled(false);
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
                    safeThis->m_metricCombo->setEnabled(true);
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

    if (m_axisX) {
        m_chart->removeAxis(m_axisX);
        delete m_axisX;
        m_axisX = nullptr;
    }
    if (m_axisY) {
        m_chart->removeAxis(m_axisY);
        delete m_axisY;
        m_axisY = nullptr;
    }

    m_axisX = new QDateTimeAxis();
    m_axisX->setFormat("HH:mm:ss");
    m_axisX->setTitleText("Время");
    m_chart->addAxis(m_axisX, Qt::AlignBottom);

    m_axisY = new QValueAxis();
    m_axisY->setTitleText("Значение");
    m_chart->addAxis(m_axisY, Qt::AlignLeft);

    int metricIndex = m_metricCombo->currentIndex();
    
    qreal minY = std::numeric_limits<qreal>::max();
    qreal maxY = std::numeric_limits<qreal>::lowest();

    m_currentPoints.clear();
    m_currentPoints.reserve(m_dataRecords.size());

    QVector<QPointF> currentSegment;
    qint64 lastTime = -1;
    QColor seriesColor("#2196F3");

    auto createSegmentHelper = [&](const QVector<QPointF>& segmentData) {
        if (segmentData.isEmpty()) return;

        QLineSeries* series = new QLineSeries();
        
        // Порядок важен
        m_chart->addSeries(series);
        series->setUseOpenGL(true);
        series->setColor(seriesColor);
        series->replace(segmentData);
        
        series->attachAxis(m_axisX);
        series->attachAxis(m_axisY);
        
        m_seriesList.append(series);
    };

    for (const TelemetryRecord& rec : std::as_const(m_dataRecords)) {
        std::optional<double> optY;

        switch (metricIndex) {
            case 0: optY = rec.current; break;
            case 1: optY = rec.temp; break;
            case 2: optY = rec.flow; break;
            case 3: optY = rec.hall; break;
            case 4: optY = rec.ioncurrent; break;
            case 5: optY = rec.iondetect; break;
            case 6: optY = rec.pressure; break;
        }

        if (!optY.has_value()) {
            createSegmentHelper(currentSegment);
            currentSegment.clear();
            lastTime = rec.time;
            continue;
        }

        qreal y = optY.value();

        if (y < minY) minY = y;
        if (y > maxY) maxY = y;

        QPointF pt(rec.time, y);
        m_currentPoints.append(pt);

        if (lastTime != -1 && (rec.time - lastTime) > 10000) {
            createSegmentHelper(currentSegment);
            currentSegment.clear();
        }

        currentSegment.append(pt);
        lastTime = rec.time;
    }

    createSegmentHelper(currentSegment);

    if (minY > maxY) {
        minY = 0.0;
        maxY = 1.0;
    }

    m_axisX->setRange(QDateTime::fromMSecsSinceEpoch(m_dataRecords.first().time), 
                      QDateTime::fromMSecsSinceEpoch(m_dataRecords.last().time));
    
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
    if (m_currentPoints.isEmpty()) return;

    QAbstractSeries* firstSeries = m_seriesList.isEmpty() ? nullptr : m_seriesList.first();

    QPointF chartPos = m_chart->mapToValue(mousePixelPos, firstSeries);
    qreal targetX = chartPos.x();

    auto it = std::lower_bound(m_currentPoints.begin(), m_currentPoints.end(), targetX,
                               [](const QPointF& p, qreal x) { return p.x() < x; });

    QPointF closestPoint;
    if (it == m_currentPoints.end()) {
        closestPoint = m_currentPoints.last();
    } else if (it == m_currentPoints.begin()) {
        closestPoint = m_currentPoints.first();
    } else {
        QPointF p1 = *(it - 1);
        QPointF p2 = *it;
        closestPoint = (std::abs(p1.x() - targetX) < std::abs(p2.x() - targetX)) ? p1 : p2;
    }

    QPointF closestPixelPos = m_chart->mapToPosition(closestPoint, firstSeries);
    
    if ((std::abs(closestPixelPos.y() - mousePixelPos.y()) > 30) ||
        (std::abs(closestPixelPos.x() - mousePixelPos.x()) > 30))
    {
        QToolTip::hideText();
        return;
    }

    QDateTime dt = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(closestPoint.x()));
    QString text = QString("Время: %1\nЗначение: %2")
                   .arg(dt.toString("HH:mm:ss.zzz"))
                   .arg(closestPoint.y(), 0, 'f', 4);
                   
    QToolTip::showText(QCursor::pos(), text, m_chartView);
}
