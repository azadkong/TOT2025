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
};
