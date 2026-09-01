#include "PassDetailDialog.h"

#include <QDateTime>
#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

#include "../core/Orbit/Sgp4OrbitPropagator.h"
#include "../core/SatelliteNaming.h"

namespace SatelliteTracker {

PassDetailDialog::PassDetailDialog(const Satellite &satellite, const ObserverLocation &location,
                                    QWidget *parent)
    : QDialog(parent)
    , m_satellite(satellite)
    , m_location(location)
{
    setWindowTitle(SatelliteNaming::shortName(satellite.name) + QStringLiteral(" — Upcoming Passes"));
    resize(560, 320);

    auto *layout = new QVBoxLayout(this);

    m_statusLabel = new QLabel(QStringLiteral("Computing…"), this);
    layout->addWidget(m_statusLabel);

    m_table = new QTableWidget(0, 5, this);
    m_table->setHorizontalHeaderLabels(
        {QStringLiteral("AOS"), QStringLiteral("TCA"), QStringLiteral("LOS"),
         QStringLiteral("Max El"), QStringLiteral("Max El Az")});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    layout->addWidget(m_table, 1);

    m_watcher = new QFutureWatcher<QVector<PassResult>>(this);
    connect(m_watcher, &QFutureWatcher<QVector<PassResult>>::finished,
            this, &PassDetailDialog::onPassesComputed);

    const QString line1 = satellite.tleLine1;
    const QString line2 = satellite.tleLine2;
    const ObserverLocation loc = location;
    QFuture<QVector<PassResult>> future = QtConcurrent::run([line1, line2, loc]() {
        Sgp4OrbitPropagator propagator;
        if (!loc.isConfigured || !propagator.loadTle(line1, line2)) {
            return QVector<PassResult>();
        }
        return findUpcomingPasses(propagator, QDateTime::currentDateTimeUtc(),
                                   loc.latitudeDeg, loc.longitudeDeg, loc.altitudeMeters);
    });
    m_watcher->setFuture(future);
}

void PassDetailDialog::onPassesComputed()
{
    const QVector<PassResult> passes = m_watcher->result();

    if (passes.isEmpty()) {
        m_statusLabel->setText(QStringLiteral("No passes found in the next several days."));
        return;
    }

    m_statusLabel->setText(QStringLiteral("Next %1 passes (UTC):").arg(passes.size()));

    m_table->setRowCount(passes.size());
    const QString fmt = QStringLiteral("yyyy-MM-dd HH:mm:ss");
    for (int row = 0; row < passes.size(); ++row) {
        const PassResult &pass = passes.at(row);
        const QString aosText = pass.state == PassState::CurrentlyInView
            ? QStringLiteral("in view")
            : pass.aosUtc.toString(fmt);
        m_table->setItem(row, 0, new QTableWidgetItem(aosText));
        m_table->setItem(row, 1, new QTableWidgetItem(pass.tcaUtc.toString(fmt)));
        m_table->setItem(row, 2, new QTableWidgetItem(
            pass.losUtc.isValid() ? pass.losUtc.toString(fmt) : QStringLiteral("—")));
        m_table->setItem(row, 3, new QTableWidgetItem(
            QString::number(pass.maxElevationDeg, 'f', 1) + QStringLiteral("°")));
        m_table->setItem(row, 4, new QTableWidgetItem(
            QString::number(pass.maxElevAzimuthDeg, 'f', 0) + QStringLiteral("°")));
    }
}

} // namespace SatelliteTracker
