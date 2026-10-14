#include "theme.h"
#include "colour.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>

using namespace Qt::StringLiterals;

bool Theme::isDark() const
{
    return Colour::isDark(color(u"bg"_s));
}

namespace ThemeStore
{

// Catppuccin Mocha: what a theme leaves out
static const QHash<QString, QString> s_defaults = {
    {u"bg"_s, u"#1e1e2e"_s},
    {u"surface"_s, u"#313244"_s},
    {u"surfaceHover"_s, u"#45475a"_s},
    {u"surfaceActive"_s, u"#585b70"_s},
    {u"fg"_s, u"#cdd6f4"_s},
    {u"fgDim"_s, u"#a6adc8"_s},
    {u"accent"_s, u"#89b4fa"_s},
    {u"accentFg"_s, u"#1e1e2e"_s},
    {u"warning"_s, u"#f9e2af"_s},
    {u"error"_s, u"#f38ba8"_s},
    {u"view"_s, u"#181825"_s},
    {u"terminalBackground"_s, u"#1e1e2e"_s},
    {u"terminalForeground"_s, u"#cdd6f4"_s},
    {u"terminalCursor"_s, u"#f5e0dc"_s},
    {u"terminalSelection"_s, u"#585b70"_s},
};
static const QStringList s_defaultTerminal = {
    u"#45475a"_s, u"#f38ba8"_s, u"#a6e3a1"_s, u"#f9e2af"_s, u"#89b4fa"_s, u"#f5c2e7"_s, u"#94e2d5"_s, u"#a6adc8"_s,
    u"#585b70"_s, u"#f37799"_s, u"#89d88b"_s, u"#ebd391"_s, u"#74a8fc"_s, u"#f2aede"_s, u"#6bd7ca"_s, u"#bac2de"_s,
};
static const QStringList s_defaultDesktops = {
    u"#89b4fa"_s, u"#cba6f7"_s, u"#f5c2e7"_s, u"#fab387"_s, u"#a6e3a1"_s, u"#94e2d5"_s, u"#f9e2af"_s, u"#f38ba8"_s,
};

static QString configDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/kde-rice"_L1;
}

static QString dataDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/kde-rice"_L1;
}

QString userDir()
{
    return configDir() + "/themes"_L1;
}

QStringList dirs()
{
    QStringList found = {userDir()};
    found += QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, u"kde-rice/themes"_s, QStandardPaths::LocateDirectory);
#ifdef KDERICE_SOURCE_THEMES
    // not installed: the repository's own, for running from the build directory
    found += QStringLiteral(KDERICE_SOURCE_THEMES);
#endif
    found.removeDuplicates();
    return found;
}

QStringList names()
{
    QStringList found;
    for (const QString &dir : dirs()) {
        const auto files = QDir(dir).entryList({u"*.json"_s}, QDir::Files | QDir::Readable);
        for (const QString &file : files) {
            found += file.chopped(5);
        }
    }
    found.removeDuplicates();
    found.sort();
    return found;
}

QString title(const QString &name)
{
    QStringList words = name.split(QRegularExpression(u"[-_]"_s), Qt::SkipEmptyParts);
    for (QString &word : words) {
        word[0] = word[0].toUpper();
    }
    return words.join(u' ');
}

QString pathOf(const QString &name)
{
    if (name.isEmpty() || name.contains(u'/')) {
        return {};
    }
    for (const QString &dir : dirs()) {
        const QString path = dir + u'/' + name + ".json"_L1;
        if (QFileInfo::exists(path)) {
            return path;
        }
    }
    return {};
}

static std::optional<QColor> colourOf(const QJsonValue &value)
{
    static const QRegularExpression pattern(u"^#([0-9a-f]{3}|[0-9a-f]{6}|[0-9a-f]{8})$"_s, QRegularExpression::CaseInsensitiveOption);
    if (!value.isString() || !pattern.match(value.toString()).hasMatch()) {
        return std::nullopt;
    }
    // "#aarrggbb" is allowed in a theme; what is written down is opaque
    QColor colour(value.toString());
    colour.setAlpha(255);
    return colour;
}

static QList<QColor> listOf(const QJsonValue &value, qsizetype atLeast, const QStringList &fallback, const QString &what, QString *warnings)
{
    QList<QColor> colours;
    if (value.isArray()) {
        for (const QJsonValue &item : value.toArray()) {
            const auto colour = colourOf(item);
            if (!colour) {
                colours.clear();
                break;
            }
            colours += *colour;
        }
    }
    if (!value.isUndefined() && colours.size() < atLeast) {
        *warnings += u"\"%1\" needs %2 colours at least, ignored\n"_s.arg(what).arg(atLeast);
        colours.clear();
    }
    if (colours.isEmpty()) {
        for (const QString &colour : fallback) {
            colours += QColor(colour);
        }
    }
    return colours;
}

std::optional<Theme> load(const QString &name, QString *error)
{
    const QString path = pathOf(name);
    QFile file(path);
    if (path.isEmpty() || !file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = u"no theme named \"%1\""_s.arg(name);
        }
        return std::nullopt;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (!document.isObject()) {
        if (error) {
            *error = u"%1: %2"_s.arg(path, parseError.errorString());
        }
        return std::nullopt;
    }

    Theme theme;
    theme.name = name;
    theme.title = title(name);
    theme.path = path;
    QString warnings;
    const QJsonObject colors = document[u"colors"_s].toObject();
    for (auto it = s_defaults.cbegin(); it != s_defaults.cend(); ++it) {
        theme.colors.insert(it.key(), QColor(it.value()));
    }
    for (auto it = colors.constBegin(); it != colors.constEnd(); ++it) {
        if (it.key() == "terminal"_L1 || it.key() == "desktops"_L1) {
            continue;
        }
        if (const auto colour = colourOf(it.value())) {
            theme.colors.insert(it.key(), *colour);
        } else {
            warnings += u"\"%1\" is not a colour, ignored\n"_s.arg(it.key());
        }
    }
    theme.terminal = listOf(colors[u"terminal"_s], 16, s_defaultTerminal, u"terminal"_s, &warnings).mid(0, 16);
    theme.desktops = listOf(colors[u"desktops"_s], 2, s_defaultDesktops, u"desktops"_s, &warnings);
    theme.apps = document[u"apps"_s].toObject().toVariantMap();
    if (error) {
        *error = warnings.trimmed();
    }
    return theme;
}

static QString choicePath()
{
    return configDir() + "/theme.json"_L1;
}

Choice choice()
{
    Choice choice;
    QFile file(choicePath());
    if (file.open(QIODevice::ReadOnly)) {
        const QJsonObject object = QJsonDocument::fromJson(file.readAll()).object();
        choice.theme = object[u"theme"_s].toString();
        const QJsonObject backgrounds = object[u"backgrounds"_s].toObject();
        for (auto it = backgrounds.constBegin(); it != backgrounds.constEnd(); ++it) {
            choice.backgrounds.insert(it.key(), it.value().toString());
        }
    }
    return choice;
}

void saveChoice(const Choice &choice)
{
    QJsonObject backgrounds;
    for (auto it = choice.backgrounds.cbegin(); it != choice.backgrounds.cend(); ++it) {
        backgrounds.insert(it.key(), it.value());
    }
    QJsonObject object;
    object.insert(u"theme"_s, choice.theme);
    object.insert(u"backgrounds"_s, backgrounds);
    QDir().mkpath(configDir());
    QSaveFile file(choicePath());
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(object).toJson());
        file.commit();
    }
}

QString backgroundsDir(const QString &name)
{
    return dataDir() + "/backgrounds/"_L1 + name;
}

bool isVideo(const QString &path)
{
    static const QRegularExpression pattern(u"\\.(mp4|webm|mkv|mov|gif)$"_s, QRegularExpression::CaseInsensitiveOption);
    return pattern.match(path).hasMatch();
}

QStringList backgrounds(const QString &name)
{
    static const QStringList patterns = {u"*.png"_s, u"*.jpg"_s, u"*.jpeg"_s, u"*.webp"_s, u"*.avif"_s, u"*.jxl"_s,
                                         u"*.mp4"_s, u"*.webm"_s, u"*.mkv"_s, u"*.mov"_s, u"*.gif"_s};
    QDir dir(backgroundsDir(name));
    dir.setNameFilters(patterns);
    QStringList found;
    const auto files = dir.entryInfoList(QDir::Files | QDir::Readable, QDir::Name | QDir::IgnoreCase);
    for (const QFileInfo &file : files) {
        found += file.absoluteFilePath();
    }
    return found;
}

QString backgroundFor(const QString &name, const Choice &choice)
{
    const QStringList all = backgrounds(name);
    const QString chosen = choice.backgrounds.value(name);
    for (const QString &path : all) {
        if (QFileInfo(path).fileName() == chosen) {
            return path;
        }
    }
    return all.value(0);
}

static QString thumbnailPath(const QString &background)
{
    const QFileInfo info(background);
    // a new picture under an old name is a new thumbnail
    const QByteArray key = (info.absoluteFilePath() + QString::number(info.lastModified().toSecsSinceEpoch())).toUtf8();
    return QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + "/kde-rice/thumbnails/"_L1
        + QString::fromLatin1(QCryptographicHash::hash(key, QCryptographicHash::Sha1).toHex()) + ".jpg"_L1;
}

QString thumbnail(const QString &background)
{
    const QString path = thumbnailPath(background);
    return QFileInfo::exists(path) ? path : QString();
}

QString makeThumbnail(const QString &background)
{
    const QString path = thumbnailPath(background);
    if (QFileInfo::exists(path)) {
        return path;
    }
    QDir().mkpath(QFileInfo(path).absolutePath());
    QStringList arguments = {u"-v"_s, u"error"_s, u"-y"_s};
    if (isVideo(background)) {
        arguments += {u"-ss"_s, u"2"_s};
    }
    arguments += {u"-i"_s, background, u"-frames:v"_s, u"1"_s, u"-vf"_s,
                  u"scale=480:270:force_original_aspect_ratio=increase,crop=480:270"_s, u"-q:v"_s, u"4"_s, path + ".part.jpg"_L1};
    QProcess ffmpeg;
    ffmpeg.setStandardInputFile(QProcess::nullDevice());
    ffmpeg.start(u"ffmpeg"_s, arguments);
    if (!ffmpeg.waitForFinished(30000) || ffmpeg.exitCode() != 0) {
        QFile::remove(path + ".part.jpg"_L1);
        return {};
    }
    QFile::remove(path);
    QFile::rename(path + ".part.jpg"_L1, path);
    return path;
}

}
