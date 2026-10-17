#include "pdfextract.h"
#include <QCoreApplication>
#include <QTextStream>
#include <QDebug>

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    const QStringList args = a.arguments();
    if (args.size() < 2) {
        qWarning() << "usage: pdfextract_test <pdf-file>";
        return 1;
    }
    const QString text = extractPdfTextSimple(args.at(1));
    QTextStream out(stdout);
    out << "=== extracted ===\n" << text << "\n=== end ===\n";
    return 0;
}
