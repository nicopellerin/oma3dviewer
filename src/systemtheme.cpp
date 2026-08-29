#include "systemtheme.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>

namespace {
QString omarchyStateDirectory() {
    return QDir::homePath() + QStringLiteral("/.local/state/omarchy");
}
}

SystemTheme::SystemTheme(QObject *parent)
    : QObject(parent) {
    connect(&m_watcher, &QFileSystemWatcher::fileChanged,
            this, &SystemTheme::refresh);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged,
            this, &SystemTheme::refresh);
    m_pollTimer.setInterval(1000);
    connect(&m_pollTimer, &QTimer::timeout, this, &SystemTheme::refresh);
    refresh();
}

QColor SystemTheme::mix(const QColor &base, const QColor &tint,
                        qreal amount) const {
    return QColor::fromRgbF(base.redF() + (tint.redF() - base.redF()) * amount,
                            base.greenF() + (tint.greenF() - base.greenF()) * amount,
                            base.blueF() + (tint.blueF() - base.blueF()) * amount);
}

void SystemTheme::refresh() {
    reload();
    watchThemePaths();
}

void SystemTheme::watchThemePaths() {
    // Theme switches replace symlinks under the state directory, which
    // silently drops watches on the old targets, so re-resolve and re-arm
    // the whole watch list after every change.
    if (!m_watcher.files().isEmpty())
        m_watcher.removePaths(m_watcher.files());
    if (!m_watcher.directories().isEmpty())
        m_watcher.removePaths(m_watcher.directories());

    const QString stateDirectory = omarchyStateDirectory();
    QStringList paths;
    const QStringList candidates{
        stateDirectory,
        stateDirectory + QStringLiteral("/current"),
        stateDirectory + QStringLiteral("/current/theme"),
        stateDirectory + QStringLiteral("/current/theme/colors.toml")};
    for (const QString &candidate : candidates) {
        if (QFileInfo::exists(candidate))
            paths.append(candidate);
    }
    QStringList unwatchedPaths;
    if (!paths.isEmpty())
        unwatchedPaths = m_watcher.addPaths(paths);

    // Fall back to polling while there is nothing watchable yet or any
    // required watch could not be registered.
    if (paths.isEmpty() || !unwatchedPaths.isEmpty())
        m_pollTimer.start();
    else
        m_pollTimer.stop();
}

void SystemTheme::reload() {
    QFile file(omarchyStateDirectory()
               + QStringLiteral("/current/theme/colors.toml"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    const QByteArray contents = file.readAll();
    if (contents == m_lastContents)
        return;
    m_lastContents = contents;

    QHash<QString, QString> values;
    const QRegularExpression assignment(
        QStringLiteral(R"(^\s*([A-Za-z0-9_]+)\s*=\s*[\"']([^\"']+)[\"']\s*$)"));

    const QString text = QString::fromUtf8(contents);
    for (const QString &line : text.split(QLatin1Char('\n'))) {
        const QRegularExpressionMatch match = assignment.match(line);
        if (match.hasMatch())
            values.insert(match.captured(1), match.captured(2));
    }

    const auto color = [&values](const QString &key, const QColor &fallback) {
        const QColor candidate(values.value(key));
        return candidate.isValid() ? candidate : fallback;
    };

    m_darkMode = values.value(QStringLiteral("mode"), QStringLiteral("dark"))
        != QStringLiteral("light");
    m_pageColor = color(QStringLiteral("background"), m_pageColor);
    m_stageColor = color(QStringLiteral("dark_background"), m_pageColor.darker(112));
    m_inkColor = color(QStringLiteral("foreground"), m_inkColor);
    m_mutedColor = color(QStringLiteral("dark_foreground"), m_mutedColor);
    m_accentColor = color(QStringLiteral("accent"), m_accentColor);
    m_errorColor = color(QStringLiteral("red"), m_errorColor);
    emit themeChanged();
}
