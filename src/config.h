#ifndef WINEXECSAFE_CONFIG_H
#define WINEXECSAFE_CONFIG_H

#include "common.h"

bool ParseCommandLine(int argc, wchar_t* argv[], Config& config);
bool LoadConfigFile(const std::wstring& path, Config& config);
void MergeConfiguration(Config& cliConfig, Config& mergedConfig);
bool ValidateConfiguration(const Config& config);

#endif // WINEXECSAFE_CONFIG_H