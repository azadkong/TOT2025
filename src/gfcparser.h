#pragma once
#include <QString>
#include <QHash>
#include <QVector>
#include <QPair>

/**
 * 极简 GFC 文本解析辅助：
 * - 统计 DATA 段内实例行的类名（如 #12=GFCWALL(...); -> 类名 GFCWALL）
 * - 记录每类实例出现次数
 * - 记录每个实例编号在文本中的位置（预留给后续“定位/跳转/属性区联动”）
 */

struct ParsedInstance {
    int index = -1;          // #123 -> 123
    QString classUpper;      // 文本中的大写类名，如 GFCVECTOR3D
    QStringList params;      // 顶层逗号分隔的参数（已按括号栈切分）
    int start = -1;          // 实例起始字符位置（#开始处）
    int end = -1;            // 实例结束字符位置（含右括号与可选分号）
};

struct GfcInstanceRef {
    int index = -1;    // 实例号，如 #12 -> 12
    QString cls;       // 类名，如 GFCWALL
    int pos = -1;      // 该行在文本里的起始位置（字符offset）
};

class GfcParser {
public:
    // 从整份文本中扫描，返回各类出现次数，并填充 instanceRefs
    static QHash<QString, int> countClasses(const QString& wholeText,
                                            QVector<GfcInstanceRef>* instanceRefs = nullptr);

    // 将 #123 这样的实例引用提取为数值 123，失败返回 -1
    static int parseInstanceIndex(const QString& token);

    static bool parseInstanceAt(const QString& text, int startPos, ParsedInstance* out);

private:
    // ★ 新增：切分顶层参数（忽略嵌套括号/字符串内部逗号）
    static QStringList splitTopLevelCsv(const QString& s);
};
