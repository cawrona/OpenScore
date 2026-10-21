// SPDX-License-Identifier: AGPL-3.0-or-later
#include "ActionMap.h"
#include "Library.h"
#include "MidiInput.h"

#include <QFileInfo>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("openscore"));
    QCoreApplication::setApplicationName(QStringLiteral("OpenScore"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    QGuiApplication::setDesktopFileName(QStringLiteral("openscore"));
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    Library library;
    ActionMap actions;
    MidiInput midi;
    QObject::connect(&midi, &MidiInput::message, &actions, &ActionMap::handleMidi);

    // New or moved files show up when you come back from the file manager.
    QObject::connect(&app, &QGuiApplication::applicationStateChanged, &library, [&](Qt::ApplicationState s) {
        if (s == Qt::ApplicationActive)
            library.rescan();
    });

    // `openscore score.pdf` opens straight into the reader.
    QString startupFile;
    const QStringList args = app.arguments();
    if (args.size() > 1 && QFileInfo(args.at(1)).isFile())
        startupFile = QFileInfo(args.at(1)).absoluteFilePath();

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("library"), &library);
    engine.rootContext()->setContextProperty(QStringLiteral("actionMap"), &actions);
    engine.rootContext()->setContextProperty(QStringLiteral("midi"), &midi);
    engine.rootContext()->setContextProperty(QStringLiteral("startupFile"), startupFile);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(-1); },
                     Qt::QueuedConnection);
    engine.loadFromModule("OpenScore", "Main");

    return app.exec();
}
