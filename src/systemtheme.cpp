#include "systemtheme.h"

#include <QDir>
#include <QFile>
#include <QHash>
#include <QRegularExpression>

SystemTheme::SystemTheme(QObject *parent)
    : QObject(parent) {
    m_pollTimer.setInterval(1000);
    connect(&m_pollTimer, &QTimer::timeout, this, &SystemTheme::reload);
    reload();
    m_pollTimer.start();
}

void SystemTheme::reload() {
    const QString path = QDir::homePath()
        + QStringLiteral("/.local/state/omarchy/current/theme/colors.toml");
    QFile file(path);
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
    m_selectionColor = color(QStringLiteral("selection"), m_selectionColor);
    m_errorColor = color(QStringLiteral("red"), m_errorColor);
    emit themeChanged();
}

