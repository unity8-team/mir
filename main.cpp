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
    QJson::Parser parser;
    QByteArray text = "{ \"foo\":\"bar\" }";

    // Parse JSON
    bool ok = true;
    QVariantMap result = parser.parse(QByteArray(text), &ok).toMap();
    if (!ok) {
        fprintf(stderr, "Errors:\n%s\n", cstr(parser.errorString()));
        return 1;
    }

    printf("%s\n", cstr(result["foo"].toString()));

    return 0;
}
