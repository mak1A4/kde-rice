#include "panelstyle.h"

#include <KCompressionDevice>
#include <KConfigGroup>
#include <KSharedConfig>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>

using namespace Qt::StringLiterals;

static const auto s_style = u"kde-rice"_s;
// Breeze's own, which the style is made from
static constexpr int s_breezeOpacity = 85;

static KConfigGroup settings()
{
    auto config = KSharedConfig::openConfig(u"kde-ricerc"_s);
    config->reparseConfiguration();
    return config->group(u"Panels"_s);
}

static QString currentStyle()
{
    auto plasmarc = KSharedConfig::openConfig(u"plasmarc"_s);
    plasmarc->reparseConfiguration();
    return plasmarc->group(u"Theme"_s).readEntry("name", u"default"_s);
}

static QString styleDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/plasma/desktoptheme/"_L1 + s_style;
}

static QString stateFile()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericStateLocation) + "/kde-rice/plasma-style-before"_L1;
}

PanelStyle::PanelStyle(QObject *parent)
    : QObject(parent)
{
}

int PanelStyle::opacity() const
{
    return std::clamp(settings().readEntry("Opacity", s_breezeOpacity), 0, 100);
}

bool PanelStyle::inUse() const
{
    return currentStyle() == s_style;
}

// Writes `contents` to `path` if that changes it. True if it did.
static bool writeFile(const QString &path, const QByteArray &contents)
{
    QFile current(path);
    if (current.open(QIODevice::ReadOnly) && current.readAll() == contents) {
        return false;
    }
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size() && file.commit();
}

bool PanelStyle::write(int percent) const
{
    const QString breeze = QStandardPaths::locate(QStandardPaths::GenericDataLocation, u"plasma/desktoptheme/default"_s, QStandardPaths::LocateDirectory);
    if (breeze.isEmpty()) {
        return false;
    }
    bool changed = false;
    changed |= writeFile(styleDir() + "/metadata.json"_L1, R"({
    "KPlugin": {
        "Description": "Breeze, with the panels as see-through as kde-rice says",
        "Id": "kde-rice",
        "License": "LGPL",
        "Name": "kde-rice"
    },
    "X-Plasma-API": "5.0"
}
)");
    // Breeze's settings for the style (adaptive transparency among them)
    QFile plasmarc(breeze + "/plasmarc"_L1);
    if (plasmarc.open(QIODevice::ReadOnly)) {
        changed |= writeFile(styleDir() + "/plasmarc"_L1, plasmarc.readAll());
    }
    // The panel's background, with blur behind it and without: its nine
    // pieces are the colour scheme's background at 85 %.
    // Each element by itself, as its attributes come in any order.
    static const QRegularExpression element(u"<[^>]+>"_s);
    const QString opacity = "opacity:"_L1 + QString::number(percent / 100.0, 'g', 3);
    for (const auto *variant : {"translucent/widgets", "widgets"}) {
        KCompressionDevice svgz(breeze + u'/' + QString::fromLatin1(variant) + "/panel-background.svgz"_L1, KCompressionDevice::GZip);
        if (!svgz.open(QIODevice::ReadOnly)) {
            continue;
        }
        const QString source = QString::fromUtf8(svgz.readAll());
        QString svg;
        qsizetype from = 0;
        for (auto it = element.globalMatch(source); it.hasNext();) {
            const auto match = it.next();
            QString tag = match.captured();
            if (tag.contains("class=\"ColorScheme-Background\""_L1)) {
                tag.replace("opacity:0.85"_L1, opacity);
            }
            svg += QStringView(source).mid(from, match.capturedStart() - from);
            svg += tag;
            from = match.capturedEnd();
        }
        svg += QStringView(source).mid(from);
        changed |= writeFile(styleDir() + u'/' + QString::fromLatin1(variant) + "/panel-background.svg"_L1, svg.toUtf8());
    }
    return changed;
}

void PanelStyle::setOpacity(int percent)
{
    percent = std::clamp(percent, 0, 100);
    KConfigGroup group = settings();
    group.writeEntry("Opacity", percent, KConfig::Notify);
    group.sync();

    const bool rewritten = write(percent);
    const QString current = currentStyle();
    if (current != s_style) {
        // what there was, for going back
        QDir().mkpath(QFileInfo(stateFile()).absolutePath());
        writeFile(stateFile(), current.toUtf8());
        QProcess::startDetached(u"plasma-apply-desktoptheme"_s, {s_style});
    } else if (rewritten) {
        // Plasma reads a style's pictures again when it is applied anew,
        // and it does not apply the one it has: by way of Breeze
        QProcess::execute(u"plasma-apply-desktoptheme"_s, {u"default"_s});
        QProcess::startDetached(u"plasma-apply-desktoptheme"_s, {s_style});
    }
    Q_EMIT changed();
}

void PanelStyle::restore()
{
    QFile before(stateFile());
    const QString previous = before.open(QIODevice::ReadOnly) ? QString::fromUtf8(before.readAll()).trimmed() : QString();
    QProcess::execute(u"plasma-apply-desktoptheme"_s, {previous.isEmpty() || previous == s_style ? u"default"_s : previous});
    QFile::remove(stateFile());
    Q_EMIT changed();
}
