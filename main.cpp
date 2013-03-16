#include <stdio.h>
#include <QtCore/QTextStream>
#include <QtCore/QFile>
#include <QtXml/qdom.h>
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

static QVariantMap*
config_from_dom(const QDomElement& mElement) {
    QVariantMap* cfg_data = new QVariantMap();
    QVariantList output_list;

    QDomElement cloneElement = mElement.firstChildElement("clone");
    if (!cloneElement.isNull()) {
        QString val = cloneElement.text();
        cfg_data->insert("cloned", val);
    }

    QDomElement outputElement = mElement.firstChildElement("output");
    while (!outputElement.isNull()) {
        QDomElement e = outputElement.firstChildElement();
        QVariantMap output;
        output.insert("name", outputElement.attribute("name"));
        while (!e.isNull()) {
            output.insert(e.tagName(), e.text());
            e = e.nextSiblingElement();
        }
        output_list.append(output);
        outputElement = outputElement.nextSiblingElement("output");
    }
    cfg_data->insert("outputs", output_list);
    return cfg_data;
}

static QVariantList*
dom_to_data(const QDomDocument& mDocument)
{
    QDomElement mDocumentElement = mDocument.documentElement();

    // Doublecheck that monitors file version="1"
    if (QString("1") != mDocumentElement.attribute("version"))
        return NULL;

    // Process all DOM elements into a list of maps
    QVariantList* configurations = new QVariantList();
    QDomElement mElement = mDocumentElement.firstChildElement("configuration");
    while (!mElement.isNull()) {
        QVariantMap* cfg_data = config_from_dom(mElement);
        configurations->append(*cfg_data);
        delete cfg_data;
        mElement = mElement.nextSiblingElement("configuration");
    }
    return configurations;
}

int
main(int argc, char *argv[])
{
    if (argc < 2)
        return fatal(1, "Usage: %s <path-to-monitors.xml>\n", "monxml2json");
    QString monitors_fname = argv[1];

    // Load file text
    QFile *monitors_xml_file = new QFile(monitors_fname);
    if (!monitors_xml_file->open(QIODevice::ReadOnly | QIODevice::Text))
        return fatal(1, "Error: Could not open %s", cstr(monitors_fname));

    // Load text into DOM
    QDomDocument mDocument;
    if (!mDocument.setContent(monitors_xml_file->readAll()))
        return fatal(2, "Error: Could not DOMify contents of %s\n", cstr(monitors_fname));
    monitors_xml_file->close();
    delete monitors_xml_file;

    // Convert DOM into a Qt variant data structure
    QVariantList* configurations = dom_to_data(mDocument);
    if (!configurations)
        return fatal(3, "Error: %s is wrong XML format - could not deserialize.\n", cstr(monitors_fname));

    // Serialize to JSON
    QJson::Serializer serializer;
    serializer.setIndentMode(QJson::IndentFull);
    QByteArray text = serializer.serialize(*configurations);
    delete configurations;
    printf("%s\n", text.constData());
    return 0;
}
