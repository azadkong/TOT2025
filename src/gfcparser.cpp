#include "gfcparser.h"
#include <QRegularExpression>

QHash<QString, int> GfcParser::countClasses(const QString &wholeText,
                                            QVector<GfcInstanceRef>* instanceRefs)
{
    QHash<QString, int> counts;
    if (instanceRefs) instanceRefs->clear();

    // 匹配类似：  #41=GFCWALL(...);
    QRegularExpression reLine(R"(#\s*([0-9]+)\s*=\s*([A-Za-z_0-9]+)\s*\()");

    auto it = reLine.globalMatch(wholeText);
    while (it.hasNext()) {
        auto m = it.next();
        int idx = m.captured(1).toInt();
        QString cls = m.captured(2).trimmed();

        counts[cls] += 1;

        if (instanceRefs) {
            GfcInstanceRef ref;
            ref.index = idx;
            ref.cls = cls;
            ref.pos = m.capturedStart(0);
            instanceRefs->push_back(ref);
        }
    }
    return counts;
}

int GfcParser::parseInstanceIndex(const QString &token)
{
    // 形如 "#123"
    QRegularExpression re(R"(^\s*#\s*([0-9]+)\s*$)");
    auto m = re.match(token);
    if (!m.hasMatch()) return -1;
    return m.captured(1).toInt();
}
