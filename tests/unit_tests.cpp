#include <windows.h>
#include <cassert>
#include <iostream>
#include "../src/config.h"

void TestTrimString() {
    std::cout << "Testing TrimString..." << std::endl;
    
    assert(TrimString(L"  hello  ") == L"hello");
    assert(TrimString(L"hello") == L"hello");
    assert(TrimString(L"  ") == L"");
    assert(TrimString(L"\t\nhello\r\n") == L"hello");
    
    std::cout << "  TrimString tests passed!" << std::endl;
}

void TestParseBool() {
    std::cout << "Testing ParseBool..." << std::endl;
    
    assert(ParseBool(L"true") == true);
    assert(ParseBool(L"TRUE") == true);
    assert(ParseBool(L"True") == true);
    assert(ParseBool(L"yes") == true);
    assert(ParseBool(L"YES") == true);
    assert(ParseBool(L"1") == true);
    
    assert(ParseBool(L"false") == false);
    assert(ParseBool(L"FALSE") == false);
    assert(ParseBool(L"False") == false);
    assert(ParseBool(L"no") == false);
    assert(ParseBool(L"NO") == false);
    assert(ParseBool(L"0") == false);
    assert(ParseBool(L"invalid") == false);
    
    std::cout << "  ParseBool tests passed!" << std::endl;
}

void TestParseCommandLine() {
    std::cout << "Testing ParseCommandLine..." << std::endl;
    
    wchar_t* args1[] = {L"program", L"--executable", L"C:\\test.exe", L"--jail-dir", L"C:\\jail"};
    Config config1;
    assert(ParseCommandLine(5, args1, config1));
    assert(config1.executable == L"C:\\test.exe");
    assert(config1.jailDir == L"C:\\jail");
    assert(config1.allowNetwork == false);
    assert(config1.cleanup == true);
    
    wchar_t* args2[] = {L"program", L"--allow-network", L"--no-cleanup", L"--verbose"};
    Config config2;
    assert(ParseCommandLine(4, args2, config2));
    assert(config2.allowNetwork == true);
    assert(config2.cleanup == false);
    assert(config2.verbose == true);
    
    wchar_t* args3[] = {L"program", L"--executable"};
    Config config3;
    assert(!ParseCommandLine(2, args3, config3));
    
    std::cout << "  ParseCommandLine tests passed!" << std::endl;
}

void TestMergeConfiguration() {
    std::cout << "Testing MergeConfiguration..." << std::endl;
    
    Config base;
    base.executable = L"C:\\base.exe";
    base.jailDir = L"C:\\base_jail";
    base.allowNetwork = false;
    base.cleanup = true;
    
    Config cli;
    cli.executable = L"C:\\cli.exe";
    cli.allowNetwork = true;
    cli.noCleanup = false;
    
    MergeConfiguration(cli, base);
    
    assert(base.executable == L"C:\\cli.exe");
    assert(base.jailDir == L"C:\\base_jail");
    assert(base.allowNetwork == true);
    assert(base.cleanup == false);
    
    std::cout << "  MergeConfiguration tests passed!" << std::endl;
}

int main() {
    std::cout << "=== Running WinExecSafe Unit Tests ===" << std::endl;
    
    TestTrimString();
    TestParseBool();
    TestParseCommandLine();
    TestMergeConfiguration();
    
    std::cout << "=== All Unit Tests Passed! ===" << std::endl;
    return 0;
}