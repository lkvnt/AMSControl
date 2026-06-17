#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFile>
#include <QTextStream>
#include <QToolTip>
#include <QDateTime>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QCursor>
#include <QLabel>
#include "DataViewerWindow.h"
#include "Theme.h"
#include "SettingsManager.h"

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
            chart()->setAnimationOptions(QChart::NoAnimation);
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
            chart()->setAnimationOptions(QChart::SeriesAnimations);
            
        } else {
            QChartView::mouseReleaseEvent(event);
        }
    }

    void wheelEvent(QWheelEvent *event) override {
        qreal factor = event->angleDelta().y() > 0 ? 1.2 : 1.0 / 1.2;
        QRectF plotArea = chart()->plotArea();

        QPointF mousePos;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        mousePos = event->position();
#else
        mousePos = event->pos();
#endif
        if (plotArea.contains(mousePos)) {
            QRectF newRect = plotArea;
            newRect.setWidth(plotArea.width() / factor);
            newRect.setHeight(plotArea.height() / factor);

            qreal xRatio = (mousePos.x() - plotArea.left()) / plotArea.width();
            qreal yRatio = (mousePos.y() - plotArea.top()) / plotArea.height();

            newRect.moveLeft(mousePos.x() - newRect.width() * xRatio);
            newRect.moveTop(mousePos.y() - newRect.height() * yRatio);

            QChart::AnimationOptions currentOptions = chart()->animationOptions();
            chart()->setAnimationOptions(QChart::NoAnimation); // Временно отключаем анимацию для плавного зума
            chart()->zoomIn(newRect);
            chart()->setAnimationOptions(currentOptions);

            event->accept();
        } else {
            QChartView::wheelEvent(event);
        }
    }
};

DataViewerWindow::DataViewerWindow(const QString& filePath, QWidget *parent) 
    : QMainWindow(parent), m_filePath(filePath) {
    
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle("Просмотр логов данных: " + filePath.split('/').last());
    resize(1000, 700);

    cancelFlag = std::make_shared<std::atomic<bool>>(false);

    m_tooltipTimer = new QTimer(this);
    m_tooltipTimer->setSingleShot(true);
    connect(m_tooltipTimer, &QTimer::timeout, this, [this]() {
        showCustomTooltip(m_lastMousePos);
    });

    this->setStyleSheet(Theme::getAppStylesheet());

    auto *centralWidget = new QWidget(this);
    auto *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(15, 15, 15, 15);
    mainLayout->setSpacing(12);

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

    QString themeName = SettingsManager::instance().get("theme", "dark").toString();
    bool isDark = (themeName == "dark");
    QString color1 = isDark ? "#2196F3" : "#005cc5";
    QString color2 = isDark ? "#F44336" : "#C62828";

    m_label1 = new QLabel(QString("График 1 (<b style='color:%1;'>Синий</b>):").arg(color1));
    m_label1->setTextFormat(Qt::RichText);
    comboLayout1->addWidget(m_label1);
    m_metricCombo1 = new QComboBox();
    m_metricCombo1->addItems(metrics);
    connect(m_metricCombo1, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DataViewerWindow::onMetricChanged);
    comboLayout1->addWidget(m_metricCombo1);

    auto *comboLayout2 = new QHBoxLayout();
    m_label2 = new QLabel(QString("График 2 (<b style='color:%1;'>Красный</b>):").arg(color2));
    m_label2->setTextFormat(Qt::RichText);
    comboLayout2->addWidget(m_label2);
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
    m_chart->setMargins(QMargins(0, 0, 0, 0));

    m_chart->setCacheMode(QChart::DeviceCoordinateCache);
    m_chart->setAnimationOptions(QChart::SeriesAnimations);
    m_chart->setAnimationDuration(100);
    m_chart->setTheme(isDark ? QChart::ChartThemeDark : QChart::ChartThemeLight);
    m_chart->setBackgroundVisible(false);

    m_axisX = nullptr;
    m_axisY = nullptr;
    m_axisY2 = nullptr;

    m_chartView = new ChartViewPanZoom(m_chart);
    m_chartView->setFrameShape(QFrame::NoFrame);
    m_chartView->setRenderHint(QPainter::Antialiasing);
    m_chartView->setStyleSheet("background: transparent;");

    m_chartView->viewport()->installEventFilter(this);
    m_chartView->setMouseTracking(true);

    m_tooltipWidget = new QLabel(m_chartView);
    m_tooltipWidget->setStyleSheet(QString("QLabel { background-color: %1; border: 1px solid %2; border-radius: 6px; padding: 8px; color: %3; font-size: 12px; }")
                                   .arg(isDark ? "#1e1e1e" : "#ffffff")
                                   .arg(isDark ? "#444" : "#c5c5c5")
                                   .arg(isDark ? "#e0e0e0" : "#111111"));
    m_tooltipWidget->setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus);
    m_tooltipWidget->setAttribute(Qt::WA_ShowWithoutActivating);
    m_tooltipWidget->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_tooltipWidget->hide();

    m_lineH1 = new QGraphicsLineItem(m_chart);
    m_lineV1 = new QGraphicsLineItem(m_chart);
    m_lineH2 = new QGraphicsLineItem(m_chart);
    m_lineV2 = new QGraphicsLineItem(m_chart);

    QPen pen1(QColor(color1), 1, Qt::DashLine);
    m_lineH1->setPen(pen1); m_lineV1->setPen(pen1);
    m_lineH1->setZValue(11); m_lineV1->setZValue(11);

    QPen pen2(QColor(color2), 1, Qt::DashLine);
    m_lineH2->setPen(pen2); m_lineV2->setPen(pen2);
    m_lineH2->setZValue(11); m_lineV2->setZValue(11);

    m_lineH1->hide(); m_lineV1->hide();
    m_lineH2->hide(); m_lineV2->hide();

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
            QPen pen(color);
            pen.setWidth(2);
            series->setPen(pen);
            // series->setUseOpenGL(true);
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
    QString themeName = SettingsManager::instance().get("theme", "dark").toString();
    bool isDark = (themeName == "dark");

    generateSeries(metric1, isDark ? QColor("#2196F3") : QColor("#005cc5"), data1);
    m_currentPoints1 = data1.currentPoints;
    
    if (metric2 >= 0) {
        generateSeries(metric2, isDark ? QColor("#F44336") : QColor("#C62828"), data2);
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

    m_axisY = createYAxis("Значение 1", isDark ? "#2196F3" : "#005cc5", Qt::AlignLeft);

    if (metric2 >= 0) {
        m_axisY2 = createYAxis("Значение 2", isDark ? "#F44336" : "#C62828", Qt::AlignRight);
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

void DataViewerWindow::applyTheme() {
    this->setStyleSheet(Theme::getAppStylesheet());
    
    QString themeName = SettingsManager::instance().get("theme", "dark").toString();
    bool isDark = (themeName == "dark");
    QString color1 = isDark ? "#2196F3" : "#005cc5";
    QString color2 = isDark ? "#F44336" : "#C62828";
    
    if (m_label1) m_label1->setText(QString("График 1 (<b style='color:%1;'>Синий</b>):").arg(color1));
    if (m_label2) m_label2->setText(QString("График 2 (<b style='color:%1;'>Красный</b>):").arg(color2));
    
    m_chart->setTheme(isDark ? QChart::ChartThemeDark : QChart::ChartThemeLight);
    m_chart->setBackgroundVisible(false);
    
    m_tooltipWidget->setStyleSheet(QString("QLabel { background-color: %1; border: 1px solid %2; border-radius: 6px; padding: 8px; color: %3; font-size: 12px; }")
                                   .arg(isDark ? "#1e1e1e" : "#ffffff")
                                   .arg(isDark ? "#444" : "#c5c5c5")
                                   .arg(isDark ? "#e0e0e0" : "#111111"));
    
    QPen pen1(QColor(color1), 1, Qt::DashLine);
    m_lineH1->setPen(pen1); m_lineV1->setPen(pen1);
    QPen pen2(QColor(color2), 1, Qt::DashLine);
    m_lineH2->setPen(pen2); m_lineV2->setPen(pen2);

    updateChart();
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
    if (watched == m_chartView->viewport()) {
        
        if (event->type() == QEvent::MouseMove) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->pos() == m_lastMousePos) return QMainWindow::eventFilter(watched, event);
            m_lastMousePos = mouseEvent->pos();

            m_tooltipWidget->hide();
            m_lineH1->hide(); m_lineV1->hide();
            m_lineH2->hide(); m_lineV2->hide();

            m_tooltipTimer->start(100);
        } 
        else if (event->type() == QEvent::Leave) {
            m_tooltipTimer->stop();

            m_tooltipWidget->hide();
            m_lineH1->hide(); m_lineV1->hide();
            m_lineH2->hide(); m_lineV2->hide();
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

qreal DataViewerWindow::findClosest(const QVector<QPointF>& pts, const QPointF& mousePixelPos, 
                               QAbstractSeries* series, QPointF& outPt, QPointF& outPix) {
    if (pts.isEmpty() || !series) return std::numeric_limits<qreal>::max();
    if (pts.size() < 2) {
            outPt = pts.first();
            outPix = m_chart->mapToPosition(outPt, series);
            return std::sqrt(std::pow(outPix.x() - mousePixelPos.x(), 2) + std::pow(outPix.y() - mousePixelPos.y(), 2));
    }
    
    QPointF targetVal = m_chart->mapToValue(mousePixelPos, series);
    qreal targetX = targetVal.x();
    qreal targetY = targetVal.y();

    QPointF valLeft = m_chart->mapToValue(mousePixelPos - QPointF(100, 0), series);
    QPointF valRight = m_chart->mapToValue(mousePixelPos + QPointF(100, 0), series);
    qreal searchMinX = std::min(valLeft.x(), valRight.x());
    qreal searchMaxX = std::max(valLeft.x(), valRight.x());

    auto itStart = std::lower_bound(pts.begin(), pts.end(), searchMinX, [](const QPointF& p, qreal x) { return p.x() < x; });
    auto itEnd = std::lower_bound(pts.begin(), pts.end(), searchMaxX, [](const QPointF& p, qreal x) { return p.x() < x; });

    int startIndex = std::distance(pts.begin(), itStart);
    int endIndex = std::distance(pts.begin(), itEnd);

    auto itCenter = std::lower_bound(pts.begin(), pts.end(), targetX, [](const QPointF& p, qreal x) { return p.x() < x; });
    int centerIdx = std::distance(pts.begin(), itCenter);

    startIndex = std::min(startIndex, std::max(1, centerIdx - 50));
    endIndex = std::max(endIndex, std::min(static_cast<int>(pts.size() - 1), centerIdx + 50));

    startIndex = std::max(1, startIndex - 1);
    endIndex = std::min(static_cast<int>(pts.size() - 1), endIndex + 1);

    qreal minDist = std::numeric_limits<qreal>::max();
    QPointF bestPt;
    QPointF bestPix;
    
    for (int i = startIndex; i <= endIndex; ++i) {
        QPointF p1 = pts[i - 1];
        QPointF p2 = pts[i];

        if ((p2.x() - p1.x()) > 10000 || p2.x() == p1.x()) {
            QPointF pix1 = m_chart->mapToPosition(p1, series);
            qreal d1 = std::sqrt(std::pow(pix1.x() - mousePixelPos.x(), 2) + std::pow(pix1.y() - mousePixelPos.y(), 2));
            if (d1 < minDist) { minDist = d1; bestPt = p1; bestPix = pix1; }

            QPointF pix2 = m_chart->mapToPosition(p2, series);
            qreal d2 = std::sqrt(std::pow(pix2.x() - mousePixelPos.x(), 2) + std::pow(pix2.y() - mousePixelPos.y(), 2));
            if (d2 < minDist) { minDist = d2; bestPt = p2; bestPix = pix2; }
            continue;
        }

        if (targetX >= p1.x() && targetX <= p2.x()) {
            qreal ratioX = (targetX - p1.x()) / (p2.x() - p1.x());
            qreal interpY = p1.y() + ratioX * (p2.y() - p1.y());
            QPointF ptVert(targetX, interpY);
            QPointF pixVert = m_chart->mapToPosition(ptVert, series);
            
            qreal distVert = std::sqrt(std::pow(pixVert.x() - mousePixelPos.x(), 2) + std::pow(pixVert.y() - mousePixelPos.y(), 2));
            if (distVert < minDist) {
                minDist = distVert; bestPt = ptVert; bestPix = pixVert;
            }
        }

        qreal minY = std::min(p1.y(), p2.y());
        qreal maxY = std::max(p1.y(), p2.y());
        if (targetY >= minY && targetY <= maxY && p1.y() != p2.y()) {
            qreal ratioY = (targetY - p1.y()) / (p2.y() - p1.y());
            qreal interpX = p1.x() + ratioY * (p2.x() - p1.x());
            QPointF ptHoriz(interpX, targetY);
            QPointF pixHoriz = m_chart->mapToPosition(ptHoriz, series);
            
            qreal distHoriz = std::sqrt(std::pow(pixHoriz.x() - mousePixelPos.x(), 2) + std::pow(pixHoriz.y() - mousePixelPos.y(), 2));
            if (distHoriz < minDist) {
                minDist = distHoriz; bestPt = ptHoriz; bestPix = pixHoriz;
            }
        }

        QPointF pix1 = m_chart->mapToPosition(p1, series);
        qreal d1 = std::sqrt(std::pow(pix1.x() - mousePixelPos.x(), 2) + std::pow(pix1.y() - mousePixelPos.y(), 2));
        if (d1 < minDist) { minDist = d1; bestPt = p1; bestPix = pix1; }

        QPointF pix2 = m_chart->mapToPosition(p2, series);
        qreal d2 = std::sqrt(std::pow(pix2.x() - mousePixelPos.x(), 2) + std::pow(pix2.y() - mousePixelPos.y(), 2));
        if (d2 < minDist) { minDist = d2; bestPt = p2; bestPix = pix2; }
    }
    if (minDist == std::numeric_limits<qreal>::max()) return minDist;
    outPt = bestPt;
    outPix = bestPix;
    return minDist;
}

void DataViewerWindow::showCustomTooltip(const QPointF& mousePixelPos) {
    QPointF pt1, pix1, pt2, pix2;
    qreal dist1 = findClosest(m_currentPoints1, mousePixelPos, m_seriesMetric1, pt1, pix1);
    qreal dist2 = findClosest(m_currentPoints2, mousePixelPos, m_seriesMetric2, pt2, pix2);
    
    if (dist1 > 30 && dist2 > 30) {
        m_tooltipWidget->hide();
        m_lineH1->hide(); m_lineV1->hide();
        m_lineH2->hide(); m_lineV2->hide();
        return;
    } else {
        QString text = "";
        QRectF plotArea = m_chart->plotArea();

        QString themeName = SettingsManager::instance().get("theme", "dark").toString();
        bool isDark = (themeName == "dark");

        if (dist1 < 30) {
            char format = 'f';
            if (m_metricCombo1->currentIndex() == 6) format = 'e';
            QDateTime dt = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(pt1.x()));
            text += QString("<b style='color:%1;'>Синий график</b><br>Время: %2<br>Значение: %3")
                    .arg(isDark ? "#2196F3" : "#005cc5")
                    .arg(dt.toString("HH:mm:ss.zzz"))
                    .arg(pt1.y(), 0, format, 4);

            m_lineV1->setLine(pix1.x(), pix1.y(), pix1.x(), plotArea.bottom());
            m_lineH1->setLine(plotArea.left(), pix1.y(), pix1.x(), pix1.y());
            m_lineV1->show();
            m_lineH1->show();
        } else {
            m_lineV1->hide();
            m_lineH1->hide();
        }

        if (dist2 < 30) {
            char format = 'f';
            if (m_metricCombo2->currentIndex() - 1 == 6) format = 'e';
            QDateTime dt = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(pt2.x()));
            if (text != "") text += "<br>";
            text += QString("<b style='color:%1;'>Красный график</b><br>Время: %2<br>Значение: %3")
                    .arg(isDark ? "#F44336" : "#C62828")
                    .arg(dt.toString("HH:mm:ss.zzz"))
                    .arg(pt2.y(), 0, format, 4);
            
            m_lineV2->setLine(pix2.x(), pix2.y(), pix2.x(), plotArea.bottom());
            m_lineH2->setLine(pix2.x(), pix2.y(), plotArea.right(), pix2.y());
            m_lineV2->show();
            m_lineH2->show();
        } else {
            m_lineV2->hide();
            m_lineH2->hide();
        }

        m_tooltipWidget->setText(text);
        m_tooltipWidget->adjustSize();
        
        int x = m_lastMousePos.x() + 15;
        int y = m_lastMousePos.y() + 15;
        
        if (x + m_tooltipWidget->width() > m_chartView->viewport()->width()) {
            x = m_lastMousePos.x() - m_tooltipWidget->width() - 5;
        }
        if (y + m_tooltipWidget->height() > m_chartView->viewport()->height()) {
            y = m_lastMousePos.y() - m_tooltipWidget->height() - 5;
        }
        
        QPoint globalPos = m_chartView->viewport()->mapToGlobal(QPoint(x, y));
        m_tooltipWidget->move(globalPos);
        m_tooltipWidget->show();
    }
}
