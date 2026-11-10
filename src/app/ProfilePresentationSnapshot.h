#pragma once
#include "model/ProfileListData.h"
#include "app/TargetWindowDetailsController.h"
#include <QVector>
class ProfileManager;
class Project;

// GUI-owner-thread model reads. Consumers render frozen values only.
class ProfilePresentationSnapshot {
public:
    struct List {QVector<ProfileListRow> rows;QString activeId;};
    static List list(const ProfileManager&);
    static bool isDefault(const ProfileManager*);
    static QString documentTitle(const Project*);
    static TargetWindowDetailsController::Request targetDetails(
        const ProfileManager*,const Project*,TargetWindowDetailsController::Request);
};
