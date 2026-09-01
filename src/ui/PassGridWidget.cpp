#include "PassGridWidget.h"

#include <algorithm>

#include <QDateTime>
#include <QGridLayout>
#include <QSet>
#include <QTimer>

#include "PassCard.h"

namespace SatelliteTracker {

namespace {
constexpr int kCardMinWidth = 380;
constexpr int kTickIntervalMs = 1000;
}

PassGridWidget::PassGridWidget(QWidget *parent)
    : QScrollArea(parent)
{
    setWidgetResizable(true);

    m_content = new QWidget(this);
    m_gridLayout = new QGridLayout(m_content);
    setWidget(m_content);

    m_tickTimer = new QTimer(this);
    m_tickTimer->setInterval(kTickIntervalMs);
    connect(m_tickTimer, &QTimer::timeout, this, [this]() {
        const QDateTime nowUtc = QDateTime::currentDateTimeUtc();
        for (PassCard *card : std::as_const(m_cardsByNoradId)) {
            card->tick(nowUtc);
        }
    });
    m_tickTimer->start();
}

void PassGridWidget::setActiveSatellites(const QVector<Satellite> &activeSatellites)
{
    QSet<int> stillActive;
    stillActive.reserve(activeSatellites.size());

    for (const Satellite &s : activeSatellites) {
        stillActive.insert(s.noradId);

        auto it = m_cardsByNoradId.find(s.noradId);
        if (it == m_cardsByNoradId.end()) {
            auto *card = new PassCard(s, m_content);
            card->setObserverLocation(m_location);
            m_cardsByNoradId.insert(s.noradId, card);
        } else {
            it.value()->updateSatellite(s);
        }
    }

    for (auto it = m_cardsByNoradId.begin(); it != m_cardsByNoradId.end();) {
        if (!stillActive.contains(it.key())) {
            it.value()->deleteLater();
            it = m_cardsByNoradId.erase(it);
        } else {
            ++it;
        }
    }

    reflow();
}

void PassGridWidget::setObserverLocation(const ObserverLocation &location)
{
    m_location = location;
    for (PassCard *card : std::as_const(m_cardsByNoradId)) {
        card->setObserverLocation(location);
    }
}

void PassGridWidget::applyPassResults(const QHash<int, PassResult> &resultsByNoradId)
{
    for (auto it = resultsByNoradId.constBegin(); it != resultsByNoradId.constEnd(); ++it) {
        auto cardIt = m_cardsByNoradId.find(it.key());
        if (cardIt != m_cardsByNoradId.end()) {
            cardIt.value()->setPassResult(it.value());
        }
    }
}

void PassGridWidget::resizeEvent(QResizeEvent *event)
{
    QScrollArea::resizeEvent(event);
    reflow();
}

void PassGridWidget::reflow()
{
    QList<int> noradIds = m_cardsByNoradId.keys();
    std::sort(noradIds.begin(), noradIds.end());

    const int columns = qMax(1, viewport()->width() / kCardMinWidth);
    for (int i = 0; i < noradIds.size(); ++i) {
        m_gridLayout->addWidget(m_cardsByNoradId.value(noradIds.at(i)), i / columns, i % columns);
    }
}

} // namespace SatelliteTracker
