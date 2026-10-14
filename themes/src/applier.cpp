#include "applier.h"
#include "colour.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>

#include <cstdio>

using namespace Qt::StringLiterals;

namespace ThemeApplier
{

static const auto s_wallpaper = u"io.github.mak1a4.kde-rice.background"_s;

static QString hex(const QColor &colour)
{
    return colour.name(QColor::HexRgb);
}

// KDE's colour for what is selected. A selected row in Dolphin and in KDE's
// lists is filled with a share of it over the view (a third, two thirds
// under the pointer), the text on it is the ordinary text, and round it goes
// an outline in the colour itself, a little darker (or on a dark view
// lighter). With the accent as it is, that asks too much twice: the text is
// hard to read in every theme whose accent is not as bright as Breeze's
// blue, and the outline, drawn in steps at a fractional scale, shows its
// steps the more the darker it is against the view. So the accent is taken
// towards the view until the text can be read and the outline stands out
// from the view by no more than 1.7: pale, at the user's wish.
static QColor selectionFor(const Theme &theme)
{
    const QColor accent = theme.color(u"accent"_s);
    const QColor view = theme.color(u"view"_s);
    const QColor bg = theme.color(u"bg"_s);
    const QColor fg = theme.color(u"fg"_s);
    const bool dark = Colour::isDark(view);
    QColor selection = accent;
    // Added up step by step as the shell did, rounding errors and all: the
    // same themes give the same colours to the digit (and the last step,
    // 0.9, is never reached).
    for (double share = 0; share <= 0.9; share += 0.05) {
        const QColor candidate = Colour::between(accent, view, share);
        // Not so pale that it turns light: under the pointer Breeze draws a
        // selected row a tenth lighter than the colour, and on a light
        // theme that must still be darker than the window behind it (see
        // ChangeSelectionColor in kde.colors.tpl).
        if (!dark && share > 0) {
            const QColor lifted = Colour::scaled(candidate, 1.1);
            if (Colour::luminance(lifted) >= Colour::luminance(bg) || Colour::contrast(lifted, bg) < 1.1) {
                break;
            }
        }
        selection = candidate;
        // as Dolphin makes the outline: Qt's darker(110) or lighter(110)
        const QColor outline = Colour::scaled(selection, dark ? 1.1 : 1 / 1.1);
        if (Colour::contrast(fg, Colour::between(view, selection, 0.65)) >= 4.5 && Colour::contrast(outline, view) <= 1.7) {
            break;
        }
    }
    return selection;
}

QHash<QString, QString> values(const Theme &theme)
{
    const bool dark = theme.isDark();
    QHash<QString, QColor> colours;
    for (const auto *name : {"bg", "view", "surface", "surfaceHover", "surfaceActive", "fg", "fgDim", "accent", "accentFg", "warning", "error",
                             "terminalBackground", "terminalForeground", "terminalCursor", "terminalSelection"}) {
        colours.insert(QString::fromLatin1(name), theme.color(QString::fromLatin1(name)));
    }
    for (qsizetype i = 0; i < theme.terminal.size(); ++i) {
        colours.insert(u"terminal%1"_s.arg(i), theme.terminal[i]);
    }
    const auto c = [&colours](const char *name) {
        return colours.value(QString::fromLatin1(name));
    };
    // what KDE's colour scheme has names for and the theme does not
    colours.insert(u"positive"_s, theme.terminal[2]);
    colours.insert(u"link"_s, theme.terminal[4]);
    colours.insert(u"visited"_s, theme.terminal[5]);
    colours.insert(u"viewAlternate"_s, Colour::between(c("view"), c("bg"), 0.5));
    colours.insert(u"buttonAlternate"_s, Colour::between(c("surface"), c("accent"), 0.3));
    const QColor selection = selectionFor(theme);
    colours.insert(u"selection"_s, selection);
    // on it, where it is drawn whole (selected text): whichever reads better
    const QColor selectionFg =
        Colour::contrast(c("fg"), selection) >= Colour::contrast(c("accentFg"), selection) ? c("fg") : c("accentFg");
    colours.insert(u"selectionFg"_s, selectionFg);
    colours.insert(u"selectionAlternate"_s, Colour::between(selection, c("bg"), 0.4));
    colours.insert(u"selectionInactive"_s, Colour::between(selectionFg, selection, 0.4));
    // "complementary" is the dark set KDE uses on top of pictures: of a
    // light theme its text colour as background
    colours.insert(u"complementaryBg"_s, dark ? c("view") : c("fg"));
    colours.insert(u"complementaryFg"_s, dark ? c("fg") : c("bg"));
    colours.insert(u"complementaryDim"_s, dark ? c("fgDim") : Colour::between(c("bg"), c("fg"), 0.35));

    QHash<QString, QString> values;
    values.insert(u"name"_s, theme.name);
    values.insert(u"title"_s, theme.title);
    // as KDE knows the colour scheme
    values.insert(u"scheme"_s, "KDERice"_L1 + QString(theme.title).remove(QRegularExpression(u"[^A-Za-z0-9]"_s)));
    values.insert(u"mode"_s, dark ? u"dark"_s : u"light"_s);
    values.insert(u"nvim"_s, theme.apps.value(u"nvim"_s, u"default"_s).toString());
    values.insert(u"vscode"_s, theme.apps.value(u"vscode"_s).toString());
    values.insert(u"btop"_s, theme.apps.value(u"btop"_s).toString());
    for (auto it = colours.cbegin(); it != colours.cend(); ++it) {
        const QColor &colour = it.value();
        values.insert(it.key(), hex(colour));
        values.insert(it.key() + "_rgb"_L1, u"%1,%2,%3"_s.arg(colour.red()).arg(colour.green()).arg(colour.blue()));
        values.insert(it.key() + "_strip"_L1, hex(colour).mid(1));
    }
    // the dark edge libadwaita draws where one surface lies on another
    values.insert(u"shade"_s, dark ? u"rgba(0, 0, 0, 0.36)"_s : u"rgba(0, 0, 0, 0.12)"_s);
    return values;
}

QString outputDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericStateLocation) + "/kde-rice/theme"_L1;
}

static QString stateDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericStateLocation) + "/kde-rice"_L1;
}

static QString configHome()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
}

static QString templatesDir()
{
    QString dir = QStandardPaths::locate(QStandardPaths::GenericDataLocation, u"kde-rice/templates"_s, QStandardPaths::LocateDirectory);
#ifdef KDERICE_SOURCE_TEMPLATES
    if (dir.isEmpty()) {
        dir = QStringLiteral(KDERICE_SOURCE_TEMPLATES);
    }
#endif
    return dir;
}

// Writes `text` to `path` if that changes the file. True if it did.
static bool write(const QString &path, const QByteArray &text)
{
    QFile current(path);
    if (current.open(QIODevice::ReadOnly) && current.readAll() == text) {
        return false;
    }
    current.close();
    QDir().mkpath(QFileInfo(path).absolutePath());
    // into the file, which may be a link into somewhere else (dotfiles)
    QFileInfo info(path);
    if (info.isSymLink()) {
        QFile file(info.symLinkTarget());
        return file.open(QIODevice::WriteOnly | QIODevice::Truncate) && file.write(text) == text.size();
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.write(text);
    return file.commit();
}

struct Context {
    QHash<QString, QString> values;
    QString templates;
    Options options;
    QStringList problems;

    // Fills template `name` in as file `path`. True if that changed the file.
    bool render(const QString &name, const QString &path)
    {
        QFile file(templates + u'/' + name);
        if (!file.open(QIODevice::ReadOnly)) {
            problems += u"template %1 not found in %2"_s.arg(name, templates);
            return false;
        }
        QString text = QString::fromUtf8(file.readAll());
        static const QRegularExpression placeholder(u"\\{\\{ ([A-Za-z0-9_]+) \\}\\}"_s);
        QString filled;
        qsizetype from = 0;
        for (auto it = placeholder.globalMatch(text); it.hasNext();) {
            const auto match = it.next();
            filled += QStringView(text).mid(from, match.capturedStart() - from);
            filled += values.value(match.captured(1));
            from = match.capturedEnd();
        }
        filled += QStringView(text).mid(from);
        return write(path, filled.toUtf8());
    }

    // Runs a program and waits for it (briefly). Its exit code, -1 if it
    // could not run.
    int run(const QString &program, const QStringList &arguments, int timeout = 10000)
    {
        QProcess process;
        process.setStandardInputFile(QProcess::nullDevice());
        process.start(program, arguments);
        if (!process.waitForFinished(timeout)) {
            process.kill();
            return -1;
        }
        return process.exitStatus() == QProcess::NormalExit ? process.exitCode() : -1;
    }

    bool tell() const
    {
        return !options.filesOnly;
    }
};

static void applyKde(Context &context)
{
    const QString scheme = context.values[u"scheme"_s];
    const QString mode = context.values[u"mode"_s];
    const QString data = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    const bool recoloured = context.render(u"kde.colors.tpl"_s, data + "/color-schemes/"_L1 + scheme + ".colors"_L1);
    auto globals = KSharedConfig::openConfig(u"kdeglobals"_s, KConfig::NoGlobals);
    const QString current = globals->group(u"General"_s).readEntry("ColorScheme");
    if (context.tell() && (recoloured || current != scheme)) {
        // what KDE had before the first theme, to go back to by hand
        const QString before = stateDir() + "/colorscheme-before"_L1;
        if (!current.startsWith("KDERice"_L1) && !current.isEmpty() && !QFileInfo::exists(before)) {
            write(before, current.toUtf8() + '\n');
        }
        // It will not take the scheme it already has, also not after its
        // file has changed: by way of another one, then.
        if (current == scheme) {
            context.run(u"plasma-apply-colorscheme"_s, {mode == "dark"_L1 ? u"BreezeDark"_s : u"BreezeLight"_s});
        }
        if (context.run(u"plasma-apply-colorscheme"_s, {scheme}) != 0) {
            context.problems += u"plasma-apply-colorscheme %1 failed"_s.arg(scheme);
        }
    }

    // an icon theme that comes as a pair goes with the mode
    if (context.tell()) {
        globals->reparseConfiguration();
        const QString icons = globals->group(u"Icons"_s).readEntry("Theme");
        QString wanted = icons;
        if (mode == "dark"_L1 && icons.endsWith("-Light"_L1)) {
            wanted = icons.chopped(6) + "-Dark"_L1;
        } else if (mode == "light"_L1 && icons.endsWith("-Dark"_L1)) {
            wanted = icons.chopped(5) + "-Light"_L1;
        }
        if (wanted != icons
            && !QStandardPaths::locate(QStandardPaths::GenericDataLocation, "icons/"_L1 + wanted, QStandardPaths::LocateDirectory).isEmpty()) {
            context.run(u"/usr/lib/plasma-changeicons"_s, {wanted});
        }
    }
}

// GTK applications have KDE's colours from KDE itself, which gives them to
// its Breeze theme for GTK. All but the ones made with libadwaita, which
// take no theme: for those, libadwaita's own colour names, in a file of ours
// that the user's style sheet imports. KDE rewrites that style sheet at a
// change of colour scheme and keeps what it finds there. An application
// reads it when it starts.
static void applyGtk(Context &context)
{
    const QString dir = configHome() + "/gtk-4.0"_L1;
    if (!QFileInfo::exists(dir) && QStandardPaths::findExecutable(u"gtk4-launch"_s).isEmpty()) {
        return;
    }
    context.render(u"gtk4.css.tpl"_s, dir + "/kde-rice.css"_L1);
    const QByteArray line = "@import 'kde-rice.css';";
    QFile sheet(dir + "/gtk.css"_L1);
    if (sheet.open(QIODevice::ReadWrite | QIODevice::Append)) {
        sheet.seek(0);
        if (!sheet.readAll().contains(line)) {
            sheet.write("\n" + line + "\n");
        }
    }
}

static void applyTerminals(Context &context)
{
    const QString out = outputDir();
    // Ghostty reads its configuration again at this signal
    if (context.render(u"ghostty.conf.tpl"_s, out + "/ghostty.conf"_L1) && context.tell()) {
        context.run(u"pkill"_s, {u"-USR2"_s, u"-x"_s, u"ghostty"_s});
    }
    // WezTerm watches the file itself (wezterm.add_to_config_reload_watch_list)
    context.render(u"wezterm.lua.tpl"_s, out + "/wezterm.lua"_L1);
    if (context.render(u"tmux.conf.tpl"_s, out + "/tmux.conf"_L1) && context.tell()) {
        context.run(u"tmux"_s, {u"source-file"_s, out + "/tmux.conf"_L1});
    }
    if (context.render(u"nvim.lua.tpl"_s, out + "/nvim.lua"_L1) && context.tell()) {
        const QString runtime = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
        const auto sockets = QDir(runtime).entryInfoList({u"nvim.*.0"_s}, QDir::System);
        for (const QFileInfo &socket : sockets) {
            context.run(u"nvim"_s, {u"--server"_s, socket.absoluteFilePath(), u"--remote-send"_s, "<Cmd>luafile "_L1 + out + "/nvim.lua<CR>"_L1}, 2000);
        }
    }
    write(out + "/mode"_L1, context.values[u"mode"_s].toUtf8() + '\n');
}

static void applyBtop(Context &context)
{
    if (QStandardPaths::findExecutable(u"btop"_s).isEmpty()) {
        return;
    }
    const QString dir = configHome() + "/btop"_L1;
    const QString theme = dir + "/themes/kde-rice.theme"_L1;
    bool retinted = false;
    // the theme btop ships of the same name where it has one
    QFile own(u"/usr/share/btop/themes/%1.theme"_s.arg(context.values[u"btop"_s]));
    if (!context.values[u"btop"_s].isEmpty() && own.open(QIODevice::ReadOnly)) {
        retinted = write(theme, own.readAll());
    } else {
        retinted = context.render(u"btop.theme.tpl"_s, theme);
    }
    // btop is told once which theme is its own; it writes the rest of its
    // configuration itself
    QFile conf(dir + "/btop.conf"_L1);
    QByteArray text;
    if (conf.open(QIODevice::ReadOnly)) {
        text = conf.readAll();
        conf.close();
    }
    static const QRegularExpression entry(u"^color_theme = .*$"_s, QRegularExpression::MultilineOption);
    const QString wanted = u"color_theme = \"kde-rice\""_s;
    QString changed = QString::fromUtf8(text);
    if (entry.match(changed).hasMatch()) {
        changed.replace(entry, wanted);
    } else {
        changed += wanted + u'\n';
    }
    write(dir + "/btop.conf"_L1, changed.toUtf8());
    if (retinted && context.tell()) {
        context.run(u"pkill"_s, {u"-USR2"_s, u"-x"_s, u"btop"_s});
    }
}

// VS Code's settings name the colour theme, and it reads them again when
// they change. All three names: with "follow the system" on, it takes the
// dark or the light one and not the first. Its extension has to be
// installed.
static void applyVsCode(Context &context)
{
    const QString label = context.values[u"vscode"_s];
    if (label.isEmpty()) {
        return;
    }
    for (const auto *editor : {"Code", "Code - OSS", "VSCodium"}) {
        const QString path = configHome() + u'/' + QString::fromLatin1(editor) + "/User/settings.json"_L1;
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            continue;
        }
        QString text = QString::fromUtf8(file.readAll());
        file.close();
        for (const auto *key : {"workbench.colorTheme", "workbench.preferredDarkColorTheme", "workbench.preferredLightColorTheme"}) {
            const QRegularExpression existing(u"(\"%1\"\\s*:\\s*)\"[^\"]*\""_s.arg(QRegularExpression::escape(QString::fromLatin1(key))));
            const QString quoted = u"\"%1\""_s.arg(label);
            if (existing.match(text).hasMatch()) {
                text.replace(existing, "\\1"_L1 + quoted);
            } else {
                const qsizetype brace = text.indexOf(u'{');
                if (brace >= 0) {
                    text.insert(brace + 1, u"\n    \"%1\": %2,"_s.arg(QString::fromLatin1(key), quoted));
                }
            }
        }
        write(path, text.toUtf8());
    }
}

// ---- the background ------------------------------------------------------

static QString evaluateInPlasma(const QString &script)
{
    auto message = QDBusMessage::createMethodCall(u"org.kde.plasmashell"_s, u"/PlasmaShell"_s, u"org.kde.PlasmaShell"_s, u"evaluateScript"_s);
    message << script;
    const QDBusReply<QString> reply = QDBusConnection::sessionBus().call(message, QDBus::Block, 10000);
    return reply.isValid() ? reply.value().trimmed() : QString();
}

static QString desktopPlugin()
{
    return evaluateInPlasma(u"const d = desktops().find(d => d.screen >= 0) ?? desktops()[0]; print(d ? d.wallpaperPlugin : '')"_s);
}

static void setDesktopPlugin(const QString &plugin)
{
    evaluateInPlasma(u"for (const d of desktops()) if (d.wallpaperPlugin !== '%1') d.wallpaperPlugin = '%1'"_s.arg(plugin));
}

static KConfigGroup lockGroup()
{
    return KSharedConfig::openConfig(u"kscreenlockerrc"_s, KConfig::NoGlobals)->group(u"Greeter"_s);
}

// Leaves `from` as the only file in `dir`, as a link named after the theme:
// a new name is what the wallpaper takes for a new background. The new link
// is made before the old one is taken away, so the directory is never empty
// in between: an empty one means "no background" to the wallpaper.
static void fill(const QString &dir, const QString &from, const QString &name)
{
    QDir().mkpath(dir);
    QString target;
    if (!from.isEmpty()) {
        target = dir + u'/' + name + u'.' + QFileInfo(from).suffix();
        if (QFileInfo(target).symLinkTarget() != from) {
            QFile::remove(target + ".part"_L1);
            QFile::link(from, target + ".part"_L1);
            std::rename(QFile::encodeName(target + ".part"_L1).constData(), QFile::encodeName(target).constData());
        }
    }
    const auto entries = QDir(dir).entryInfoList(QDir::Files | QDir::System | QDir::Hidden | QDir::NoDotAndDotDot);
    for (const QFileInfo &entry : entries) {
        if (entry.absoluteFilePath() != target) {
            QFile::remove(entry.absoluteFilePath());
        }
    }
}

static void applyBackground(Context &context, const QString &file)
{
    const bool installed = !QStandardPaths::locate(QStandardPaths::GenericDataLocation, "plasma/wallpapers/"_L1 + s_wallpaper, QStandardPaths::LocateDirectory).isEmpty();
    const QString own = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/kde-rice/background"_L1;
    const QString before = stateDir() + "/wallpaper-before"_L1;
    fill(own, file, context.values[u"name"_s]);
    if (!context.tell()) {
        return;
    }
    if (!file.isEmpty()) {
        if (!installed) {
            context.problems += u"the wallpaper plugin %1 is not installed: the background is not shown"_s.arg(s_wallpaper);
            return;
        }
        // what the desktop and the lock screen showed before, noted once
        const QString desktop = desktopPlugin();
        KConfigGroup lock = lockGroup();
        const QString locked = lock.readEntry("WallpaperPlugin", u"org.kde.image"_s);
        if (!QFileInfo::exists(before) && !desktop.isEmpty()) {
            QDir().mkpath(stateDir());
            write(before, (desktop + u'\n' + locked + u'\n').toUtf8());
        }
        if (!desktop.isEmpty() && desktop != s_wallpaper) {
            setDesktopPlugin(s_wallpaper);
        }
        if (locked != s_wallpaper) {
            lock.writeEntry("WallpaperPlugin", s_wallpaper, KConfig::Notify);
            lock.sync();
        }
        return;
    }
    // no background: what they had before, back
    QFile noted(before);
    if (!noted.open(QIODevice::ReadOnly)) {
        return;
    }
    const QStringList lines = QString::fromUtf8(noted.readAll()).split(u'\n');
    noted.close();
    const QString desktop = lines.value(0).isEmpty() ? u"org.kde.image"_s : lines.value(0);
    const QString locked = lines.value(1).isEmpty() ? u"org.kde.image"_s : lines.value(1);
    if (desktopPlugin() == s_wallpaper) {
        setDesktopPlugin(desktop);
    }
    KConfigGroup lock = lockGroup();
    if (lock.readEntry("WallpaperPlugin") == s_wallpaper) {
        // Plasma's own is what the lock screen has without the entry
        if (locked == "org.kde.image"_L1) {
            lock.deleteEntry("WallpaperPlugin", KConfig::Notify);
        } else {
            lock.writeEntry("WallpaperPlugin", locked, KConfig::Notify);
        }
        lock.sync();
    }
    QFile::remove(before);
}

QStringList apply(const Theme &theme, const QString &background, const Options &options)
{
    Context context{values(theme), templatesDir(), options, {}};
    if (context.templates.isEmpty()) {
        return {u"kde-rice's templates are not installed"_s};
    }
    QDir().mkpath(outputDir());
    applyKde(context);
    applyGtk(context);
    applyTerminals(context);
    applyBtop(context);
    applyVsCode(context);
    applyBackground(context, background);
    return context.problems;
}

}
