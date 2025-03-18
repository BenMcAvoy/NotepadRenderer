#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <TlHelp32.h>
#undef ERROR

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <thread>
#include <string>
#include <chrono>
#include <vector>

// Terminal color definitions
#define GREEN_TEXT "\033[32m"
#define RED_TEXT "\033[91m"
#define RESET_TEXT "\033[0m"

// Log helpers
#define INFO(x) std::cout << GREEN_TEXT << "[+] " << x << RESET_TEXT << std::endl
#define ERROR(x) std::cerr << RED_TEXT << "[!] " << x << " (" << __FUNCTION__ << " | " __FILE__ << ":" << __LINE__ << ")"  << RESET_TEXT << std::endl

namespace fs = std::filesystem;

// Constants
constexpr const char* TARGET_PROCESS = "notepad.exe";
constexpr const char* TARGET_PATH = "C:\\Windows\\System32\\notepad.exe";
constexpr int PROCESS_STARTUP_DELAY_MS = 200;

// Function declarations
std::vector<fs::path> findFiles();
fs::path findDllToInject(const std::vector<fs::path>& files);
DWORD findOrStartProcess(const std::string_view& processName, const std::string_view& executablePath);
bool injectDll(DWORD pid, const fs::path& dllPath);
bool isDllInjected(DWORD pid, const fs::path& dllPath, MODULEENTRY32* outModule = nullptr);
bool unloadDll(DWORD pid, MODULEENTRY32& module);
fs::path getTempDirectory();
fs::path copyFilesToTemp(const std::vector<fs::path>& files);
fs::path getTempFilePath(const fs::path& tempDir, const fs::path& file);

int main(void) {
    // Find all files in the current directory
    std::vector<fs::path> files = findFiles();
    if (files.empty()) {
        ERROR("No files found in current directory");
        return 1;
    }

    INFO("Found " + std::to_string(files.size()) + " files in directory");
    
    // Find the DLL to inject among the files
    fs::path dllPath = findDllToInject(files);
    if (dllPath.empty()) {
        ERROR("No DLL found in current directory");
        return 1;
    }

    INFO("DLL found for injection: `" + dllPath.filename().string() + "`");

    // Find or start target process
    DWORD pid = findOrStartProcess(TARGET_PROCESS, TARGET_PATH);
    if (pid == 0) {
        ERROR("Failed to find or start process");
        return 1;
    }

    // Give process time to initialize
    std::this_thread::sleep_for(std::chrono::milliseconds(PROCESS_STARTUP_DELAY_MS));

    // Create temp directory and copy all files
    fs::path tempDir = copyFilesToTemp(files);
    if (tempDir.empty()) {
        ERROR("Failed to copy files to temp directory");
        return 1;
    }

    // Get the path to the DLL in the temp directory
    fs::path tempDllPath = getTempFilePath(tempDir, dllPath.filename());

    // Check if the DLL is already injected
    MODULEENTRY32 module = { 0 };
    if (isDllInjected(pid, tempDllPath, &module)) {
        INFO("DLL already injected, unloading it first");

        if (!unloadDll(pid, module)) {
            ERROR("Failed to unload DLL");
            return 1;
        }
        
        // Brief pause to ensure DLL is fully unloaded
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    INFO("Injecting DLL into " + std::string(TARGET_PROCESS));
    
    if (!injectDll(pid, tempDllPath)) {
        ERROR("DLL injection failed");
        return 1;
    }

    // No need to keep the launcher running anymore - hook is managed by the DLL
    return 0;
}

std::vector<fs::path> findFiles() {
    std::vector<fs::path> files;
    
    // Get module directory
    char modulePath[MAX_PATH];
    if (!GetModuleFileNameA(nullptr, modulePath, MAX_PATH)) {
        ERROR("GetModuleFileNameA failed");
        return files;
    }
    
    fs::path moduleDir = fs::path(modulePath).parent_path();
    INFO("Searching for files in: " + moduleDir.string());

    try {
        // Get all files in the directory
        for (const auto& entry : fs::directory_iterator(moduleDir)) {
            if (fs::is_regular_file(entry.status())) {
                files.push_back(entry.path());
            }
        }
    } catch (const fs::filesystem_error& e) {
        ERROR("Error iterating directory: " + std::string(e.what()));
    }

    return files;
}

fs::path findDllToInject(const std::vector<fs::path>& files) {
    // Find the first DLL in the list of files
    for (const auto& file : files) {
        if (file.extension() == ".dll") {
            return file;
        }
    }
    return {};
}

fs::path getTempDirectory() {
    fs::path tempBase = fs::temp_directory_path();
    
    // Create a unique directory name using timestamp
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    
    fs::path tempDir = tempBase / ("InbetweenLines_" + std::to_string(timestamp));
    
    try {
        // Create the directory if it doesn't exist
        if (!fs::exists(tempDir)) {
            fs::create_directory(tempDir);
        }
    } catch (const fs::filesystem_error& e) {
        ERROR("Failed to create temp directory: " + std::string(e.what()));
        return {};
    }
    
    return tempDir;
}

fs::path copyFilesToTemp(const std::vector<fs::path>& files) {
    if (files.empty()) {
        ERROR("No files to copy");
        return {};
    }
    
    fs::path tempDir = getTempDirectory();
    if (tempDir.empty()) {
        return {};
    }
    
    INFO("Created temp directory: " + tempDir.string());
    
    int copyCount = 0;
    for (const auto& file : files) {
        try {
            fs::path destPath = tempDir / file.filename();
            
            // Copy the file to the temp directory
            fs::copy_file(file, destPath, fs::copy_options::overwrite_existing);
            copyCount++;
            
        } catch (const fs::filesystem_error& e) {
            ERROR("Failed to copy file " + file.filename().string() + ": " + std::string(e.what()));
        }
    }
    
    INFO("Copied " + std::to_string(copyCount) + " files to temp directory");
    
    return tempDir;
}

fs::path getTempFilePath(const fs::path& tempDir, const fs::path& file) {
    if (tempDir.empty() || file.empty()) {
        ERROR("Invalid parameters for getTempFilePath");
        return {};
    }
    
    return tempDir / file;
}

DWORD findOrStartProcess(const std::string_view& processName, const std::string_view& executablePath) {
    // Create snapshot of running processes
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) {
        ERROR("CreateToolhelp32Snapshot failed");
        return 0;
    }

    // Search for the process
    PROCESSENTRY32 pe32 = { sizeof(PROCESSENTRY32) };
    if (!Process32First(hSnap, &pe32)) {
        ERROR("Process32First failed");
        CloseHandle(hSnap);
        return 0;
    }

    DWORD pid = 0;
    do {
        if (strcmp(pe32.szExeFile, processName.data()) == 0) {
            pid = pe32.th32ProcessID;
            break;
        }
    } while (Process32Next(hSnap, &pe32));

    CloseHandle(hSnap);

    if (pid == 0) {
        INFO("Process not found, starting it");
        STARTUPINFOA si = { sizeof(STARTUPINFOA) };
        PROCESS_INFORMATION pi = { 0 };
        
        if (!CreateProcessA(executablePath.data(), nullptr, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
            ERROR("CreateProcess failed");
            return 0;
        }
        
        pid = pi.dwProcessId;
        
        // Close handles we don't need anymore
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    } else {
        INFO("Process found with PID: " + std::to_string(pid));
    }
    
    return pid;
}

bool injectDll(DWORD pid, const fs::path& dllPath) {
    // Validate input parameters
    if (pid == 0 || dllPath.empty() || !fs::exists(dllPath)) {
        ERROR("Invalid parameters for DLL injection");
        return false;
    }

    // Open target process
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (hProcess == nullptr) {
        ERROR("OpenProcess failed");
        return false;
    }
    
    // Convert DLL path to string and ensure it's null-terminated
    std::string dllPathStr = dllPath.string();
    size_t pathSize = dllPathStr.size() + 1; // Include null terminator
    
    // Allocate memory in target process
    LPVOID pDll = VirtualAllocEx(hProcess, nullptr, pathSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (pDll == nullptr) {
        ERROR("VirtualAllocEx failed");
        CloseHandle(hProcess);
        return false;
    }
    
    // Write DLL path to target process memory
    if (!WriteProcessMemory(hProcess, pDll, dllPathStr.c_str(), pathSize, nullptr)) {
        ERROR("WriteProcessMemory failed");
        VirtualFreeEx(hProcess, pDll, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }
    
    // Get LoadLibraryA address
    LPVOID pLoadLibrary = GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");
    if (pLoadLibrary == nullptr) {
        ERROR("GetProcAddress failed for LoadLibraryA");
        VirtualFreeEx(hProcess, pDll, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }
    
    // Create remote thread to load DLL
    HANDLE hThread = CreateRemoteThread(
        hProcess, 
        nullptr, 
        0, 
        reinterpret_cast<LPTHREAD_START_ROUTINE>(pLoadLibrary), 
        pDll, 
        0, 
        nullptr
    );
    
    if (hThread == nullptr) {
        ERROR("CreateRemoteThread failed");
        VirtualFreeEx(hProcess, pDll, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }
    
    // Wait for thread to complete
    WaitForSingleObject(hThread, INFINITE);
    
    // Check if DLL was loaded successfully
    DWORD exitCode = 0;
    if (GetExitCodeThread(hThread, &exitCode) && exitCode == 0) {
        ERROR("DLL loading failed in remote thread");
        VirtualFreeEx(hProcess, pDll, 0, MEM_RELEASE);
        CloseHandle(hThread);
        CloseHandle(hProcess);
        return false;
    }
    
    // Clean up resources
    VirtualFreeEx(hProcess, pDll, 0, MEM_RELEASE);
    CloseHandle(hThread);
    CloseHandle(hProcess);
    
    return true;
}

bool isDllInjected(DWORD pid, const fs::path& dllPath, MODULEENTRY32* outModule) {
    if (pid == 0 || dllPath.empty()) {
        ERROR("Invalid parameters for isDllInjected");
        return false;
    }
    
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
    if (hSnap == INVALID_HANDLE_VALUE) {
        ERROR("CreateToolhelp32Snapshot failed");
        return false;
    }
    
    MODULEENTRY32 me32 = { sizeof(MODULEENTRY32) };
    if (!Module32First(hSnap, &me32)) {
        DWORD err = GetLastError();
        if (err != ERROR_NO_MORE_FILES) {
            ERROR("Module32First failed");
        }
        CloseHandle(hSnap);
        return false;
    }
    
    // Extract just the filename from the DLL path we want to check.
    std::string targetFilename = dllPath.filename().string();
    bool found = false;
    
    do {
        // Extract the filename from the module's full path.
        std::string moduleFilename = fs::path(me32.szExePath).filename().string();
        if (_stricmp(moduleFilename.c_str(), targetFilename.c_str()) == 0) {
            found = true;
            if (outModule != nullptr) {
                *outModule = me32;
            }
            break;
        }
    } while (Module32Next(hSnap, &me32));
    
    CloseHandle(hSnap);
    return found;
}

bool unloadDll(DWORD pid, MODULEENTRY32& module) {
    if (pid == 0 || module.modBaseAddr == nullptr) {
        ERROR("Invalid parameters for unloadDll");
        return false;
    }
    
    // Open target process
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (hProcess == nullptr) {
        ERROR("OpenProcess failed");
        return false;
    }

    // Get the address of FreeLibrary in kernel32.dll
    LPVOID pFreeLibrary = GetProcAddress(GetModuleHandleA("kernel32.dll"), "FreeLibrary");
    if (pFreeLibrary == nullptr) {
        ERROR("GetProcAddress failed");
        CloseHandle(hProcess);
        return false;
    }

    // Create remote thread to call FreeLibrary
    HANDLE hThread = CreateRemoteThread(
        hProcess, 
        nullptr, 
        0, 
        reinterpret_cast<LPTHREAD_START_ROUTINE>(pFreeLibrary), 
        module.hModule,
        0, 
        nullptr
    );

    if (hThread == nullptr) {
        ERROR("CreateRemoteThread failed");
        CloseHandle(hProcess);
        return false;
    }

    // Wait for thread to complete
    WaitForSingleObject(hThread, INFINITE);
    
    // Verify FreeLibrary succeeded
    DWORD exitCode = 0;
    if (GetExitCodeThread(hThread, &exitCode) && exitCode == 0) {
        ERROR("FreeLibrary failed in remote thread");
        CloseHandle(hThread);
        CloseHandle(hProcess);
        return false;
    }

    // Clean up resources
    CloseHandle(hThread);
    CloseHandle(hProcess);
    
    return true;
}
