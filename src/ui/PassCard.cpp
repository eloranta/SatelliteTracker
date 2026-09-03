#include "PassCard.h"

#include <QAreaSeries>
#include <QChart>
#include <QChartView>
#include <QDateTimeAxis>
#include <QLabel>
#include <QLineSeries>
#include <QMouseEvent>
#include <QScatterSeries>
#include <QTimeZone>
#include <QValueAxis>
#include <QVBoxLayout>

#include "../core/Orbit/Sgp4OrbitPropagator.h"
#include "../core/SatelliteNaming.h"
#include "PassDetailDialog.h"

namespace SatelliteTracker {

namespace {
const QColor kBeforeAosColor(220, 60, 60, 180);  // red: not yet risen
const QColor kAfterAosColor(60, 180, 90, 180);   // green: risen (AOS reached)
}

PassCard::PassCard(const Satellite &satellite, QWidget *parent)
    : QFrame(parent)
{
    setFrameShape(QFrame::StyledPanel);

    auto *layout = new QVBoxLayout(this);

    m_headerLabel = new QLabel(this);
    QFont headerFont = m_headerLabel->font();
    headerFont.setBold(true);
    headerFont.setPointSize(headerFont.pointSize() - 1);
    m_headerLabel->setFont(headerFont);
    m_headerLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_headerLabel);

    m_chart = new QChart();
    m_chart->legend()->hide();
    m_chart->setMargins(QMargins(2, 2, 2, 2));

    // m_curveSeries is never added to the chart directly -- it's just the
    // data source for m_areaSeries, whose fill color flips red/green in
    // tick() based on whether AOS has been reached yet.
    m_curveSeries = new QLineSeries();
    m_areaSeries = new QAreaSeries(m_curveSeries);
    m_areaSeries->setColor(kBeforeAosColor);
    m_areaSeries->setPen(Qt::NoPen);

    m_nowMarkerSeries = new QScatterSeries();
    m_nowMarkerSeries->setMarkerSize(10.0);

    m_nowCursorSeries = new QLineSeries();
    QPen cursorPen(QColor(40, 40, 40));
    cursorPen.setStyle(Qt::DashLine);
    cursorPen.setWidth(1);
    m_nowCursorSeries->setPen(cursorPen);

    m_chart->addSeries(m_areaSeries);
    m_chart->addSeries(m_nowCursorSeries);
    m_chart->addSeries(m_nowMarkerSeries);

    m_timeAxis = new QDateTimeAxis();
    m_timeAxis->setFormat(QStringLiteral("HH:mm:ss"));
    m_elevAxis = new QValueAxis();
    m_elevAxis->setTitleText(QStringLiteral("Elevation (°)"));
    m_elevAxis->setRange(0.0, 90.0);
    m_chart->addAxis(m_timeAxis, Qt::AlignBottom);
    m_chart->addAxis(m_elevAxis, Qt::AlignLeft);
    m_areaSeries->attachAxis(m_timeAxis);
    m_areaSeries->attachAxis(m_elevAxis);
    m_nowCursorSeries->attachAxis(m_timeAxis);
    m_nowCursorSeries->attachAxis(m_elevAxis);
    m_nowMarkerSeries->attachAxis(m_timeAxis);
    m_nowMarkerSeries->attachAxis(m_elevAxis);

    m_chartView = new QChartView(m_chart, this);
    m_chartView->setRenderHint(QPainter::Antialiasing);
    m_chartView->setMinimumHeight(110);
    layout->addWidget(m_chartView, 1);

    m_summaryLabel = new QLabel(this);
    QFont summaryFont = m_summaryLabel->font();
    summaryFont.setPointSize(qMax(7, summaryFont.pointSize() - 2));
    m_summaryLabel->setFont(summaryFont);
    m_summaryLabel->setStyleSheet(QStringLiteral("color: #888;"));
    m_summaryLabel->setWordWrap(true);
    layout->addWidget(m_summaryLabel);

    updateSatellite(satellite);
    rebuildChartForPass();
}

void PassCard::updateSatellite(const Satellite &satellite)
{
    const bool tleChanged = satellite.tleLine1 != m_satellite.tleLine1
                          || satellite.tleLine2 != m_satellite.tleLine2;
    m_satellite = satellite;
    updateHeaderLabel();

    if (tleChanged || !m_propagator) {
        auto propagator = std::make_unique<Sgp4OrbitPropagator>();
        propagator->loadTle(satellite.tleLine1, satellite.tleLine2);
        m_propagator = std::move(propagator);
    }
}

void PassCard::updateHeaderLabel()
{
    const QString name = SatelliteNaming::shortName(m_satellite.name);

    QString aosText;
    if (m_lastPass.state == PassState::UpcomingPass || m_lastPass.state == PassState::CurrentlyInView) {
        const QDateTime aosLocal = m_lastPass.aosUtc.toLocalTime();
        const QString datePart = aosLocal.date() == QDate::currentDate()
            ? QStringLiteral("Today")
            : aosLocal.date().toString(QStringLiteral("yyyy-MM-dd"));
        aosText = datePart + QLatin1Char(' ') + aosLocal.toString(QStringLiteral("HH:mm:ss"));
    }

    m_headerLabel->setText(aosText.isEmpty() ? name : name + QLatin1Char(' ') + aosText);
}

void PassCard::setObserverLocation(const ObserverLocation &location)
{
    m_location = location;
}

QDateTime PassCard::aosSortKey() const
{
    if (m_lastPass.state == PassState::NoPassInWindow) {
        return QDateTime(QDate(9999, 1, 1), QTime(0, 0), QTimeZone::UTC);
    }
    return m_lastPass.aosUtc;
}

void PassCard::setPassResult(const PassResult &pass)
{
    m_lastPass = pass;
    rebuildChartForPass();

    if (pass.state != PassState::NoPassInWindow && isHidden()) {
        show();
        emit visibilityMaybeChanged();
    }
}

void PassCard::rebuildChartForPass()
{
    if (m_lastPass.state == PassState::NoPassInWindow || m_lastPass.curve.isEmpty()) {
        m_curveSeries->clear();
        m_nowMarkerSeries->clear();
        m_nowCursorSeries->clear();
        m_summaryLabel->setText(QStringLiteral("No pass in the next 24h"));
        updateHeaderLabel();
        return;
    }

    // Each card gets exactly one setPassResult() call for its whole
    // lifetime (PassGridWidget rebuilds cards wholesale each recompute
    // cycle rather than reusing them), and findNextPass()/findUpcomingPasses()
    // already return a fully-correct, standalone curve (true AOS included,
    // even for a CurrentlyInView pass) -- so no cross-call merging needed.
    QList<QPointF> points;
    points.reserve(m_lastPass.curve.size());
    for (const ElevationPoint &p : m_lastPass.curve) {
        points.append(QPointF(double(p.utc.toMSecsSinceEpoch()), p.elevationDeg));
    }
    m_curveSeries->replace(points);
    m_elevAxis->setRange(0.0, 90.0);

    // Use AOS/LOS directly rather than the curve's first/last sampled point
    // -- the curve's adaptive step doesn't necessarily land exactly on LOS,
    // which would otherwise clip the axis a few seconds short of the true
    // end of the pass.
    const QDateTime axisEnd = m_lastPass.losUtc.isValid() ? m_lastPass.losUtc : m_lastPass.curve.last().utc;
    m_timeAxis->setRange(m_lastPass.aosUtc, axisEnd);

    // Re-evaluated on the next tick(); default to red until then.
    m_areaIsGreen = false;
    m_areaSeries->setColor(kBeforeAosColor);

    const QString aosText = m_lastPass.state == PassState::CurrentlyInView
        ? QStringLiteral("in view")
        : m_lastPass.aosUtc.toString(QStringLiteral("HH:mm:ss"));
    const QString losText = m_lastPass.losUtc.isValid()
        ? m_lastPass.losUtc.toString(QStringLiteral("HH:mm:ss"))
        : QStringLiteral("—");

    m_summaryLabel->setText(
        QStringLiteral("AOS %1 · TCA %2 (%3° az %4°) · LOS %5")
            .arg(aosText, m_lastPass.tcaUtc.toString(QStringLiteral("HH:mm:ss")),
                 QString::number(m_lastPass.maxElevationDeg, 'f', 0),
                 QString::number(m_lastPass.maxElevAzimuthDeg, 'f', 0), losText));

    updateHeaderLabel();
}

void PassCard::tick(const QDateTime &nowUtc)
{
    // Pass is over: hide until a fresh PassResult (its next upcoming pass)
    // arrives via setPassResult(), rather than showing a stale, ended pass.
    if (m_lastPass.losUtc.isValid() && nowUtc > m_lastPass.losUtc) {
        if (!isHidden()) {
            hide();
            emit visibilityMaybeChanged();
        }
        return;
    }

    if (!m_propagator || !m_location.isConfigured) {
        m_nowMarkerSeries->clear();
        m_nowCursorSeries->clear();
        return;
    }

    const LookAngle look = m_propagator->computeLookAngle(
        nowUtc, m_location.latitudeDeg, m_location.longitudeDeg, m_location.altitudeMeters);
    const double elevNow = look.valid ? look.elevationDeg : -90.0;

    if (elevNow > 0.0) {
        m_nowMarkerSeries->replace({QPointF(double(nowUtc.toMSecsSinceEpoch()), elevNow)});
    } else {
        m_nowMarkerSeries->clear();
    }

    if (m_lastPass.state != PassState::NoPassInWindow) {
        const bool aosReached = m_lastPass.state == PassState::CurrentlyInView
                              || (m_lastPass.aosUtc.isValid() && nowUtc >= m_lastPass.aosUtc);
        if (aosReached != m_areaIsGreen) {
            m_areaIsGreen = aosReached;
            m_areaSeries->setColor(aosReached ? kAfterAosColor : kBeforeAosColor);
        }

        // tick() already returned above once nowUtc passed losUtc, so
        // aosReached here means "between AOS and LOS" -- draw the vertical
        // now-cursor only for that span.
        if (aosReached) {
            const double x = double(nowUtc.toMSecsSinceEpoch());
            m_nowCursorSeries->replace({QPointF(x, 0.0), QPointF(x, 90.0)});
        } else {
            m_nowCursorSeries->clear();
        }
    }
}

void PassCard::mouseDoubleClickEvent(QMouseEvent *event)
{
    PassDetailDialog dialog(m_satellite, m_location, this);
    dialog.exec();
    QFrame::mouseDoubleClickEvent(event);
}

} // namespace SatelliteTracker
