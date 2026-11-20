#pragma once

#include <vector>

struct SearchCriterion
{
    CString paramSys;
    CString paramCaption;
    CString value;
};

struct UserSettings
{
    CString lastUser;
    CString lastServer;
    CString lastDatabase;
    CString lastNameFilter;
    std::vector<SearchCriterion> lastCriteria;

    CString FilePath() const;
    bool Load();
    bool Save() const;
};
