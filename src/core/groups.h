#pragma once

#include "types.h"

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

namespace sheaf {

// A sidebar entry. Built in groups match on MIME types, custom groups can also
// match on a folder and on tags
struct Group {
    QString id;
    QString title;
    QStringList iconNames;
    QStringList patterns;
    QString folder;
    QStringList tags;
    bool builtin = true;
    bool favoritesOnly = false;
    bool hidden = false;

    QString displayTitle() const;
    QString iconName() const { return iconNames.isEmpty() ? QStringLiteral("text-x-generic") : iconNames.first(); }
};

bool mimeMatches(const QStringList &patterns, const QString &mime);
bool groupMatches(const Group &group, const DocumentInfo &doc);

// Stable colour index per broad document family, used for placeholders
int familyForMime(const QString &mime);

class GroupsConfig {
public:
    static GroupsConfig defaults();

    // A missing file gives defaults. A damaged file gives defaults and a warning
    static GroupsConfig load(const QString &file, QString *warning = nullptr);
    bool save(const QString &file, QString *error = nullptr) const;

    QVector<Group> allGroups() const;
    QVector<Group> visibleGroups() const;
    Group groupById(const QString &id, bool *found = nullptr) const;

    QString addCustom(Group group);
    bool updateCustom(const Group &group);
    bool removeCustom(const QString &id);
    void setHidden(const QString &id, bool hidden);
    void move(const QString &id, int delta);
    void resetToDefaults();

    // Merges custom groups and visibility from a Thingy groups.json
    int importThingy(const QString &file, QString *error = nullptr);

    QString activeGroup() const { return m_active; }
    void setActiveGroup(const QString &id) { m_active = id; }

    static bool validCustomId(const QString &id);
    static QVector<Group> builtinGroups();

private:
    void normalize();
    static bool parseCustom(const QJsonObject &object, Group *group);

    QStringList m_hidden;
    QStringList m_order;
    QVector<Group> m_custom;
    QString m_active = QStringLiteral("all");
};

} // namespace sheaf
