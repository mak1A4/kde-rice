// kde-rice-trim-icons <directory>...: the app icons in the directories (SVG
// files, links left alone, symbolic ones skipped) trimmed to fill their
// canvas (see src/trim.h). For building kde-rice-whitesur.
#include "trim.h"

#include <QCoreApplication>
#include <QDirIterator>
#include <QFileInfo>
#include <QTextStream>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    int trimmed = 0, total = 0;
    const QStringList dirs = app.arguments().mid(1);
    for (const QString &dir : dirs) {
        QDirIterator files(dir, {QStringLiteral("*.svg")}, QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
        while (files.hasNext()) {
            const QString path = files.next();
            if (path.contains(QLatin1String("symbolic"))) {
                continue;
            }
            ++total;
            trimmed += IconTrim::trimFile(path) ? 1 : 0;
        }
    }
    QTextStream(stdout) << trimmed << " of " << total << " icons trimmed\n";
    return 0;
}
