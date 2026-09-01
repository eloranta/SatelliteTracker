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
constexpr int kImminentSeconds = 5 * 60;
const QColor kBeforeAosColor(220, 60, 60, 180);  // red: not yet risen
const QColor kAfterAosColor(60, 180, 90, 180);   // green: risen (AOS reached)
}

PassCard::PassCard(const Satellite &satellite, QWidget *parent)
    : QFrame(parent)
{
    setFrameShape(QFrame::StyledPanel);

    auto *layout = new QVBoxLayout(this);

    auto *headerRow = new QHBoxLayout();
    m_headerLabel = new QLabel(this);
    QFont headerFont = m_headerLabel->font();
    headerFont.setBold(true);
    headerFont.setPointSize(headerFont.pointSize() - 1);
    m_headerLabel->setFont(headerFont);
    m_statusChip = new QLabel(this);
    QFont chipFont = m_statusChip->font();
    chipFont.setPointSize(qMax(7, chipFont.pointSize() - 2));
    m_statusChip->setFont(chipFont);
    m_statusChip->setStyleSheet(QStringLiteral("color: #888;"));
    headerRow->addWidget(m_headerLabel, 1);
    headerRow->addWidget(m_statusChip);
    layout->addLayout(headerRow);

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

    m_headerLabel->setText(QStringLiteral("%1 (%2)")
                                .arg(SatelliteNaming::shortName(satellite.name))
                                .arg(satellite.noradId));

    if (tleChanged || !m_propagator) {
        auto propagator = std::make_unique<Sgp4OrbitPropagator>();
        propagator->loadTle(satellite.tleLine1, satellite.tleLine2);
        m_propagator = std::move(propagator);
    }
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
    // CurrentlyInView's aosUtc is just "now" as of the last recompute (see
    // PassResult's aosUtc comment) -- already-visible passes naturally sort
    // near the front either way, so no special-casing needed here.
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
        m_axisEndAnchor = QDateTime();
        return;
    }

    QList<QPointF> points;
    points.reserve(m_lastPass.curve.size());
    for (const ElevationPoint &p : m_lastPass.curve) {
        points.append(QPointF(double(p.utc.toMSecsSinceEpoch()), p.elevationDeg));
    }
    m_curveSeries->replace(points);
    m_elevAxis->setRange(0.0, 90.0);

    // A satellite CurrentlyInView reports aosUtc == the recompute window's
    // start (i.e. "now"), which shifts every ~30s -- rescaling the time
    // axis to that each cycle would make it creep forward mid-pass. Only
    // rescale when this looks like a genuinely different pass (LOS/TCA far
    // from the one the axis was last set for), not a refinement of the
    // same one.
    const QDateTime anchor = m_lastPass.losUtc.isValid() ? m_lastPass.losUtc : m_lastPass.tcaUtc;
    constexpr qint64 kSamePassToleranceSecs = 300;
    if (!m_axisEndAnchor.isValid() || qAbs(m_axisEndAnchor.secsTo(anchor)) > kSamePassToleranceSecs) {
        m_timeAxis->setRange(m_lastPass.curve.first().utc, m_lastPass.curve.last().utc);
        m_axisEndAnchor = anchor;
    }

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
        m_statusChip->setText(QStringLiteral("Idle"));
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

    m_statusChip->setText(statusFor(nowUtc, elevNow, look.valid));
}

QString PassCard::statusFor(const QDateTime &nowUtc, double elevationNowDeg, bool haveFix) const
{
    if (m_lastPass.state == PassState::NoPassInWindow) {
        return QStringLiteral("No upcoming pass");
    }
    if (!haveFix) {
        return QStringLiteral("Idle");
    }
    if (elevationNowDeg > 0.0) {
        return nowUtc <= m_lastPass.tcaUtc ? QStringLiteral("In View") : QStringLiteral("Setting");
    }
    if (m_lastPass.aosUtc.isValid()) {
        const qint64 secsToAos = nowUtc.secsTo(m_lastPass.aosUtc);
        if (secsToAos >= 0 && secsToAos <= kImminentSeconds) {
            return QStringLiteral("Rising");
        }
    }
    return QStringLiteral("Idle");
}

void PassCard::mouseDoubleClickEvent(QMouseEvent *event)
{
    PassDetailDialog dialog(m_satellite, m_location, this);
    dialog.exec();
    QFrame::mouseDoubleClickEvent(event);
}

} // namespace SatelliteTracker
