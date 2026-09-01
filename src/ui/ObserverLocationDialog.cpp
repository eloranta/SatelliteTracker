#include "ObserverLocationDialog.h"

#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "../core/Maidenhead.h"

namespace SatelliteTracker {

ObserverLocationDialog::ObserverLocationDialog(const ObserverLocation &initial, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Observer Location"));

    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout();

    m_locatorEdit = new QLineEdit(initial.gridLocator, this);
    m_locatorEdit->setPlaceholderText(QStringLiteral("e.g. KP20 or KP20ab"));
    form->addRow(QStringLiteral("Grid locator:"), m_locatorEdit);

    m_altitudeSpin = new QDoubleSpinBox(this);
    m_altitudeSpin->setRange(-500.0, 9000.0);
    m_altitudeSpin->setSuffix(QStringLiteral(" m"));
    m_altitudeSpin->setValue(initial.altitudeMeters);
    form->addRow(QStringLiteral("Altitude:"), m_altitudeSpin);

    layout->addLayout(form);

    m_validationLabel = new QLabel(this);
    m_validationLabel->setStyleSheet(QStringLiteral("color: #c00;"));
    layout->addWidget(m_validationLabel);

    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(m_buttonBox);

    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_locatorEdit, &QLineEdit::textChanged, this, &ObserverLocationDialog::onLocatorTextChanged);

    onLocatorTextChanged(m_locatorEdit->text());
}

void ObserverLocationDialog::onLocatorTextChanged(const QString &text)
{
    QString error;
    const bool valid = Maidenhead::isValidLocator(text, &error);
    m_validationLabel->setText(valid ? QString() : error);
    m_buttonBox->button(QDialogButtonBox::Ok)->setEnabled(valid);
}

ObserverLocation ObserverLocationDialog::result() const
{
    ObserverLocation loc;
    loc.gridLocator = m_locatorEdit->text().trimmed();
    loc.altitudeMeters = m_altitudeSpin->value();

    Maidenhead::GeoCoordinate coord;
    if (Maidenhead::locatorToLatLon(loc.gridLocator, &coord)) {
        loc.latitudeDeg = coord.latitudeDeg;
        loc.longitudeDeg = coord.longitudeDeg;
        loc.isConfigured = true;
    }
    return loc;
}

} // namespace SatelliteTracker
