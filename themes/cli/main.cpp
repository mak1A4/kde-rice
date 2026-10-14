// kde-rice-theme: the themes from a terminal or a script, and for the parts
// of kde-rice that are not the hub (the launcher).
//
//   kde-rice-theme list                     every theme, "dark" or "light" after each
//   kde-rice-theme current                  the chosen theme and its background
//   kde-rice-theme set <theme> [<file>|--none]
//                                           choose and apply a theme, with one of its
//                                           backgrounds (by file name) or none
//   kde-rice-theme apply                    apply the chosen theme again
//   kde-rice-theme backgrounds [<theme>]    a theme's backgrounds
//   kde-rice-theme values [<theme>]         what the templates are filled in with
//   kde-rice-theme icons use                kde-rice's icon theme in use
//   kde-rice-theme icons base [<theme>]     its base theme (shown, or set)
//   kde-rice-theme icons set <name> <file>  your own picture for an icon name
//   kde-rice-theme icons reset <name>       the base theme's again
//   kde-rice-theme icons find <name>        that icon in every installed theme
//
// KDERICE_FILES_ONLY=1 writes the files and tells no program (for trying it out).

#include "applier.h"
#include "icons.h"
#include "theme.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QTextStream>

#include <algorithm>

using namespace Qt::StringLiterals;

static QTextStream out(stdout);
static QTextStream err(stderr);

static int usage()
{
    err << "usage: kde-rice-theme list | current | set <theme> [<background>|--none] | apply | backgrounds [<theme>] | values [<theme>]\n";
    return 2;
}

static int applyChosen(const ThemeStore::Choice &choice)
{
    QString warnings;
    const auto theme = ThemeStore::load(choice.theme, &warnings);
    if (!theme) {
        err << warnings << "\n";
        return 1;
    }
    if (!warnings.isEmpty()) {
        err << theme->path << ": " << warnings << "\n";
    }
    const QString background = choice.backgrounds.value(choice.theme) == "-"_L1 ? QString() : ThemeStore::backgroundFor(choice.theme, choice);
    ThemeApplier::Options options;
    options.filesOnly = qEnvironmentVariableIntValue("KDERICE_FILES_ONLY") == 1;
    const QStringList problems = ThemeApplier::apply(*theme, background, options);
    for (const QString &problem : problems) {
        err << problem << "\n";
    }
    return problems.isEmpty() ? 0 : 1;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QStringList arguments = app.arguments().mid(1);
    if (arguments.isEmpty()) {
        return usage();
    }
    const QString command = arguments.takeFirst();
    ThemeStore::Choice choice = ThemeStore::choice();

    if (command == "list"_L1) {
        const QStringList names = ThemeStore::names();
        for (const QString &name : names) {
            const auto theme = ThemeStore::load(name);
            out << name << "\t" << (theme && theme->isDark() ? "dark" : "light") << "\n";
        }
        return 0;
    }
    if (command == "current"_L1) {
        out << choice.theme << "\t" << QFileInfo(ThemeStore::backgroundFor(choice.theme, choice)).fileName() << "\n";
        return choice.theme.isEmpty() ? 1 : 0;
    }
    if (command == "set"_L1 && !arguments.isEmpty()) {
        const QString name = arguments.takeFirst();
        if (ThemeStore::pathOf(name).isEmpty()) {
            err << "no theme named \"" << name << "\"\n";
            return 1;
        }
        if (!arguments.isEmpty()) {
            const QString background = arguments.takeFirst();
            const QStringList all = ThemeStore::backgrounds(name);
            const bool known = std::any_of(all.cbegin(), all.cend(), [&background](const QString &path) {
                return QFileInfo(path).fileName() == background;
            });
            if (background != "--none"_L1 && !known) {
                err << "\"" << name << "\" has no background \"" << background << "\"\n";
                return 1;
            }
            // "-": none, chosen
            choice.backgrounds.insert(name, background == "--none"_L1 ? u"-"_s : background);
        }
        choice.theme = name;
        ThemeStore::saveChoice(choice);
        return applyChosen(choice);
    }
    if (command == "apply"_L1) {
        if (choice.theme.isEmpty()) {
            err << "no theme chosen\n";
            return 1;
        }
        return applyChosen(choice);
    }
    if (command == "backgrounds"_L1) {
        const QStringList all = ThemeStore::backgrounds(arguments.value(0, choice.theme));
        for (const QString &path : all) {
            out << path << "\n";
        }
        return 0;
    }
    if (command == "icons"_L1 && !arguments.isEmpty()) {
        const QString what = arguments.takeFirst();
        const auto theme = ThemeStore::load(choice.theme);
        const QString mode = theme && !theme->isDark() ? u"light"_s : u"dark"_s;
        if (what == "use"_L1) {
            IconStore::write(mode);
            IconStore::use();
            return 0;
        }
        if (what == "base"_L1) {
            if (!arguments.isEmpty()) {
                IconStore::setBase(arguments.first());
                if (IconStore::write(mode) && IconStore::inUse()) {
                    IconStore::reload();
                }
            }
            out << IconStore::base() << "\t" << IconStore::baseFor(mode) << "\n";
            return 0;
        }
        if (what == "set"_L1 && arguments.size() == 2) {
            if (!IconStore::setOverride(arguments[0], arguments[1])) {
                err << "cannot use " << arguments[1] << " for " << arguments[0] << " (an SVG or PNG file is needed)\n";
                return 1;
            }
            IconStore::reload();
            return 0;
        }
        if (what == "reset"_L1 && arguments.size() == 1) {
            IconStore::removeOverride(arguments[0]);
            IconStore::reload();
            return 0;
        }
        if (what == "find"_L1 && arguments.size() == 1) {
            const auto themes = IconStore::installed();
            for (const auto &installed : themes) {
                const QString path = IconStore::file(installed.id, arguments[0]);
                if (!path.isEmpty()) {
                    out << installed.id << "\t" << path << "\n";
                }
            }
            return 0;
        }
        return usage();
    }
    if (command == "values"_L1) {
        const auto theme = ThemeStore::load(arguments.value(0, choice.theme));
        if (!theme) {
            return 1;
        }
        const auto values = ThemeApplier::values(*theme);
        QStringList keys = values.keys();
        keys.sort();
        for (const QString &key : keys) {
            out << key << "\t" << values[key] << "\n";
        }
        return 0;
    }
    return usage();
}
