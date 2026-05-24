#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFile>
#include <QTextStream>
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

    m_chartView->viewport()->installEventFilter(this);
    m_chartView->setMouseTracking(true);

    mainLayout->addWidget(m_chartView);
    setCentralWidget(centralWidget);

    // Загрузка данных
    m_series->setUseOpenGL(true);
    loadData(m_filePath);
}

void DataViewerWindow::loadData(const QString& filePath) {
    // 1. Блокируем элементы управления и показываем статус
    setWindowTitle("Загрузка данных... Пожалуйста, подождите.");
    m_metricCombo->setEnabled(false);
    m_resetZoomBtn->setEnabled(false);

    // 2. QPointer безопасно обнулится, если окно будет закрыто (предотвратит краш)
    QPointer<DataViewerWindow> safeThis = this;

    // 3. Создаем и запускаем рабочий поток
    QThread *thread = QThread::create([safeThis, filePath]() {
        QVector<TelemetryRecord> parsedRecords;
        QFile file(filePath);
        
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            while (!in.atEnd()) {
                // Если пользователь закрыл окно во время чтения — экстренно прерываем цикл!
                if (!safeThis) break; 

                QString line = in.readLine();
                QStringList tokens = line.split(',');
                
                if (tokens.isEmpty()) continue;
                bool isNumber = false;
                tokens[0].toLongLong(&isNumber);
                if (!isNumber) continue;

                if (tokens.size() >= 7) {
                    TelemetryRecord rec;
                    rec.time       = tokens[0].toLongLong();
                    rec.current    = tokens[1].toDouble();
                    rec.temp       = tokens[2].toDouble();
                    rec.flow       = tokens[3].toDouble();
                    rec.hall       = tokens[4].toDouble();
                    rec.ioncurrent = tokens[5].toDouble();
                    rec.pressure   = tokens[6].toDouble();
                    parsedRecords.append(rec);
                }
            }
        }

        // 4. Возвращаемся в главный поток (через invoke и QueuedConnection) для перерисовки UI (если окно еще живо)
        if (safeThis) {
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

    // 5. Поток сам удалит свой объект из памяти после завершения
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void DataViewerWindow::updateChart() {
    if (m_dataRecords.isEmpty()) return;

    int metricIndex = m_metricCombo->currentIndex();
    
    qreal minY = std::numeric_limits<qreal>::max();
    qreal maxY = std::numeric_limits<qreal>::lowest();

    // 1. Создаем временный массив для быстрой вставки
    QVector<QPointF> newPoints;
    newPoints.reserve(m_dataRecords.size());

    for (const TelemetryRecord& rec : qAsConst(m_dataRecords)) {
        // qint64 timeMs = static_cast<qint64>(obj["time"].toDouble());
        qreal y = 0.0;

        switch (metricIndex) {
            case 0: y = rec.current; break;
            case 1: y = rec.temp; break;
            case 2: y = rec.flow; break;
            case 3: y = rec.hall; break;
            case 4: y = rec.ioncurrent; break;
            case 5: y = rec.pressure; break;
        }

        newPoints.append(QPointF(rec.time, y));
        if (y < minY) minY = y;
        if (y > maxY) maxY = y;
    }

    // 2. Сохраняем в кэш для тултипов
    m_currentPoints = newPoints;

    // 3. МГНОВЕННО заменяем все точки на графике разом
    m_series->replace(newPoints);

    // Масштабируем оси
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
    updateChart(); // Возвращаем к изначальным границам
}

bool DataViewerWindow::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_chartView->viewport() && event->type() == QEvent::MouseMove) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
        // Передаем координаты мыши в наш алгоритм поиска
        showCustomTooltip(mouseEvent->pos());
    }
    return QMainWindow::eventFilter(watched, event);
}

void DataViewerWindow::showCustomTooltip(const QPointF& mousePixelPos) {
    if (m_currentPoints.isEmpty()) return;

    // 1. Переводим пиксели окна в значения графика (время)
    QPointF chartPos = m_chart->mapToValue(mousePixelPos);
    qreal targetX = chartPos.x();

    // 2. Бинарный поиск (очень быстрый поиск в отсортированном по времени массиве)
    auto it = std::lower_bound(m_currentPoints.begin(), m_currentPoints.end(), targetX,
                               [](const QPointF& p, qreal x) { return p.x() < x; });

    QPointF closestPoint;
    if (it == m_currentPoints.end()) {
        closestPoint = m_currentPoints.last();
    } else if (it == m_currentPoints.begin()) {
        closestPoint = m_currentPoints.first();
    } else {
        // Сравниваем две соседние точки, чтобы выбрать самую близкую к курсору
        QPointF p1 = *(it - 1);
        QPointF p2 = *it;
        closestPoint = (std::abs(p1.x() - targetX) < std::abs(p2.x() - targetX)) ? p1 : p2;
    }

    // 3. Защита: показываем тултип, только если мышь находится рядом с линией графика
    // Вычисляем, где на экране (в пикселях) должна быть наша найденная точка
    QPointF closestPixelPos = m_chart->mapToPosition(closestPoint);
    
    // Если курсор мыши слишком высоко или низко от реальной линии (погрешность 40 пикселей)
    if (std::abs(closestPixelPos.y() - mousePixelPos.y()) > 40) {
        QToolTip::hideText(); // Прячем тултип (мышь "в небе" или "под землей")
        return;
    }

    // 4. Показываем всплывающую подсказку
    QDateTime dt = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(closestPoint.x()));
    QString text = QString("Время: %1\nЗначение: %2")
                   .arg(dt.toString("HH:mm:ss.zzz"))
                   .arg(closestPoint.y(), 0, 'f', 4);
                   
    QToolTip::showText(QCursor::pos(), text, m_chartView);
}
