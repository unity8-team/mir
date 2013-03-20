#include <stdio.h>
#include <QtCore/QTextStream>
#include <QtCore/QFile>
#include <qjson/parser.h>
#include <qjson/serializer.h>

static const char*
cstr(const QString& s)
{
    return s.toAscii().constData();
}

static int
fatal(int error_number, const char* msg, const char* detail)
{
    fprintf(stderr, msg, detail);
    return error_number;
}


int main(int argc, char *argv[])
{
    if (argc < 2)
        return fatal(1, "Usage: %s <path-to-monitors.json> [command]\n", "monjson");
    QString monitors_fname = argv[1];

    QString command = "list";
    if (argc > 2)
        command = argv[2];

    // Load configuration text
    QFile *monitors_file = new QFile(monitors_fname);
    if (!monitors_file->open(QIODevice::ReadOnly | QIODevice::Text))
        return fatal(1, "Error: Could not open %s", cstr(monitors_fname));
    QByteArray text = monitors_file->readAll();
    delete monitors_file;

    // Parse JSON
    QJson::Parser parser;
    bool ok = true;
    QVariantList result = parser.parse(QByteArray(text), &ok).toList();
    if (!ok) {
        fprintf(stderr, "Errors:\n%s\n", cstr(parser.errorString()));
        return 1;
    }

    // Process results
    if (command == "list") {
        for (int i=0; i<result.length(); i++) {
            QVariantMap config = result[i].toMap();
            QString pad = "";
            printf("%2i ", i);
            if (config["cloned"].toString() == "yes")
                printf("cloned ");
            else
                printf("       ");
            QVariantList outputs = config["outputs"].toList();
            for (int j=0; j<outputs.length(); j++) {
                QVariantMap output = outputs[j].toMap();
                if (output["height"].toString() == "")
                    continue;
                printf("%s", cstr(pad));
                printf("%-8s ", cstr(output["vendor"].toString()));
                printf("%-8s ", cstr(output["name"].toString()));
                printf("%sx%s", cstr(output["height"].toString()), cstr(output["width"].toString()));
                printf("@%s", cstr(output["x"].toString()));
                printf(",%s", cstr(output["y"].toString()));
                if (output["primary"] == "yes")
                    printf("  primary");
                printf("\n");
                pad = "          ";
            }
            printf("\n");
        }
    }

    return 0;
}
