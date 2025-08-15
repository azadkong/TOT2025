#include "gfcparser.h"
#include <QRegularExpression>

// === 放在 gfcparser.cpp 末尾（或合适位置） ===
#include <QRegularExpression>

static inline bool isQuoteEscape(const QString& t, int i) {
    // STEP风格字符串用两个连续单引号表示转义，如 'it''s ok'
    return (i + 1 < t.size() && t[i] == '\'' && t[i + 1] == '\'');
}

QStringList GfcParser::splitTopLevelCsv(const QString& s)
{
    QStringList out;
    QString cur; cur.reserve(s.size());
    int depth = 0;
    bool inStr = false;
    for (int i = 0; i < s.size(); ++i) {
        const QChar ch = s[i];
        if (inStr) {
            cur += ch;
            if (isQuoteEscape(s, i)) { cur += s[i + 1]; ++i; continue; }
            if (ch == '\'') inStr = false;
            continue;
        }
        if (ch == '\'') { inStr = true; cur += ch; continue; }
        if (ch == '(') { ++depth; cur += ch; continue; }
        if (ch == ')') { --depth; cur += ch; continue; }
        if (ch == ',' && depth == 0) {
            out << cur.trimmed(); cur.clear(); continue;
        }
        cur += ch;
    }
    if (!cur.trimmed().isEmpty() || (s.size() && s.back() == ',')) out << cur.trimmed();
    return out;
}

bool GfcParser::parseInstanceAt(const QString& text, int startPos, ParsedInstance* out)
{
    if (!out) return false;
    *out = ParsedInstance{};

    // 从当前行起点开始尝试匹配：#n=CLASS(
    int lineStart = text.lastIndexOf('\n', qMax(0, startPos));
    if (lineStart < 0) lineStart = 0; else ++lineStart;

    QRegularExpression re(
        R"(#\s*([0-9]+)\s*=\s*([A-Za-z0-9_]+)\s*\()",
        QRegularExpression::CaseInsensitiveOption);
    auto m = re.match(text, lineStart);
    if (!m.hasMatch()) {
        // 再从传入位置尝试一次（例如传进来就是 '#' 的位置）
        m = re.match(text, startPos);
        if (!m.hasMatch()) return false;
    }

    const int hashStart = m.capturedStart(0);
    const int idx = m.captured(1).toInt();
    const QString clsUpper = m.captured(2).toUpper();

    // 左括号位置
    const int openPos = text.indexOf('(', m.capturedEnd(0) - 1);
    if (openPos < 0) return false;

    // 向后扫描，找到与之匹配的右括号 closePos（指向')'本身）
    int closePos = -1;
    int depth = 0;
    bool inStr = false;
    for (int i = openPos; i < text.size(); ++i) {
        const QChar ch = text[i];
        if (inStr) {
            // STEP 风格转义：两个单引号表示一个 '
            if (i + 1 < text.size() && text[i] == '\'' && text[i + 1] == '\'') { ++i; continue; }
            if (ch == '\'') inStr = false;
            continue;
        }
        if (ch == '\'') { inStr = true; continue; }
        if (ch == '(') { ++depth; continue; }
        if (ch == ')') {
            --depth;
            if (depth == 0) { closePos = i; break; }
            continue;
        }
    }
    if (closePos < 0) return false; // 未闭合

    // 实例结束位置（含右括号与可选分号）
    int endPos = closePos + 1;
    if (endPos < text.size() && text[endPos] == ';') ++endPos;

    // 只取 openPos+1 .. closePos-1 的内容作为参数区 —— 不会把 ')' 带进去
    const QString paramZone = text.mid(openPos + 1, closePos - (openPos + 1));
    const QStringList params = splitTopLevelCsv(paramZone);

    out->index = idx;
    out->classUpper = clsUpper;
    out->params = params;
    out->start = hashStart;
    out->end = endPos;
    return true;
}



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
