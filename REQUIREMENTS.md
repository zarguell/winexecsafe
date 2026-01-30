## C++ AppContainer Process Jail Wrapper - Requirements Specification

### Background

We need to sandbox untrusted processes (such as AI agents running code) on Windows systems that cannot reliably run Docker containers. The solution must use Windows AppContainer technology to create a filesystem jail that: [malwaretech](https://www.malwaretech.com/2015/09/advanced-desktop-application-sandboxing.html)
- Restricts file read/write operations to a specific subdirectory
- Allows execution of system commands (like `cmd.exe`, `dir`, `ls` via PowerShell) that exist outside the jail
- Provides kernel-level isolation without requiring full virtualization [blahcat.github](https://blahcat.github.io/2020-12-29-cheap-sandboxing-with-appcontainers/)
- Works on Windows 8+ (including Windows 10/11) [learn.microsoft](https://learn.microsoft.com/en-us/windows/win32/secauthz/appcontainer-isolation)

### Objective

Create a C++ command-line wrapper program that launches any target executable within an AppContainer sandbox, restricting filesystem access to a designated jail directory while allowing system binaries to execute normally. [github](https://github.com/forderud/RunInSandbox)

### Detailed Task List

#### ✅ Task 1: Project Setup and Dependencies
- [x] Create a new C++ console application project (Visual Studio or CMake-based)
- [x] Target Windows SDK 10.0 or higher
- [x] Include required headers: `<windows.h>`, `<userenv.h>`, `<sddl.h>`, `<aclapi.h>`
- [x] Link required libraries: `userenv.lib`, `advapi32.lib`
- [x] Set character set to Unicode (use wide string functions throughout)

#### ✅ Task 2: Command-Line Argument Parsing
- [x] Implement argument parser that accepts:
  - `--config <path>`: Path to INI configuration file (optional, default: `C:\ProgramData\winexecsafe\config.ini`)
  - `--executable <path>`: Full path to the target executable to run in the jail (required if not in config)
  - `--args <string>`: Command-line arguments to pass to the target executable (optional)
  - `--jail-dir <path>`: Path to the jail directory where process can read/write (required if not in config)
  - `--container-name <name>`: Unique name for the AppContainer profile (default: auto-generate from timestamp)
  - `--allow-network`: Flag to grant network capabilities (optional, default: false)
  - `--working-dir <path>`: Working directory inside jail (optional, default: jail root)
  - `--cleanup`: Delete AppContainer profile after process exits (optional, default: true)

**Precedence rules**:
- [x] Command-line arguments override config file settings
- [x] If no `--config` specified, attempt to load from default path `C:\ProgramData\winexecsafe\config.ini`
- [x] If config file doesn't exist and required args not provided via command line, exit with error
- [x] Command-line only mode: if all required args provided via CLI, config file is optional

Validate that:
- [ ] Executable path exists and is a valid file
- [ ] Jail directory exists (create if it doesn't)
- [ ] Paths are absolute, not relative

#### ✅ Task 3: Configuration File Loading and Parsing
- [x] Implement `LoadConfigFile()` function that reads INI file from specified path:

**INI File Format** (`C:\ProgramData\winexecsafe\config.ini`):
```ini
[General]
Executable=C:\path\to\target.exe
Arguments=--arg1 value1 --arg2 value2
JailDirectory=C:\jail\directory
WorkingDirectory=C:\jail\directory\work
ContainerName=MyAppContainer
AllowNetwork=false
Cleanup=true

[Permissions]
# Additional read-only directories (optional, semicolon-separated)
AdditionalReadPaths=C:\Program Files\MyApp;C:\SharedData

[Logging]
# Optional: Enable detailed logging
Verbose=false
LogFile=C:\ProgramData\winexecsafe\logs\wrapper.log
```

**Implementation requirements**:
- [x] Use Windows API `GetPrivateProfileString()` function to read INI values [pkg.go](https://pkg.go.dev/golang.org/x/sys/windows)
- [x] Read each setting from appropriate section (e.g., `GetPrivateProfileString(L"General", L"Executable", ...)`)
- [x] Parse boolean values (true/false, yes/no, 1/0)
- [x] Handle missing sections/keys gracefully with appropriate defaults
- [x] Support comments in INI file (lines starting with `#` or `;`)
- [x] Trim whitespace from values
- [x] Expand environment variables in paths using `ExpandEnvironmentStrings()` (e.g., `%USERPROFILE%\jail`)

**Directory creation**:
- [x] Check if `C:\ProgramData\winexecsafe\` directory exists
- [x] Create it with `CreateDirectory()` if missing
- [ ] Set appropriate permissions so non-admin users can read config (but only admins can write)

**Error handling**:
- [x] If config file path is explicitly specified via `--config` but file doesn't exist, exit with clear error
- [x] If using default config path and file doesn't exist, continue without error (rely on command-line args)
- [x] Log warning if config file has invalid/unparseable values, use defaults for those specific settings
- [x] Validate all paths from config file same as command-line arguments

**Merging logic**:
- [x] Create `MergeConfiguration()` function that:
  - [x] Loads config file first (if exists)
  - [x] Overlays command-line arguments on top
  - [x] Command-line args always take precedence
  - [x] Returns final merged configuration structure

#### ✅ Task 4: AppContainer Profile Management
- [x] Implement `CreateOrGetAppContainerProfile()` function that:
  - [x] Calls `CreateAppContainerProfile()` with the specified container name [learn.microsoft](https://learn.microsoft.com/en-us/windows/win32/api/userenv/nf-userenv-createappcontainerprofile)
  - [x] Provides display name and description parameters
  - [x] Handles `HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS)` by calling `DeriveAppContainerSidFromAppContainerName()` instead [scorpiosoftware](https://scorpiosoftware.net/2019/01/15/fun-with-appcontainers/)
  - [x] Stores the returned `PSID` (AppContainer SID) for later use
  - [x] Implements error checking for all API calls with descriptive error messages

- [x] Implement `DeleteAppContainerProfile()` cleanup function:
  - [x] Calls `DeleteAppContainerProfile()` with container name
  - [x] Only executes if `--cleanup` flag is set
  - [x] Handles errors gracefully (log but don't fail)

#### ✅ Task 5: Filesystem Permission Configuration
- [x] Implement `GrantDirectoryAccess()` function that modifies ACLs to allow AppContainer access: [stackoverflow](https://stackoverflow.com/questions/73139781/run-net-application-in-windows-appcontainer)
  - [x] Takes jail directory path and AppContainer SID as parameters
  - [x] Calls `GetNamedSecurityInfo()` to retrieve current DACL
  - [x] Creates new ACE (Access Control Entry) granting `GENERIC_READ | GENERIC_WRITE | GENERIC_EXECUTE` to the AppContainer SID
  - [x] Uses `SetEntriesInAcl()` to merge new ACE with existing DACL
  - [x] Calls `SetNamedSecurityInfo()` to apply modified DACL
  - [x] Applies recursively to all subdirectories and files in jail directory
  - [x] Implements proper error handling and memory cleanup with `LocalFree()`

- [x] Additionally grant read-only access to:
  - [x] `C:\Windows\System32` (for system binaries like cmd.exe)
  - [x] `C:\Windows\SysWOW64` (for 32-bit binaries on 64-bit Windows)
  - [x] Any additional paths specified in `[Permissions].AdditionalReadPaths` from config file
  - [x] Only if these directories exist

#### ✅ Task 6: Capability Configuration
- [x] Implement `BuildCapabilities()` function that:
  - [x] Creates array of `SID_AND_ATTRIBUTES` structures [blahcat.github](https://blahcat.github.io/2020-12-29-cheap-sandboxing-with-appcontainers/)
  - [x] If `--allow-network` flag is set, add capabilities:
    - [x] `SECURITY_CAPABILITY_INTERNET_CLIENT`
    - [x] `SECURITY_CAPABILITY_INTERNET_CLIENT_SERVER`
  - [x] Uses `AllocateAndInitializeSid()` to create capability SIDs from well-known SID authorities [malwaretech](https://www.malwaretech.com/2015/09/advanced-desktop-application-sandboxing.html)
  - [x] Returns populated `SECURITY_CAPABILITIES` structure with:
    - [x] AppContainer SID
    - [x] Capabilities array
    - [x] Capability count
    - [x] Reserved fields set to 0

#### ✅ Task 7: Process Creation with Extended Attributes
- [x] Implement `LaunchInAppContainer()` function that:
  - [x] Initializes `STARTUPINFOEX` structure (not basic `STARTUPINFO`) [learn.microsoft](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessa)
  - [x] Allocates attribute list using `InitializeProcThreadAttributeList()`:
    - [x] First call with NULL to get required size
    - [x] Allocate buffer of correct size
    - [x] Second call to initialize
  - [x] Calls `UpdateProcThreadAttribute()` with:
    - [x] `PROC_THREAD_ATTRIBUTE_SECURITY_CAPABILITIES`
    - [x] Pointer to `SECURITY_CAPABILITIES` structure [learn.microsoft](https://learn.microsoft.com/en-us/windows/win32/secauthz/appcontainer-isolation)
  - [x] Constructs command line string: `"<executable>" <args>`
  - [x] Sets working directory to the jail directory or specified `--working-dir`
  - [x] Calls `CreateProcess()` with:
    - [x] `CREATE_SUSPENDED` flag initially
    - [x] `EXTENDED_STARTUPINFO_PRESENT` flag
    - [x] Extended startup info containing AppContainer attributes
  - [x] Resumes main thread with `ResumeThread()` after validation
  - [x] Waits for process completion with `WaitForSingleObject()`
  - [x] Retrieves exit code with `GetExitCodeProcess()`
  - [x] Returns child process exit code to caller

#### ✅ Task 8: Error Handling and Logging
- [x] Create helper function `LogError()` that uses `GetLastError()` and `FormatMessage()` to convert error codes to readable strings
- [x] Log to stderr with format: `[ERROR] <function_name>: <error_message> (Code: <hex_code>)`
- [x] If `[Logging].LogFile` is specified in config, also write to that file with timestamps
- [x] If `[Logging].Verbose=true`, log all major steps (profile creation, ACL modifications, process launch)
- [x] For each Windows API call, check return value and log errors immediately
- [x] Provide context-specific error messages (e.g., "Failed to create AppContainer profile", "Failed to grant directory access")
- [x] Use RAII patterns or explicit cleanup blocks to ensure resources are freed even on error paths

#### ✅ Task 9: Resource Cleanup
- [x] Implement proper cleanup in reverse order of allocation:
  - [x] Close process and thread handles with `CloseHandle()`
  - [x] Free attribute list with `DeleteProcThreadAttributeList()`
  - [x] Free capability SIDs with `FreeSid()`
  - [x] Free ACL memory with `LocalFree()`
  - [x] Free AppContainer SID with `FreeSid()`
  - [x] Delete AppContainer profile if `--cleanup` flag is set
  - [x] Use smart pointers or scope guards where possible to prevent leaks

#### ✅ Task 10: Main Program Flow
- [x] Structure `main()` function to:
  1. [x] Parse command-line arguments
  2. [x] Load configuration file (if specified or if default exists)
  3. [x] Merge command-line args with config file (CLI takes precedence)
  4. [x] Validate final merged configuration
  5. [x] Create or retrieve AppContainer profile
  6. [x] Grant filesystem access to jail directory (and system directories, plus any additional paths from config)
  7. [x] Build capability list based on flags
  8. [x] Launch target executable in AppContainer
  9. [x] Wait for process completion
  10. [x] Clean up all resources
  11. [x] Return child process exit code as program exit code

#### Task 11: Testing and Validation
Create test cases for:
- **Basic execution**: Run `cmd.exe /c echo Hello` in jail, verify output
- **Filesystem isolation**: Attempt to read/write files outside jail, verify ACCESS_DENIED errors
- **Jail directory access**: Create/read/write files within jail directory, verify success
- **System command execution**: Run `dir`, `type`, PowerShell commands that use system binaries
- **Exit code propagation**: Verify wrapper returns same exit code as child process
- **Config file loading**: Test with valid config file, verify settings applied
- **Config precedence**: Provide conflicting settings via config and CLI, verify CLI wins
- **Missing config file**: Test with non-existent default config path, verify graceful handling
- **Invalid config values**: Test with malformed INI file, verify defaults used
- **Additional read paths**: Configure extra directories in config, verify access granted
- **Error conditions**: Invalid executable path, non-existent jail directory, insufficient permissions
- **Cleanup**: Verify AppContainer profile is deleted after execution (check with `powershell Get-AppxPackage`)

### Expected Deliverables

1. Single C++ source file (or header + implementation pair)
2. CMakeLists.txt or Visual Studio project file
3. Sample `config.ini` file with all options documented
4. README with build instructions and usage examples (both CLI and config file modes)
5. Working executable that can sandbox any Windows program
6. All code must compile without warnings at `/W4` level

### Success Criteria

- Target executable runs successfully within AppContainer
- Configuration can be provided via INI file, command-line, or combination
- Command-line arguments correctly override config file settings
- File operations restricted to jail directory (verified by attempting to access `C:\` or user directories and receiving errors)
- System commands execute normally (cmd.exe, PowerShell work correctly)
- Additional read paths from config file properly granted
- Clean exit with proper resource cleanup (no memory leaks detectable by tools like Application Verifier)
- Exit code from sandboxed process correctly propagated to wrapper
- Config file parsing handles malformed input gracefully
