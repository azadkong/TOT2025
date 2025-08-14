#include "expressparser.h"
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>

QString ExpressParser::trimSemicolon(const QString &s) {
    QString t = s.trimmed();
    if (t.endsWith(';')) t.chop(1);
    return t.trimmed();
}

bool ExpressParser::parseFile(const QString &filePath, QString* err)
{
    classes_.clear();

    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (err) *err = QStringLiteral("无法打开EXP文件：%1").arg(filePath);
        return false;
    }
    QTextStream in(&f);
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
    in.setEncoding(QStringConverter::Utf8);
#else
    in.setCodec("utf-8");
#endif

    // 粗粒度状态机：在 ENTITY ... END_ENTITY; 块之间收集
    QRegularExpression reEntity(
        R"(^\s*ENTITY\s+([A-Za-z_][A-Za-z0-9_]*)\s*;?)",
        QRegularExpression::CaseInsensitiveOption);
    QRegularExpression reSubtype(R"(^\s*SUBTYPE\s+OF\s*\(\s*([A-Za-z_][A-Za-z0-9_]*)\s*\)\s*;?)", QRegularExpression::CaseInsensitiveOption);
    QRegularExpression reAttrLine(R"(^\s*([A-Za-z_][A-Za-z0-9_]*)\s*:\s*(.+);$)");

    bool inEntity = false;
    QString current;
    QStringList attrLines;
    QString parent;

    while (!in.atEnd()) {
        QString line = in.readLine();

        if (!inEntity) {
            auto m = reEntity.match(line);
            if (m.hasMatch()) {
                inEntity = true;
                current = m.captured(1).trimmed();
                attrLines.clear();
                parent.clear();
            }
            continue;
        }

        // inEntity == true
        if (line.trimmed().startsWith("END_ENTITY", Qt::CaseInsensitive)) {
            // 收尾
            ExpClassInfo info;
            info.name = current;
            info.parent = parent;
            for (const QString& al : attrLines) {
                info.attributes << trimSemicolon(al);
            }
            classes_.insert(info.name, info);

            inEntity = false;
            current.clear();
            attrLines.clear();
            parent.clear();
            continue;
        }

        // SUBTYPE OF
        {
            auto m = reSubtype.match(line);
            if (m.hasMatch()) {
                parent = m.captured(1).trimmed();
                continue;
            }
        }

        // 属性行（尽量简单匹配）
        {
            auto m = reAttrLine.match(line);
            if (m.hasMatch()) {
                attrLines << line.trimmed();
                continue;
            }
        }

        // 其它行忽略（如 WHERE、UNIQUE等），骨架阶段不处理
    }

    return true;
}

QHash<QString, QSet<QString>> ExpressParser::buildChildrenMap() const
{
    QHash<QString, QSet<QString>> children;
    for (const auto& k : classes_.keys()) {
        children[k]; // ensure key exists
    }
    for (const auto& info : classes_) {
        if (!info.parent.isEmpty()) {
            children[info.parent].insert(info.name);
        }
    }
    return children;
}
