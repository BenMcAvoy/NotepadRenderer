#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <thread>
#include <vector>
#include <string>
#include <filesystem>
#include <fstream>
#include <format>

#include "notepad.h"

static bool running = true;

int main() {
	IL::Notepad notepad;

	std::ofstream log("log.txt");
	log << "DLL injected, cwd: " << std::filesystem::current_path().string() << std::endl;

	std::ifstream badappleFile("badapple.txt");
	std::vector<std::string> badappleFrames;
	if (!badappleFile.is_open()) {
		log << "Failed to open badapple.txt" << std::endl;
		return 1;
	}

	std::string frame;
	while (std::getline(badappleFile, frame)) {
		badappleFrames.emplace_back(frame);
	}

	int frameCount = 0;
	for (const auto& frame : badappleFrames) {
		if (!running) {
			break;
		}

		notepad.Begin();
		notepad.Text(frame, 0, 0, false);

		// Display the time/progress
		float currentTime = frameCount++ / 30.0f; // Convert frame to seconds at 30 fps
		float totalTime = badappleFrames.size() / 30.0f; // Total duration in seconds
		std::string time = std::format(" {:.1f}s/{:.1f}s  By Ben McAvoy (https://github.com/BenMcAvoy)", currentTime, totalTime);
		notepad.Text(time, 1, IL::NOTEPAD_HEIGHT - 1, false);
		notepad.End();
	}

	return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
		CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)main, hModule, 0, nullptr);

		char dllPath[MAX_PATH];
		GetModuleFileNameA(hModule, dllPath, MAX_PATH);
		std::filesystem::current_path(std::filesystem::path(dllPath).parent_path());
    }

    return TRUE;
}
