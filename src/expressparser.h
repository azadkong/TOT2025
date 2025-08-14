#pragma once
#include <QString>
#include <QHash>
#include <QSet>
#include <QStringList>
#include <QVector>
#include <optional>

/**
 * 轻量 EXPRESS(.exp) 解析器（工程骨架用）：
 * - 解析 ENTITY 名称
 * - 解析 SUBTYPE OF(父类)
 * - 解析属性行（形如：  Name : Type; ）
 * 注意：为简化语法，本解析器不求覆盖全部 EXPRESS 语法，仅按常见格式提取。
 */

struct ExpClassInfo {
    QString name;
    QString parent;            // 若无则为空
    QStringList attributes;    // 原始属性定义行（去掉末尾分号）
};

class ExpressParser {
public:
    bool parseFile(const QString& filePath, QString* err = nullptr);

    const QHash<QString, ExpClassInfo>& classes() const { return classes_; }
    QHash<QString, QSet<QString>> buildChildrenMap() const;

private:
    QHash<QString, ExpClassInfo> classes_;

    static QString trimSemicolon(const QString& s);
};
