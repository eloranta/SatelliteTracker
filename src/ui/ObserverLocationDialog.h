#pragma once

#include <QDialog>

#include "../core/ObserverLocation.h"

class QLineEdit;
class QDoubleSpinBox;
class QLabel;
class QDialogButtonBox;

namespace SatelliteTracker {

// Minimal observer-location editor: a Maidenhead grid locator (validated
// live) plus an optional altitude. A preview of the full M7 Settings
// dialog, scoped here to just the one setting M2's pass finder needs.
class ObserverLocationDialog : public QDialog {
    Q_OBJECT
public:
    explicit ObserverLocationDialog(const ObserverLocation &initial, QWidget *parent = nullptr);

    ObserverLocation result() const;

private slots:
    void onLocatorTextChanged(const QString &text);

private:
    QLineEdit *m_locatorEdit = nullptr;
    QDoubleSpinBox *m_altitudeSpin = nullptr;
    QLabel *m_validationLabel = nullptr;
    QDialogButtonBox *m_buttonBox = nullptr;
};

} // namespace SatelliteTracker
