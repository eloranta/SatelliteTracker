#include <QApplication>
#include <QMessageBox>

#include "data/Database.h"
#include "ui/MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("SatelliteTracker"));
    QApplication::setApplicationName(QStringLiteral("SatelliteTracker"));

    const QString dbConnectionName = QStringLiteral("satellite_tracker_main");
    QString dbError;
    if (!SatelliteTracker::Database::openAndMigrate(dbConnectionName, &dbError)) {
        QMessageBox::critical(nullptr, QStringLiteral("SatelliteTracker"),
                               QStringLiteral("Could not open the local database:\n%1\n\n"
                                               "Path: %2")
                                   .arg(dbError, SatelliteTracker::Database::defaultDatabasePath()));
        return 1;
    }

    SatelliteTracker::MainWindow window(dbConnectionName);
    window.show();

    return app.exec();
}
